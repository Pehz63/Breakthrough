# smoke_test_gui.ps1 - Standard GUI smoke test.
#
# Builds (optionally), launches breakthrough_gui.exe, waits a few seconds to let
# it initialize and render, captures a screenshot, then closes it. Use this after
# any GUI change to confirm the window still opens and renders without crashing.
#
# Usage (from the project root):
#   .\smoke_test_gui.ps1            # screenshot the already-built exe
#   .\smoke_test_gui.ps1 -Build     # rebuild first, then screenshot
#   .\smoke_test_gui.ps1 -Seconds 5 # wait longer before capturing
#   .\smoke_test_gui.ps1 -KeepOpen  # leave the window open after capturing
#
# The screenshot is written to build\ (git-ignored), so it never clutters commits.
# Exit code 0 = the process stayed alive and a screenshot was captured;
# non-zero = build failed or the process exited early (likely a crash).

param(
    [switch]$Build,
    [int]$Seconds = 3,
    [string]$Exe = ".\breakthrough_gui.exe",
    [string]$Out = "build\gui_smoke.png",
    [switch]$KeepOpen
)

$ErrorActionPreference = "Stop"

# Always stop stray instances first: a running exe locks the output file and
# blocks rebuilds (LNK1104).
Get-Process -Name breakthrough_gui -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue

if ($Build) {
    Write-Host "Building..."
    if (Test-Path $Exe) { Remove-Item $Exe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_gui.bat" | Select-Object -Last 3
    if (-not (Test-Path $Exe)) {
        Write-Error "Build did not produce $Exe."
        exit 1
    }
}

if (-not (Test-Path $Exe)) {
    Write-Error "$Exe not found. Run with -Build, or run .\build_gui.bat first."
    exit 1
}

# Make sure the output directory exists.
$outDir = Split-Path $Out -Parent
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }

Write-Host "Launching $Exe ..."
$p = Start-Process -FilePath $Exe -PassThru
Start-Sleep -Seconds $Seconds

if ($p.HasExited) {
    Write-Error "Process exited early (exit code $($p.ExitCode)). Likely a startup crash."
    exit 2
}

Write-Host "Process alive (PID $($p.Id)). Capturing screen -> $Out"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$bounds = [System.Windows.Forms.SystemInformation]::VirtualScreen
$bmp = New-Object System.Drawing.Bitmap($bounds.Width, $bounds.Height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$bmp.Save($Out)
$g.Dispose()
$bmp.Dispose()

if (-not $KeepOpen) {
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    Write-Host "Closed the window."
} else {
    Write-Host "Left the window open (PID $($p.Id))."
}

Write-Host "Smoke test OK. Screenshot saved to $Out"
exit 0
