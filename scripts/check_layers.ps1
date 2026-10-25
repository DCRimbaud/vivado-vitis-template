# ============================================================================
# check_layers.ps1 -- dependency direction & compile isolation check
# for the layered application.
#
# Layer dependency whitelist (project headers):
#   common : common
#   cfg    : common, cfg
#   drv    : common, cfg, plt, drv
#   plt    : common, cfg, plt
#   mw     : common, cfg, drv, plt, mw
#   app    : common, cfg, drv, plt, mw, app
#
# Xilinx headers: fully allowed in drv/plt; in other layers only a baseline
#   whitelist {xstatus xil_printf xil_types xil_assert xil_io xil_cache
#   sleep xparameters} is allowed (hardware-independent / address constants).
#
# Known-debt list: pre-existing couplings from the original code that are
#   recorded but must not grow (reported as [DEBT], not counted as failures).
#   New templates must not add known-debt entries. The list is only for
#   pre-existing business projects that need a staged migration.
# Same-layer cycles: business layers may be split into subdirectories
#   (e.g. src/app/cmd, src/app/flow, src/app/svc); the layer name is the
#   first directory under src/. Include cycles between same-layer modules
#   are reported as [CYCLE]. Unknown-layer files are [SKIP]-ed and counted
#   (never skipped silently - silent skips produce false passes).
# ============================================================================
param()
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $here

# ---------- preconditions ----------
# 找不到 src\ 时必须报错退出：否则后面的 Get-ChildItem 只是打空，
# 脚本会打印 "LAYER CHECK OK" 造成静默假通过（曾实测复现）。
if (-not (Test-Path -LiteralPath 'src' -PathType Container)) {
    Write-Host "[ERROR] 找不到 src\ 目录（当前目录: $((Get-Location).Path)）" -ForegroundColor Red
    Write-Host "        用法：本脚本放进工程的 tools\ 下，从工程根运行（src\ 与 tools\ 同级）" -ForegroundColor Red
    exit 2
}

$bad = 0
$debt = 0

# ---------- 层识别（支持业务层子目录：app/cmd、app/flow、app/svc） ----------
# 层 = 相对 src 的第一段目录；第一段不在白名单时回退到文件所在目录名（兼容旧工程）。
$srcRoot = (Resolve-Path -LiteralPath 'src').Path
function Get-LayerName([string]$fullPath) {
    $rel = $fullPath.Substring($srcRoot.Length + 1)
    $first = ($rel -split '[\\/]')[0]
    if ($allowDep.ContainsKey($first)) { return $first }
    $dirName = Split-Path (Split-Path $fullPath -Parent) -Leaf
    if ($allowDep.ContainsKey($dirName)) { return $dirName }
    return $null
}

$allowDep = @{
    'common' = @('common')
    'cfg'    = @('common', 'cfg')
    'drv'    = @('common', 'cfg', 'plt', 'drv')
    'mw'     = @('common', 'cfg', 'drv', 'plt', 'mw')
    'plt'    = @('common', 'cfg', 'plt')
    'app'    = @('common', 'cfg', 'drv', 'plt', 'mw', 'app')
}

# ---------- header -> layer map ----------
$owner = @{}
$headers = Get-ChildItem "src" -Recurse -File |
           Where-Object { $_.Extension -eq '.h' }
$headers | ForEach-Object {
    $owner[$_.Name] = (Get-LayerName $_.FullName)
}
$xilOk = @('drv', 'plt')
$xilWhite = @('xstatus.h', 'xil_printf.h', 'xil_types.h', 'xil_assert.h', 'xil_io.h', 'xil_cache.h', 'sleep.h', 'xparameters.h')

# pre-existing couplings recorded as debt (file|include); do not add new ones
$knownDebt = @()

# ---------- header name conflict ----------
$dup = $headers | Group-Object Name | Where-Object { $_.Count -gt 1 }
foreach ($g in $dup) {
    Write-Host "[NAME CONFLICT] '$($g.Name)' x$($g.Count)"
    $bad++
}

# ---------- scan ----------
$unknownLayer = 0
$files = Get-ChildItem "src" -Recurse -File |
         Where-Object { ($_.Extension -eq '.c') -or ($_.Extension -eq '.h') }
foreach ($f in $files) {
    $layer = Get-LayerName $f.FullName
    $rel = $f.FullName.Replace($here + '\', '')
    if (-not $layer) {
        $parent = Split-Path $rel -Parent
        if ($parent -eq 'src') {
            Write-Host "[INFO ] $rel : 根组合根文件，跳过层检查"
        } else {
            Write-Host "[SKIP ] $rel : 层无法识别（src 第一段目录不在白名单）"
            $unknownLayer++
        }
        continue
    }

    foreach ($hit in (Select-String -Path $f.FullName -Pattern '#include\s+["<]([^">]+)[">]' -AllMatches)) {
        foreach ($g in $hit.Matches) {
            $incPath = $g.Groups[1].Value
            $inc = Split-Path $incPath -Leaf
            $violation = $null

            if ($owner.ContainsKey($inc)) {
                $tgt = $owner[$inc]
                if ($allowDep[$layer] -notcontains $tgt) { $violation = "LAYER -> $inc ($layer -> $tgt)" }
            }
            elseif ($inc -match '^(x[a-z0-9_]*\.h|xtime_l\.h|sleep\.h)$') {
                if (($xilOk -notcontains $layer) -and ($xilWhite -notcontains $inc)) { $violation = "ISOLATION-XIL -> $inc" }
            }

            if ($violation) {
                if ($knownDebt -contains ($rel + '|' + $inc)) {
                    Write-Host "[DEBT ] $rel : $violation"
                    $debt++
                } else {
                    Write-Host "[VIOLATION] $rel : $violation"
                    $bad++
                }
            }
        }
    }
}

# ---------- 同层环检测（模块级 include 图，只保留同层边） ----------
# 模块 = 文件基名（app_cmd.c / app_cmd.h 同属模块 app_cmd）。
# 同层内 A、B 互相 include 即同层环；报 [CYCLE]，计入 $bad。
$edges = @{}
foreach ($f in $files) {
    $layer = Get-LayerName $f.FullName
    if (-not $layer) { continue }
    $from = $layer + '|' + [IO.Path]::GetFileNameWithoutExtension($f.Name)
    foreach ($hit in (Select-String -Path $f.FullName -Pattern '#include\s+["<]([^">]+)[">]' -AllMatches)) {
        foreach ($g in $hit.Matches) {
            $inc = Split-Path $g.Groups[1].Value -Leaf
            if (-not $owner.ContainsKey($inc)) { continue }
            if ($owner[$inc] -ne $layer) { continue }
            $to = $layer + '|' + [IO.Path]::GetFileNameWithoutExtension($inc)
            if ($to -eq $from) { continue }
            if (-not $edges.ContainsKey($from)) { $edges[$from] = New-Object System.Collections.Generic.HashSet[string] }
            [void]$edges[$from].Add($to)
        }
    }
}
$color = @{}
$reportedCycles = New-Object System.Collections.Generic.HashSet[string]
function Find-Cycle([string]$node, [System.Collections.Generic.List[string]]$stack) {
    $color[$node] = 1
    [void]$stack.Add($node)
    if ($edges.ContainsKey($node)) {
        foreach ($next in $edges[$node]) {
            $state = if ($color.ContainsKey($next)) { $color[$next] } else { 0 }
            if ($state -eq 0) {
                Find-Cycle $next $stack
            } elseif ($state -eq 1) {
                $idx = $stack.IndexOf($next)
                if ($idx -ge 0) {
                    $path = @($stack[$idx..($stack.Count - 1)]) + $next
                    $key = (@($path | Sort-Object) -join ';')
                    if (-not $reportedCycles.Contains($key)) {
                        [void]$reportedCycles.Add($key)
                        Write-Host ("[CYCLE] " + ($path -join ' -> '))
                        $script:bad++
                    }
                }
            }
        }
    }
    $stack.RemoveAt($stack.Count - 1)
    $color[$node] = 2
}
foreach ($n in @($edges.Keys)) {
    $state = if ($color.ContainsKey($n)) { $color[$n] } else { 0 }
    if ($state -eq 0) { Find-Cycle $n (New-Object 'System.Collections.Generic.List[string]') }
}

Pop-Location
Write-Host "known debt entries: $debt"
if ($unknownLayer -gt 0) { Write-Host "unknown-layer files skipped: $unknownLayer" }
if ($bad -eq 0) { Write-Host "LAYER CHECK OK (dependency direction + same-layer cycles + compile isolation; debt registered)"; exit 0 }
Write-Host "$bad NEW VIOLATION(S)"; exit 1
