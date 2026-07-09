[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$ReleaseBucket = "mach1-releases",
    [string]$AwsProfile = "mach1",
    [string]$Region = "us-east-1"
)

$ErrorActionPreference = "Stop"

function Get-SuggestedVersion {
    $versionFiles = @(
        "m1-monitor\VERSION",
        "m1-panner\VERSION",
        "m1-player\VERSION",
        "m1-orientationmanager\VERSION",
        "m1-transcoder\VERSION",
        "services\m1-system-helper\VERSION"
    )

    $versions = foreach ($file in $versionFiles) {
        if (Test-Path -LiteralPath $file) {
            $raw = (Get-Content -LiteralPath $file -TotalCount 1).Trim()
            $parsed = $null
            if ([version]::TryParse($raw, [ref]$parsed)) {
                [pscustomobject]@{
                    Raw = $raw
                    Parsed = $parsed
                }
            }
        }
    }

    $latest = $versions | Sort-Object Parsed | Select-Object -Last 1
    if ($latest) {
        return $latest.Raw
    }

    if (Test-Path -LiteralPath "VERSION") {
        return (Get-Content -LiteralPath "VERSION" -TotalCount 1).Trim()
    }

    return ""
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        Write-Host "ERROR: Command failed with exit code $LASTEXITCODE`: $FilePath $($Arguments -join ' ')" -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repoRoot

$installerPath = "installer\win\Output\Mach1 Spatial System Installer.exe"
if (-not (Test-Path -LiteralPath $installerPath)) {
    Write-Host "ERROR: Installer not found. Run 'make package-from-ci VERSION=x.x' or 'make installer-pkg' first." -ForegroundColor Red
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

if ([string]::IsNullOrWhiteSpace($Version)) {
    $centralVersion = ""
    if (Test-Path -LiteralPath "VERSION") {
        $centralVersion = (Get-Content -LiteralPath "VERSION" -TotalCount 1).Trim()
    }

    $suggestedVersion = Get-SuggestedVersion
    Write-Host "Current central version: $centralVersion"
    Write-Host "Suggested version: $suggestedVersion"
    $Version = Read-Host "Version [$suggestedVersion]"
    if ([string]::IsNullOrWhiteSpace($Version)) {
        $Version = $suggestedVersion
    }
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "ERROR: Version is required for deployment." -ForegroundColor Red
    exit 1
}

$destination = "s3://$ReleaseBucket/$Version/Mach1 Spatial System Installer.exe"
$args = @(
    "s3", "cp",
    $installerPath,
    $destination,
    "--region", $Region,
    "--content-disposition", "Mach1 Spatial System Installer.exe"
)

if (-not [string]::IsNullOrWhiteSpace($AwsProfile)) {
    $args += @("--profile", $AwsProfile)
}

Write-Host "Uploading installer to $destination..."
Invoke-Checked $aws.Source $args

Write-Host "Installer deployed to s3://$ReleaseBucket/$Version/"
Write-Host "REMINDER: Update the Avid Store Submission per version update!"
