<#
.SYNOPSIS
  Copies the built HEX to a Raspberry Pi and programs the PIC with Pickle
  (Raspberry Pi GPIO bit-bang, low-voltage programming).

.DESCRIPTION
  Requires OpenSSH client on Windows (ssh/scp) and key-based login to the Pi.
  Pickle must be installed and configured on the Pi (tools/pi/install-pickle.sh).
  Verify the p14 command syntax on your Pickle version with `p14` (no args).

.EXAMPLE
  tools\flash-pi.ps1 -PiHost pi@raspberrypi.local
#>
[CmdletBinding()]
param(
    [string]$PiHost = "pi@raspberrypi.local",
    [string]$Hex = "",
    [string]$P14 = "p14",
    [string]$P14Args = "lvp program"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
if ($Hex -eq "") { $Hex = Join-Path $Root "out\cli\solrbrite.hex" }
if (-not (Test-Path $Hex)) { throw "HEX not found: $Hex (build first)" }

$remote = "/tmp/solrbrite.hex"
Write-Host "Copying $Hex -> ${PiHost}:$remote"
& scp $Hex "${PiHost}:$remote"
if ($LASTEXITCODE -ne 0) { throw "scp failed" }

Write-Host "Programming via Pickle: $P14 $P14Args $remote"
& ssh $PiHost "$P14 $P14Args $remote && $P14 lvp verify $remote"
if ($LASTEXITCODE -ne 0) { throw "Pickle programming failed (exit $LASTEXITCODE)" }
Write-Host "Done." -ForegroundColor Green
