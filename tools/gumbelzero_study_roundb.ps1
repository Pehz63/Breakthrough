<#
.SYNOPSIS
  Gumbel-Zero Pass 2, Round B: seed replication + extended rung ladder for the
  top 8 draws from Round A.

.DESCRIPTION
  Round A (tools/gumbelzero_study.ps1) screened 21 draws at 1 seed and 3 rungs
  (100/400/1500). Most draws, including REF, were still rising monotonically
  at rung 1500 -- exactly the situation Docs/model-training-playbook.md warns
  about (theory 46: a ladder that stops before the peak understates
  everything on it). Reviewed with the developer; the chosen fix is BOTH
  seed-replicating the top 8 AND extending the ladder to a 4th rung (4000),
  applied to every seed of every promoted draw so the "consistent rungs"
  rule still holds within this round.

  The 8 draws (by their own rung-1500 Elo in the Round A pinned screening
  fit): REF, R17, R3, R8, R19, R18, R7, R14. Each keeps its Round-A seed
  (re-trained, deterministic, so its 100/400/1500 checkpoints reproduce
  byte-identically and it additionally gets a 4000 checkpoint) plus 2 new
  seeds (original+10000, original+20000).

  Slots: Round A owns 660..722; this claims 723..818 (8 draws x 3 seeds x 4
  rungs = 96 checkpoints).

.PARAMETER Phase
  all | train | roster | play | screen. Phases are resumable and idempotent.
#>
param(
    [int]$Workers = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [ValidateSet("all","train","roster","play","screen")]
    [string]$Phase = "all",
    [int]$GamesPerPair = 8,
    [string]$ScreenStore = "ranking/matches_screen_gz.jsonl",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Root      = Split-Path -Parent $PSScriptRoot
$TrainExe  = Join-Path $Root "train.exe"
$RankExe   = Join-Path $Root "rank.exe"
$SweepDir  = Join-Path $Root "models\sweep"
$Ledger    = Join-Path $SweepDir "gumbelzero_pass2_roundb_study.csv"
$RosterOut = Join-Path $Root "ranking\roster_gumbelzero_pass2_roundb.txt"
$CohortOut = Join-Path $Root "ranking\cohort_gumbelzero_pass2_roundb.txt"
$BaseRoster= Join-Path $Root "ranking\roster.txt"
$Standings = Join-Path $Root "ranking\standings.tsv"

$SharedRungs = 100,400,1500,4000

# Top 8 Round-A draws (block, original seed, sims, lr, l2, replaycap, replaywarm, batch, open)
# -- copied verbatim from models/sweep/gumbelzero_pass2_study.csv rows.
$TopDraws = @(
    [PSCustomObject]@{ Block="REF"; Seed=2101; Sims=50;  Lr=0.01;  L2=0.0;    RCap=2000; RWarm=32;  Batch=32; Open=4 }
    [PSCustomObject]@{ Block="R17"; Seed=2117; Sims=200; Lr=0.03;  L2=0.0;    RCap=8000; RWarm=128; Batch=8;  Open=8 }
    [PSCustomObject]@{ Block="R3";  Seed=2103; Sims=100; Lr=0.03;  L2=0.0;    RCap=2000; RWarm=32;  Batch=8;  Open=8 }
    [PSCustomObject]@{ Block="R8";  Seed=2108; Sims=200; Lr=0.01;  L2=0.0003; RCap=500;  RWarm=16;  Batch=32; Open=4 }
    [PSCustomObject]@{ Block="R19"; Seed=2119; Sims=100; Lr=0.01;  L2=0.0003; RCap=2000; RWarm=32;  Batch=8;  Open=0 }
    [PSCustomObject]@{ Block="R18"; Seed=2118; Sims=200; Lr=0.003; L2=0.001;  RCap=2000; RWarm=32;  Batch=8;  Open=4 }
    [PSCustomObject]@{ Block="R7";  Seed=2107; Sims=200; Lr=0.01;  L2=0.001;  RCap=8000; RWarm=128; Batch=32; Open=0 }
    [PSCustomObject]@{ Block="R14"; Seed=2114; Sims=100; Lr=0.01;  L2=0.0003; RCap=2000; RWarm=32;  Batch=64; Open=4 }
)

# ---- Expand draws x 3 seeds into training-run cells ----
$Cells = @()
$slot = 723
foreach ($row in $TopDraws) {
    # NOTE: computing the +10000/+20000 offsets inline inside the @(...) list
    # (`@($row.Seed, $row.Seed + 10000, $row.Seed + 20000)`) is a PowerShell
    # parsing trap -- it silently expands to 5 elements
    # ($row.Seed, $row.Seed, 10000, $row.Seed, 20000), not 3, discovered
    # 2026-08-17 when it trained the original seed 3x and shared the literal
    # seeds 10000/20000 across every draw instead of offsetting them
    # per-draw. Precomputing into locals first avoids it.
    $s1 = $row.Seed; $s2 = $row.Seed + 10000; $s3 = $row.Seed + 20000
    foreach ($seed in @($s1, $s2, $s3)) {
        $c = [ordered]@{
            Block = $row.Block; Seed = $seed; Rungs = $SharedRungs
            Sims = $row.Sims; Lr = $row.Lr; L2 = $row.L2
            ReplayCap = $row.RCap; ReplayWarm = $row.RWarm
            Batch = $row.Batch; Open = $row.Open
        }
        $c.Slots = @()
        foreach ($r in $SharedRungs) { $c.Slots += $script:slot; $script:slot++ }
        $Cells += [PSCustomObject]$c
    }
}

$totalAgents = ($Cells | ForEach-Object { $_.Slots.Count } | Measure-Object -Sum).Sum
Write-Host "Gumbel-Zero Pass 2 (Round B): $($TopDraws.Count) draws x 3 seeds -> $($Cells.Count) training runs -> $totalAgents cohort agents (slots 723..$($slot-1))"

if (-not (Test-Path $SweepDir)) { New-Item -ItemType Directory -Path $SweepDir | Out-Null }
if (-not (Test-Path $Ledger)) {
    "block,seed,sims,lr,l2,replaycap,replaywarm,batch,open,rung,slot,model" |
        Out-File -FilePath $Ledger -Encoding ascii
}
$done = @{}
Import-Csv $Ledger | ForEach-Object { $done["$($_.block)|$($_.seed)|$($_.rung)"] = $_ }

function RunTraining {
    $jobs = @()
    foreach ($c in $Cells) {
        $key = "$($c.Block)|$($c.Seed)|$($c.Rungs[-1])"
        if ($done.ContainsKey($key)) { Write-Host "  [skip] $($c.Block) seed $($c.Seed) already trained"; continue }
        $out = "models/sweep/gz2b_$($c.Block)_s$($c.Seed)"
        $a = @("gumbelzero", "--out", $out,
               "--ckpt-at", ($c.Rungs -join ","),
               "--games", $c.Rungs[-1],
               "--sims", $c.Sims, "--lr", $c.Lr, "--l2", $c.L2,
               "--replay-capacity", $c.ReplayCap, "--replay-warmup", $c.ReplayWarm,
               "--batch-size", $c.Batch, "--open-plies", $c.Open,
               "--seed", $c.Seed, "--report-every", 0)
        Write-Host "  train $($c.Block) seed $($c.Seed): sims=$($c.Sims) lr=$($c.Lr) l2=$($c.L2) replay=$($c.ReplayCap)/$($c.ReplayWarm) batch=$($c.Batch) open=$($c.Open)"
        if ($DryRun) { continue }
        while (@($jobs | Where-Object { -not $_.HasExited }).Count -ge $Workers) { Start-Sleep -Milliseconds 400 }
        $jobs += Start-Process -FilePath $TrainExe -ArgumentList $a -PassThru -NoNewWindow `
                     -RedirectStandardOutput "$SweepDir\gz2b_$($c.Block)_s$($c.Seed).log"
    }
    if (-not $DryRun) {
        Write-Host "  waiting for $(@($jobs | Where-Object { -not $_.HasExited }).Count) run(s)..."
        $jobs | ForEach-Object { $_.WaitForExit() }
    }
    foreach ($c in $Cells) {
        $out = "models/sweep/gz2b_$($c.Block)_s$($c.Seed)"
        for ($i = 0; $i -lt $c.Rungs.Count; $i++) {
            $rung = $c.Rungs[$i]; $sl = $c.Slots[$i]
            $src = Join-Path $Root "$($out)_g$rung.txt"
            $dst = Join-Path $SweepDir "slot$sl.txt"
            if (-not (Test-Path $src)) { Write-Warning "  missing rung $rung for $($c.Block) s$($c.Seed)"; continue }
            Copy-Item $src $dst -Force
            $key = "$($c.Block)|$($c.Seed)|$rung"
            if (-not $done.ContainsKey($key)) {
                "$($c.Block),$($c.Seed),$($c.Sims),$($c.Lr),$($c.L2),$($c.ReplayCap),$($c.ReplayWarm),$($c.Batch),$($c.Open),$rung,$sl,$dst" |
                    Add-Content -Path $Ledger -Encoding Ascii
            }
        }
    }
}

function BuildRoster {
    $hashes = @{}
    & $RankExe check 2>&1 | Select-String 'models/sweep/slot(\d+)\.txt = ([0-9a-f]{8})' | ForEach-Object {
        $hashes[[int]$_.Matches[0].Groups[1].Value] = $_.Matches[0].Groups[2].Value
    }
    $lines = @(); $ids = @()
    foreach ($c in $Cells) {
        for ($i = 0; $i -lt $c.Rungs.Count; $i++) {
            $sl = $c.Slots[$i]
            if (-not $hashes.ContainsKey($sl)) { Write-Warning "  no hash for slot $sl (not trained?)"; continue }
            $id = "gaz(sims=$($c.Sims))@1.learned(s$sl,$($hashes[$sl]))@1"
            $lines += "on $id"; $ids += $id
        }
    }
    Get-Content $BaseRoster | Out-File -FilePath $RosterOut -Encoding ascii
    "" | Add-Content -Path $RosterOut -Encoding ascii
    "# ---- Gumbel-Zero Pass 2 Round B cohort ($($ids.Count) agents), added $(Get-Date -Format yyyy-MM-dd) ----" |
        Add-Content -Path $RosterOut -Encoding ascii
    $lines | Add-Content -Path $RosterOut -Encoding ascii
    $ids   | Out-File -FilePath $CohortOut -Encoding ascii
    Write-Host "  roster -> $RosterOut ($($ids.Count) cohort agents appended)"
    Write-Host "  cohort -> $CohortOut"
    & $RankExe check --roster $RosterOut 2>&1 | Select-String -NotMatch '^model hash' | Select-Object -Last 4
}

function PlayCohort {
    Write-Host "  playing cohort at $GamesPerPair games/pair, $Workers shards (cohort-filtered)"
    & (Join-Path $PSScriptRoot "run_rank.ps1") -Workers $Workers -Store $ScreenStore -NoRate play `
        --roster $RosterOut --cohort $CohortOut --games $GamesPerPair
}

function ScreenCohort {
    Write-Host "  pinned fit over $ScreenStore (roster frozen at ranking/standings.tsv)"
    & $RankExe rate --roster $RosterOut --in $ScreenStore --pin $Standings
}

switch ($Phase) {
    "train"  { RunTraining }
    "roster" { BuildRoster }
    "play"   { PlayCohort }
    "screen" { ScreenCohort }
    "all"    { RunTraining; BuildRoster; PlayCohort; ScreenCohort }
}
