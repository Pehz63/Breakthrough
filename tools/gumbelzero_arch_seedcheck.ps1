<#
.SYNOPSIS
  Seed-replication follow-up to the mlp/conv architecture sweep (round 6,
  plans/gumbel-mcts-arch-results-6-silver-thistle.md): re-trains the top 5
  mlp blocks' exact recipes at 2 additional seeds each, to check whether
  their single-seed Elo (led by M34's 1023 +/- 20, rank #2 of the pool)
  survives training-seed noise before it's treated as a stable number.

.DESCRIPTION
  Fixed cell list, not a random draw -- every field below was read directly
  from plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.tsv (never
  hand-transcribed, after a hand-transcription error in that results doc's
  prose put M34's replay-capacity/warmup at (2000,32) instead of its actual
  (8000,128), caught while preparing this script). Seed offsets (+10000,
  +20000 from each block's original seed) match Round B's convention
  exactly (gumbelzero_study_roundb.ps1). Search-shape (cvisit/cscale/m)
  stays fixed per block at its original certification values, so this
  isolates TRAINING-seed variance only, not search-shape variance.

.PARAMETER Workers
.PARAMETER Phase
  all | train | roster | play | screen | export.
#>
param(
    [int]$Workers = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [ValidateSet("all","train","roster","play","screen","export")]
    [string]$Phase = "all",
    [int]$GamesPerPair = 16
)

$ErrorActionPreference = "Stop"
$Root       = Split-Path -Parent $PSScriptRoot
$TrainExe   = Join-Path $Root "train.exe"
$RankExe    = Join-Path $Root "rank.exe"
$SweepDir   = Join-Path $Root "models\sweep"
$Ledger     = Join-Path $SweepDir "gumbelzero_arch_seedcheck.csv"
$RosterOut  = Join-Path $Root "ranking\roster_gumbelzero_arch_seedcheck.txt"
$CohortOut  = Join-Path $Root "ranking\cohort_gumbelzero_arch_seedcheck.txt"
$BaseRoster = Join-Path $Root "ranking\roster_screening_pool.txt"
$Standings  = Join-Path $Root "ranking\standings.tsv"
$PinnedStandings = Join-Path $Root "ranking\standings_screen_gzarchseed_pinned.tsv"
$ScreenStore = Join-Path $Root "ranking\matches_screen_gzarchseed.jsonl"
$ResultsOut = Join-Path $Root "plans\gumbel-mcts-arch-seedcheck-agents-6-silver-thistle.tsv"
$ResultsWideOut = Join-Path $Root "plans\gumbel-mcts-arch-seedcheck-agents-6-silver-thistle.wide.tsv"

$SharedRungs = 100,400,1500,4000

# Top 5 mlp blocks by rung=4000 Elo, exact recipe read from the round-6 TSV.
$Blocks = @(
    [ordered]@{ Block="M34"; Seed=8944; Arch="32";    Sims=500; Lr=0.01;  L2=0; ReplayCap=8000; ReplayWarm=128; Batch=128; Open=8; Cvisit=800;  Cscale=70;  M=20 }
    [ordered]@{ Block="M14"; Seed=8924; Arch="64";    Sims=300; Lr=0.01;  L2=0; ReplayCap=8000; ReplayWarm=128; Batch=128; Open=8; Cvisit=600;  Cscale=100; M=12 }
    [ordered]@{ Block="M39"; Seed=8949; Arch="32";    Sims=500; Lr=0.003; L2=0; ReplayCap=2000; ReplayWarm=32;  Batch=8;   Open=0; Cvisit=400;  Cscale=70;  M=12 }
    [ordered]@{ Block="M10"; Seed=8920; Arch="64,32"; Sims=500; Lr=0.003; L2=0; ReplayCap=8000; ReplayWarm=128; Batch=32;  Open=4; Cvisit=800;  Cscale=70;  M=8  }
    [ordered]@{ Block="M31"; Seed=8941; Arch="32";    Sims=300; Lr=0.03;  L2=0; ReplayCap=8000; ReplayWarm=128; Batch=8;   Open=8; Cvisit=1000; Cscale=40;  M=8  }
)
$SeedOffsets = 10000, 20000

# Slots 1770.. -- round 6's arch sweep owns 1402..1769 (src/CLAUDE.md's slot
# ledger); highest locally-trained slot file is 1769, no .gitignore exception
# above 707, so 1770 is free.
$Cells = @()
$slot = 1770
foreach ($b in $Blocks) {
    foreach ($off in $SeedOffsets) {
        $c = [ordered]@{
            Block = $b.Block; Seed = $b.Seed + $off; Rungs = $SharedRungs
            ModelType = "mlp"; Arch = $b.Arch
            Sims = $b.Sims; Lr = $b.Lr; L2 = $b.L2
            ReplayCap = $b.ReplayCap; ReplayWarm = $b.ReplayWarm
            Batch = $b.Batch; Open = $b.Open
            Cvisit = $b.Cvisit; Cscale = $b.Cscale; M = $b.M
        }
        $c.Slots = @()
        foreach ($r in $SharedRungs) { $c.Slots += $script:slot; $script:slot++ }
        $Cells += [PSCustomObject]$c
    }
}

$totalAgents = ($Cells | ForEach-Object { $_.Slots.Count } | Measure-Object -Sum).Sum
Write-Host "Seed-replication follow-up: $($Cells.Count) new training runs (5 blocks x 2 seed offsets) -> $totalAgents cohort agents (slots 1770..$($slot-1))"

if (-not (Test-Path $SweepDir)) { New-Item -ItemType Directory -Path $SweepDir | Out-Null }
if (-not (Test-Path $Ledger)) {
    "block,seed,modeltype,arch,sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m,rung,slot,model" |
        Out-File -FilePath $Ledger -Encoding ascii
}
$done = @{}
Import-Csv $Ledger | ForEach-Object { $done["$($_.block)|$($_.seed)|$($_.rung)"] = $_ }

function RunTraining {
    $jobs = @()
    foreach ($c in $Cells) {
        $key = "$($c.Block)|$($c.Seed)|$($c.Rungs[-1])"
        if ($done.ContainsKey($key)) { Write-Host "  [skip] $($c.Block) seed $($c.Seed) already trained"; continue }
        $out = "models/sweep/gza_sc_$($c.Block)_s$($c.Seed)"
        $a = @("gumbelzero", "--out", $out,
               "--ckpt-at", ($c.Rungs -join ","),
               "--games", $c.Rungs[-1],
               "--sims", $c.Sims, "--lr", $c.Lr, "--l2", $c.L2,
               "--replay-capacity", $c.ReplayCap, "--replay-warmup", $c.ReplayWarm,
               "--batch-size", $c.Batch, "--open-plies", $c.Open,
               "--model-type", $c.ModelType, "--mlp-hidden", $c.Arch,
               "--seed", $c.Seed, "--report-every", 0)
        Write-Host "  train $($c.Block) seed $($c.Seed): mlp($($c.Arch)) sims=$($c.Sims) lr=$($c.Lr) l2=$($c.L2) replay=$($c.ReplayCap)/$($c.ReplayWarm) batch=$($c.Batch) open=$($c.Open) | cert: cvisit=$($c.Cvisit) cscale=$($c.Cscale) m=$($c.M)"
        while (@($jobs | Where-Object { -not $_.HasExited }).Count -ge $Workers) { Start-Sleep -Milliseconds 400 }
        $jobs += Start-Process -FilePath $TrainExe -ArgumentList $a -PassThru -NoNewWindow `
                     -RedirectStandardOutput "$SweepDir\gza_sc_$($c.Block)_s$($c.Seed).log"
    }
    Write-Host "  waiting for $(@($jobs | Where-Object { -not $_.HasExited }).Count) run(s)..."
    $jobs | ForEach-Object { $_.WaitForExit() }

    foreach ($c in $Cells) {
        $out = "models/sweep/gza_sc_$($c.Block)_s$($c.Seed)"
        for ($i = 0; $i -lt $c.Rungs.Count; $i++) {
            $rung = $c.Rungs[$i]; $sl = $c.Slots[$i]
            $src = Join-Path $Root "$($out)_g$rung.txt"
            $dst = Join-Path $SweepDir "slot$sl.txt"
            if (-not (Test-Path $src)) { Write-Warning "  missing rung $rung for $($c.Block) s$($c.Seed)"; continue }
            Copy-Item $src $dst -Force
            $key = "$($c.Block)|$($c.Seed)|$rung"
            if (-not $done.ContainsKey($key)) {
                "$($c.Block),$($c.Seed),$($c.ModelType),`"$($c.Arch)`",$($c.Sims),$($c.Lr),$($c.L2),$($c.ReplayCap),$($c.ReplayWarm),$($c.Batch),$($c.Open),$($c.Cvisit),$($c.Cscale),$($c.M),$rung,$sl,$dst" |
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
            $id = "gaz(sims=$($c.Sims),cvisit=$($c.Cvisit),cscale=$($c.Cscale),m=$($c.M))@1.learned(s$sl,$($hashes[$sl]))@1"
            $lines += "on $id"; $ids += $id
        }
    }
    $marker = "# ---- Gumbel-Zero arch-sweep seed-replication cohort ($($ids.Count) agents), added $(Get-Date -Format yyyy-MM-dd) ----"
    Get-Content $BaseRoster | Out-File -FilePath $RosterOut -Encoding ascii
    "" | Add-Content -Path $RosterOut -Encoding ascii
    $marker | Add-Content -Path $RosterOut -Encoding ascii
    $lines | Add-Content -Path $RosterOut -Encoding ascii

    $canonLines = & $RankExe canon --roster $RosterOut
    $canonLines | Out-File -FilePath $RosterOut -Encoding ascii

    $afterMarker = $false
    $canonLines | ForEach-Object {
        if ($_ -eq $marker) { $script:afterMarker = $true; return }
        if ($script:afterMarker -and $_ -match '^on\s+(\S.*)$') { $Matches[1] }
    } | Out-File -FilePath $CohortOut -Encoding ascii

    Write-Host "  roster -> $RosterOut ($($ids.Count) cohort agents appended, base = $BaseRoster)"
    Write-Host "  cohort -> $CohortOut"
    & $RankExe check --roster $RosterOut 2>&1 | Select-String -NotMatch '^model hash' | Select-Object -Last 4
}

function PlayCohort {
    $ids = Get-Content $CohortOut | Where-Object { $_.Trim() -ne "" }
    Write-Host "  gauntlet-playing $($ids.Count) checkpoints at $GamesPerPair games/opponent vs roster_screening_pool.txt, $Workers shards"

    $shardFiles = @()
    $buckets = New-Object 'System.Collections.Generic.List[System.Collections.Generic.List[string]]'
    for ($w = 0; $w -lt $Workers; $w++) { $buckets.Add((New-Object 'System.Collections.Generic.List[string]')) }
    for ($i = 0; $i -lt $ids.Count; $i++) { $buckets[$i % $Workers].Add($ids[$i]) }

    $jobs = @()
    for ($w = 0; $w -lt $Workers; $w++) {
        if ($buckets[$w].Count -eq 0) { continue }
        $shardStore = "$ScreenStore.$w"
        $shardFiles += $shardStore
        $bat = Join-Path $SweepDir "gza_sc_play_worker$w.bat"
        $lines = @("@echo off")
        foreach ($id in $buckets[$w]) {
            $lines += "`"$RankExe`" gauntlet --roster `"$BaseRoster`" --in `"$shardStore`" --id `"$id`" --games $GamesPerPair --keep"
            $lines += "if errorlevel 1 exit /b 1"
        }
        $lines += "exit /b 0"
        $lines | Out-File -FilePath $bat -Encoding ascii
        $p = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "`"$bat`"" -PassThru -NoNewWindow `
                 -RedirectStandardOutput "$SweepDir\gza_sc_play_worker$w.log"
        [void]$p.Handle
        $jobs += $p
    }
    Write-Host "  waiting for $($jobs.Count) shard(s)..."
    $jobs | ForEach-Object { $_.WaitForExit() }
    $bad = $jobs | Where-Object { $_.ExitCode -ne 0 }
    if ($bad) { throw "  $($bad.Count) gauntlet shard(s) exited non-zero; store NOT merged, inspect gza_sc_play_worker*.log" }
    foreach ($sf in $shardFiles) {
        if (Test-Path $sf) {
            Get-Content $sf | Add-Content -Path $ScreenStore -Encoding Ascii
            Remove-Item $sf -Force
        }
    }
    Write-Host "  merged $($shardFiles.Count) shard(s) -> $ScreenStore"
}

function ScreenCohort {
    Write-Host "  pinned fit over $ScreenStore (screening pool frozen at ranking/standings.tsv)"
    & $RankExe rate --roster $RosterOut --in $ScreenStore --pin $Standings
}

function ExportResults {
    $notes = @(
        "Gumbel-Zero mlp/conv architecture sweep (round 6) seed-replication follow-up: top 5 mlp blocks re-trained at 2 additional seeds each, same fixed cvisit/cscale/m per block.",
        "elo/games/cpu_ms_move/playstyle columns are from a PINNED screening fit against ranking/roster_screening_pool.txt: screening only, not certification."
    )
    & (Join-Path $PSScriptRoot "export_cohort_results.ps1") `
        -Ledger $Ledger `
        -PinnedStandings $PinnedStandings `
        -Out $ResultsOut `
        -HeaderComment $notes `
        -GroupBy block,seed -WideBy rung -WideOut $ResultsWideOut
}

switch ($Phase) {
    "train"  { RunTraining }
    "roster" { BuildRoster }
    "play"   { PlayCohort }
    "screen" { ScreenCohort }
    "export" { ExportResults }
    "all"    { RunTraining; BuildRoster; PlayCohort; ScreenCohort; ExportResults }
}
