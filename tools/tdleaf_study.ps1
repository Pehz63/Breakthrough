<#
.SYNOPSIS
  TD-Leaf(lambda) self-play study: train a diverse cohort, play it into the
  existing roster, and rate it on a FROZEN roster scale.

.DESCRIPTION
  The game count is NOT an input to this study. Each training run writes
  checkpoints on a game-count ladder (--ckpt-at) and every rung is rated as its
  own agent, so the learning curve is an output. Nothing in this repo or in the
  literature fixes the count for online self-play learning, which has never been
  run here (see plans/tdleaf-plan-1-amber-pangolin.md for the two withdrawn
  claims that previously stood in for it).

  Rating is deliberately two-stage:
    SCREEN  rank.exe rate --pin ranking/standings.tsv
            The 158 existing agents are held at their current Elo and only the
            cohort is solved. The reference scale cannot drift mid-study, and
            the fit uses cohort-vs-cohort games too, so the cohort's internal
            order is resolved -- which per-candidate gauntlets cannot do.
            A pinned fit can NEVER dethrone a champion: their ratings are
            inputs to it.
    CERTIFY A plain unpinned refit, run deliberately as the LAST step once the
            cohort to keep has been chosen (ranking/CHAMPION.md rule 1). Only
            this can reorder the table or dethrone anything. Not run by this
            script -- see -Phase certify-note.

  Cohort design is one-axis-at-a-time around a base config, so every arm is
  comparable to the base and to the seed band measured at the base.

.PARAMETER Workers
  Parallel training processes, and shards for the play phase.

.PARAMETER Phase
  all | train | roster | play | screen. Phases are resumable and idempotent.

.PARAMETER GamesPerPair
  Games per pair for the play phase. 8 is screening level; 32 is the project's
  certification standard (ranking/CHAMPION.md rule 2).
#>
param(
    [int]$Workers = 12,
    [ValidateSet("all","train","roster","play","screen")]
    [string]$Phase = "all",
    [int]$GamesPerPair = 8,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Root      = Split-Path -Parent $PSScriptRoot
$TrainExe  = Join-Path $Root "train.exe"
$RankExe   = Join-Path $Root "rank.exe"
$SweepDir  = Join-Path $Root "models\sweep"
$Ledger    = Join-Path $SweepDir "tdleaf_study.csv"
$RosterOut = Join-Path $Root "ranking\roster_tdleaf.txt"
$CohortOut = Join-Path $Root "ranking\cohort_tdleaf.txt"
$BaseRoster= Join-Path $Root "ranking\roster.txt"
$Standings = Join-Path $Root "ranking\standings.tsv"

# The evaluator wrapper every cohort agent is rated at. ONE head for the whole
# cohort (ranking/CHAMPION.md rule 6): mixing heads is not an evaluator comparison.
$Wrapper = "ab(d6,tt,ord,nb200k)@1"
# All cells train a linear v2 piece-square value model, so they share one arch
# descriptor. Matches the existing roster's form for identical models, e.g.
# learned(s6,eac8ab99,value,lin,129-1,con100)@1.
$Arch = "value,lin,129-1,con100"

# ---- Base configuration ----
# init = the promoted replay-trained linear model (models/pst_value.txt). Chosen
# as the champion-class starting point per the developer's "both arms" decision;
# block C runs the from-scratch arm against it.
$Base = @{ Init = "models/pst_value.txt"; Lambda = 0.7; Lr = 0.01; Depth = 6; NodeBudget = 200000; OpenPlies = 4 }

# ---- Cohort: one axis at a time around the base ----
# Slots 128..165 (the ML_SLOTS raise to 256 on 2026-07-29 opened 128+).
$Cells = @()
$slot = 128
function AddCell([string]$block, [hashtable]$over, [int]$seed, [int[]]$rungs) {
    $c = $Base.Clone()
    foreach ($k in $over.Keys) { $c[$k] = $over[$k] }
    $c.Block = $block; $c.Seed = $seed; $c.Rungs = $rungs
    $c.Slots = @()
    foreach ($r in $rungs) { $c.Slots += $script:slot; $script:slot++ }
    $script:Cells += $c
}

# A: learning curve AND the training-seed band, measured at every rung.
#    4 seeds x 5 rungs = 20 agents. The block that answers "how many games".
foreach ($s in 1001, 2002, 3003, 4004) { AddCell "A-base" @{} $s @(100, 250, 500, 1000, 2000) }
# B: lambda. 0 = pure one-step TD, 1 = outcome-supervised on PV leaves (the
#    closed form proved in tests), vs the base's 0.7.
foreach ($s in 1001, 2002) { AddCell "B-lam0"  @{ Lambda = 0.0 } $s @(500, 2000) }
foreach ($s in 1001, 2002) { AddCell "B-lam1"  @{ Lambda = 1.0 } $s @(500, 2000) }
# C: from-scratch init (the second arm of the developer's "both" decision).
foreach ($s in 1001, 2002) { AddCell "C-scratch" @{ Init = "" } $s @(500, 2000) }
# D: learning rate, an axis previously left as an unjustified constant.
AddCell "D-lr003" @{ Lr = 0.003 } 1001 @(500, 2000)
AddCell "D-lr03"  @{ Lr = 0.03  } 1001 @(500, 2000)
# E: generator depth control. Tests whether the repo's supervised-era "teacher
#    depth is irrelevant" finding carries over to a bootstrapped target.
AddCell "E-d4" @{ Depth = 4; NodeBudget = 0 } 1001 @(500, 2000)

$totalAgents = ($Cells | ForEach-Object { $_.Slots.Count } | Measure-Object -Sum).Sum
Write-Host "TD-Leaf study: $($Cells.Count) training runs -> $totalAgents cohort agents (slots 128..$($slot-1))"

if (-not (Test-Path $SweepDir)) { New-Item -ItemType Directory -Path $SweepDir | Out-Null }
if (-not (Test-Path $Ledger)) {
    "block,seed,lambda,lr,depth,nodebudget,openplies,init,rung,slot,model" | Out-File -FilePath $Ledger -Encoding ascii
}
$done = @{}
Import-Csv $Ledger | ForEach-Object { $done["$($_.block)|$($_.seed)|$($_.rung)"] = $_ }

# ---- Phase: train ----
function RunTraining {
    $jobs = @()
    foreach ($c in $Cells) {
        $key = "$($c.Block)|$($c.Seed)|$($c.Rungs[-1])"
        if ($done.ContainsKey($key)) { Write-Host "  [skip] $($c.Block) seed $($c.Seed) already trained"; continue }
        $out = "models/sweep/tdl_$($c.Block)_s$($c.Seed)"
        $a = @("tdleaf", "--out", $out,
               "--ckpt-at", ($c.Rungs -join ","),
               "--games", $c.Rungs[-1],
               "--depth", $c.Depth, "--node-budget", $c.NodeBudget,
               "--lambda", $c.Lambda, "--lr", $c.Lr,
               "--open-plies", $c.OpenPlies, "--seed", $c.Seed,
               "--report-every", 0)
        if ($c.Init -ne "") { $a += @("--init", $c.Init) }
        Write-Host "  train $($c.Block) seed $($c.Seed): lambda=$($c.Lambda) lr=$($c.Lr) d$($c.Depth) init=$(if($c.Init){'champ'}else{'scratch'})"
        if ($DryRun) { continue }
        while (@($jobs | Where-Object { -not $_.HasExited }).Count -ge $Workers) { Start-Sleep -Milliseconds 400 }
        $jobs += Start-Process -FilePath $TrainExe -ArgumentList $a -PassThru -NoNewWindow `
                     -RedirectStandardOutput "$SweepDir\tdl_$($c.Block)_s$($c.Seed).log"
    }
    if (-not $DryRun) {
        Write-Host "  waiting for $(@($jobs | Where-Object { -not $_.HasExited }).Count) run(s)..."
        $jobs | ForEach-Object { $_.WaitForExit() }
    }

    # Publish each rung into its own slot file and record it.
    foreach ($c in $Cells) {
        $out = "models/sweep/tdl_$($c.Block)_s$($c.Seed)"
        for ($i = 0; $i -lt $c.Rungs.Count; $i++) {
            $rung = $c.Rungs[$i]; $sl = $c.Slots[$i]
            $src = Join-Path $Root "$($out)_g$rung.txt"
            $dst = Join-Path $SweepDir "slot$sl.txt"
            if (-not (Test-Path $src)) { Write-Warning "  missing rung $rung for $($c.Block) s$($c.Seed)"; continue }
            Copy-Item $src $dst -Force
            $key = "$($c.Block)|$($c.Seed)|$rung"
            if (-not $done.ContainsKey($key)) {
                "$($c.Block),$($c.Seed),$($c.Lambda),$($c.Lr),$($c.Depth),$($c.NodeBudget),$($c.OpenPlies),$($c.Init),$rung,$sl,$dst" |
                    Add-Content -Path $Ledger -Encoding Ascii
            }
        }
    }
}

# ---- Phase: roster ----
# Writes roster_tdleaf.txt (the base roster plus the cohort) and cohort_tdleaf.txt
# (the id list --cohort reads, so only pairs touching a cohort agent are played).
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
            $id = "$Wrapper.learned(s$sl,$($hashes[$sl]),$Arch)@1"
            $lines += "on $id"; $ids += $id
        }
    }
    Get-Content $BaseRoster | Out-File -FilePath $RosterOut -Encoding ascii
    "" | Add-Content -Path $RosterOut -Encoding ascii
    "# ---- TD-Leaf study cohort ($($ids.Count) agents), added $(Get-Date -Format yyyy-MM-dd) ----" |
        Add-Content -Path $RosterOut -Encoding ascii
    $lines | Add-Content -Path $RosterOut -Encoding ascii
    $ids   | Out-File -FilePath $CohortOut -Encoding ascii
    Write-Host "  roster -> $RosterOut ($($ids.Count) cohort agents appended)"
    Write-Host "  cohort -> $CohortOut"
    & $RankExe check --roster $RosterOut 2>&1 | Select-String -NotMatch '^model hash' | Select-Object -Last 4
}

# ---- Phase: play ----
function PlayCohort {
    Write-Host "  playing cohort at $GamesPerPair games/pair, $Workers shards (cohort-filtered)"
    & (Join-Path $PSScriptRoot "run_rank.ps1") -Workers $Workers play `
        --roster $RosterOut --cohort $CohortOut --games $GamesPerPair
}

# ---- Phase: screen ----
function ScreenCohort {
    Write-Host "  pinned fit (roster frozen at ranking/standings.tsv)"
    & $RankExe rate --roster $RosterOut --pin $Standings
    Write-Host ""
    Write-Host "Cohort results are in ranking/standings_pinned.tsv (canonical files untouched)."
    Write-Host "CERTIFICATION is a separate, deliberate step: choose which cohort agents to keep,"
    Write-Host "append them to ranking/roster.txt, then run a plain unpinned 'rank.exe run'."
}

switch ($Phase) {
    "train"  { RunTraining }
    "roster" { BuildRoster }
    "play"   { PlayCohort }
    "screen" { ScreenCohort }
    "all"    { RunTraining; BuildRoster; PlayCohort; ScreenCohort }
}
