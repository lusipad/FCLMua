[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet('Install','Start','Stop','Uninstall','Restart','Reinstall')]
    [string]$Action = 'Restart',

    [string]$ServiceName = 'FclMusa',

    [string]$DriverPath = 'dist\driver\x64\Release\FclMusaDriver.sys'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Assert-Elevated {
    $currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = [Security.Principal.WindowsPrincipal]::new($currentIdentity)
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "This script must be run as Administrator."
    }
}

function Resolve-DriverPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (-not (Test-Path -Path $Path -PathType Leaf)) {
        throw "Driver SYS not found at '$Path'. Build Release driver first (dist\driver\x64\Release\FclMusaDriver.sys)."
    }
    return (Resolve-Path $Path).ProviderPath
}

function Invoke-Sc {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Arguments
    )

    & sc.exe $Arguments
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        Write-Warning "sc.exe $Arguments failed with exit code $exitCode."
    }
    return $exitCode
}

Assert-Elevated

Write-Host "Driver service: $ServiceName" -ForegroundColor Cyan

switch ($Action) {
    'Stop' {
        Write-Host "Stopping driver service..." -ForegroundColor Cyan
        Invoke-Sc "stop `"$ServiceName`""
    }

    'Uninstall' {
        Write-Host "Stopping driver service (if running)..." -ForegroundColor Cyan
        Invoke-Sc "stop `"$ServiceName`"" | Out-Null

        Write-Host "Deleting driver service..." -ForegroundColor Cyan
        Invoke-Sc "delete `"$ServiceName`""
    }

    'Install' {
        $fullPath = Resolve-DriverPath -Path $DriverPath
        Write-Host "Installing driver service from: $fullPath" -ForegroundColor Cyan

        # Try to delete any existing service first (ignore errors).
        Invoke-Sc "delete `"$ServiceName`"" | Out-Null

        $args = @(
            "create `"$ServiceName`""
            "type= kernel"
            "start= demand"
            "binPath= `"$fullPath`""
            "DisplayName= `"FCL+Musa Collision Driver`""
        ) -join ' '

        Invoke-Sc $args | Out-Null
    }

    'Start' {
        Write-Host "Starting driver service..." -ForegroundColor Cyan
        Invoke-Sc "start `"$ServiceName`""
    }

    'Restart' {
        Write-Host "Restarting driver service..." -ForegroundColor Cyan
        Invoke-Sc "stop `"$ServiceName`"" | Out-Null

        $fullPath = Resolve-DriverPath -Path $DriverPath

        # Ensure service exists.
        Invoke-Sc "query `"$ServiceName`"" | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Service not found. Creating service before restart..." -ForegroundColor Yellow
            $args = @(
                "create `"$ServiceName`""
                "type= kernel"
                "start= demand"
                "binPath= `"$fullPath`""
                "DisplayName= `"FCL+Musa Collision Driver`""
            ) -join ' '
            Invoke-Sc $args | Out-Null
        }

        Invoke-Sc "start `"$ServiceName`""
    }

    'Reinstall' {
        Write-Host "Reinstalling driver service..." -ForegroundColor Cyan
        Invoke-Sc "stop `"$ServiceName`"" | Out-Null
        Invoke-Sc "delete `"$ServiceName`"" | Out-Null

        $fullPath = Resolve-DriverPath -Path $DriverPath
        $args = @(
            "create `"$ServiceName`""
            "type= kernel"
            "start= demand"
            "binPath= `"$fullPath`""
            "DisplayName= `"FCL+Musa Collision Driver`""
        ) -join ' '
        Invoke-Sc $args | Out-Null

        Invoke-Sc "start `"$ServiceName`""
    }

    default {
        throw "Unsupported action: $Action"
    }
}

Write-Host "Action '$Action' completed." -ForegroundColor Green

