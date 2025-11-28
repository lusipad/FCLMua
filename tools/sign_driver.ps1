[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$DriverPath,

    [string]$CertificateSubject = "CN=FclMusaTestCert",
    [string]$TimestampUrl = "http://timestamp.digicert.com",
    [string]$CertificatePassword = "FclMusaTestPass!2025",
    [string]$SigntoolPath
)

$ErrorActionPreference = "Stop"

function Resolve-FullPath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return (Resolve-Path -Path $Path).ProviderPath
}

function Resolve-Signtool {
    param([string]$ExplicitPath)

    if ($ExplicitPath) {
        if (Test-Path -Path $ExplicitPath -PathType Leaf) {
            return (Resolve-FullPath -Path $ExplicitPath)
        }
        throw "Signtool override path not found: $ExplicitPath"
    }

    $command = Get-Command -Name "signtool.exe" -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $candidateRoots = @()
    if (${env:ProgramFiles(x86)}) {
        $candidateRoots += (Join-Path -Path ${env:ProgramFiles(x86)} -ChildPath "Windows Kits\10\bin")
    }
    if ($env:ProgramFiles) {
        $candidateRoots += (Join-Path -Path $env:ProgramFiles -ChildPath "Windows Kits\10\bin")
    }
    $candidateRoots = $candidateRoots | Where-Object { $_ -and (Test-Path $_) }

    foreach ($root in $candidateRoots) {
        $versionDirs = Get-ChildItem -Path $root -Directory -ErrorAction SilentlyContinue |
            Sort-Object -Property LastWriteTime -Descending
        foreach ($versionDir in $versionDirs) {
            $x64Candidate = Join-Path -Path $versionDir.FullName -ChildPath "x64\signtool.exe"
            if (Test-Path -Path $x64Candidate -PathType Leaf) {
                return $x64Candidate
            }
        }
    }

    foreach ($root in $candidateRoots) {
        $fallback = Get-ChildItem -Path $root -Filter "signtool.exe" -Recurse -ErrorAction SilentlyContinue |
            Sort-Object -Property LastWriteTime -Descending
        if ($fallback) {
            return $fallback[0].FullName
        }
    }

    throw "signtool.exe not found. Provide -SigntoolPath or install Windows Kits."
}

if (-not (Test-Path -Path $DriverPath -PathType Leaf)) {
    throw "Driver file not found: $DriverPath"
}

$driverFullPath = Resolve-FullPath -Path $DriverPath
$driverDirectory = Split-Path -Path $driverFullPath -Parent
$pfxPath = Join-Path -Path $driverDirectory -ChildPath "FclMusaTestCert.pfx"
$cerPath = Join-Path -Path $driverDirectory -ChildPath "FclMusaTestCert.cer"
$securePassword = ConvertTo-SecureString -String $CertificatePassword -AsPlainText -Force
$certStore = "Cert:\CurrentUser\My"

Write-Host "Locating certificate subject: $CertificateSubject in $certStore..."
$existingCert = $null
try {
    $existingCert = Get-ChildItem -Path $certStore |
        Where-Object { $_.Subject -eq $CertificateSubject } |
        Sort-Object -Property NotAfter -Descending |
        Select-Object -First 1
} catch {
    throw ("Unable to access certificate store {0}: {1}" -f $certStore, $_.Exception.Message)
}

if (-not $existingCert) {
    Write-Host "Subject not found. Creating new self-signed certificate..." -ForegroundColor Yellow
    $existingCert = New-SelfSignedCertificate `
        -Type CodeSigning `
        -Subject $CertificateSubject `
        -CertStoreLocation $certStore `
        -KeyExportPolicy Exportable `
        -KeyLength 2048 `
        -HashAlgorithm SHA256 `
        -FriendlyName "FCL+Musa Test Certificate"
} else {
    Write-Host "Existing certificate found. Reusing..." -ForegroundColor Green
}

Export-PfxCertificate -Cert $existingCert -FilePath $pfxPath -Password $securePassword -Force | Out-Null
Export-Certificate -Cert $existingCert -FilePath $cerPath -Force | Out-Null

$signtool = Resolve-Signtool -ExplicitPath $SigntoolPath

$signArguments = @(
    "sign",
    "/fd", "sha256",
    "/f", $pfxPath,
    "/p", $CertificatePassword
)

if ($TimestampUrl) {
    $signArguments += @("/tr", $TimestampUrl, "/td", "sha256")
}

$signArguments += $driverFullPath

Write-Host "Signing $driverFullPath ..."
& $signtool @signArguments

if ($LASTEXITCODE -ne 0) {
    throw "signtool failed with exit code $LASTEXITCODE."
}

Write-Host "Driver signed successfully: $driverFullPath" -ForegroundColor Green
Write-Host "Certificate exported to: $cerPath" -ForegroundColor Yellow
Write-Host "Import this certificate into Trusted Root and Trusted Publisher on target machines." -ForegroundColor Yellow
