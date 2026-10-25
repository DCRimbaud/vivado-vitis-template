<#
.SYNOPSIS
  Generate CPU app skeleton (embedded-c skill): copy template + fill header placeholders.

.DESCRIPTION
  Project/device/date/tool placeholders are filled. Public author metadata is fixed in the SPDX header.

.EXAMPLE
  .\new_app_scaffold.ps1 -Dest <...\your_app.sdk\app\src> -Platform mb `
      -Project "<ProjectName>" -Device "<DevicePart>" -Date "2026/09" -ToolVersions "Vitis 2025.2"
#>
param(
    [Parameter(Mandatory = $true)][string]$Dest,
    [ValidateSet("mb", "zynq", "zynq_freertos")][string]$Platform = "mb",
    [string]$Project = "<PROJECT>",
    [string]$Device  = "<DEVICE>",
    [string]$Date    = (Get-Date -Format "yyyy/MM"),
    [string]$ToolVersions = "Vitis 2025.2",
    [switch]$Force
)

$app = "microblaze-bare"
if ($Platform -eq "zynq") { $app = "zynq-bare" }
if ($Platform -eq "zynq_freertos") { $app = "zynq-freertos" }

$repoRoot = Split-Path -Parent $PSScriptRoot
$tpl = Join-Path $repoRoot "templates\$app\src"

if (-not (Test-Path -LiteralPath $tpl)) {
    Write-Error "template not found: $tpl"
    exit 1
}
if (-not (Test-Path -LiteralPath $Dest)) {
    New-Item -ItemType Directory -Force -Path $Dest | Out-Null
}
elseif (-not $Force -and @(Get-ChildItem -LiteralPath $Dest -Force).Count -gt 0) {
    Write-Error "destination is not empty: $Dest (use -Force only after reviewing the target)"
    exit 2
}

Copy-Item -Recurse -Force -Path "$tpl\*" -Destination $Dest

# Baseline resource-budget header: kept outside the per-platform skeleton so
# heap/stack/pool/DMA budgets have one maintained source.
$memoryCfg = Join-Path $repoRoot "templates\modules\cfg_memory.h"
$cfgDir = Join-Path $Dest "cfg"
if ((Test-Path -LiteralPath $memoryCfg) -and (Test-Path -LiteralPath $cfgDir -PathType Container)) {
    Copy-Item -Force -LiteralPath $memoryCfg -Destination (Join-Path $cfgDir "cfg_memory.h")
}

$enc = New-Object System.Text.UTF8Encoding($false)
$n = 0
$scaffoldFiles = Get-ChildItem -Path $Dest -Recurse -File |
                 Where-Object { ($_.Extension -eq '.c') -or ($_.Extension -eq '.h') }
$scaffoldFiles | ForEach-Object {
    $t = [System.IO.File]::ReadAllText($_.FullName, [System.Text.Encoding]::UTF8)
    $t = $t.Replace("__PROJECT__", $Project).Replace("__DEVICE__", $Device).Replace("__DATE__", $Date)
    $t = $t.Replace("__TOOL_VERSIONS__", $ToolVersions)

    [System.IO.File]::WriteAllText($_.FullName, $t, $enc)
    $n++
}

Write-Host "scaffold created : $Dest"
Write-Host "platform        : $Platform ($app)"
Write-Host "files updated   : $n"
Write-Host "next steps      :"
Write-Host "  1) Vitis: add include paths (src and all subdirs; include src/plt/rtos for OSAL)"
Write-Host "  2) check drv_uart device id / plt_int irq ids against xparameters.h"
Write-Host "  3) run scripts\check_syntax.ps1 / check_headers.ps1 / check_layers.ps1 / check_build.ps1 (mb) or use -Toolchain arm for syntax/headers"
Write-Host "     Vitis 2025.2 SDT BSP: add -Sdt (or let the script auto-detect BASEADDR style)"
Write-Host "  4) 核对文件头 Tool Versions / Target Devices 是否为该工程实际值（占位符不会被自动猜出）"
Write-Host "  5) FreeRTOS 工程只允许 src/plt/rtos/freertos 出现 FreeRTOS 头；先跑 check_api_surface.ps1"
Write-Host "  6) 按工程实际值修改 cfg/cfg_memory.h；heap/stack/pool 预算不能沿用示例默认值"
