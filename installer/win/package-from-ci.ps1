[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$Commit = "",
    [string]$ArtifactsBucket = "mach1-build-artifacts",
    [string]$CiArtifactsDir = "ci-artifacts",
    [string]$Region = "us-east-1"
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

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        Exit-WithMessage "Command failed with exit code $LASTEXITCODE`: $FilePath $($Arguments -join ' ')" $LASTEXITCODE
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

Invoke-Checked $aws.Source @("s3", "cp", $s3Path, $zipPath, "--region", $Region)

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
    Exit-WithMessage "CI zip did not contain M1-Transcoder and $localTranscoder does not exist. Re-run CI with the updated workflow or build m1-transcoder locally."
}

Write-Host "Windows CI artifacts downloaded, extracted, and installed into build directories."
