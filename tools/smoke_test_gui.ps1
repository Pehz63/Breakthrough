# smoke_test_gui.ps1 - Standard GUI smoke test.
#
# Builds (optionally), runs breakthrough_gui.exe in its hidden --capture mode for
# a few seconds, and saves the last frame as a screenshot. The window is created
# hidden and never takes focus, so this is safe to run while a full-screen game or
# anything else is in the foreground. Use it after any GUI change to confirm the
# app still starts, renders, and exits cleanly.
#
# Usage (from the project root):
#   .\tools\smoke_test_gui.ps1            # screenshot the already-built exe
#   .\tools\smoke_test_gui.ps1 -Build     # rebuild first, then screenshot
#   .\tools\smoke_test_gui.ps1 -Frames 300
#   .\tools\smoke_test_gui.ps1 -Visible   # the old behavior: open a real window,
#                                         # grab the whole screen, close it
#
# -Visible refuses to run while a game from a Steam or Epic library is running,
# since it would pop a window over it. For more scenarios (library, editor,
# analysis, simple mode, agent vs agent) use tools\gui_shot.ps1.
#
# The screenshot goes to build\ (git-ignored). Exit code 0 = it built, ran, and
# wrote the screenshot. Non-zero = the build failed, the app crashed, or it hung.

param(
    [switch]$Build,
    [int]$Frames = 180,
    [string]$Exe = ".\breakthrough_gui.exe",
    [string]$Out = "build\gui_smoke.png",
    [switch]$Visible,
    [int]$Seconds = 3
)

$ErrorActionPreference = "Stop"

if ($Build) {
    # A running exe locks the output file and blocks the rebuild (LNK1104).
    Get-Process -Name breakthrough_gui -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
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

$outDir = Split-Path $Out -Parent
if ($outDir -and -not (Test-Path $outDir)) { New-Item -ItemType Directory -Force $outDir | Out-Null }
if (Test-Path $Out) { Remove-Item $Out -Force }

if (-not $Visible) {
    Write-Host "Running $Exe hidden for $Frames frames..."
    $full = Join-Path (Get-Location) $Out
    $p = Start-Process -FilePath $Exe -ArgumentList @("--capture", "`"$full`"", "--frames", "$Frames") `
        -PassThru -NoNewWindow -RedirectStandardOutput "$full.log" -RedirectStandardError "$full.err"
    $null = $p.Handle
    if (-not $p.WaitForExit(120000)) {
        $p.Kill()
        Write-Error "The capture run did not finish within 120 s (hung)."
        exit 3
    }
    if ($p.ExitCode -ne 0 -or -not (Test-Path $Out)) {
        Write-Error "The capture run failed (exit code $($p.ExitCode)). See $full.log"
        exit 2
    }
    Write-Host "Smoke test OK. Screenshot saved to $Out"
    exit 0
}

# ---- -Visible: a real window and a full-screen grab ----
$games = Get-Process | Where-Object { $_.Path -and ($_.Path -match '\\steamapps\\common\\|\\Epic Games\\') }
if ($games) {
    Write-Error ("Not opening a visible window while a game is running: " + (($games | Select-Object -ExpandProperty Name -Unique) -join ", ") + ". Run without -Visible instead.")
    exit 4
}
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
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
Write-Host "Smoke test OK. Screenshot saved to $Out"
exit 0
