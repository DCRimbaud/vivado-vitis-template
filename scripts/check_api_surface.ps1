# ============================================================================
# check_api_surface.ps1 -- public C API boundary check
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\check_api_surface.ps1
#   powershell -ExecutionPolicy Bypass -File tools\check_api_surface.ps1 -Root <project>
#
# Hard failures:
#   - FreeRTOS headers outside src\plt\rtos\freertos
#   - FreeRTOS types in any public header
#   - Xilinx/FreeRTOS/log headers and MMIO access in common/
#   - Xilinx device-driver headers or driver types outside drv/plt
#   - mutable extern globals and mutable char ** in public headers
#
# Warnings:
#   - function-pointer typedef without obvious ctx/self/ref/arg/user parameter
# ============================================================================
param(
    [string]$Root = "."
)

$ErrorActionPreference = "Stop"
$here = (Resolve-Path -LiteralPath $Root).Path
$src = Join-Path $here "src"
if (-not (Test-Path -LiteralPath $src -PathType Container)) {
    Write-Host "[ERROR] src directory not found under: $here"
    exit 2
}

$fail = 0
$warn = 0
$files = Get-ChildItem -LiteralPath $src -Recurse -File |
         Where-Object { ($_.Extension -eq '.c') -or ($_.Extension -eq '.h') }
$headers = Get-ChildItem -LiteralPath $src -Recurse -File |
           Where-Object { $_.Extension -eq '.h' }

function Get-RelativePath([string]$fullPath) {
    return $fullPath.Substring($here.Length + 1).Replace('\', '/')
}

function Test-UnderPath([string]$relativePath, [string]$prefix) {
    return $relativePath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)
}

# ---- FreeRTOS header isolation ----
foreach ($f in $files) {
    $rel = Get-RelativePath $f.FullName
    $text = [System.IO.File]::ReadAllText($f.FullName, [System.Text.Encoding]::UTF8)
    if ($text -match '#include\s+[*"<](FreeRTOS\.h|task\.h|queue\.h|semphr\.h|portmacro\.h)') {
        if (-not (Test-UnderPath $rel 'src/plt/rtos/freertos/')) {
            Write-Host "[FAIL] FreeRTOS header outside backend: $rel"
            $fail++
        }
    }
}

# ---- Public header OS types and mutable command argv ----
$rtosTypes = 'TickType_t|TaskHandle_t|QueueHandle_t|SemaphoreHandle_t|BaseType_t|pdMS_TO_TICKS|portMAX_DELAY'
$xlnxDriverTypes = '\b(XUartLite|XUartPs|XUartPsv|XSpi|XSpiPs|XGpio|XGpioPs|XIic|XIicPs|XEmacLite|XEmacPs|XAxiEthernet|XUsb|XUsbPs|XUsbPsu|XScuGic|XIntc|XAxiDma|XAxiCdma|XDmaPs|XMcdma|XZDma|XBram|XTmrCtr|XTtcPs|XScuTimer|XScuWdt|XWdtPs|XQspiPs|XQspiPsu|XOspiPsv|XTime)\b'
$xlnxDriverHeader = '(?i)^(xuartlite|xuartps|xuartpsv|xspi|xspips|xgpio|xgpiops|xiic|xiicps|xemaclite|xemacps|xaxiethernet|xusb|xusbps|xusbpsu|xscugic|xintc|xaxidma|xaxicdma|xdmaps|xmcdma|xzdma|xbram|xtmrctr|xttcps|xscutimer|xscuwdt|xwdtps|xqspips|xqspipsu|xospipsv|xtime_l)(_[a-z0-9_]+)?\.h$'
foreach ($h in $headers) {
    $rel = Get-RelativePath $h.FullName
    $text = [System.IO.File]::ReadAllText($h.FullName, [System.Text.Encoding]::UTF8)

    if ($text -match $rtosTypes) {
        Write-Host "[FAIL] FreeRTOS type leaked into public header: $rel"
        $fail++
    }

    if ($text -match '\bchar\s*\*\s*\*') {
        Write-Host "[FAIL] mutable char ** in public header: $rel"
        Write-Host "       use 'const char *const argv[]' for read-only command views"
        $fail++
    }

    if (-not ((Test-UnderPath $rel 'src/drv/') -or (Test-UnderPath $rel 'src/plt/'))) {
        if ($text -match $xlnxDriverTypes) {
            Write-Host "[FAIL] Xilinx driver type leaked into public header: $rel"
            Write-Host "       keep X*_Config/instance/BD/endpoint types in drv/xlnx or plt"
            $fail++
        }
    }

    if ($text -match '(?m)^\s*extern\s+(?!const\b)[^;]+;') {
        Write-Host "[FAIL] mutable extern global in public header: $rel"
        Write-Host "       expose a const object or a read-only accessor"
        $fail++
    }
}

# ---- Xilinx device-driver include boundary ----
foreach ($f in $files) {
    $rel = Get-RelativePath $f.FullName
    if ((Test-UnderPath $rel 'src/drv/') -or (Test-UnderPath $rel 'src/plt/')) {
        continue
    }
    $text = [System.IO.File]::ReadAllText($f.FullName, [System.Text.Encoding]::UTF8)
    foreach ($line in ($text -split "`n")) {
        if ($line -match '#include\s+["<]([^">]+)[">]') {
            $inc = $Matches[1]
            $leaf = Split-Path $inc -Leaf
            if ($leaf -match $xlnxDriverHeader) {
                Write-Host "[FAIL] Xilinx device-driver header outside drv/plt: $rel -> $inc"
                $fail++
            }
        }
    }
}

# ---- common purity ----
foreach ($f in $files) {
    $rel = Get-RelativePath $f.FullName
    if (-not (Test-UnderPath $rel 'src/common/')) { continue }
    $text = [System.IO.File]::ReadAllText($f.FullName, [System.Text.Encoding]::UTF8)
    if (($text -match '#include\s+[<"](FreeRTOS\.h|task\.h|queue\.h|semphr\.h|portmacro\.h|xparameters\.h|sleep\.h|xil_[^"]+\.h)') -or
        ($text -match '\*\(\s*volatile\s+(const\s+)?u(8|16|32|64)\s*\*')) {
        Write-Host "[FAIL] hardware/OS/printf dependency in common: $rel"
        $fail++
    }
}

# ---- Callback context warning ----
foreach ($h in $headers) {
    $rel = Get-RelativePath $h.FullName
    $lineNo = 0
    foreach ($line in [System.IO.File]::ReadAllLines($h.FullName, [System.Text.Encoding]::UTF8)) {
        $lineNo++
        if ($line -match 'typedef\s+.*\(\s*\*\s*[A-Za-z_][A-Za-z0-9_]*\s*\)\s*\(') {
            if ($line -notmatch '\b(ctx|self|ref|arg|user|argv)\b') {
                Write-Host "[WARN] callback typedef has no visible context parameter: $rel`:$lineNo"
                Write-Host "       $($line.Trim())"
                $warn++
            }
        }
    }
}

Write-Host ""
Write-Host "API SURFACE RESULT: fail=$fail warn=$warn"
if ($fail -gt 0) { exit 1 }
exit 0
