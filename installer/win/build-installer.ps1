[CmdletBinding()]
param(
    [string]$InnoSetupPath = ""
)

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
            $value = $value.Trim('"').Trim("'")
            $values[$key] = $value
        }
    }

    return $values
}

function Resolve-InnoSetup {
    param(
        [string]$ConfiguredPath,
        [hashtable]$Config
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($ConfiguredPath)) {
        $candidates += $ConfiguredPath.Trim().Trim('"').Trim("'")
    }
    if ($Config.ContainsKey("WIN_INNO_PATH")) {
        $candidates += $Config["WIN_INNO_PATH"].Trim().Trim('"').Trim("'")
    }

    $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($command) {
        $candidates += $command.Source
    }

    $candidates += @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
    )

    foreach ($candidate in $candidates) {
        if (-not [string]::IsNullOrWhiteSpace($candidate) -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    Write-Host "ERROR: Inno Setup compiler was not found." -ForegroundColor Red
    Write-Host "Set WIN_INNO_PATH in Makefile.variables or install Inno Setup 6."
    Write-Host "Install options:"
    Write-Host "  winget install JRSoftware.InnoSetup"
    Write-Host "  choco install innosetup"
    exit 1
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

$config = Read-MakefileVariables (Join-Path $repoRoot "Makefile.variables")
$inno = Resolve-InnoSetup $InnoSetupPath $config
$issPath = Join-Path $repoRoot "installer\win\installer.iss"
$installerPath = Join-Path $repoRoot "installer\win\Output\Mach1 Spatial System Installer.exe"

Write-Host "Building Windows installer with Inno Setup..."
Write-Host "Using ISCC: $inno"
Invoke-Checked $inno @($issPath)

if (-not (Test-Path -LiteralPath $installerPath)) {
    Write-Host "ERROR: Installer was not created at $installerPath" -ForegroundColor Red
    exit 1
}

Write-Host "Signing Windows installer with Azure Trusted Signing..."
& (Join-Path $PSScriptRoot "sign-file.ps1") -FilePath $installerPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Installer created and signed: installer\win\Output\Mach1 Spatial System Installer.exe"
