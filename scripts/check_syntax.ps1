# ============================================================================
# check_syntax.ps1 -- per-file syntax check (MicroBlaze, Vitis)
# Project: <app> (layered layout)
# Usage: powershell -ExecutionPolicy Bypass -File tools\check_syntax.ps1
#        [-Toolchain mb|arm] [-Bsp <bsp include dir>] [-Gcc <compiler path>]
#        [-VitisVersion <ver, default 2025.2>] [-ExtraInclude <dir,...>] [-Sdt]
#
# Notes:
#  * BSP include 解析顺序: -Bsp -> $env:XILINX_BSP -> 工程内自动探测 (*.sdk\*_bsp\include)。
#  * mb-gcc 必须匹配 -VitisVersion（默认 2025.2）：显式 -Gcc（版本须一致）->
#    版本专用环境变量 -> 版本匹配的 XILINX_VITIS -> 版本匹配的 PATH。
#    不匹配不再静默回退，直接报错退出（本机 env 2021.1 vs 工程 2025.2 的坑）。
#  * Compile flags below follow YOUR BSP MicroBlaze config
#    (-mcpu / -mxl-* / float mode) -- adjust if your BSP differs.
#  * Use the ELF toolchain (mb-gcc), NOT the linux_toolchain variant --
#    the latter predefines __linux__ which collides with Xilinx sleep.h.
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

# ---- SDT detection (Vitis 2025.x): drivers switch to BASEADDR APIs ----
# BSP headers expose XPAR_*_BASEADDR but may not expose the old DEVICE_ID
# macros. Allow explicit -Sdt and otherwise infer from the BSP fact set.
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
if ($Toolchain -eq "arm") {
    $flags = @("-fsyntax-only", "-Wall", "-mcpu=cortex-a9")
} else {
    $flags = @("-fsyntax-only", "-Wall",
               "-mlittle-endian", "-mxl-barrel-shift", "-mxl-pattern-compare", "-mcpu=v11",
               "-mno-xl-soft-mul", "-mhard-float", "-ffunction-sections", "-fdata-sections")
}
if ($isSdt) { $flags += '-DSDT' }

$fail = 0
$files = Get-ChildItem src -Recurse -File |
         Where-Object { $_.Extension -eq '.c' }
foreach ($f in $files) {
    $out = & $Gcc @flags @inc $f.FullName 2>&1
    if ($LASTEXITCODE -ne 0) {
        $fail++
        Write-Host "=== FAIL: $($f.FullName.Replace($here + '\', '')) ==="
        $out | Where-Object { $_ -match 'error|warning' } | Select-Object -First 10 | ForEach-Object { Write-Host "  $_" }
    }
}
Pop-Location
if ($fail -eq 0) { Write-Host "SYNTAX CHECK PASSED ($($files.Count) files)"; exit 0 }
Write-Host "$fail file(s) failed"
exit 1
