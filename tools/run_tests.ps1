# run_tests.ps1 - Build (optionally) and run the Breakthrough test suite.
#
# Usage (always run from the project root):
#   .\tools\run_tests.ps1           # run already-built tests.exe
#   .\tools\run_tests.ps1 -Build    # rebuild tests.exe first, then run
#
# Exit code 0 = all tests passed; non-zero = build failed or tests reported failures.

param(
    [switch]$Build,
    [string]$Exe = ".\tests.exe"
)

$ErrorActionPreference = "Stop"

if ($Build) {
    Write-Host "Building tests..."
    if (Test-Path $Exe) { Remove-Item $Exe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_tests.bat"
    if (-not (Test-Path $Exe)) {
        Write-Error "Build did not produce $Exe."
        exit 1
    }
    Write-Host "Build OK -> $Exe"
}

if (-not (Test-Path $Exe)) {
    Write-Error "$Exe not found. Run with -Build, or run .\build_tests.bat first."
    exit 1
}

Write-Host "Running $Exe ..."
& $Exe
$code = $LASTEXITCODE
if ($code -eq 0) {
    Write-Host "All tests passed."
} else {
    Write-Error "Tests FAILED (exit code $code)."
}
exit $code
