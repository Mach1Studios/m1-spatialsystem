# Simple Azure CodeSigning wrapper for Makefile
# Usage: sign-file.ps1 <file_path>

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath
)

# Load config from Makefile.variables
$configFile = Join-Path $PSScriptRoot "..\..\Makefile.variables"
$config = @{}

Get-Content $configFile | ForEach-Object {
    if ($_ -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*[:?+]?=\s*(.*)$') {
        $key = $matches[1]
        $value = $matches[2].Trim()
        if ($value.StartsWith("#")) {
            $value = ""
        }
        $value = $value.Trim('"').Trim("'")
        $config[$key] = $value
    }
}

function Get-ConfigValue {
    param([string]$Name)

    if ($config.ContainsKey($Name) -and -not [string]::IsNullOrWhiteSpace($config[$Name])) {
        return $config[$Name]
    }

    return [Environment]::GetEnvironmentVariable($Name)
}

function Resolve-SignTool {
    param([string]$ConfiguredPath)

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ConfiguredPath)) {
        $candidates += $ConfiguredPath.Trim().Trim('"').Trim("'")
    }

    $command = Get-Command signtool.exe -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    $kitsRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
    if (Test-Path -LiteralPath $kitsRoot) {
        $sdkSignTools = Get-ChildItem -LiteralPath $kitsRoot -Directory -ErrorAction SilentlyContinue |
            ForEach-Object {
                $parsedVersion = $null
                [pscustomobject]@{
                    Path = Join-Path $_.FullName "x64\signtool.exe"
                    Version = if ([version]::TryParse($_.Name, [ref]$parsedVersion)) { $parsedVersion } else { [version]"0.0" }
                }
            } |
            Where-Object { Test-Path -LiteralPath $_.Path } |
            Sort-Object Version -Descending |
            Select-Object -ExpandProperty Path

        $candidates += $sdkSignTools
    }

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    Write-Host "ERROR: signtool.exe was not found." -ForegroundColor Red
    Write-Host "Install the Windows SDK signing tools, or set WIN_SIGNTOOL_PATH to the installed signtool.exe."
    Write-Host "Expected SDK location: C:\Program Files (x86)\Windows Kits\10\bin\<version>\x64\signtool.exe"
    exit 1
}

# Set Azure environment variables
$env:AZURE_CLIENT_ID = Get-ConfigValue 'AZURE_CLIENT_ID'
$env:AZURE_TENANT_ID = Get-ConfigValue 'AZURE_TENANT_ID'
$env:AZURE_CLIENT_SECRET = Get-ConfigValue 'AZURE_CLIENT_SECRET'
if ([string]::IsNullOrWhiteSpace($env:AZURE_CLIENT_SECRET)) {
    $env:AZURE_CLIENT_SECRET = Get-ConfigValue 'AZURE_SECRET_ID'
}

# Get paths
$signtoolPath = Resolve-SignTool (Get-ConfigValue 'WIN_SIGNTOOL_PATH')
$dlibPath = Get-ConfigValue 'AZURE_DLIB_PATH'
$metadataPath = Get-ConfigValue 'AZURE_METADATA_PATH'
$timestampUrl = Get-ConfigValue 'AZURE_TIMESTAMP_URL'

# Check if file exists
if (-not (Test-Path $FilePath)) {
    Write-Host "ERROR: File not found: $FilePath" -ForegroundColor Red
    exit 1
}

Write-Host "Signing: $FilePath" -ForegroundColor Cyan
Write-Host "Using signtool: $signtoolPath"

# Sign with Azure Trusted Signing
# NOTE: Do NOT specify /sha1 - Azure will provide the certificate through the DLib
# The certificate is generated daily by Azure (3-day lifespan)
& $signtoolPath sign `
    /v `
    /fd SHA256 `
    /tr $timestampUrl `
    /td SHA256 `
    /dlib $dlibPath `
    /dmdf $metadataPath `
    $FilePath

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SUCCESS] Signed successfully" -ForegroundColor Green
    exit 0
} else {
    Write-Host "[FAILED] Signing failed with exit code: $LASTEXITCODE" -ForegroundColor Red
    exit $LASTEXITCODE
}

