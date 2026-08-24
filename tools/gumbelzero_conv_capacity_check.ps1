<#
.SYNOPSIS
  Conv-capacity follow-up to the mlp/conv architecture sweep (round 6,
  plans/gumbel-mcts-arch-results-6-silver-thistle.md): tests whether conv's
  underperformance vs mlp (median 685 vs 713, max 806 vs 1023 at rung=4000)
  is explained by two capacity asymmetries the round-6 sweep never varied,
  rather than by the conv architecture itself.

.DESCRIPTION
  Round 6 gave every mlp draw two nonlinear hidden layers on BOTH heads
  (value AND policy), while every conv draw got a value head with NO hidden
  FC layer after the conv stack (--mlp-hidden was never passed for conv) and
  a policy head that was unconditionally linear (src/ml_gumbelzero.cpp's
  policyHead construction was gated on modelType=="mlp" alone, so a conv
  value head could never pair with an MLP policy head through any existing
  flag). Both are now independently fixable: --mlp-hidden already applied to
  conv's FC head (just never exercised for it in round 6), and this study
  adds a new --policy-mlp flag (decoupling policy-head architecture from
  value-head architecture, src/ml_gumbelzero.cpp/.h) that gives a conv run's
  policy head an MLPModel too. Full rationale + code citations: chat
  transcript same day. The flag change itself is verified by
  tools/run_tests.ps1 -Build (3703 assertions, incl. tests/test_gumbelzero.cpp)
  and a Pass-1 sanity gauntlet confirming the new combination is loadable and
  playable through the real gaz(...) search path.

  2x2 factorial (FC head: none/32 x policy head: linear/mlp) x 3 seeds, all
  built on ONE base recipe: block C15, round 6's single best conv checkpoint
  (806 Elo at rung=4000, conv channels 16,16,16, sims=300 lr=0.003 l2=0
  replay=500/16 batch=8 open=0, cert cvisit=400 cscale=70 m=16 -- read
  directly from plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.wide.tsv,
  never hand-transcribed). The (FC=none, policy=linear) cell is an exact
  replication of C15's own recipe, so it also serves as this study's
  properly-seed-replicated baseline -- round 6 was 1 seed per draw for the
  whole population, this study is the first 3-seed check specifically on
  C15's cell. Seed offsets (+0/+10000/+20000 from C15's own seed 8975) match
  the seed-replication follow-up's convention exactly
  (gumbelzero_arch_seedcheck.ps1).

  Same rung ladder (100/400/1500/4000) as round 6, same pool-only-gauntlet
  screening mechanism, same slot-hash roster-ID construction.

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
$Ledger     = Join-Path $SweepDir "gumbelzero_conv_capacity.csv"
$RosterOut  = Join-Path $Root "ranking\roster_gumbelzero_conv_capacity.txt"
$CohortOut  = Join-Path $Root "ranking\cohort_gumbelzero_conv_capacity.txt"
$BaseRoster = Join-Path $Root "ranking\roster_screening_pool.txt"
$Standings  = Join-Path $Root "ranking\standings.tsv"
$PinnedStandings = Join-Path $Root "ranking\standings_screen_gzconvcap_pinned.tsv"
$ScreenStore = Join-Path $Root "ranking\matches_screen_gzconvcap.jsonl"
$ResultsOut  = Join-Path $Root "plans\gumbel-mcts-conv-capacity-agents-6-silver-thistle.tsv"
$ResultsWideOut = Join-Path $Root "plans\gumbel-mcts-conv-capacity-agents-6-silver-thistle.wide.tsv"

$SharedRungs = 100,400,1500,4000

# C15's exact recipe (round 6, 806 Elo at rung=4000), the base every cell here varies on.
$Base = [ordered]@{
    Seed=8975; ConvChannels="16,16,16"
    Sims=300; Lr=0.003; L2=0.0; ReplayCap=500; ReplayWarm=16; Batch=8; Open=0
    Cvisit=400; Cscale=70; M=16
}
$FcOptions = @(
    [ordered]@{ Tag="noFC"; MlpHidden="" }
    [ordered]@{ Tag="fc32"; MlpHidden="32" }
)
$PolicyOptions = @(
    [ordered]@{ Tag="linPolicy"; PolicyMlp=$false }
    [ordered]@{ Tag="mlpPolicy"; PolicyMlp=$true }
)
$SeedOffsets = 0, 10000, 20000

# Slots 1810.. -- highest locally-trained slot file is 1809 (the seed-
# replication follow-up's range), so 1810 is free.
$Cells = @()
$slot = 1810
foreach ($fc in $FcOptions) {
    foreach ($pol in $PolicyOptions) {
        foreach ($off in $SeedOffsets) {
            $c = [ordered]@{
                Block = "conv_$($fc.Tag)_$($pol.Tag)"; Seed = $Base.Seed + $off; Rungs = $SharedRungs
                ModelType = "conv"; ConvChannels = $Base.ConvChannels
                MlpHidden = $fc.MlpHidden; PolicyMlp = $pol.PolicyMlp
                Sims = $Base.Sims; Lr = $Base.Lr; L2 = $Base.L2
                ReplayCap = $Base.ReplayCap; ReplayWarm = $Base.ReplayWarm
                Batch = $Base.Batch; Open = $Base.Open
                Cvisit = $Base.Cvisit; Cscale = $Base.Cscale; M = $Base.M
            }
            $c.Slots = @()
            foreach ($r in $SharedRungs) { $c.Slots += $script:slot; $script:slot++ }
            $Cells += [PSCustomObject]$c
        }
    }
}

$totalAgents = ($Cells | ForEach-Object { $_.Slots.Count } | Measure-Object -Sum).Sum
Write-Host "Conv-capacity follow-up: $($Cells.Count) new training runs (2 FC x 2 policy x 3 seeds) -> $totalAgents cohort agents (slots 1810..$($slot-1))"

if (-not (Test-Path $SweepDir)) { New-Item -ItemType Directory -Path $SweepDir | Out-Null }
if (-not (Test-Path $Ledger)) {
    "block,seed,modeltype,convchannels,mlphidden,policymlp,sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m,rung,slot,model" |
        Out-File -FilePath $Ledger -Encoding ascii
}
$done = @{}
Import-Csv $Ledger | ForEach-Object { $done["$($_.block)|$($_.seed)|$($_.rung)"] = $_ }

function RunTraining {
    $jobs = @()
    foreach ($c in $Cells) {
        $key = "$($c.Block)|$($c.Seed)|$($c.Rungs[-1])"
        if ($done.ContainsKey($key)) { Write-Host "  [skip] $($c.Block) seed $($c.Seed) already trained"; continue }
        $out = "models/sweep/gza_cc_$($c.Block)_s$($c.Seed)"
        $a = @("gumbelzero", "--out", $out,
               "--ckpt-at", ($c.Rungs -join ","),
               "--games", $c.Rungs[-1],
               "--sims", $c.Sims, "--lr", $c.Lr, "--l2", $c.L2,
               "--replay-capacity", $c.ReplayCap, "--replay-warmup", $c.ReplayWarm,
               "--batch-size", $c.Batch, "--open-plies", $c.Open,
               "--model-type", $c.ModelType, "--conv-channels", $c.ConvChannels,
               "--seed", $c.Seed, "--report-every", 0)
        if ($c.MlpHidden -ne "") { $a += "--mlp-hidden"; $a += $c.MlpHidden }
        if ($c.PolicyMlp) { $a += "--policy-mlp" }
        Write-Host "  train $($c.Block) seed $($c.Seed): conv($($c.ConvChannels)) fc=$($c.MlpHidden) policy-mlp=$($c.PolicyMlp) sims=$($c.Sims) lr=$($c.Lr) l2=$($c.L2) | cert: cvisit=$($c.Cvisit) cscale=$($c.Cscale) m=$($c.M)"
        while (@($jobs | Where-Object { -not $_.HasExited }).Count -ge $Workers) { Start-Sleep -Milliseconds 400 }
        $jobs += Start-Process -FilePath $TrainExe -ArgumentList $a -PassThru -NoNewWindow `
                     -RedirectStandardOutput "$SweepDir\gza_cc_$($c.Block)_s$($c.Seed).log"
    }
    Write-Host "  waiting for $(@($jobs | Where-Object { -not $_.HasExited }).Count) run(s)..."
    $jobs | ForEach-Object { $_.WaitForExit() }

    foreach ($c in $Cells) {
        $out = "models/sweep/gza_cc_$($c.Block)_s$($c.Seed)"
        for ($i = 0; $i -lt $c.Rungs.Count; $i++) {
            $rung = $c.Rungs[$i]; $sl = $c.Slots[$i]
            $src = Join-Path $Root "$($out)_g$rung.txt"
            $dst = Join-Path $SweepDir "slot$sl.txt"
            if (-not (Test-Path $src)) { Write-Warning "  missing rung $rung for $($c.Block) s$($c.Seed)"; continue }
            Copy-Item $src $dst -Force
            $key = "$($c.Block)|$($c.Seed)|$rung"
            if (-not $done.ContainsKey($key)) {
                "$($c.Block),$($c.Seed),$($c.ModelType),`"$($c.ConvChannels)`",`"$($c.MlpHidden)`",$($c.PolicyMlp),$($c.Sims),$($c.Lr),$($c.L2),$($c.ReplayCap),$($c.ReplayWarm),$($c.Batch),$($c.Open),$($c.Cvisit),$($c.Cscale),$($c.M),$rung,$sl,$dst" |
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
    $marker = "# ---- Gumbel-Zero conv-capacity follow-up ($($ids.Count) agents), added $(Get-Date -Format yyyy-MM-dd) ----"
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
        $bat = Join-Path $SweepDir "gza_cc_play_worker$w.bat"
        $lines = @("@echo off")
        foreach ($id in $buckets[$w]) {
            $lines += "`"$RankExe`" gauntlet --roster `"$BaseRoster`" --in `"$shardStore`" --id `"$id`" --games $GamesPerPair --keep"
            $lines += "if errorlevel 1 exit /b 1"
        }
        $lines += "exit /b 0"
        $lines | Out-File -FilePath $bat -Encoding ascii
        $p = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "`"$bat`"" -PassThru -NoNewWindow `
                 -RedirectStandardOutput "$SweepDir\gza_cc_play_worker$w.log"
        [void]$p.Handle
        $jobs += $p
    }
    Write-Host "  waiting for $($jobs.Count) shard(s)..."
    $jobs | ForEach-Object { $_.WaitForExit() }
    $bad = $jobs | Where-Object { $_.ExitCode -ne 0 }
    if ($bad) { throw "  $($bad.Count) gauntlet shard(s) exited non-zero; store NOT merged, inspect gza_cc_play_worker*.log" }
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
        "Gumbel-Zero mlp/conv architecture sweep (round 6) conv-capacity follow-up: C15's exact recipe (round 6's best conv checkpoint, 806 Elo) crossed with FC head {none,32} x policy head {linear,mlp} x 3 seeds.",
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
