# web_preloads.ps1 - print the em++ --preload-file arguments for the web build.
#
# The web page runs gui/presets.txt's agents. A learned agent's canonical id
# carries its model file's hash (rankFileHash8 in src/ranking.cpp: FNV-1a-64 over
# the file's bytes), and loading the agent checks it. The page therefore bundles
# byte-exact copies kept in gui/web_models/, which git stores with line-ending
# conversion off (gui/web_models/.gitattributes), so a Linux checkout such as the
# GitHub Pages workflow gets the same bytes that were hashed on Windows. Each copy
# is mounted at the path the engine reads (rankSlotFile: slot 0 lin_value, 1
# lin_policy, 2 pst_value, 3 and up models/sweep/slotN.txt).
#
#   tools\web_preloads.ps1          print the arguments (build_web.bat and
#                                   build_web.sh capture the one output line)
#   tools\web_preloads.ps1 -Sync    first copy the preset model files from
#                                   models\ into gui\web_models\
#
# Either way every snapshot's hash is checked against the id that names it, and a
# missing file or a mismatch fails with a message on stderr.
param([switch]$Sync)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
$snap = "gui/web_models"

function SlotFile([int]$slot) {
    switch ($slot) {
        0 { return "models/lin_value.txt" }
        1 { return "models/lin_policy.txt" }
        2 { return "models/pst_value.txt" }
        default { return "models/sweep/slot$slot.txt" }
    }
}

# First 8 hex chars of FNV-1a-64 over the file's bytes, as rankFileHash8. The
# 64-bit state is kept as two 32-bit halves so no product overflows an int64.
# Multiplying by the FNV prime 2^40 + 435 is h*435 plus (lo << 8) into the high half.
# The starting value is the engine's 1469598103934665603 (0x14650fb0739d0383),
# which is not the standard FNV offset basis. Ids are minted with it, so match it.
function Hash8([string]$path) {
    $mask = [long]4294967295
    [long]$hi = 342167472       # 0x14650fb0
    [long]$lo = 1939669891      # 0x739d0383
    foreach ($byte in [System.IO.File]::ReadAllBytes((Join-Path $root $path))) {
        $lo = $lo -bxor $byte
        $a = $lo * 435
        $hi = ($hi * 435 + ($a -shr 32) + (($lo -shl 8) -band $mask)) -band $mask
        $lo = $a -band $mask
    }
    return "{0:x8}" -f $hi
}

# mlAutoLoadDefaultSlots loads these two at startup. They carry no id hash.
$files = New-Object System.Collections.Generic.List[string]
$files.Add("models/lin_value.txt")
$files.Add("models/lin_policy.txt")
$want = @{}   # file -> hash named by a preset id
foreach ($line in Get-Content gui\presets.txt) {
    if ($line -match '^\s*#' -or $line.Trim() -eq "") { continue }
    foreach ($m in [regex]::Matches($line, 'model=(\d+),([0-9a-f]{8})')) {
        $f = SlotFile ([int]$m.Groups[1].Value)
        if (-not $files.Contains($f)) { $files.Add($f) }
        $want[$f] = $m.Groups[2].Value
    }
}

$parts = @()
$bad = $false
foreach ($f in $files) {
    $copy = "$snap/$f"
    if ($Sync) {
        if (-not (Test-Path $f)) { [Console]::Error.WriteLine("missing model file for a preset: $f"); exit 1 }
        New-Item -ItemType Directory -Force (Split-Path -Parent $copy) | Out-Null
        Copy-Item $f $copy -Force
    }
    if (-not (Test-Path $copy)) {
        [Console]::Error.WriteLine("missing $copy. Run tools\web_preloads.ps1 -Sync on a machine whose models\ has $f.")
        $bad = $true
        continue
    }
    if ($want.ContainsKey($f)) {
        $h = Hash8 $copy
        if ($h -ne $want[$f]) {
            [Console]::Error.WriteLine("$copy hashes to $h but gui/presets.txt names $($want[$f]). Run -Sync if models\ holds the right file, else re-pin the preset id.")
            $bad = $true
        }
    }
    $parts += "--preload-file $copy@$f"
}
if ($bad) { exit 1 }
Write-Output ($parts -join " ")
