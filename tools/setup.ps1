<#
.SYNOPSIS
  Installs the tools needed to build this project on Windows.

.DESCRIPTION
  1. VS Code extensions (MPLAB Extension Pack, PowerShell) via the `code` CLI.
  2. MPLAB XC8 compiler (free mode) via winget, or from -Xc8InstallerUrl.
  3. PIC12-16F1xxx Device Family Pack (DFP) into tools\packs\ for the
     command-line build (the MPLAB VS Code extension manages its own packs).

  Safe to re-run: each step is skipped when already present.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\setup.ps1
.EXAMPLE
  # newer XC8 than winget offers (copy the URL from microchip.com/xc8):
  tools\setup.ps1 -Xc8InstallerUrl https://ww1.microchip.com/.../xc8-v4.00-full-install-windows-x64-installer.exe
#>
[CmdletBinding()]
param(
    [string]$DfpVersion = "1.9.258",
    [string]$Xc8InstallerUrl = "",
    [switch]$SkipExtensions,
    [switch]$SkipXc8,
    [switch]$SkipDfp
)

$ErrorActionPreference = "Stop"
$Root      = Split-Path -Parent $PSScriptRoot
$Downloads = Join-Path $PSScriptRoot "downloads"
$Packs     = Join-Path $PSScriptRoot "packs"
$Xc8Page   = "https://www.microchip.com/en-us/tools-resources/develop/mplab-xc-compilers/xc8"
$PacksPage = "https://packs.download.microchip.com/"

function Write-Step($msg) { Write-Host "==> $msg" -ForegroundColor Cyan }

function Find-Xc8 {
    $base = Join-Path $env:ProgramFiles "Microchip\xc8"
    if (-not (Test-Path $base)) { return $null }
    Get-ChildItem $base -Directory |
        Where-Object { Test-Path (Join-Path $_.FullName "bin\xc8-cc.exe") } |
        Sort-Object { [version]($_.Name.TrimStart('v')) } -Descending |
        Select-Object -First 1
}

# --- 1. VS Code extensions -------------------------------------------------
if (-not $SkipExtensions) {
    Write-Step "VS Code extensions"
    $code = Get-Command code -ErrorAction SilentlyContinue
    if ($null -eq $code) {
        Write-Warning "VS Code CLI 'code' not on PATH; install the recommended extensions from the Extensions view instead."
    } else {
        foreach ($ext in @("Microchip.mplab-extension-pack", "ms-vscode.powershell")) {
            & $code.Source --install-extension $ext
        }
    }
}

# --- 2. XC8 ------------------------------------------------------------------
if (-not $SkipXc8) {
    Write-Step "MPLAB XC8 compiler"
    $xc8 = Find-Xc8
    if ($null -ne $xc8 -and $Xc8InstallerUrl -eq "") {
        Write-Host "Found XC8 $($xc8.Name) at $($xc8.FullName)"
    } elseif ($Xc8InstallerUrl -ne "") {
        New-Item -ItemType Directory -Force $Downloads | Out-Null
        $exe = Join-Path $Downloads (Split-Path -Leaf $Xc8InstallerUrl)
        if (-not (Test-Path $exe)) {
            Write-Host "Downloading $Xc8InstallerUrl"
            Invoke-WebRequest -Uri $Xc8InstallerUrl -OutFile $exe -UseBasicParsing
        }
        Write-Host "Starting installer (choose the Free license when asked)..."
        Start-Process -FilePath $exe -Wait
    } else {
        $winget = Get-Command winget -ErrorAction SilentlyContinue
        if ($null -ne $winget) {
            Write-Host "Installing via winget (package Microchip.MPLABXC8CCompiler)..."
            & $winget.Source install --id Microchip.MPLABXC8CCompiler -e `
                --accept-package-agreements --accept-source-agreements
        } else {
            Write-Warning "winget not available. Download XC8 manually from $Xc8Page"
            Start-Process $Xc8Page
        }
    }
    $xc8 = Find-Xc8
    if ($null -eq $xc8) {
        Write-Warning "XC8 still not found under $env:ProgramFiles\Microchip\xc8 - finish the installer, then re-run."
    } else {
        Write-Host "XC8 ready: $($xc8.FullName)\bin\xc8-cc.exe"
    }
}

# --- 3. Device Family Pack --------------------------------------------------
if (-not $SkipDfp) {
    Write-Step "PIC12-16F1xxx_DFP $DfpVersion"
    $dest = Join-Path $Packs "PIC12-16F1xxx_DFP\$DfpVersion"
    if (Test-Path (Join-Path $dest "xc8")) {
        Write-Host "Found DFP at $dest"
    } else {
        New-Item -ItemType Directory -Force $Downloads | Out-Null
        $name = "Microchip.PIC12-16F1xxx_DFP.$DfpVersion.atpack"
        $pack = Join-Path $Downloads $name
        $url  = "https://packs.download.microchip.com/$name"
        try {
            if (-not (Test-Path $pack)) {
                Write-Host "Downloading $url"
                Invoke-WebRequest -Uri $url -OutFile $pack -UseBasicParsing
            }
            # .atpack is a zip; Expand-Archive insists on a .zip extension
            $zip = [IO.Path]::ChangeExtension($pack, ".zip")
            Copy-Item $pack $zip -Force
            New-Item -ItemType Directory -Force $dest | Out-Null
            Expand-Archive -Path $zip -DestinationPath $dest -Force
            Remove-Item $zip
            if (Test-Path (Join-Path $dest "xc8")) {
                Write-Host "DFP extracted to $dest"
            } else {
                Write-Warning "Extracted, but no 'xc8' folder found in $dest - check the pack contents."
            }
        } catch {
            Write-Warning "DFP download failed ($($_.Exception.Message))."
            Write-Warning "Get $name from $PacksPage, put it in $Downloads and re-run."
        }
    }
}

Write-Step "Done. Build with: tools\build.ps1  (or VS Code: Terminal > Run Build Task)"
