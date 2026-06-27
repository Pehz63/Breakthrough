# run_train.ps1 - Build (optionally) and run the Breakthrough ML trainer.
#
# Usage (always run from the project root):
#   .\tools\run_train.ps1 -Build selfplay-supervised --games 300 --epochs 12
#   .\tools\run_train.ps1 tournament --games 4
#   .\tools\run_train.ps1 docs
#
# -Build rebuilds train.exe via build_train.bat first. Remaining args are passed
# straight through to train.exe.

param(
    [switch]$Build,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args
)

$ErrorActionPreference = "Stop"
$Exe = ".\train.exe"

if ($Build) {
    Write-Host "Building train.exe..."
    if (Test-Path $Exe) { Remove-Item $Exe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_train.bat"
    if (-not (Test-Path $Exe)) {
        Write-Error "Build did not produce $Exe."
        exit 1
    }
    Write-Host "Build OK -> $Exe"
}

if (-not (Test-Path $Exe)) {
    Write-Error "$Exe not found. Run with -Build first."
    exit 1
}

& $Exe @Args
exit $LASTEXITCODE
