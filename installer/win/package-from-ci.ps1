[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$Commit = "",
    [string]$ArtifactsBucket = "mach1-build-artifacts",
    [string]$CiArtifactsDir = "ci-artifacts",
    [string]$Region = "us-east-1",
    [string]$AwsProfile = "mach1"
)

$ErrorActionPreference = "Stop"

function Exit-WithMessage {
    param(
        [string]$Message,
        [int]$Code = 1
    )

    Write-Host "ERROR: $Message" -ForegroundColor Red
    exit $Code
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments,
        [switch]$Quiet
    )

    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $output = & $FilePath @Arguments 2>&1
        $exitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }

    if (-not $Quiet -and $output) {
        $output | ForEach-Object { Write-Host $_ }
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
    }
}

function Write-NativeOutput {
    param([object[]]$Output)

    if (-not $Output) {
        return
    }

    $Output | ForEach-Object {
        $line = $_.ToString()
        if ($line -and $line -ne "System.Management.Automation.RemoteException") {
            Write-Host "  $line"
        }
    }
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $result = Invoke-Native $FilePath $Arguments
    if ($result.ExitCode -ne 0) {
        Exit-WithMessage "Command failed with exit code $($result.ExitCode): $FilePath $($Arguments -join ' ')" $result.ExitCode
    }
}

function Copy-ArtifactDirectory {
    param(
        [string]$Source,
        [string]$Destination,
        [bool]$Required = $true
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        if ($Required) {
            Exit-WithMessage "Expected CI artifact was not found: $Source"
        }

        Write-Host "NOTE: Optional CI artifact not found, skipping: $Source" -ForegroundColor Yellow
        return
    }

    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force | Copy-Item -Destination $Destination -Recurse -Force
    Write-Host "Installed CI artifact: $Source -> $Destination"
}

function Update-ProcessPath {
    $machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = @($machinePath, $userPath) -join ";"
}

function Resolve-Npm {
    Update-ProcessPath

    $npm = Get-Command npm.cmd -ErrorAction SilentlyContinue
    if ($npm) {
        return $npm.Source
    }

    $candidates = @(
        "$env:ProgramFiles\nodejs\npm.cmd",
        "${env:ProgramFiles(x86)}\nodejs\npm.cmd",
        "$env:AppData\npm\npm.cmd"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Invoke-ElevatedTranscoderBuild {
    param(
        [string]$NpmPath,
        [string]$TranscoderDir
    )

    Write-Host "M1-Transcoder local build needs Windows symlink privileges for electron-builder."
    Write-Host "Requesting elevation for the local transcoder build..."

    $buildScript = Join-Path ([System.IO.Path]::GetTempPath()) "m1-transcoder-elevated-build.ps1"
    $logFile = Join-Path ([System.IO.Path]::GetTempPath()) "m1-transcoder-elevated-build.log"
    $escapedNpm = $NpmPath.Replace("'", "''")
    $escapedTranscoderDir = $TranscoderDir.Replace("'", "''")
    $escapedLogFile = $logFile.Replace("'", "''")

    @"
`$ErrorActionPreference = 'Stop'
Set-Location '$escapedTranscoderDir'
`$env:Path = @([Environment]::GetEnvironmentVariable('Path', 'Machine'), [Environment]::GetEnvironmentVariable('Path', 'User')) -join ';'
Remove-Item -LiteralPath '$escapedLogFile' -Force -ErrorAction SilentlyContinue
function Invoke-Logged {
    param(
        [string]`$FilePath,
        [string[]]`$Arguments,
        [switch]`$AllowExistingNodeModules
    )
    ">> `$FilePath `$(`$Arguments -join ' ')" | Tee-Object -FilePath '$escapedLogFile' -Append
    & `$FilePath @Arguments 2>&1 | Tee-Object -FilePath '$escapedLogFile' -Append
    `$exitCode = `$LASTEXITCODE
    "Command exit code: `$exitCode" | Tee-Object -FilePath '$escapedLogFile' -Append
    if (`$exitCode -ne 0) {
        if (`$AllowExistingNodeModules -and (Test-Path -LiteralPath 'node_modules')) {
            "WARNING: npm install returned `$exitCode, but node_modules exists; continuing to package step." | Tee-Object -FilePath '$escapedLogFile' -Append
            return
        }
        exit `$exitCode
    }
}
Invoke-Logged '$escapedNpm' @('install', '--no-audit', '--no-fund') -AllowExistingNodeModules
Invoke-Logged '$escapedNpm' @('run', 'package-win')
"@ | Set-Content -LiteralPath $buildScript -Encoding UTF8

    $argumentList = "-NoProfile -ExecutionPolicy Bypass -File `"$buildScript`""
    try {
        $process = Start-Process -FilePath "powershell.exe" -ArgumentList $argumentList -Verb RunAs -Wait -PassThru
    }
    catch {
        Write-Host "ERROR: Failed to start elevated PowerShell for the transcoder build." -ForegroundColor Red
        Write-Host $_.Exception.Message
        Write-Host "Run this command from an Administrator PowerShell or enable Windows Developer Mode, then retry."
        exit 1
    }
    finally {
        Remove-Item -LiteralPath $buildScript -Force -ErrorAction SilentlyContinue
    }

    if (Test-Path -LiteralPath $logFile) {
        Get-Content -LiteralPath $logFile | ForEach-Object { Write-Host $_ }
    }

    if ($process.ExitCode -ne 0) {
        Exit-WithMessage "Elevated local M1-Transcoder build failed with exit code $($process.ExitCode)." $process.ExitCode
    }
}

function Build-LocalTranscoder {
    param([string]$OutputPath)

    $npm = Resolve-Npm
    if (-not $npm) {
        Write-Host "M1-Transcoder is missing from CI artifacts and npm was not found on PATH." -ForegroundColor Red
        Write-Host "Install Node.js/npm, then reopen PowerShell so PATH refreshes."
        Write-Host "Recommended:"
        Write-Host "  make setup"
        Write-Host "Or install Node.js LTS manually from https://nodejs.org/"
        exit 1
    }

    Write-Host "CI zip did not contain M1-Transcoder; building it locally..."
    $transcoderDir = Join-Path $repoRoot "m1-transcoder"

    if (-not (Test-IsAdministrator)) {
        Invoke-ElevatedTranscoderBuild $npm $transcoderDir
    }
    else {
        Push-Location $transcoderDir
        try {
            $installResult = Invoke-Native $npm @("install", "--no-audit", "--no-fund")
            if ($installResult.ExitCode -ne 0 -and -not (Test-Path -LiteralPath "node_modules")) {
                Exit-WithMessage "Command failed with exit code $($installResult.ExitCode): $npm install --no-audit --no-fund" $installResult.ExitCode
            }
            elseif ($installResult.ExitCode -ne 0) {
                Write-Host "WARNING: npm install returned $($installResult.ExitCode), but node_modules exists; continuing to package step." -ForegroundColor Yellow
            }
            Invoke-Checked $npm @("run", "package-win")
        }
        finally {
            Pop-Location
        }
    }

    if (-not (Test-Path -LiteralPath $OutputPath)) {
        Exit-WithMessage "Local M1-Transcoder build completed, but $OutputPath was not created."
    }

    Write-Host "Built local M1-Transcoder artifact: $OutputPath"
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repoRoot

if ([string]::IsNullOrWhiteSpace($Version) -and [string]::IsNullOrWhiteSpace($Commit)) {
    Write-Host "Specify VERSION or COMMIT:" -ForegroundColor Yellow
    Write-Host "  make package-from-ci VERSION=2.1"
    Write-Host "  make package-from-ci COMMIT=abc12345"
    exit 1
}

$aws = Get-Command aws -ErrorAction SilentlyContinue
if (-not $aws) {
    Write-Host "AWS CLI was not found on PATH." -ForegroundColor Red
    Write-Host "Install AWS CLI v2, then reopen the terminal and run 'aws configure' or configure the mach1 profile."
    Write-Host "Windows install options:"
    Write-Host "  winget install Amazon.AWSCLI"
    Write-Host "  choco install awscli"
    Write-Host "  https://docs.aws.amazon.com/cli/latest/userguide/getting-started-install.html"
    exit 1
}

$awsProfileArgs = @()
if (-not [string]::IsNullOrWhiteSpace($AwsProfile)) {
    $awsProfileArgs += @("--profile", $AwsProfile)
}

$identityArgs = @("sts", "get-caller-identity", "--region", $Region) + $awsProfileArgs
$identityResult = Invoke-Native $aws.Source $identityArgs -Quiet
if ($identityResult.ExitCode -ne 0) {
    Write-Host "AWS credentials are not available for this packaging run." -ForegroundColor Red
    if ($identityResult.Output) {
        Write-Host "AWS output:" -ForegroundColor Yellow
        Write-NativeOutput $identityResult.Output
    }
    if (-not [string]::IsNullOrWhiteSpace($AwsProfile)) {
        Write-Host "Tried AWS profile: $AwsProfile"
        Write-Host "Configure it with: aws configure --profile $AwsProfile"
        Write-Host "Or use the default AWS credential chain with: make package-from-ci VERSION=$Version AWS_PROFILE="
    }
    else {
        Write-Host "Configure credentials with: aws configure"
        Write-Host "Or pass a profile with: make package-from-ci VERSION=$Version AWS_PROFILE=mach1"
    }
    exit 1
}

New-Item -ItemType Directory -Force -Path $CiArtifactsDir | Out-Null
$zipPath = Join-Path $CiArtifactsDir "windows-builds.zip"

if (-not [string]::IsNullOrWhiteSpace($Version)) {
    $s3Path = "s3://$ArtifactsBucket/builds/$Version/windows-builds.zip"
    Write-Host "Downloading Windows CI artifacts for version $Version..."
}
else {
    $s3Path = "s3://$ArtifactsBucket/commits/$Commit/windows-builds.zip"
    Write-Host "Downloading Windows CI artifacts for commit $Commit..."
}

$downloadArgs = @("s3", "cp", $s3Path, $zipPath, "--region", $Region) + $awsProfileArgs
Invoke-Checked $aws.Source $downloadArgs

Write-Host "Extracting Windows CI artifacts..."
Expand-Archive -Path $zipPath -DestinationPath $CiArtifactsDir -Force

$windowsArtifacts = Join-Path $CiArtifactsDir "windows"
if (-not (Test-Path -LiteralPath $windowsArtifacts)) {
    Write-Host "Archive contents:" -ForegroundColor Yellow
    Get-ChildItem -LiteralPath $CiArtifactsDir -Force | ForEach-Object { Write-Host "  $($_.FullName)" }
    Exit-WithMessage "windows-builds.zip did not contain the expected 'windows' directory."
}

Copy-ArtifactDirectory (Join-Path $windowsArtifacts "M1-Monitor") "m1-monitor\build\M1-Monitor_artefacts\Release"
Copy-ArtifactDirectory (Join-Path $windowsArtifacts "M1-Panner") "m1-panner\build\M1-Panner_artefacts\Release"
Copy-ArtifactDirectory (Join-Path $windowsArtifacts "M1-Player") "m1-player\build\M1-Player_artefacts\Release"
Copy-ArtifactDirectory (Join-Path $windowsArtifacts "m1-orientationmanager") "m1-orientationmanager\build\m1-orientationmanager_artefacts\Release"
Copy-ArtifactDirectory (Join-Path $windowsArtifacts "m1-system-helper") "services\m1-system-helper\build\m1-system-helper_artefacts\Release"

$transcoderArtifact = Join-Path $windowsArtifacts "M1-Transcoder"
$localTranscoder = "m1-transcoder\dist\M1-Transcoder.exe"
if (Test-Path -LiteralPath $transcoderArtifact) {
    Copy-ArtifactDirectory $transcoderArtifact "m1-transcoder\dist"
}
elseif (Test-Path -LiteralPath $localTranscoder) {
    Write-Host "NOTE: CI zip did not contain M1-Transcoder; using existing local $localTranscoder" -ForegroundColor Yellow
}
else {
    Build-LocalTranscoder $localTranscoder
}

Write-Host "Windows CI artifacts downloaded, extracted, and installed into build directories."
