<#
.SYNOPSIS
Create and trust a local development code-signing certificate for ultimateTax.

.DESCRIPTION
- Creates a self-signed code-signing certificate.
- Exports a PFX (private key) and CER (public key).
- Trusts the CER for the current user (Root + TrustedPublisher).
- Optionally trusts it for local machine (requires admin).
- Prints the environment variables used by ledger signing BAT files.

Example:
  powershell -ExecutionPolicy Bypass -File .\scripts\setup-dev-codesign.ps1

  powershell -ExecutionPolicy Bypass -File .\scripts\setup-dev-codesign.ps1 `
    -Subject "CN=ultimateTax Dev Code Signing" `
    -PfxPassword "change-me"
#>

[CmdletBinding()]
param(
  [string]$Subject = "CN=ultimateTax Dev Code Signing",
  [string]$OutDir = "",
  [string]$PfxName = "ultimateTax-dev-codesign.pfx",
  [string]$CerName = "ultimateTax-dev-codesign.cer",
  [string]$PfxPassword = "",
  [int]$YearsValid = 5,
  [switch]$TrustLocalMachine
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($OutDir)) {
  $baseDir = $PSScriptRoot
  if ([string]::IsNullOrWhiteSpace($baseDir)) {
    $baseDir = Split-Path -Parent $PSCommandPath
  }
  if ([string]::IsNullOrWhiteSpace($baseDir)) {
    $baseDir = (Get-Location).Path
  }
  $OutDir = Join-Path $baseDir "certs"
}

function Test-IsAdmin {
  $id = [Security.Principal.WindowsIdentity]::GetCurrent()
  $p = New-Object Security.Principal.WindowsPrincipal($id)
  return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

if ($YearsValid -lt 1 -or $YearsValid -gt 20) {
  throw "YearsValid must be between 1 and 20."
}

if (-not (Test-Path $OutDir)) {
  New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$pfxPath = Join-Path $OutDir $PfxName
$cerPath = Join-Path $OutDir $CerName
$notAfter = (Get-Date).AddYears($YearsValid)

Write-Host "Creating development code-signing cert..."
$cert = New-SelfSignedCertificate `
  -Type CodeSigningCert `
  -Subject $Subject `
  -KeyAlgorithm RSA `
  -KeyLength 3072 `
  -HashAlgorithm SHA256 `
  -CertStoreLocation "Cert:\CurrentUser\My" `
  -NotAfter $notAfter

if (-not $cert) {
  throw "Failed to create certificate."
}

if ([string]::IsNullOrWhiteSpace($PfxPassword)) {
  $secure = Read-Host "Enter password for exported PFX" -AsSecureString
} else {
  $secure = ConvertTo-SecureString -String $PfxPassword -AsPlainText -Force
}

Write-Host "Exporting PFX: $pfxPath"
Export-PfxCertificate -Cert $cert -FilePath $pfxPath -Password $secure | Out-Null

Write-Host "Exporting CER: $cerPath"
Export-Certificate -Cert $cert -FilePath $cerPath | Out-Null

Write-Host "Trusting certificate for CurrentUser (Root + TrustedPublisher)..."
Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\Root" | Out-Null
Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\CurrentUser\TrustedPublisher" | Out-Null

if ($TrustLocalMachine) {
  if (-not (Test-IsAdmin)) {
    throw "TrustLocalMachine requested but this shell is not elevated. Re-run as Administrator."
  }
  Write-Host "Trusting certificate for LocalMachine (Root + TrustedPublisher)..."
  Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\LocalMachine\Root" | Out-Null
  Import-Certificate -FilePath $cerPath -CertStoreLocation "Cert:\LocalMachine\TrustedPublisher" | Out-Null
}

Write-Host ""
Write-Host "Done."
Write-Host "Subject:    $($cert.Subject)"
Write-Host "Thumbprint: $($cert.Thumbprint)"
Write-Host "PFX:        $pfxPath"
Write-Host "CER:        $cerPath"
Write-Host ""
Write-Host "Set these variables before running any ledger build BAT:"
Write-Host "  set UTAX_SIGN_PFX=$pfxPath"
Write-Host "  set UTAX_SIGN_PFX_PASS=<your-pfx-password>"
Write-Host "  set UTAX_SIGN_TIMESTAMP_URL=http://timestamp.digicert.com"
Write-Host ""
Write-Host "PowerShell session equivalents:"
Write-Host "  `$env:UTAX_SIGN_PFX = '$pfxPath'"
Write-Host "  `$env:UTAX_SIGN_PFX_PASS = '<your-pfx-password>'"
Write-Host "  `$env:UTAX_SIGN_TIMESTAMP_URL = 'http://timestamp.digicert.com'"
