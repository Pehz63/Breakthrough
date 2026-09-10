# web_preloads.ps1 - print the emcc --preload-file arguments for the web build.
#
# The web page runs gui/presets.txt's agents, and a learned agent's canonical id
# is checked against its model file's hash when it loads, so every model slot a
# preset names must be bundled at the same relative path it has on disk. The
# slot -> file mapping mirrors rankSlotFile in src/ranking.cpp (slot 0
# lin_value, 1 lin_policy, 2 pst_value, 3 and up models/sweep/slotN.txt).
# build_web.bat captures this script's single output line.
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function SlotFile([int]$slot) {
    switch ($slot) {
        0 { return "models/lin_value.txt" }
        1 { return "models/lin_policy.txt" }
        2 { return "models/pst_value.txt" }
        default { return "models/sweep/slot$slot.txt" }
    }
}

# mlAutoLoadDefaultSlots loads these two at startup.
$files = New-Object System.Collections.Generic.List[string]
$files.Add("models/lin_value.txt")
$files.Add("models/lin_policy.txt")
foreach ($line in Get-Content gui\presets.txt) {
    if ($line -match '^\s*#' -or $line.Trim() -eq "") { continue }
    foreach ($m in [regex]::Matches($line, 'model=(\d+)')) {
        $f = SlotFile ([int]$m.Groups[1].Value)
        if (-not $files.Contains($f)) { $files.Add($f) }
    }
}
$parts = @()
foreach ($f in $files) {
    if (-not (Test-Path $f)) { [Console]::Error.WriteLine("missing model file for a preset: $f"); exit 1 }
    $parts += "--preload-file $f@$f"
}
Write-Output ($parts -join " ")
