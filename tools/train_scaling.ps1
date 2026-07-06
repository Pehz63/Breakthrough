# train_scaling.ps1 - Data-scaling study for the sparse piece-square (v2) value
# model, holding fixed everything the 78-candidate sweep showed does not matter
# (teacher depth = 2, dilution decay 0.3 -> 0.05 over 30 plies, no L2, no
# bootstrap) and increasing the training-game count until Elo converges.
#
# Phases:
#   1  Self-play scaling: games double from -StartGames until the mean screening
#      Elo (over -Seeds training seeds) improves by less than -ConvergeElo, or
#      -MaxGames is hit.
#   2  Replay scaling (skip with -NoReplay): the same fit on 4000- and
#      8000-game samples extracted from the real ranked match history
#      (ranking/matches.jsonl), the sweep's other promising data source.
#   3  Epoch probe: the best self-play size retrained with double the epochs
#      (the 6-epoch overfit guideline was calibrated on ~250-game datasets and
#      may under-train much larger ones).
#   4  d6 confirmation gauntlet of the best candidate overall.
#
# Screening protocol matches the sweep: wrap each model in
# ab(d4,tt,ord,nb200k)@1 and gauntlet vs the frozen rated pool, 4 games per
# opponent. Results append to models/sweep/scaling.csv; the script is RESUMABLE
# (rows already in the CSV are skipped), so re-running after an interruption
# continues where it left off.
#
# Usage: .\tools\train_scaling.ps1
#        .\tools\train_scaling.ps1 -StartGames 50 -MaxGames 50 -Seeds 1001 -NoReplay -NoProbe   (pipeline dry run)

param(
    [int]$StartGames = 250,
    [int]$MaxGames = 4000,
    [int[]]$Seeds = @(1001, 2002),
    [int]$ConvergeElo = 20,
    [int]$Epochs = 6,
    [double]$Lr = 0.05,
    [int]$GauntletGames = 4,
    [string]$Wrapper = "ab(d4,tt,ord,nb200k)@1",
    [string]$D6Wrapper = "ab(d6,tt,ord,nb200k)@1",
    [string]$Csv = "models/sweep/scaling.csv",
    [switch]$NoReplay,
    [switch]$NoProbe
)

$ErrorActionPreference = "Stop"
$TrainExe = ".\train.exe"
$RankExe = ".\rank.exe"
if (-not (Test-Path $TrainExe)) { Write-Error "$TrainExe not found (build with tools\run_train.ps1 -Build)."; exit 1 }
if (-not (Test-Path $RankExe))  { Write-Error "$RankExe not found (build with tools\run_rank.ps1 -Build)."; exit 1 }
if (-not (Test-Path "models/sweep")) { New-Item -ItemType Directory -Force -Path "models/sweep" | Out-Null }

# Fixed recipe (what the sweep said matters is only the data, so everything
# else is pinned): d2 teacher, decaying dilution, feature v2, no L2.
$FixedTrainArgs = @("--feature-version", "2", "--lr", $Lr,
                    "--gen-depth", "2", "--gen-random", "0.3",
                    "--gen-random-floor", "0.05", "--gen-random-decay-plies", "30")

# Resumable result log.
$done = @{}
if (Test-Path $Csv) {
    Import-Csv $Csv | ForEach-Object { $done["$($_.phase)|$($_.games)|$($_.seed)|$($_.epochs)"] = $_ }
    Write-Host "Resuming: $($done.Count) completed cells found in $Csv"
} else {
    "phase,games,seed,epochs,slot,hash,elo,pm,id" | Out-File -FilePath $Csv -Encoding ascii
}

$script:slotCycle = 100   # slots 100..125 cycle for this study (well clear of the sweep's 3..80 and the trainer scratch at 126/127)
function NextSlot {
    $s = $script:slotCycle
    $script:slotCycle++
    if ($script:slotCycle -gt 125) { $script:slotCycle = 100 }
    $s
}

# Train one candidate into a slot file and gauntlet it. Returns the result row
# (also appended to the CSV), or $null on failure.
function RunCell([string]$phase, [int]$games, [int]$seed, [int]$epochs, [string[]]$extraTrainArgs) {
    $key = "$phase|$games|$seed|$epochs"
    if ($done.ContainsKey($key)) {
        Write-Host "  [skip] $key already done (Elo $($done[$key].elo))"
        return $done[$key]
    }
    $s = NextSlot
    $out = "models/sweep/slot$s"
    Write-Host "  train: phase=$phase games=$games seed=$seed epochs=$epochs -> slot $s"
    $trainArgs = @("selfplay-supervised", "--out", $out, "--games", $games, "--epochs", $epochs, "--seed", $seed) + $FixedTrainArgs + $extraTrainArgs
    & $TrainExe @trainArgs 2>&1 | ForEach-Object { Write-Host "    $_" }
    if (-not (Test-Path "$out.txt")) { Write-Warning "  no model produced, skipping cell"; return $null }

    $checkOut = & $RankExe check 2>&1
    $hashLine = $checkOut | Select-String ("models/sweep/slot" + $s + "\.txt = ([0-9a-f]+) \(slot " + $s + "\)")
    if (-not $hashLine) { Write-Warning "  no hash for slot $s, skipping cell"; return $null }
    $hash = $hashLine.Matches[0].Groups[1].Value
    $id = "$Wrapper.learned(s$s,$hash)@1"

    Write-Host "  gauntlet: $id"
    $gOut = & $RankExe gauntlet --id $id --games $GauntletGames --seed $seed 2>&1
    $eloLine = $gOut | Select-String 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)'
    if (-not $eloLine) { Write-Warning "  gauntlet reported no Elo, skipping cell"; return $null }
    $elo = [int]$eloLine.Matches[0].Groups[1].Value
    $pm = [int]$eloLine.Matches[0].Groups[2].Value
    Write-Host "  -> Elo $elo +/- $pm"

    $row = [PSCustomObject]@{ phase=$phase; games=$games; seed=$seed; epochs=$epochs; slot=$s; hash=$hash; elo=$elo; pm=$pm; id=$id }
    "$phase,$games,$seed,$epochs,$s,$hash,$elo,$pm,$id" | Add-Content -Path $Csv -Encoding Ascii
    $done[$key] = $row
    return $row
}

# ---- Phase 1: self-play scaling with convergence stop ----
Write-Host "=== Phase 1: self-play game-count scaling (start $StartGames, cap $MaxGames, stop when mean gain < $ConvergeElo) ==="
$games = $StartGames
$prevMean = $null
$bestRow = $null
$sizeMeans = @()
while ($games -le $MaxGames) {
    Write-Host ""
    Write-Host "--- size: $games games ---"
    $elos = @()
    foreach ($seed in $Seeds) {
        $row = RunCell "selfplay" $games $seed $Epochs @()
        if ($row) {
            $elos += [int]$row.elo
            if (-not $bestRow -or [int]$row.elo -gt [int]$bestRow.elo) { $bestRow = $row }
        }
    }
    if ($elos.Count -eq 0) { Write-Warning "size $games produced no results; stopping"; break }
    $mean = ($elos | Measure-Object -Average).Average
    $sizeMeans += [PSCustomObject]@{ games=$games; mean=[math]::Round($mean,1); n=$elos.Count }
    Write-Host "--- size $games mean Elo: $([math]::Round($mean,1)) over $($elos.Count) seed(s) ---"
    if ($null -ne $prevMean -and ($mean - $prevMean) -lt $ConvergeElo) {
        Write-Host "Converged: gain $([math]::Round($mean - $prevMean,1)) < $ConvergeElo. Stopping the scaling loop."
        break
    }
    $prevMean = $mean
    $games = $games * 2
}
Write-Host ""
Write-Host "Self-play scaling curve:"
$sizeMeans | Format-Table -AutoSize

# ---- Phase 2: replay-data scaling ----
if (-not $NoReplay) {
    Write-Host "=== Phase 2: replay-data scaling (extract from ranking/matches.jsonl) ==="
    foreach ($sample in 4000, 8000) {
        $replayFile = "data/replay_scaling_$sample.jsonl"
        if (-not (Test-Path $replayFile)) {
            Write-Host "  extract: $sample games -> $replayFile"
            & $RankExe extract --out $replayFile --feature-version 2 --sample $sample --seed 777 2>&1 |
                Select-Object -Last 2 | ForEach-Object { Write-Host "    $_" }
        }
        foreach ($seed in $Seeds) {
            $row = RunCell "replay" $sample $seed $Epochs @("--from-data", $replayFile)
            if ($row -and [int]$row.elo -gt [int]$bestRow.elo) { $bestRow = $row }
        }
    }
}

# ---- Phase 3: epoch probe at the best self-play size ----
if (-not $NoProbe -and $sizeMeans.Count -gt 0) {
    $bestSize = ($sizeMeans | Sort-Object mean -Descending | Select-Object -First 1).games
    Write-Host "=== Phase 3: epoch probe (size $bestSize, epochs $($Epochs*2)) ==="
    foreach ($seed in $Seeds) {
        $row = RunCell "epoch-probe" $bestSize $seed ($Epochs * 2) @()
        if ($row -and [int]$row.elo -gt [int]$bestRow.elo) { $bestRow = $row }
    }
}

# ---- Phase 4: d6 confirmation of the best candidate ----
if ($bestRow) {
    Write-Host ""
    Write-Host "=== Phase 4: d6 confirmation of the best candidate ==="
    Write-Host "Best screening cell: phase=$($bestRow.phase) games=$($bestRow.games) seed=$($bestRow.seed) epochs=$($bestRow.epochs) Elo $($bestRow.elo)"
    Copy-Item "models/sweep/slot$($bestRow.slot).txt" "models/sweep/scaling_best.txt" -Force
    $h = $bestRow.hash
    $d6id = "$D6Wrapper.learned(s$($bestRow.slot),$h)@1"
    & $RankExe gauntlet --id $d6id --games $GauntletGames --seed 42 2>&1 | Select-Object -Last 3 | ForEach-Object { Write-Host $_ }
    Write-Host "Best model copied to models/sweep/scaling_best.txt (promote by copying over models/pst_value.txt)"
}
Write-Host ""
Write-Host "All results in $Csv"
