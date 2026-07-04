# run_rank.ps1 - Build (optionally) and run the agent Elo ranker (rank.exe).
#
# Serial (default, gives clean ms/move timing):
#   .\tools\run_rank.ps1 -Build run --games 8
#   .\tools\run_rank.ps1 check
#   .\tools\run_rank.ps1 history --agent "ab(d4"
#
# Parallel play across processes (rows are tagged par=K and excluded from the
# default ms/move figures):
#   .\tools\run_rank.ps1 -Workers 8 --games 8
#
# The engine keeps board/eval state in globals, so games cannot share a process.
# Each worker plays pending-game indices with index % K == shard and writes its
# own <store>.<shard> file; those are appended to the store only after every
# worker exits cleanly, then a single rate pass runs. Do not pass --out or
# --shard/--of together with -Workers (the driver owns them in that mode).

# PositionalBinding=$false so pass-through tokens like "--games" land in $Args
# instead of being bound to $Store positionally.
[CmdletBinding(PositionalBinding = $false)]
param(
    [switch]$Build,
    [int]$Workers = 1,
    [string]$Store = "ranking/matches.jsonl",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Args
)

$ErrorActionPreference = "Stop"
$Exe = ".\rank.exe"

if ($Build) {
    Write-Host "Building rank.exe..."
    if (Test-Path $Exe) { Remove-Item $Exe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_rank.bat"
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

if ($Workers -le 1) {
    if ($null -eq $Args -or $Args.Count -eq 0) { & $Exe; exit $LASTEXITCODE }
    & $Exe @Args
    exit $LASTEXITCODE
}

# Parallel mode: any leading "play" or "run" token is absorbed (the driver runs
# sharded play + one rate); the remaining --key value options go to both phases.
$extra = @()
if ($null -ne $Args -and $Args.Count -gt 0) {
    $extra = $Args
    if ($extra[0] -eq "play" -or $extra[0] -eq "run") {
        if ($extra.Count -gt 1) { $extra = $extra[1..($extra.Count - 1)] } else { $extra = @() }
    }
}

if (-not (Test-Path "ranking")) { New-Item -ItemType Directory "ranking" | Out-Null }

Write-Host "Launching $Workers rank shards (store=$Store)..."
$procs = @()
$shardFiles = @()
for ($s = 0; $s -lt $Workers; $s++) {
    $sf = "$Store.$s"
    if (Test-Path $sf) { Remove-Item $sf -Force }
    $shardFiles += $sf
    $playArgs = @("play", "--shard", $s, "--of", $Workers, "--in", $Store, "--out", $sf) + $extra
    $procs += Start-Process -FilePath $Exe -ArgumentList $playArgs -NoNewWindow -PassThru
}

Write-Host "Waiting for $($procs.Count) shards..."
$procs | Wait-Process

$bad = @($procs | Where-Object { $null -ne $_.ExitCode -and $_.ExitCode -ne 0 })
if ($bad.Count -gt 0) {
    Write-Error "A shard failed; shard files were NOT merged (inspect $Store.<shard>)."
    exit 1
}

# Append each shard's rows to the permanent store, then clean up.
foreach ($sf in $shardFiles) {
    if (Test-Path $sf) {
        Get-Content $sf | Add-Content -Path $Store -Encoding Ascii
        Remove-Item $sf -Force
    }
}

& $Exe rate --in $Store @extra
exit $LASTEXITCODE
