<#
.SYNOPSIS
  Copies the built HEX to a Raspberry Pi and programs the PIC with Pickle
  (Raspberry Pi GPIO bit-bang, low-voltage programming).

.DESCRIPTION
  Requires OpenSSH client on Windows (ssh/scp) and key-based login to the Pi.
  Pickle must be installed and configured on the Pi (tools/pi/install-pickle.sh).
  Verify the p14 command syntax on your Pickle version with `p14` (no args).

  Refuses a HEX whose configuration turns LVP off (the HV build,
  solrbrite-hv.hex). The chip refuses to clear LVP during low-voltage
  programming, so the result would be the HV firmware with pin 4 still MCLR:
  a button that does nothing, and a failed verify.

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

# CONFIG2 is at word address 0x8008; HEX files for 14-bit PICs use byte
# addresses, so it sits at 0x10010. LVP is CONFIG2 bit 13.
$Config2ByteAddress = 0x10010
$LvpBit = 0x2000

function Get-HexWord {
    <#
    .SYNOPSIS
      Returns the 16-bit little-endian word at a byte address of an Intel HEX
      file, or -1 when the file doesn't set both bytes in one record.
    #>
    [OutputType([int])]
    param(
        [string]$Path,
        [int]$ByteAddress
    )
    [int]$upperAddress = 0
    foreach ($line in [System.IO.File]::ReadLines($Path)) {
        if (-not $line.StartsWith(":")) { continue }
        [int]$count  = [Convert]::ToInt32($line.Substring(1, 2), 16)
        [int]$offset = [Convert]::ToInt32($line.Substring(3, 4), 16)
        [int]$type   = [Convert]::ToInt32($line.Substring(7, 2), 16)
        if ($type -eq 4) {
            $upperAddress = [Convert]::ToInt32($line.Substring(9, 4), 16) -shl 16
            continue
        }
        if ($type -ne 0) { continue }
        [int]$start = $upperAddress + $offset
        if (($ByteAddress -ge $start) -and ($ByteAddress + 1 -lt $start + $count)) {
            [int]$at = 9 + 2 * ($ByteAddress - $start)
            [int]$low  = [Convert]::ToInt32($line.Substring($at, 2), 16)
            [int]$high = [Convert]::ToInt32($line.Substring($at + 2, 2), 16)
            return ($high -shl 8) -bor $low
        }
    }
    return -1
}

$Root = Split-Path -Parent $PSScriptRoot
if ($Hex -eq "") { $Hex = Join-Path $Root "out\cli\solrbrite.hex" }
if (-not (Test-Path $Hex)) { throw "HEX not found: $Hex (build first)" }
$Hex = (Resolve-Path $Hex).Path

[int]$config2 = Get-HexWord -Path $Hex -ByteAddress $Config2ByteAddress
if ($config2 -lt 0) {
    throw "No CONFIG2 word in $Hex, so its LVP setting can't be checked. Not programming."
}
if (($config2 -band $LvpBit) -eq 0) {
    throw "$Hex has LVP off (the HV build). The Pi can't write that setting, so the button wouldn't work. Use a high-voltage programmer (README section 5), or build the LVP version with tools\build.ps1."
}

$remote = "/tmp/solrbrite.hex"
Write-Host "Copying $Hex -> ${PiHost}:$remote"
& scp $Hex "${PiHost}:$remote"
if ($LASTEXITCODE -ne 0) { throw "scp failed" }

Write-Host "Programming via Pickle: $P14 $P14Args $remote"
& ssh $PiHost "$P14 $P14Args $remote && $P14 lvp verify $remote"
if ($LASTEXITCODE -ne 0) { throw "Pickle programming failed (exit $LASTEXITCODE)" }
Write-Host "Done." -ForegroundColor Green
