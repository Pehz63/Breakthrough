# train_vs_champion.ps1 - Vs-champion training-data study. Tests every plausible
# way to source value-model training data from games involving the reigning
# champion, against the established replay and self-play baselines.
#
# Developer theories on record (verdicts go in the results doc):
#   Theory 1: a model trained only against the champion still does fine overall
#             (the pool is chip-count-like) but loses to differently-built bots.
#   Theory 2: "you'll never beat the champion by playing against a random
#             dilution of the champion". Oracle games and branch-mined winning
#             lines are the promising counter-data, not dilution games.
#
# Phases:
#   1  Generation: rank.exe pairgen datasets (sharded across -Workers processes)
#      plus the replay extract and the self-play control.
#   2  Training arms: each dataset -> linear v2 PST fits (K seed replicas),
#      screened by rank.exe gauntlet at the d4 wrapper. Resumable via the CSV.
#   3  Bootstrap arm (gated): if the best champion-sourced cell screens within
#      50 Elo of the replay baseline, the best model itself regenerates games
#      vs the champion and retrains once.
#   4  Promotion: the best cell per family is copied to a reserved slot
#      (94..99), d6-confirmed, rostered, and the full pool is re-rated.
#   5  Analysis: per-agent opponent-bucket residuals (champion / classic-like /
#      diverse) for the theory verdicts, plus raw head-to-head vs the champion.
#
# Slots: 81..92 cycle for screening cells, 93 is the bootstrap generator
# scratch, 94..99 are the permanent promoted-family slots (theory1, bootstrap,
# dilution, branch, oracle, replay baseline). The sweep owns 3..80 and the
# scaling study owns 100..125, so nothing collides.
#
# Every screening cell's model is also archived to models/sweep/vsc_<arm>_<seed>.txt
# before its cycling slot is reused, so promotion never loses a winner.
#
# Usage: .\tools\train_vs_champion.ps1                (full study, one long run, resumable)
#        .\tools\train_vs_champion.ps1 -DryRun        (tiny end-to-end pipeline check)
#        .\tools\train_vs_champion.ps1 -AnalysisOnly  (recompute the bucket tables)

param(
    [int]$GamesMain = 4000,
    [int]$GamesOracle = 2000,
    [int]$GamesBranchBase = 2000,
    [int[]]$SeedsMain = @(1001, 2002, 3003),
    [int[]]$SeedsSecondary = @(1001, 2002),
    [int]$Epochs = 6,
    [double]$Lr = 0.05,
    [int]$GauntletGames = 4,
    [int]$Workers = 8,
    [string]$Wrapper = "ab(d4,tt,ord,nb200k)@1",
    [string]$D6Wrapper = "ab(d6,tt,ord,nb200k)@1",
    [string]$Csv = "models/sweep/vs_champ.csv",
    [switch]$DryRun,
    [switch]$SkipPromotion,
    [switch]$AnalysisOnly
)

$ErrorActionPreference = "Stop"
$TrainExe = ".\train.exe"
$RankExe = ".\rank.exe"

# Pinned agents. The champion is the active #1 this study is aimed at. The
# oracle is allowed to search deeper than 6 because it is a teacher, not the
# goal agent (it gets rostered as a reference point, clearly not the target).
$Champion  = "ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2"
$OracleId  = "ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2"
$ClassicD2 = "ab(d2)@1.classic(t1,c4,w0,l0)@2"
$DilArgs   = @("--dil-apply", "a", "--dil-start", "0.3", "--dil-floor", "0.05", "--dil-decay-plies", "30")

if ($DryRun) {
    $GamesMain = 40
    $GamesOracle = 6
    $GamesBranchBase = 20
    $SeedsMain = @(1001)
    $SeedsSecondary = @(1001)
    $GauntletGames = 2
    $Workers = [Math]::Min($Workers, 2)
    $SkipPromotion = $true
    Write-Host "[dry run] tiny sizes, 1 seed, promotion skipped"
}

if (-not (Test-Path $TrainExe)) { Write-Error "$TrainExe not found (build with tools\run_train.ps1 -Build)."; exit 1 }
if (-not (Test-Path $RankExe))  { Write-Error "$RankExe not found (build with tools\run_rank.ps1 -Build)."; exit 1 }
if (-not (Test-Path "models/sweep")) { New-Item -ItemType Directory -Force -Path "models/sweep" | Out-Null }
if (-not (Test-Path "data")) { New-Item -ItemType Directory -Force -Path "data" | Out-Null }

# The learner side of the theory-1 arms: the current best PST at depth 2.
$checkOut = & $RankExe check 2>&1
$pstLine = $checkOut | Select-String "models/pst_value\.txt = ([0-9a-f]+) \(slot 2\)"
if (-not $pstLine) { Write-Error "cannot read the models/pst_value.txt hash from rank.exe check"; exit 1 }
$PstHash = $pstLine.Matches[0].Groups[1].Value
$PstD2 = "ab(d2,tt,ord)@1.learned(s2,$PstHash)@1"
Write-Host "learner (theory 1 generator): $PstD2"
Write-Host "champion:                     $Champion"
Write-Host "oracle:                       $OracleId"

# ============================================================
# Shared helpers
# ============================================================

# Resumable result log (same schema as the scaling study).
$done = @{}
if (Test-Path $Csv) {
    Import-Csv $Csv | ForEach-Object { $done["$($_.phase)|$($_.games)|$($_.seed)|$($_.epochs)"] = $_ }
    Write-Host "Resuming: $($done.Count) completed cells found in $Csv"
} else {
    "phase,games,seed,epochs,slot,hash,elo,pm,id" | Out-File -FilePath $Csv -Encoding ascii
}

$script:slotCycle = 81   # 81..92 cycle for screening cells
function NextSlot {
    $s = $script:slotCycle
    $script:slotCycle++
    if ($script:slotCycle -gt 92) { $script:slotCycle = 81 }
    $s
}

function SlotHash([int]$slot) {
    $checkOut = & $RankExe check 2>&1
    $line = $checkOut | Select-String ("models/sweep/slot" + $slot + "\.txt = ([0-9a-f]+) \(slot " + $slot + "\)")
    if (-not $line) { return $null }
    $line.Matches[0].Groups[1].Value
}

# Generate one pairgen dataset, sharded across worker processes, then merge the
# shard files (byte concat) and their meta tallies. Skips if the merged file
# already exists (delete it to regenerate), which makes the phase resumable.
function GenPair([string]$name, [int]$games, [string[]]$genArgs) {
    $out = "data/$name.jsonl"
    if (Test-Path $out) { Write-Host "[gen] $out exists, skipping (delete to regenerate)"; return }
    $w = [Math]::Max(1, [Math]::Min($Workers, [int][Math]::Floor($games / 4)))
    Write-Host "[gen] $name : $games games across $w worker(s) -> $out"
    $exe = (Resolve-Path $RankExe).Path
    $cwd = (Get-Location).Path
    $procs = @()
    for ($i = 0; $i -lt $w; $i++) {
        $shardArgs = @("pairgen") + $genArgs + @("--games", "$games", "--out", "$out.$i", "--shard", "$i", "--of", "$w")
        $p = Start-Process -FilePath $exe -ArgumentList $shardArgs -WorkingDirectory $cwd -NoNewWindow -PassThru -RedirectStandardOutput "$out.$i.log"
        $null = $p.Handle   # cache the handle, or PS 5.1 loses ExitCode after WaitForExit
        $procs += ,@($p, $i)
    }
    $failed = $false
    foreach ($pi in $procs) {
        $pi[0].WaitForExit()
        if ($pi[0].ExitCode -ne 0) {
            Write-Warning "shard $($pi[1]) exited $($pi[0].ExitCode), log tail:"
            Get-Content "$out.$($pi[1]).log" -Tail 5 | ForEach-Object { Write-Host "    $_" }
            $failed = $true
        }
    }
    if ($failed) { Write-Error "generation failed for $name"; exit 1 }

    # Merge the shard data files.
    $fs = [System.IO.File]::Create((Join-Path $cwd $out))
    for ($i = 0; $i -lt $w; $i++) {
        $src = [System.IO.File]::OpenRead((Join-Path $cwd "$out.$i"))
        $src.CopyTo($fs)
        $src.Close()
    }
    $fs.Close()

    # Merge the meta sidecars (sum the tallies, keep shard 0's config fields).
    $tally = $null
    for ($i = 0; $i -lt $w; $i++) {
        $m = Get-Content "$out.$i.meta.json" -Raw | ConvertFrom-Json
        if ($null -eq $tally) { $tally = $m }
        else {
            foreach ($f in "played","kept","a_wins","b_wins","draws","positions","branch_tried","branch_kept","branch_positions") {
                $tally.$f = $tally.$f + $m.$f
            }
        }
    }
    $tally.shard = 0
    $tally.of = 1
    ($tally | ConvertTo-Json -Compress) | Out-File -FilePath "$out.meta.json" -Encoding ascii
    for ($i = 0; $i -lt $w; $i++) {
        Remove-Item "$out.$i", "$out.$i.meta.json", "$out.$i.log" -Force -Confirm:$false
    }
    Write-Host "[gen] $name : played $($tally.played), kept $($tally.kept), A record $($tally.a_wins)-$($tally.b_wins)-$($tally.draws), $($tally.positions) positions (+$($tally.branch_positions) branch)"
}

# Train one cell into a cycling slot, gauntlet it at the d4 wrapper, append to
# the CSV, and archive the model so slot reuse cannot lose it.
function RunCell([string]$phase, [int]$games, [int]$seed, [string[]]$extraTrainArgs) {
    $key = "$phase|$games|$seed|$Epochs"
    if ($done.ContainsKey($key)) {
        Write-Host "  [skip] $key already done (Elo $($done[$key].elo))"
        return $done[$key]
    }
    $s = NextSlot
    $out = "models/sweep/slot$s"
    Write-Host "  train: arm=$phase games=$games seed=$seed -> slot $s"
    $trainArgs = @("selfplay-supervised", "--out", $out, "--epochs", $Epochs, "--seed", $seed,
                   "--feature-version", "2", "--lr", $Lr) + $extraTrainArgs
    & $TrainExe @trainArgs 2>&1 | Select-Object -Last 3 | ForEach-Object { Write-Host "    $_" }
    if (-not (Test-Path "$out.txt")) { Write-Warning "  no model produced, skipping cell"; return $null }

    $hash = SlotHash $s
    if (-not $hash) { Write-Warning "  no hash for slot $s, skipping cell"; return $null }
    $id = "$Wrapper.learned(s$s,$hash)@1"

    Write-Host "  gauntlet: $id"
    $gOut = & $RankExe gauntlet --id $id --games $GauntletGames --seed $seed 2>&1
    $eloLine = $gOut | Select-String 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)'
    if (-not $eloLine) { Write-Warning "  gauntlet reported no Elo, skipping cell"; return $null }
    $elo = [int]$eloLine.Matches[0].Groups[1].Value
    $pm = [int]$eloLine.Matches[0].Groups[2].Value
    Write-Host "  -> Elo $elo +/- $pm"

    Copy-Item "$out.txt" "models/sweep/vsc_${phase}_$seed.txt" -Force
    $row = [PSCustomObject]@{ phase=$phase; games=$games; seed=$seed; epochs=$Epochs; slot=$s; hash=$hash; elo=$elo; pm=$pm; id=$id }
    "$phase,$games,$seed,$Epochs,$s,$hash,$elo,$pm,$id" | Add-Content -Path $Csv -Encoding Ascii
    $done[$key] = $row
    return $row
}

function BestRowOf([string[]]$phases) {
    $rows = @()
    foreach ($k in $done.Keys) {
        $r = $done[$k]
        if ($phases -contains $r.phase) { $rows += $r }
    }
    if ($rows.Count -eq 0) { return $null }
    $rows | Sort-Object { [int]$_.elo } -Descending | Select-Object -First 1
}

# ============================================================
# Analysis: opponent-bucket residuals for the theory verdicts
# ============================================================
function Invoke-Analysis {
    if (-not ((Test-Path "ranking/ratings.tsv") -and (Test-Path "ranking/games.tsv"))) {
        Write-Warning "analysis needs ranking/ratings.tsv and ranking/games.tsv (run rank.exe rate first)"
        return
    }
    $elo = @{}
    Get-Content "ranking/ratings.tsv" | Select-Object -Skip 1 | ForEach-Object {
        $c = $_ -split "`t"
        if ($c.Count -ge 17) { $elo[$c[16]] = [double]$c[1] }
    }
    # The study's agents: whatever currently sits in the reserved slots.
    $famNames = @{ 94 = "theory1"; 95 = "bootstrap"; 96 = "dilution"; 97 = "branch"; 98 = "oracle"; 99 = "replay-baseline" }
    $agents = @()
    foreach ($slot in 94..99) {
        if (-not (Test-Path "models/sweep/slot$slot.txt")) { continue }
        $h = SlotHash $slot
        if (-not $h) { continue }
        $id = "$D6Wrapper.learned(s$slot,$h)@1"
        if ($elo.ContainsKey($id)) { $agents += ,@($famNames[$slot], $id) }
    }
    if ($agents.Count -eq 0) { Write-Warning "no promoted study agents found in the ratings yet"; return }

    function BucketOf([string]$opp) {
        if ($opp -eq $Champion) { return "champion" }
        if ($opp -match '\.classic\(' -or $opp -match '\.exp\(') { return "classic-like" }
        return "diverse"
    }

    Write-Host ""
    Write-Host "=== Opponent-bucket analysis (actual score vs Elo-expected, per game) ==="
    Write-Host "Positive residual = beats expectation against that bucket."
    foreach ($ag in $agents) {
        $fam = $ag[0]
        $id = $ag[1]
        $b = @{}
        foreach ($k in "champion","classic-like","diverse") { $b[$k] = @{ n = 0; act = 0.0; exp = 0.0; w = 0; l = 0 } }
        Get-Content "ranking/games.tsv" | Select-Object -Skip 1 | ForEach-Object {
            $c = $_ -split "`t"
            if ($c.Count -lt 7) { return }
            $wh = $c[3]
            $bl = $c[4]
            $res = $c[5]
            $opp = $null
            $score = 0.0
            if ($wh -eq $id) { $opp = $bl; if ($res -eq "W") { $score = 1.0 } elseif ($res -eq "D") { $score = 0.5 } }
            elseif ($bl -eq $id) { $opp = $wh; if ($res -eq "B") { $score = 1.0 } elseif ($res -eq "D") { $score = 0.5 } }
            if ($null -eq $opp -or -not $elo.ContainsKey($opp)) { return }
            $bk = BucketOf $opp
            $b[$bk].n++
            $b[$bk].act += $score
            $b[$bk].exp += 1.0 / (1.0 + [Math]::Pow(10.0, ($elo[$opp] - $elo[$id]) / 400.0))
            if ($score -eq 1.0) { $b[$bk].w++ } elseif ($score -eq 0.0) { $b[$bk].l++ }
        }
        Write-Host ""
        Write-Host "[$fam] $id  (Elo $($elo[$id]))"
        foreach ($k in "champion","classic-like","diverse") {
            $x = $b[$k]
            if ($x.n -eq 0) { Write-Host ("  {0,-13} no games" -f $k); continue }
            $act = $x.act / $x.n
            $exp = $x.exp / $x.n
            Write-Host ("  {0,-13} n={1,-4} W-L {2,3}-{3,-4} actual {4:0.000}  expected {5:0.000}  residual {6:+0.000;-0.000}" -f $k, $x.n, $x.w, $x.l, $act, $exp, ($act - $exp))
        }
    }
    Write-Host ""
    Write-Host "Theory 1 check: does a vs-champ agent match the baseline on champion + classic-like buckets"
    Write-Host "but run a clearly more negative residual on the diverse bucket (gap >= ~0.08)?"
    Write-Host "Theory 2 check: compare the dilution agent's champion-bucket record with the oracle and"
    Write-Host "branch agents' records. Theory 2 holds if only the latter show a real head-to-head edge."
}

if ($AnalysisOnly) { Invoke-Analysis; exit 0 }

# ============================================================
# Phase 1: dataset generation
# ============================================================
Write-Host ""
Write-Host "=== Phase 1: dataset generation ==="

GenPair "pg_pstd2_champ"     $GamesMain   (@("--a", $PstD2,     "--b", $Champion, "--seed", "101") + $DilArgs)
GenPair "pg_classicd2_champ" $GamesMain   (@("--a", $ClassicD2, "--b", $Champion, "--seed", "102") + $DilArgs)
GenPair "pg_champdil_champ"  $GamesMain   (@("--a", $Champion,  "--b", $Champion, "--seed", "103") + $DilArgs)
GenPair "pg_oracle_champ"    $GamesOracle (@("--a", $OracleId,  "--b", $Champion, "--seed", "104", "--open-plies", "6"))
GenPair "pg_champloss"       $GamesMain   (@("--a", $Champion,  "--b", $Champion, "--seed", "105", "--filter", "winner=a") + $DilArgs)
GenPair "pg_branch"          $GamesBranchBase (@("--a", $Champion, "--b", $Champion, "--seed", "106", "--filter", "winner=a", "--branch-tries", "4") + $DilArgs)

# Replay baseline extract (reuses the scaling study's file when present).
$ReplayFile = "data/replay_scaling_4000.jsonl"
if ($DryRun) { $ReplayFile = "data/replay_vsc_dry.jsonl" }
if (-not (Test-Path $ReplayFile)) {
    $sample = if ($DryRun) { 100 } else { 4000 }
    Write-Host "[gen] replay extract: $sample games -> $ReplayFile"
    & $RankExe extract --out $ReplayFile --feature-version 2 --sample $sample --seed 777 2>&1 |
        Select-Object -Last 1 | ForEach-Object { Write-Host "    $_" }
}

# Mix dataset: half champion games, half replay, both fresh independent samples.
$MixFile = "data/mix_champ_replay.jsonl"
if (-not (Test-Path $MixFile)) {
    GenPair "pg_pstd2_champ_half" ([int]($GamesMain / 2)) (@("--a", $PstD2, "--b", $Champion, "--seed", "107") + $DilArgs)
    $halfReplay = "data/replay_mix_half.jsonl"
    & $RankExe extract --out $halfReplay --feature-version 2 --sample ([int]($GamesMain / 2)) --seed 778 2>&1 |
        Select-Object -Last 1 | ForEach-Object { Write-Host "    $_" }
    Get-Content "data/pg_pstd2_champ_half.jsonl", $halfReplay | Set-Content $MixFile -Encoding ascii
    Write-Host "[gen] mix -> $MixFile"
}

# ============================================================
# Phase 2: training arms
# ============================================================
Write-Host ""
Write-Host "=== Phase 2: training arms (screen at $Wrapper) ==="

$arms = @(
    @{ name = "replay-4k";          seeds = $SeedsMain;      data = $ReplayFile },
    @{ name = "selfplay-4k";        seeds = $SeedsSecondary; data = $null },
    @{ name = "pstd2-vs-champ";     seeds = $SeedsMain;      data = "data/pg_pstd2_champ.jsonl" },
    @{ name = "classicd2-vs-champ"; seeds = $SeedsSecondary; data = "data/pg_classicd2_champ.jsonl" },
    @{ name = "champdil-vs-champ";  seeds = $SeedsMain;      data = "data/pg_champdil_champ.jsonl" },
    @{ name = "oracle-vs-champ";    seeds = $SeedsMain;      data = "data/pg_oracle_champ.jsonl" },
    @{ name = "champloss-only";     seeds = $SeedsSecondary; data = "data/pg_champloss.jsonl" },
    @{ name = "branch-wins";        seeds = $SeedsSecondary; data = "data/pg_branch.jsonl" },
    @{ name = "mix-50-50";          seeds = $SeedsSecondary; data = $MixFile }
)
foreach ($arm in $arms) {
    Write-Host ""
    Write-Host "--- arm: $($arm.name) ---"
    foreach ($seed in $arm.seeds) {
        if ($null -eq $arm.data) {
            # Self-play control: train.exe generates its own games (sweep recipe).
            RunCell $arm.name $GamesMain $seed @("--games", $GamesMain, "--gen-depth", "2", "--gen-random", "0.3",
                                                 "--gen-random-floor", "0.05", "--gen-random-decay-plies", "30") | Out-Null
        } else {
            RunCell $arm.name $GamesMain $seed @("--from-data", $arm.data, "--games", "0") | Out-Null
        }
    }
}

# ============================================================
# Phase 3: gated bootstrap arm
# ============================================================
$champArms = @("pstd2-vs-champ", "classicd2-vs-champ", "champdil-vs-champ", "oracle-vs-champ", "champloss-only", "branch-wins", "mix-50-50")
$bestReplay = BestRowOf @("replay-4k")
$bestChamp = BestRowOf $champArms
if ($bestReplay -and $bestChamp -and ([int]$bestChamp.elo -ge [int]$bestReplay.elo - 50)) {
    Write-Host ""
    Write-Host "=== Phase 3: bootstrap arm (best champion-sourced cell $($bestChamp.phase)/$($bestChamp.seed) at Elo $($bestChamp.elo) is within 50 of replay $($bestReplay.elo)) ==="
    Copy-Item "models/sweep/vsc_$($bestChamp.phase)_$($bestChamp.seed).txt" "models/sweep/slot93.txt" -Force
    $bootHash = SlotHash 93
    $BootGen = "ab(d2,tt,ord)@1.learned(s93,$bootHash)@1"
    GenPair "pg_bootstrap" $GamesMain (@("--a", $BootGen, "--b", $Champion, "--seed", "108") + $DilArgs)
    RunCell "bootstrap" $GamesMain $SeedsMain[0] @("--from-data", "data/pg_bootstrap.jsonl", "--games", "0") | Out-Null
} else {
    Write-Host ""
    Write-Host "=== Phase 3: bootstrap arm skipped (champion-sourced cells screen more than 50 Elo below replay) ==="
}

# ============================================================
# Phase 4: promotion, d6 confirms, roster, full re-rate
# ============================================================
if (-not $SkipPromotion) {
    Write-Host ""
    Write-Host "=== Phase 4: promote family bests to reserved slots, d6-confirm, roster, re-rate ==="
    $families = @(
        @{ slot = 94; label = "theory1";         arms = @("pstd2-vs-champ", "classicd2-vs-champ", "mix-50-50") },
        @{ slot = 95; label = "bootstrap";       arms = @("bootstrap") },
        @{ slot = 96; label = "dilution";        arms = @("champdil-vs-champ", "champloss-only") },
        @{ slot = 97; label = "branch";          arms = @("branch-wins") },
        @{ slot = 98; label = "oracle";          arms = @("oracle-vs-champ") },
        @{ slot = 99; label = "replay-baseline"; arms = @("replay-4k") }
    )
    $rosterAdds = @()
    foreach ($fam in $families) {
        $best = BestRowOf $fam.arms
        if (-not $best) { Write-Host "  [$($fam.label)] no completed cells, skipping"; continue }
        Copy-Item "models/sweep/vsc_$($best.phase)_$($best.seed).txt" "models/sweep/slot$($fam.slot).txt" -Force
        $h = SlotHash $fam.slot
        $d6id = "$D6Wrapper.learned(s$($fam.slot),$h)@1"
        Write-Host "  [$($fam.label)] best = $($best.phase)/$($best.seed) (screen Elo $($best.elo)) -> slot $($fam.slot)"
        $dkey = "d6-$($fam.label)|$($best.games)|$($best.seed)|$Epochs"
        if ($done.ContainsKey($dkey)) {
            Write-Host "    [skip] d6 confirm already done (Elo $($done[$dkey].elo))"
        } else {
            $gOut = & $RankExe gauntlet --id $d6id --games $GauntletGames --seed 42 2>&1
            $eloLine = $gOut | Select-String 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)'
            if ($eloLine) {
                $e = [int]$eloLine.Matches[0].Groups[1].Value
                $p = [int]$eloLine.Matches[0].Groups[2].Value
                Write-Host "    d6 confirm: Elo $e +/- $p"
                "d6-$($fam.label),$($best.games),$($best.seed),$Epochs,$($fam.slot),$h,$e,$p,$d6id" | Add-Content -Path $Csv -Encoding Ascii
                $done[$dkey] = [PSCustomObject]@{ phase="d6-$($fam.label)"; games=$best.games; seed=$best.seed; epochs=$Epochs; slot=$fam.slot; hash=$h; elo=$e; pm=$p; id=$d6id }
            } else {
                Write-Warning "    d6 confirm produced no Elo"
            }
        }
        $rosterAdds += $d6id
    }
    $rosterAdds += $OracleId

    $rosterFile = "ranking/roster.txt"
    $existing = Get-Content $rosterFile
    $added = 0
    foreach ($id in $rosterAdds) {
        if (-not ($existing | Where-Object { $_ -match [regex]::Escape($id) })) {
            Add-Content -Path $rosterFile -Value "on $id" -Encoding Ascii
            $added++
        }
    }
    Write-Host "  roster: $added new agent(s) appended to $rosterFile"

    Write-Host ""
    Write-Host "=== full pool re-rate ==="
    & .\tools\run_rank.ps1 -Workers $Workers --games 8

    Invoke-Analysis
} else {
    Write-Host ""
    Write-Host "Promotion skipped. Screening results so far:"
    Import-Csv $Csv | Sort-Object { [int]$_.elo } -Descending | Format-Table phase, games, seed, slot, elo, pm -AutoSize
}

Write-Host ""
Write-Host "All results in $Csv"
