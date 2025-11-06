# Simple Azure CodeSigning wrapper for Makefile
# Usage: sign-file.ps1 <file_path>

param(
    [Parameter(Mandatory=$true)]
    [string]$FilePath
)

# Load config from Makefile.variables
$configFile = Join-Path $PSScriptRoot "..\..\Makefile.variables"
$config = @{}

Get-Content $configFile | Where-Object { $_ -match '^\w+=' } | ForEach-Object {
    if ($_ -match '^(\w+)=(.*)$') {
        $key = $matches[1]
        $value = $matches[2].Trim('"').Trim("'")
        $config[$key] = $value
    }
}

# Set Azure environment variables
$env:AZURE_CLIENT_ID = $config['AZURE_CLIENT_ID']
$env:AZURE_TENANT_ID = $config['AZURE_TENANT_ID']
$env:AZURE_CLIENT_SECRET = $config['AZURE_CLIENT_SECRET']

# Get paths
$signtoolPath = $config['WIN_SIGNTOOL_PATH']
$dlibPath = $config['AZURE_DLIB_PATH']
$metadataPath = $config['AZURE_METADATA_PATH']
$timestampUrl = $config['AZURE_TIMESTAMP_URL']

# Check if file exists
if (-not (Test-Path $FilePath)) {
    Write-Host "ERROR: File not found: $FilePath" -ForegroundColor Red
    exit 1
}

Write-Host "Signing: $FilePath" -ForegroundColor Cyan

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

