[CmdletBinding()]
param(
    [switch]$Elevated
)

$ErrorActionPreference = "Stop"

function Test-IsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Restart-AsAdministrator {
    if ($Elevated) {
        Write-Host "ERROR: setup-prereqs.ps1 was relaunched, but the process is still not elevated." -ForegroundColor Red
        exit 1
    }

    Write-Host "Windows setup requires administrator privileges. Requesting elevation..."
    $argumentList = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -Elevated"

    try {
        $process = Start-Process -FilePath "powershell.exe" -ArgumentList $argumentList -Verb RunAs -Wait -PassThru
        exit $process.ExitCode
    }
    catch {
        Write-Host "ERROR: Failed to start elevated PowerShell. Re-run 'make setup' from an Administrator PowerShell." -ForegroundColor Red
        Write-Host $_.Exception.Message
        exit 1
    }
}

function Invoke-Native {
    param(
        [string]$FilePath,
        [string[]]$Arguments
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

    if ($output) {
        $output | ForEach-Object {
            $line = $_.ToString()
            if ($line -and $line -ne "System.Management.Automation.RemoteException") {
                Write-Host $line
            }
        }
    }

    if ($exitCode -ne 0) {
        Write-Host "ERROR: Command failed with exit code $exitCode`: $FilePath $($Arguments -join ' ')" -ForegroundColor Red
        exit $exitCode
    }
}

function Test-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
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

function Install-WithChocolatey {
    $choco = (Get-Command choco -ErrorAction Stop).Source

    Write-Host "Using Chocolatey for Windows prerequisites..."
    Invoke-Native $choco @("install", "cmake", "-y", "--force", "--no-progress", "--installargs", "ADD_CMAKE_TO_PATH=System", "--apply-install-arguments-to-dependencies")
    Invoke-Native $choco @("install", "nodejs-lts", "pkgconfiglite", "autoconf", "automake", "libtool", "-y", "--force", "--no-progress")
}

function Install-WithWinget {
    $winget = (Get-Command winget -ErrorAction Stop).Source

    Write-Host "Using winget for Windows packaging prerequisites..."
    $commonArgs = @("--exact", "--silent", "--force", "--disable-interactivity", "--accept-package-agreements", "--accept-source-agreements")
    Invoke-Native $winget (@("install", "--id", "Kitware.CMake") + $commonArgs)
    Invoke-Native $winget (@("install", "--id", "OpenJS.NodeJS.LTS") + $commonArgs)
    Invoke-Native $winget (@("install", "--id", "JRSoftware.InnoSetup") + $commonArgs)

    Write-Host "NOTE: Chocolatey is still recommended if you need pkgconfiglite/autoconf/automake/libtool for full local builds." -ForegroundColor Yellow
}

if (-not (Test-IsAdministrator)) {
    Restart-AsAdministrator
}

if (Test-Command choco) {
    Install-WithChocolatey
}
elseif (Test-Command winget) {
    Install-WithWinget
}
else {
    Write-Host "ERROR: Neither Chocolatey nor winget was found." -ForegroundColor Red
    Write-Host "Install Node.js LTS manually from https://nodejs.org/ or install one of these package managers:"
    Write-Host "  winget: https://learn.microsoft.com/windows/package-manager/winget/"
    Write-Host "  Chocolatey: https://chocolatey.org/install"
    exit 1
}

$npm = Resolve-Npm
if ($npm) {
    Write-Host "npm found at: $npm"
    Invoke-Native $npm @("install", "-g", "nodemon")
}
else {
    Write-Host "WARNING: Node.js was requested, but npm was not found after setup." -ForegroundColor Yellow
    Write-Host "Close and reopen PowerShell, then verify with: npm --version"
}

Write-Host ""
Write-Host "Windows prerequisites installed or already present."
Write-Host "If node/npm was installed for the first time, close and reopen PowerShell so your interactive PATH refreshes."
