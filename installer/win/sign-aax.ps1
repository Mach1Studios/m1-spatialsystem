[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

function Read-MakefileVariables {
    param([string]$Path)

    $values = @{}
    if (-not (Test-Path -LiteralPath $Path)) {
        return $values
    }

    Get-Content -LiteralPath $Path | ForEach-Object {
        if ($_ -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*[:?+]?=\s*(.*)$') {
            $key = $matches[1]
            $value = $matches[2].Trim()
            if ($value.StartsWith("#")) {
                $value = ""
            }
            $value = $value.Trim('"').Trim("'")
            $values[$key] = $value
        }
    }

    return $values
}

function Get-Setting {
    param(
        [hashtable]$Config,
        [string]$Name,
        [bool]$Required = $true
    )

    $value = $null
    if ($Config.ContainsKey($Name) -and -not [string]::IsNullOrWhiteSpace($Config[$Name])) {
        $value = $Config[$Name]
    }
    if ([string]::IsNullOrWhiteSpace($value) -and [Environment]::GetEnvironmentVariable($Name)) {
        $value = [Environment]::GetEnvironmentVariable($Name)
    }

    if ($null -ne $value) {
        $value = $value.Trim().Trim('"').Trim("'")
    }

    if ($Required -and [string]::IsNullOrWhiteSpace($value)) {
        Write-Host "ERROR: $Name is not configured in Makefile.variables or the environment." -ForegroundColor Red
        exit 1
    }

    return $value
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

function Sign-AaxPlugin {
    param(
        [string]$PluginPath,
        [string]$Guid,
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $PluginPath)) {
        Write-Host "ERROR: $Label AAX plugin not found at $PluginPath" -ForegroundColor Red
        exit 1
    }

    Write-Host "Signing $Label AAX plugin..."
    Invoke-Checked $wraptool @(
        "sign",
        "--signtool", $signtoolWrapper,
        "--signid", "1",
        "--verbose",
        "--installedbinaries",
        "--account", $paceAccount,
        "--wcguid", $Guid,
        "--in", $PluginPath,
        "--out", $PluginPath
    )
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
Set-Location $repoRoot

$config = Read-MakefileVariables (Join-Path $repoRoot "Makefile.variables")

$wraptool = Get-Setting $config "WRAPTOOL"
$paceAccount = Get-Setting $config "PACE_ACCOUNT"
$monitorGuid = Get-Setting $config "MONITOR_FREE_GUID"
$pannerGuid = Get-Setting $config "PANNER_FREE_GUID"

$env:SIGNTOOL_PATH = Get-Setting $config "WIN_SIGNTOOL_PATH"
$env:ACS_DLIB = Get-Setting $config "AZURE_DLIB_PATH"
$env:ACS_JSON = Get-Setting $config "AZURE_METADATA_PATH"
$env:AZURE_TENANT_ID = Get-Setting $config "AZURE_TENANT_ID"
$env:AZURE_CLIENT_ID = Get-Setting $config "AZURE_CLIENT_ID"
$azureSecret = Get-Setting $config "AZURE_CLIENT_SECRET" $false
if ([string]::IsNullOrWhiteSpace($azureSecret)) {
    $azureSecret = Get-Setting $config "AZURE_SECRET_ID"
}
$env:AZURE_CLIENT_SECRET = $azureSecret
$env:AZURE_SECRET_ID = $azureSecret

$signtoolWrapper = Join-Path $PSScriptRoot "aax-signtool.bat"
if (-not (Test-Path -LiteralPath $signtoolWrapper)) {
    Write-Host "ERROR: AAX signtool wrapper not found at $signtoolWrapper" -ForegroundColor Red
    exit 1
}

Write-Host "Signing AAX plugins on Windows..."
Write-Host "Ensure iLok License Manager is running and the USB iLok is connected."

Sign-AaxPlugin "m1-monitor\build\M1-Monitor_artefacts\Release\AAX\M1-Monitor.aaxplugin" $monitorGuid "M1-Monitor"
Sign-AaxPlugin "m1-panner\build\M1-Panner_artefacts\Release\AAX\M1-Panner.aaxplugin" $pannerGuid "M1-Panner"

Write-Host "AAX plugins signed successfully."
