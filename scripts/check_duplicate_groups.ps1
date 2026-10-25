# ============================================================================
# check_duplicate_groups.ps1 -- compare controlled copies between two projects
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\check_duplicate_groups.ps1 `
#     -LeftRoot <project-a\src> -RightRoot <project-b\src> `
#     -MustMatch common/common_types.h,plt/plt_mmio.h `
#     -Observe mw/mw_ring.c
#
# No -MustMatch: prints all same-path differences but does not fail.
# With -MustMatch: mismatched or missing files fail.
# ============================================================================
param(
    [Parameter(Mandatory = $true)][string]$LeftRoot,
    [Parameter(Mandatory = $true)][string]$RightRoot,
    [string[]]$MustMatch = @(),
    [string[]]$Observe = @()
)

$ErrorActionPreference = "Stop"
$left = (Resolve-Path -LiteralPath $LeftRoot).Path
$right = (Resolve-Path -LiteralPath $RightRoot).Path

function Get-HashOrMissing([string]$root, [string]$relative) {
    $p = Join-Path $root $relative
    if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { return $null }
    return (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
}

$fail = 0
$checked = 0
$same = 0
$different = 0

function Compare-One([string]$relative, [bool]$mustFail) {
    $script:checked++
    $a = Get-HashOrMissing $left $relative
    $b = Get-HashOrMissing $right $relative
    if (-not $a -or -not $b) {
        Write-Host "[MISSING] $relative"
        if ($mustFail) { $script:fail++ }
        return
    }
    if ($a -eq $b) {
        $script:same++
        Write-Host "[SAME   ] $relative"
        return
    }
    $script:different++
    if ($mustFail) {
        Write-Host "[DRIFT  ] $relative"
        $script:fail++
    } else {
        Write-Host "[OBSERVE] $relative"
    }
}

foreach ($relative in $MustMatch) {
    if ($relative) { Compare-One $relative $true }
}
foreach ($relative in $Observe) {
    if ($relative) { Compare-One $relative $false }
}

# No explicit groups: compare all same-path .c/.h files as observation only.
if ($MustMatch.Count -eq 0 -and $Observe.Count -eq 0) {
    $pairs = @{}
    foreach ($root in @($left, $right)) {
        $rootFiles = Get-ChildItem -LiteralPath $root -Recurse -File |
                     Where-Object { ($_.Extension -eq '.c') -or ($_.Extension -eq '.h') }
        foreach ($f in $rootFiles) {
            $rel = $f.FullName.Substring($root.Length + 1).Replace('\', '/')
            $pairs[$rel] = $true
        }
    }
    foreach ($rel in ($pairs.Keys | Sort-Object)) {
        $a = Get-HashOrMissing $left $rel
        $b = Get-HashOrMissing $right $rel
        if ($a -and $b) { Compare-One $rel $false }
    }
}

Write-Host ""
Write-Host "DUPLICATE GROUP RESULT: checked=$checked same=$same different=$different fail=$fail"
if ($fail -gt 0) { exit 1 }
exit 0
