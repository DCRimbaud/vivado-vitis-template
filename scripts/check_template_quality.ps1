# ============================================================================
# check_template_quality.ps1 -- static quality gate for maintained templates
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts\check_template_quality.ps1
#   powershell -ExecutionPolicy Bypass -File scripts\check_template_quality.ps1 -Root <embedded-c skill>
#
# Scope:
#   - templates/modules
#   - templates/microblaze-bare
#   - templates/zynq-bare
#   - templates/zynq-freertos
#
# Hard failures:
#   - missing project-header placeholders / Keywords
#   - FreeRTOS headers outside plt/rtos/freertos
#   - Xilinx driver headers outside drv/plt
#   - mutable char ** or non-const extern in public headers
#   - generic file descriptions or empty function briefs
#   - missing @author / @param contracts
#   - long functions without Chinese step comments
#   - value-returning functions without return/failure documentation
# ============================================================================
param(
    [string]$Root = ""
)

$ErrorActionPreference = "Stop"
if (-not $Root) {
    $Root = Split-Path -Parent $PSScriptRoot
}
$Root = (Resolve-Path -LiteralPath $Root).Path
$templateRoot = Join-Path $Root "templates"
if (-not (Test-Path -LiteralPath $templateRoot -PathType Container)) {
    Write-Host "[ERROR] templates directory not found under: $Root"
    exit 2
}

$roots = @(
    (Join-Path $templateRoot "modules"),
    (Join-Path $templateRoot "microblaze-bare"),
    (Join-Path $templateRoot "zynq-bare"),
    (Join-Path $templateRoot "zynq-freertos")
)

$fail = 0
$checked = 0

function Get-CFunctionBody {
    param(
        [string]$Text,
        [int]$OpenBraceIndex
    )

    $depth = 0
    $state = "normal"
    for ($i = $OpenBraceIndex; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        $next = if (($i + 1) -lt $Text.Length) { $Text[$i + 1] } else { [char]0 }

        if ($state -eq "line") {
            if ($ch -eq "`n") {
                $state = "normal"
            }
            continue
        }
        if ($state -eq "block") {
            if (($ch -eq '*') -and ($next -eq '/')) {
                $state = "normal"
                $i++
            }
            continue
        }
        if (($state -eq "string") -or ($state -eq "char")) {
            if ($ch -eq '\') {
                $i++
                continue
            }
            if ((($state -eq "string") -and ($ch -eq '"')) -or
                (($state -eq "char") -and ($ch -eq "'"))) {
                $state = "normal"
            }
            continue
        }

        if (($ch -eq '/') -and ($next -eq '/')) {
            $state = "line"
            $i++
            continue
        }
        if (($ch -eq '/') -and ($next -eq '*')) {
            $state = "block"
            $i++
            continue
        }
        if ($ch -eq '"') {
            $state = "string"
            continue
        }
        if ($ch -eq "'") {
            $state = "char"
            continue
        }
        if ($ch -eq '{') {
            $depth++
            continue
        }
        if ($ch -eq '}') {
            $depth--
            if ($depth -eq 0) {
                return $Text.Substring($OpenBraceIndex + 1, $i - $OpenBraceIndex - 1)
            }
        }
    }

    return ""
}

function Get-CCodeLineCount {
    param([string]$Body)

    $count = 0
    foreach ($line in ($Body -split "`r?`n")) {
        $trimmed = $line.Trim()
        if (-not $trimmed) {
            continue
        }
        if ($trimmed.StartsWith('/*') -or $trimmed.StartsWith('*') -or $trimmed.StartsWith('//')) {
            continue
        }
        $count++
    }
    return $count
}


$legacyRoot = Join-Path (Join-Path $templateRoot "modules") "legacy"
if (Test-Path -LiteralPath $legacyRoot) {
    Write-Host "[FAIL] retired template root present: templates/modules/legacy"
    $fail++
}
$legacyHtml = Join-Path (Join-Path $Root "docs") "embedded-c-architecture-and-style.html"
if (Test-Path -LiteralPath $legacyHtml) {
    Write-Host "[FAIL] retired HTML specification present: docs/embedded-c-architecture-and-style.html"
    $fail++
}

$freertosRx = '#include\s+[<"](FreeRTOS\.h|task\.h|queue\.h|semphr\.h|portmacro\.h)'
$xlnxDriverRx = '(?i)#include\s+[<"](xuartlite|xuartps|xuartpsv|xspi|xspips|xgpio|xgpiops|xiic|xiicps|xemaclite|xemacps|xaxiethernet|xusb|xusbps|xusbpsu|xscugic|xintc|xaxidma|xaxicdma|xdmaps|xmcdma|xzdma|xbram|xtmrctr|xttcps|xscutimer|xscuwdt|xwdtps|xqspips|xqspipsu|xospipsv|xtime_l)(_[a-z0-9_]+)?\.h'

foreach ($base in $roots) {
    if (-not (Test-Path -LiteralPath $base -PathType Container)) {
        Write-Host "[FAIL] template root missing: $base"
        $fail++
        continue
    }

    $files = Get-ChildItem -LiteralPath $base -Recurse -File |
             Where-Object { ($_.Extension -eq '.c') -or ($_.Extension -eq '.h') }
    foreach ($f in $files) {
        $checked++
        $rel = $f.FullName.Substring($Root.Length + 1).Replace('\', '/')
        $text = [System.IO.File]::ReadAllText($f.FullName, [System.Text.Encoding]::UTF8)

        if ($text -notmatch '(?m)^// SPDX-License-Identifier:\s+MIT\s*$') {
            Write-Host "[FAIL] missing SPDX MIT header: $rel"
            $fail++
        }
        if ($text -notmatch '(?m)^// Author:\s+Xue Weisen <xueweisen96@gmail\.com>\s*$') {
            Write-Host "[FAIL] missing public author: $rel"
            $fail++
        }
        if ($text -notmatch '(?m)^// GitHub:\s+https://github\.com/DCRimbaud\s*$') {
            Write-Host "[FAIL] missing GitHub identity: $rel"
            $fail++
        }
        if ($text -notmatch '(?m)^// Keywords:') {
            Write-Host "[FAIL] missing Keywords: $rel"
            $fail++
        }
        $descriptionMatch = [regex]::Match($text, '(?m)^// Description:\s*(.+?)\s*$')
        if (-not $descriptionMatch.Success) {
            Write-Host "[FAIL] missing Description: $rel"
            $fail++
        } else {
            $description = $descriptionMatch.Groups[1].Value.Trim()
            if (($description -notmatch '[\u4e00-\u9fff]') -or
                ($description -match '^(实现|初始化|模板|待补充|TODO)[。.]?$')) {
                Write-Host "[FAIL] generic or empty Description: $rel"
                $fail++
            }
        }

        foreach ($brief in [regex]::Matches($text, '(?m)^\s*\*\s*@brief\s+(.+)$')) {
            if ($brief.Groups[1].Value -notmatch '[\u4e00-\u9fff]') {
                Write-Host "[FAIL] function brief must be Chinese: $rel"
                $fail++
            }
        }

        if ($text -match '(?m)^\s*(?:void|u8|u16|u32|u64|s8|s16|s32|int|char|const\s+char\s*\*)\s+[A-Za-z_][A-Za-z0-9_]*\s*\(\s*\)\s*;') {
            Write-Host "[FAIL] old-style empty parameter prototype: $rel"
            $fail++
        }

        $functionMatches = [regex]::Matches(
            $text,
            '(?ms)^[A-Za-z_][\w\s\*]*?\b([A-Za-z_][A-Za-z0-9_]*)\s*\(([^;{}]*)\)\s*\{')
        foreach ($fn in $functionMatches) {
            $functionName = $fn.Groups[1].Value
            $parameterText = $fn.Groups[2].Value
            $prefix = $text.Substring(0, $fn.Index).TrimEnd()
            if (-not $prefix.EndsWith('*/')) {
                Write-Host "[FAIL] function missing Doxygen block: $rel :: $functionName"
                $fail++
                continue
            }

            $docStart = $prefix.LastIndexOf('/**')
            $doc = $prefix.Substring($docStart)
            if ($doc -notmatch '(?m)^\s*\*\s*@brief\s+\S') {
                Write-Host "[FAIL] function missing @brief: $rel :: $functionName"
                $fail++
            } else {
                $briefMatch = [regex]::Match($doc, '(?m)^\s*\*\s*@brief\s+(.+?)\s*$')
                $brief = $briefMatch.Groups[1].Value.Trim()
                if (($brief -notmatch '[\u4e00-\u9fff]') -or
                    ($brief -match '^(初始化|实现|处理|执行操作|模板)[。.]?$')) {
                    Write-Host "[FAIL] empty or generic @brief: $rel :: $functionName"
                    $fail++
                }
            }
            if ($doc -notmatch '@author\s+Xue Weisen') {
                Write-Host "[FAIL] function missing fixed public @author: $rel :: $functionName"
                $fail++
            }

            if (($parameterText.Trim() -ne '') -and ($parameterText.Trim() -ne 'void')) {
                foreach ($parameter in ($parameterText -split ',')) {
                    $p = $parameter.Trim()
                    if ((-not $p) -or ($p -match '\.\.\.')) {
                        continue
                    }
                    if ($p -match '([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*$') {
                        $parameterName = $Matches[1]
                        if ($doc -notmatch ('(?m)@param(?:\[[^\]]+\])?\s+' +
                                            [regex]::Escape($parameterName) + '\b')) {
                            Write-Host "[FAIL] missing @param $parameterName`: $rel :: $functionName"
                            $fail++
                        }
                    }
                }
            }

            $body = Get-CFunctionBody -Text $text -OpenBraceIndex ($fn.Index + $fn.Length - 1)
            if (-not $body) {
                Write-Host "[FAIL] could not parse function body: $rel :: $functionName"
                $fail++
                continue
            }

            $codeLines = Get-CCodeLineCount -Body $body
            $hasChineseStep = $body -match '(?s)/\*.*?[\u4e00-\u9fff].*?\*/|//[^\r\n]*[\u4e00-\u9fff]'
            if (($codeLines -ge 15) -and (-not $hasChineseStep)) {
                Write-Host "[FAIL] function >=15 code lines has no Chinese step comment: $rel :: $functionName"
                $fail++
            }
            if (($functionName -ne 'main') -and
                ($body -match '\breturn\s+[^;\s]') -and
                ($doc -notmatch '@return|@retval')) {
                Write-Host "[FAIL] value-returning function lacks @return/@retval: $rel :: $functionName"
                $fail++
            }
        }

        if (($text -match $freertosRx) -and
            ($rel -notmatch '(^|/)(plt/rtos/freertos|plt_rtos/freertos)/')) {
            Write-Host "[FAIL] FreeRTOS header outside backend: $rel"
            $fail++
        }

        $isDriverOrPlatformLayer = ($rel -match '(^|/)(drv|plt)/') -or
                                   ($rel -match '(^|/)golden/(drv_|plt_)') -or
                                   ($rel -match '(^|/)modules/(drv_|plt_)')
        if (($text -match $xlnxDriverRx) -and (-not $isDriverOrPlatformLayer)) {
            Write-Host "[FAIL] Xilinx driver header outside drv/plt: $rel"
            $fail++
        }

        if ($f.Extension -eq ".h") {
            if ($text -match '\bchar\s*\*\s*\*') {
                Write-Host "[FAIL] mutable char ** in public header: $rel"
                $fail++
            }
            if ($text -match '(?m)^\s*extern\s+(?!const\b)[^;]+;') {
                Write-Host "[FAIL] mutable extern global in public header: $rel"
                $fail++
            }
        }
    }
}

Write-Host "TEMPLATE QUALITY RESULT: files=$checked fail=$fail"
if ($fail -gt 0) {
    exit 1
}
exit 0
