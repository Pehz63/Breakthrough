# gui_shot.ps1 - screenshot the native GUI without ever showing a window.
#
# Runs breakthrough_gui.exe in its --capture mode: the window is created hidden,
# the frames render into an offscreen texture, the last one is saved as a PNG,
# and the process exits. Nothing takes focus, so this is safe to run while a
# full-screen game is open. The user's gui_settings.txt, favorites and history
# are neither read nor written in capture mode.
#
# Examples:
#   .\tools\gui_shot.ps1                                   # default scenario -> build\gui_shots\default.png
#   .\tools\gui_shot.ps1 -Scenario analysis -Moves "c1c,f6f"
#   .\tools\gui_shot.ps1 -All                              # every scenario below, in parallel
#
# Scenarios (comma-separated, applied in order): library, standings, presets,
# editor, models, analysis, view, simple, watch (simple mode's Watch), hints
# (analysis on), aivai, red, flip, nopanel, hard. -All also shoots simple mode at
# phone sizes (portrait 390x760 and 360x640, landscape 844x390), where it takes
# the stacked layout or the compact panel.
param(
    [string]$Scenario = "",
    [string]$Moves = "",
    [int]$Frames = 180,
    [string]$Size = "1280x820",
    [string]$Out = "",
    [switch]$All,
    [switch]$Build
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if ($Build) { & cmd /c ".\build_gui.bat"; if ($LASTEXITCODE -ne 0) { exit 1 } }
if (-not (Test-Path .\breakthrough_gui.exe)) { Write-Error "breakthrough_gui.exe not found (run with -Build)"; exit 1 }
$dir = Join-Path $root "build\gui_shots"
New-Item -ItemType Directory -Force $dir | Out-Null

function Start-Shot([string]$name, [string]$scen, [string]$mv, [int]$frames, [string]$size = $Size) {
    $png = Join-Path $dir "$name.png"
    $a = @("--capture", "`"$png`"", "--frames", "$frames", "--size", $size)
    if ($scen) { $a += @("--scenario", $scen) }
    if ($mv)   { $a += @("--moves", $mv) }
    $log = Join-Path $dir "$name.log"
    $p = Start-Process -FilePath .\breakthrough_gui.exe -ArgumentList $a -PassThru -NoNewWindow `
        -RedirectStandardOutput $log -RedirectStandardError "$log.err"
    $null = $p.Handle    # without an open handle, ExitCode reads back empty after exit
    $p
}

$procs = @()
if ($All) {
    $set = @(
        @("default", "", "c1c,f6f"), @("analysis", "analysis", "c1c,f6f,b1c"), @("library", "library", ""),
        @("standings", "standings", ""), @("presets", "presets", ""), @("editor", "editor", ""),
        @("models", "models", ""), @("simple", "simple", "d1d"), @("aivai", "aivai", ""),
        @("view", "view,red,flip", "c1c")
    )
    foreach ($s in $set) {
        $f = if ($s[0] -eq "aivai" -or $s[0] -eq "models") { 420 } else { $Frames }
        $procs += Start-Shot $s[0] $s[1] $s[2] $f
    }
    $procs += Start-Shot "phone" "simple,hints" "" $Frames "390x760"
    $procs += Start-Shot "phone_watch" "watch,hints" "" 420 "360x640"
    $procs += Start-Shot "phone_landscape" "simple" "" $Frames "844x390"
} else {
    $name = if ($Out) { [IO.Path]::GetFileNameWithoutExtension($Out) } elseif ($Scenario) { ($Scenario -replace ",", "_") } else { "default" }
    $procs += Start-Shot $name $Scenario $Moves $Frames
}
$ok = $true
foreach ($p in $procs) {
    if (-not $p.WaitForExit(120000)) { $p.Kill(); Write-Warning "a capture timed out (pid $($p.Id))"; $ok = $false }
    elseif ($p.ExitCode -ne 0) { Write-Warning "a capture exited with $($p.ExitCode)"; $ok = $false }
}
Get-ChildItem $dir -Filter *.png | Sort-Object LastWriteTime -Descending | Select-Object -First $procs.Count Name, Length, LastWriteTime
if (-not $ok) { exit 1 }
