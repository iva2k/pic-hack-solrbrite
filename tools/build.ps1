<#
.SYNOPSIS
  Command-line build with MPLAB XC8 (independent of the VS Code extension).

.DESCRIPTION
  Compiles src\*.c for the PIC12LF1572 into out\cli\solrbrite.hex.
  Uses the newest XC8 under "C:\Program Files\Microchip\xc8" (or -Xc8 path)
  and the DFP extracted by tools\setup.ps1 (or -Dfp path to its xc8 folder).

.PARAMETER HvProgrammingOnly
  Builds the HV variant (HV_PROGRAMMING_ONLY=1, see DECISIONS.md D1) into
  out\cli\solrbrite-hv.hex: LVP off, pin 4 a normal button input. Only a
  high-voltage programmer can write it, or reprogram the chip afterwards.

.EXAMPLE
  tools\build.ps1
.EXAMPLE
  tools\build.ps1 -HvProgrammingOnly
.EXAMPLE
  tools\build.ps1 -Clean
#>
[CmdletBinding()]
param(
    [string]$Xc8 = "",
    [string]$Dfp = "",
    [string]$Optimization = "-O2",
    [switch]$HvProgrammingOnly,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$Root   = Split-Path -Parent $PSScriptRoot
$OutDir = Join-Path $Root "out\cli"
$Device = "12LF1572"
$OutName = if ($HvProgrammingOnly) { "solrbrite-hv" } else { "solrbrite" }
# Keep the High-Endurance Flash (0x780-0x7FF) free for settings
$RomRange = "default,-780-7ff"

if ($Clean) {
    if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
    Write-Host "Cleaned $OutDir"
    exit 0
}

# --- locate compiler --------------------------------------------------------
if ($Xc8 -eq "") {
    $base = Join-Path $env:ProgramFiles "Microchip\xc8"
    $ver = $null
    if (Test-Path $base) {
        $ver = Get-ChildItem $base -Directory |
            Where-Object { Test-Path (Join-Path $_.FullName "bin\xc8-cc.exe") } |
            Sort-Object { [version]($_.Name.TrimStart('v')) } -Descending |
            Select-Object -First 1
    }
    if ($null -eq $ver) { throw "XC8 not found. Run tools\setup.ps1 first." }
    $Xc8 = Join-Path $ver.FullName "bin\xc8-cc.exe"
}
if (-not (Test-Path $Xc8)) { throw "Compiler not found: $Xc8" }

# --- locate DFP -------------------------------------------------------------
if ($Dfp -eq "") {
    $packRoot = Join-Path $PSScriptRoot "packs\PIC12-16F1xxx_DFP"
    if (Test-Path $packRoot) {
        $p = Get-ChildItem $packRoot -Directory |
            Sort-Object { [version]$_.Name } -Descending |
            Select-Object -First 1
        if ($null -ne $p -and (Test-Path (Join-Path $p.FullName "xc8"))) {
            $Dfp = Join-Path $p.FullName "xc8"
        }
    }
}

# --- compile + link ---------------------------------------------------------
New-Item -ItemType Directory -Force $OutDir | Out-Null
$sources = Get-ChildItem (Join-Path $Root "src") -Filter *.c | ForEach-Object { $_.FullName }

$xcArgs = @(
    "-mcpu=$Device",
    $Optimization,
    "-std=c99",
    "-mrom=$RomRange",
    "-Wl,-Map=$(Join-Path $OutDir "$OutName.map")",
    "-o", (Join-Path $OutDir "$OutName.elf")
)
if ($HvProgrammingOnly) {
    $xcArgs += "-DHV_PROGRAMMING_ONLY=1"
    Write-Warning "HV build: LVP is off. Only a high-voltage programmer can write $OutName.hex, or reprogram the chip afterwards."
}
if ($Dfp -ne "") {
    $xcArgs = @("-mdfp=$Dfp") + $xcArgs
} else {
    Write-Warning "No DFP in tools\packs - relying on the compiler's built-in device support."
}
$xcArgs += $sources

Write-Host "XC8 : $Xc8"
Write-Host "DFP : $(if ($Dfp) { $Dfp } else { '(none)' })"
& $Xc8 @xcArgs
if ($LASTEXITCODE -ne 0) { throw "Build failed (exit $LASTEXITCODE)" }

$hex = Join-Path $OutDir "$OutName.hex"
if (Test-Path $hex) {
    Write-Host "OK  : $hex" -ForegroundColor Green
} else {
    Write-Warning "Build finished but $hex was not found - check $OutDir"
}
