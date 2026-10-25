# ============================================================================
# check_headers.ps1 -- header self-containment test
# Project: <app> (layered layout)
# Each project header must compile when included alone (doc rule 3.3).
# Usage: powershell -ExecutionPolicy Bypass -File tools\check_headers.ps1
#        [-Toolchain mb|arm] [-Bsp <bsp include dir>] [-Gcc <compiler path>]
#        [-VitisVersion <ver, default 2025.2>] [-ExtraInclude <dir,...>] [-Sdt]
# Notes: BSP resolved via -Bsp -> $env:XILINX_BSP -> auto-probe (*.sdk\*_bsp\include);
#        mb-gcc 必须匹配 -VitisVersion（默认 2025.2），见 check_syntax。
# ============================================================================
param(
    [ValidateSet("mb", "arm")][string]$Toolchain = "mb",
    [string]$Bsp = "",
    [string]$Gcc = "",
    [string]$VitisVersion = "2025.2",
    [string[]]$ExtraInclude = @(),
    [switch]$Sdt
)

$ErrorActionPreference = "Continue"
$here = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $here

# ---- locate mb-gcc (must match -VitisVersion; never silently fall back to another version) ----
function Get-VitisVersionFromPath([string]$p) {
    foreach ($seg in ($p -split '[\\/]')) { if ($seg -match '^\d{4}\.\d$') { return $seg } }
    return ""
}
$gccVer = ""
if ($Gcc) {
    if (-not (Test-Path -LiteralPath $Gcc)) {
        Write-Host "[ERROR] -Gcc 指向的 mb-gcc 不存在: $Gcc"
        Pop-Location; exit 1
    }
    $gccVer = Get-VitisVersionFromPath $Gcc
    if ($gccVer -and ($gccVer -ne $VitisVersion)) {
        Write-Host "[ERROR] -Gcc 版本 $gccVer != 期望 $VitisVersion；如确需该版本请显式加 -VitisVersion $gccVer"
        Pop-Location; exit 1
    }
    if (-not $gccVer) { Write-Host "[WARN] 无法从路径判定 -Gcc 的 Vitis 版本，按显式指定使用: $Gcc" }
} else {
    $gccCand = @()
    $verToken = $VitisVersion.Replace('.','_')
    foreach ($name in @("AMD_VITIS_$verToken", "XILINX_VITIS_$verToken", "VITIS_$verToken")) {
        $value = [Environment]::GetEnvironmentVariable($name)
        if ($value -and ((Get-VitisVersionFromPath $value) -eq $VitisVersion)) {
            $gccRel = if ($Toolchain -eq "arm") { "gnu\aarch32\nt\gcc-arm-none-eabi\bin\arm-none-eabi-gcc.exe" } else { "gnu\microblaze\nt\bin\mb-gcc.exe" }
            $gccCand += (Join-Path $value $gccRel)
        }
    }
    if ($env:XILINX_VITIS -and ((Get-VitisVersionFromPath $env:XILINX_VITIS) -eq $VitisVersion)) {
        $gccRel = if ($Toolchain -eq "arm") { "gnu\aarch32\nt\gcc-arm-none-eabi\bin\arm-none-eabi-gcc.exe" } else { "gnu\microblaze\nt\bin\mb-gcc.exe" }
        $gccCand += (Join-Path $env:XILINX_VITIS $gccRel)
    }
    $compilerName = if ($Toolchain -eq "arm") { "arm-none-eabi-gcc.exe" } else { "mb-gcc.exe" }
    $cmd = Get-Command $compilerName -ErrorAction SilentlyContinue
    if ($cmd -and ((Get-VitisVersionFromPath $cmd.Source) -eq $VitisVersion)) { $gccCand += $cmd.Source }
    foreach ($c in $gccCand) { if ($c -and (Test-Path -LiteralPath $c)) { $Gcc = $c; $gccVer = $VitisVersion; break } }
    if (-not $Gcc) {
        Write-Host "[ERROR] 未找到 Vitis $VitisVersion 的 $compilerName（不静默改用其它版本）。"
        Write-Host "  已探测: 版本专用环境变量、版本匹配的 `$env:XILINX_VITIS、PATH"
        Write-Host "  显式指定: -Gcc <...\gnu\microblaze\nt\bin\mb-gcc.exe>；或改期望版本: -VitisVersion <ver>"
        Pop-Location; exit 1
    }
}
Write-Host "$Toolchain gcc: $Gcc (Vitis $gccVer)"

# ---- BSP include ----
# 解析顺序: -Bsp -> $env:XILINX_BSP -> 工程内自动探测 (*.sdk\*_bsp\include)
if (-not $Bsp) { $Bsp = $env:XILINX_BSP }
if (-not $Bsp) {
    $probe = Get-ChildItem -Path . -Recurse -Depth 4 -Directory -Filter '*_bsp' -ErrorAction SilentlyContinue |
             ForEach-Object { Join-Path $_.FullName 'include' } |
             Where-Object { Test-Path -LiteralPath (Join-Path $_ 'xparameters.h') } |
             Select-Object -First 1
    if ($probe) { $Bsp = $probe }
}
if (-not $Bsp -or -not (Test-Path -LiteralPath (Join-Path $Bsp 'xparameters.h'))) {
    Write-Host "BSP include 目录找不到或缺少 xparameters.h: '$Bsp'"
    Write-Host "  用 -Bsp <bsp include 目录> 指定，或设环境变量 `$env:XILINX_BSP；"
    Write-Host "  工程内 *.sdk\*_bsp\include 会被自动探测。"
    Pop-Location; exit 1
}

# ---- SDT detection (Vitis 2025.x): keep header tests on the same ABI path ----
$isSdt = [bool]$Sdt
if (-not $isSdt) {
    $xparamText = Get-Content -Raw -LiteralPath (Join-Path $Bsp 'xparameters.h')
    $isSdt = (($xparamText -match 'XPAR_[A-Z0-9_]+_0_BASEADDR') -and
              ($xparamText -notmatch 'XPAR_[A-Z0-9_]+_0_DEVICE_ID'))
}
if ($isSdt) { Write-Host "BSP mode: SDT (defines SDT)" }

$srcInc = Get-ChildItem -LiteralPath "src" -Recurse -Directory |
          ForEach-Object { "-I" + $_.FullName.Substring($here.Length + 1) }
$extraInc = $ExtraInclude | Where-Object { $_ } | ForEach-Object { "-I$_" }
$inc = @("-Isrc") + $srcInc + @("-I$Bsp") + @($extraInc)
# NOTE: MicroBlaze flags are defaults; ARM uses Cortex-A9 syntax flags.
if ($Toolchain -eq "arm") {
    $flags = @("-fsyntax-only", "-Wall", "-mcpu=cortex-a9")
} else {
    $flags = @("-fsyntax-only", "-mlittle-endian", "-mxl-barrel-shift", "-mxl-pattern-compare",
               "-mcpu=v11", "-mno-xl-soft-mul", "-mhard-float")
}
if ($isSdt) { $flags += '-DSDT' }

$tmp = Join-Path $env:TEMP ("hdr_selftest_" + [guid]::NewGuid().ToString("N") + ".c")
$fail = 0
$hdrs = Get-ChildItem "src" -Recurse -File |
        Where-Object { $_.Extension -eq '.h' }
foreach ($h in $hdrs) {
    "#include `"$($h.Name)`"" | Set-Content -Path $tmp -Encoding ASCII
    "int main(void){return 0;}"   | Add-Content -Path $tmp -Encoding ASCII
    $out = & $Gcc @flags @inc $tmp 2>&1
    if ($LASTEXITCODE -ne 0) {
        $fail++
        Write-Host "=== NOT SELF-CONTAINED: $($h.FullName.Replace($here + '\', '')) ==="
        $out | Where-Object { $_ -match 'error' } | Select-Object -First 5 | ForEach-Object { Write-Host "  $_" }
    }
}
Remove-Item $tmp -Force -ErrorAction SilentlyContinue
Pop-Location
if ($fail -eq 0) { Write-Host "HEADER SELF-CONTAINMENT PASSED ($($hdrs.Count) headers)"; exit 0 }
Write-Host "$fail header(s) not self-contained"; exit 1
