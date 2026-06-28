# run_tournament.ps1 - Parallel depth-laddered round-robin via process sharding.
#
# The engine keeps board/eval state in globals, so games can't share a process safely.
# Instead we launch K independent train.exe workers, each playing its shard of the full
# round-robin (game index %% K == shard) and appending results to a shared JSONL file,
# then rate once all workers finish.
#
# Usage (from project root; build train.exe first with .\tools\run_train.ps1 -Build ...):
#   .\tools\run_tournament.ps1                       # K = CPU count, default depths/games
#   .\tools\run_tournament.ps1 -Workers 8 -Depths "2,4,6,8,10" -Games 10 -NodeBudget 300000
#   .\tools\run_tournament.ps1 -Depths "4,6,8,10" -NodeBudget 600000 `
#       -Only "AB6-Classic-chip,LearnedPolicy" -Note "first restricted run"
#
# Every run is archived under runs/<RunId>/ (config.json, results.jsonl, elo.tsv,
# notes.md) and recorded in the agent registry (agents/registry.{jsonl,md}). A run with
# -Only restricts the roster to those agent names and LEAVES agents/library.txt +
# champion*.txt untouched (the full-roster snapshot is preserved). Pass -RunId to attach
# to / reuse a specific id; otherwise a UTC timestamp id is minted.

param(
    [int]$Workers = 0,
    [string]$Depths = "2,4,6,8,10",
    [int]$Games = 10,
    [long]$NodeBudget = 300000,
    [int]$Seed = 1,
    [string]$Out = "data/tourney.jsonl",
    [string]$Board = "boards/board1.txt",
    [string]$Only = "",
    [string]$Note = "",
    [string]$RunId = ""
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path ".\train.exe")) { Write-Error "train.exe not found. Build it: .\tools\run_train.ps1 -Build"; exit 1 }
if ($Workers -le 0) { $Workers = [int]$env:NUMBER_OF_PROCESSORS; if ($Workers -le 0) { $Workers = 4 } }
if ($RunId -eq "") { $RunId = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ") }

# Write the run config + notes header once, up front (one source of truth, in C++).
$cfgArgs = @("run-config", "--run", $RunId, "--depths", $Depths, "--node-budget", $NodeBudget,
             "--games", $Games, "--seed", $Seed, "--workers", $Workers, "--board", $Board)
if ($Only -ne "") { $cfgArgs += @("--only", $Only) }
if ($Note -ne "") { $cfgArgs += @("--note", $Note) }
& .\train.exe @cfgArgs

# Each shard writes its OWN file (concurrent appends to one file would interleave and
# corrupt lines); we concatenate them after all workers finish, then rate.
if (Test-Path $Out) { Remove-Item $Out -Force }

Write-Host "Launching $Workers shards (run=$RunId, depths=$Depths, games/pair=$Games, node-budget=$NodeBudget, only=$Only)..."
$procs = @()
$shardFiles = @()
for ($s = 0; $s -lt $Workers; $s++) {
    $sf = "$Out.$s"
    if (Test-Path $sf) { Remove-Item $sf -Force }
    $shardFiles += $sf
    $playArgs = @("tournament-play", "--shard", $s, "--of", $Workers, "--depths", $Depths,
              "--games", $Games, "--node-budget", $NodeBudget, "--seed", $Seed,
              "--out", $sf, "--board", $Board)
    if ($Only -ne "") { $playArgs += @("--only", $Only) }
    $procs += Start-Process -FilePath ".\train.exe" -ArgumentList $playArgs -NoNewWindow -PassThru
}

Write-Host "Waiting for $($procs.Count) shards..."
$procs | Wait-Process
Write-Host "All shards done. Merging + rating..."

# Concatenate shard files into the combined results file.
Get-Content ($shardFiles | Where-Object { Test-Path $_ }) | Set-Content $Out
Remove-Item $shardFiles -Force -ErrorAction SilentlyContinue

$rateArgs = @("tournament-rate", "--depths", $Depths, "--in", $Out, "--run", $RunId)
if ($Only -ne "") { $rateArgs += @("--only", $Only) }
& .\train.exe @rateArgs
exit $LASTEXITCODE
