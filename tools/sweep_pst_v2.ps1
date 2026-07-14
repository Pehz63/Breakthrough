# sweep_pst_v2.ps1 - Hyperparameter sweep for learned value models, rated as a
# single unified pool via rank.exe's Bradley-Terry system (not isolated one-off
# gauntlets), so candidates are compared against each other AND the existing pool
# simultaneously.
#
# This is the general model-sweep harness. A candidate is a Group/Slot/Meta/Args
# object where Args is a raw train.exe argument array trained into
# models/sweep/slot<N>.txt; ANY train.exe flag (feature version, model type,
# residual skip, ...) is usable in a candidate with no scaffolding change, so a
# new model type joins by adding a group. Select a subset with -Groups so new work
# runs without re-running the whole thing.
#
# Candidate groups:
#   A (36) heuristic-teacher self-play: depth {2,4,6} x games {100,250} x
#          dilution {0.1,0.2,0.3} x seed {1001,2002}, constant dilution.
#   B (12) dilution DECAY variants at depth=4,games=250: 6 (start,floor,plies)
#          combos x 2 seeds.
#   C (6)  self-play bootstrap chains: 2 independently-seeded 3-generation
#          chains (heuristic-taught gen0 -> self-play gen1 -> self-play gen2).
#   D (12) replay-based training from the EXISTING rank.exe match history
#          (rank.exe extract): sample {1000,3000,8000} x feature-version {1,2}
#          x seed {1001,2002}.
#   E (12) L2 weight-decay variants: base {depth 4, depth 6} x l2
#          {0.001,0.005,0.01} x seed {1001,2002}, dilution=0.2 constant.
#   F (4)  linear residual baseline (theory 24): plain-linear vs residual chip
#          skip {off,auto} x seed {1001,2002}, same replay data. Isolates the
#          skip at the linear level.
#   G (8)  MLP capacity comparison (theory 24): --model-type mlp x hidden {16,32}
#          x skip {off,auto} x seed {1001,2002}, same replay data. The residual-
#          vs-unconstrained-same-capacity test; MLP is full-scan so these cells
#          screen at a cheaper per-candidate Wrapper.
#
# Every candidate is trained straight to models/sweep/slot<N>.txt (N = its
# assigned ranking slot, see slotFile() in ranking.cpp), so no copy step is
# needed before hashing. Candidates screen under a search shell ($Wrapper, default
# ab(d4,tt,ord,nb200k)@1) unless the candidate overrides it (Wrapper field);
# re-gauntlet the winners at d6 separately afterward for a final, more expensive check.
#
# Usage: .\tools\sweep_pst_v2.ps1 -Build
#        .\tools\sweep_pst_v2.ps1 -Groups "F,G"     (only the new residual/MLP groups)
#        .\tools\sweep_pst_v2.ps1 -Workers 12       (shard the final rank.exe run)

param(
    [switch]$Build,
    [int]$Workers = 1,
    [int]$RateGames = 4,
    [string]$Wrapper = "ab(d4,tt,ord,nb200k)@1",
    [string]$MlpWrapper = "ab(d4,tt,ord,nb50000)@1",   # cheaper shell for full-scan MLP cells
    [string]$Roster = "ranking/roster.txt",
    [string]$Store = "ranking/matches.jsonl",
    [int]$StartSlot = 3,
    [int]$Epochs = 6,
    [double]$Lr = 0.05,
    [string]$Groups = "all",   # "all" or a comma list, e.g. "F,G" to run only the new groups
    [int[]]$Seeds = @(1001,2002,3003,4004,5005,6006),  # training-seed replicas for groups F/G (about 6, to clear the training-seed-noise band, theory 8)
    [switch]$NoRate,           # train + report stratified loss only; skip the pool rating + roster/matches append
    [switch]$RateOnly,         # skip training; hash existing slot files, roster, and rate (reuse models from an earlier -NoRate pass)
    [int]$Only = 0   # 0 = all candidates; N = dry-run just the first N (for validating the pipeline cheaply)
)

$ErrorActionPreference = "Stop"
$TrainExe = ".\train.exe"
$RankExe = ".\rank.exe"

if ($Build) {
    Write-Host "Building train.exe..."
    if (Test-Path $TrainExe) { Remove-Item $TrainExe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_train.bat"
    if (-not (Test-Path $TrainExe)) { Write-Error "Build did not produce $TrainExe."; exit 1 }
    Write-Host "Building rank.exe..."
    if (Test-Path $RankExe) { Remove-Item $RankExe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_rank.bat"
    if (-not (Test-Path $RankExe)) { Write-Error "Build did not produce $RankExe."; exit 1 }
}
if (-not (Test-Path $TrainExe)) { Write-Error "$TrainExe not found. Run with -Build first."; exit 1 }
if (-not (Test-Path $RankExe))  { Write-Error "$RankExe not found. Run with -Build first."; exit 1 }
if (-not (Test-Path "models/sweep")) { New-Item -ItemType Directory -Force -Path "models/sweep" | Out-Null }
if (-not (Test-Path "data")) { New-Item -ItemType Directory -Force -Path "data" | Out-Null }

$selectedGroups = if ($Groups -eq "all") { @("A","B","C","D","E","F","G") }
                  else { @($Groups -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
Write-Host "Groups selected: $($selectedGroups -join ',')"

$slot = $StartSlot
$candidates = @()   # each: Group, Slot, Meta (hashtable of axis values), Args (train.exe arg array), optional Wrapper/ExtractArgs

function NextSlot { $script:slot; $script:slot++ }

# Groups F/G share one replay dataset (same data recipe, so the only difference
# across their cells is model architecture / skip). Extract it once if needed.
$resReplay = "data/replay_v2_residual.jsonl"
if ((($selectedGroups -contains "F") -or ($selectedGroups -contains "G")) -and -not (Test-Path $resReplay)) {
    Write-Host "Extracting shared replay data for groups F/G -> $resReplay"
    & $RankExe extract --out $resReplay --feature-version 2 --sample 8000 --seed 4242 2>&1 | ForEach-Object { Write-Host "  $_" }
}

# ---- Group A: heuristic-teacher grid, constant dilution ----
if ($selectedGroups -contains "A") {
foreach ($depth in 2,4,6) {
    foreach ($games in 100,250) {
        foreach ($dil in 0.1,0.2,0.3) {
            foreach ($seed in 1001,2002) {
                $s = NextSlot
                $candidates += [PSCustomObject]@{
                    Group = "A"; Slot = $s
                    Meta  = @{ depth=$depth; games=$games; dilution=$dil; seed=$seed; l2=0.0 }
                    Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s", "--feature-version", "2",
                              "--games", $games, "--epochs", $Epochs, "--lr", $Lr,
                              "--gen-depth", $depth, "--gen-random", $dil, "--seed", $seed)
                }
            }
        }
    }
}
}

# ---- Group B: dilution decay variants (depth=4, games=250) ----
if ($selectedGroups -contains "B") {
$decayCombos = @(
    @{ start=0.2; floor=0.05; plies=15 }, @{ start=0.2; floor=0.05; plies=30 },
    @{ start=0.3; floor=0.05; plies=15 }, @{ start=0.3; floor=0.05; plies=30 },
    @{ start=0.4; floor=0.05; plies=30 }, @{ start=0.4; floor=0.00; plies=50 }
)
foreach ($c in $decayCombos) {
    foreach ($seed in 1001,2002) {
        $s = NextSlot
        $candidates += [PSCustomObject]@{
            Group = "B"; Slot = $s
            Meta  = @{ depth=4; games=250; dilution=$c.start; decayFloor=$c.floor; decayPlies=$c.plies; seed=$seed; l2=0.0 }
            Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s", "--feature-version", "2",
                      "--games", 250, "--epochs", $Epochs, "--lr", $Lr,
                      "--gen-depth", 4, "--gen-random", $c.start, "--gen-random-floor", $c.floor,
                      "--gen-random-decay-plies", $c.plies, "--seed", $seed)
        }
    }
}
}

# ---- Group C: self-play bootstrap chains (2 seeds x 3 generations) ----
if ($selectedGroups -contains "C") {
foreach ($seed in 1001,2002) {
    $s0 = NextSlot
    $candidates += [PSCustomObject]@{
        Group = "C"; Slot = $s0
        Meta  = @{ generation=0; bootstrapSeed=$seed; seed=$seed; l2=0.0 }
        Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s0", "--feature-version", "2",
                  "--games", 200, "--epochs", $Epochs, "--lr", $Lr,
                  "--gen-depth", 2, "--gen-random", 0.2, "--seed", $seed)
    }
    $s1 = NextSlot
    $candidates += [PSCustomObject]@{
        Group = "C"; Slot = $s1
        Meta  = @{ generation=1; bootstrapSeed=$seed; seed=$seed; l2=0.0; parentSlot=$s0 }
        Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s1", "--feature-version", "2",
                  "--games", 200, "--epochs", $Epochs, "--lr", $Lr,
                  "--gen-model", "models/sweep/slot$s0.txt", "--gen-model-explorer", "alphabeta", "--gen-depth", 4,
                  "--gen-random", 0.2, "--gen-random-floor", 0.05, "--gen-random-decay-plies", 30, "--seed", ($seed+1))
    }
    $s2 = NextSlot
    $candidates += [PSCustomObject]@{
        Group = "C"; Slot = $s2
        Meta  = @{ generation=2; bootstrapSeed=$seed; seed=$seed; l2=0.0; parentSlot=$s1 }
        Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s2", "--feature-version", "2",
                  "--games", 200, "--epochs", $Epochs, "--lr", $Lr,
                  "--gen-model", "models/sweep/slot$s1.txt", "--gen-model-explorer", "alphabeta", "--gen-depth", 4,
                  "--gen-random", 0.2, "--gen-random-floor", 0.05, "--gen-random-decay-plies", 30, "--seed", ($seed+2))
    }
}
}

# ---- Group D: replay-based training from existing match history ----
if ($selectedGroups -contains "D") {
foreach ($sample in 1000,3000,8000) {
    foreach ($fv in 1,2) {
        foreach ($seed in 1001,2002) {
            $s = NextSlot
            $replayFile = "data/replay_v${fv}_${sample}_${seed}.jsonl"
            $candidates += [PSCustomObject]@{
                Group = "D"; Slot = $s
                Meta  = @{ sampleSize=$sample; featVer=$fv; seed=$seed; l2=0.0 }
                ExtractArgs = @("extract", "--out", $replayFile, "--feature-version", $fv, "--sample", $sample, "--seed", $seed)
                Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s", "--feature-version", $fv,
                          "--from-data", $replayFile, "--epochs", $Epochs, "--lr", $Lr, "--seed", $seed)
            }
        }
    }
}
}

# ---- Group E: L2 regularization variants ----
if ($selectedGroups -contains "E") {
foreach ($depth in 4,6) {
    foreach ($l2 in 0.001,0.005,0.01) {
        foreach ($seed in 1001,2002) {
            $s = NextSlot
            $candidates += [PSCustomObject]@{
                Group = "E"; Slot = $s
                Meta  = @{ depth=$depth; games=250; dilution=0.2; l2=$l2; seed=$seed }
                Args  = @("selfplay-supervised", "--out", "models/sweep/slot$s", "--feature-version", "2",
                          "--games", 250, "--epochs", $Epochs, "--lr", $Lr,
                          "--gen-depth", $depth, "--gen-random", 0.2, "--l2", $l2, "--seed", $seed)
            }
        }
    }
}
}

# ---- Group F: linear residual chip-skip baseline (theory 24) ----
# Plain linear vs residual (auto-calibrated skip), same replay data + seed
# replicas. Isolates the skip's effect at the linear level.
if ($selectedGroups -contains "F") {
foreach ($skip in "off","auto") {
    foreach ($seed in $Seeds) {
        $s = NextSlot
        $a = @("selfplay-supervised", "--out", "models/sweep/slot$s", "--feature-version", "2",
               "--from-data", $resReplay, "--epochs", $Epochs, "--lr", $Lr, "--seed", $seed)
        if ($skip -eq "auto") { $a += @("--residual-skip", "-1") }
        $candidates += [PSCustomObject]@{
            Group = "F"; Slot = $s
            Meta  = @{ modelType="linear"; skip=$skip; seed=$seed }
            Args  = $a
        }
    }
}
}

# ---- Group G: MLP capacity comparison (theory 24) ----
# --model-type mlp x hidden {16,32} x skip {off,auto} x seed. Residual-vs-
# unconstrained-same-capacity test. MLP search is full-scan, so screen these at
# the cheaper $MlpWrapper.
if ($selectedGroups -contains "G") {
foreach ($hid in 16,32) {
    foreach ($skip in "off","auto") {
        foreach ($seed in $Seeds) {
            $s = NextSlot
            $a = @("selfplay-supervised", "--out", "models/sweep/slot$s", "--feature-version", "2",
                   "--from-data", $resReplay, "--model-type", "mlp", "--mlp-hidden", $hid,
                   "--epochs", $Epochs, "--lr", $Lr, "--seed", $seed)
            if ($skip -eq "auto") { $a += @("--residual-skip", "-1") }
            $candidates += [PSCustomObject]@{
                Group = "G"; Slot = $s
                Meta  = @{ modelType="mlp"; hidden=$hid; skip=$skip; seed=$seed }
                Args  = $a
                Wrapper = $MlpWrapper
            }
        }
    }
}
}

Write-Host "Total candidates: $($candidates.Count) (slots $StartSlot..$($slot-1))"
$candidates | Group-Object Group | ForEach-Object { Write-Host "  group $($_.Name): $($_.Count)" }
if ($Only -gt 0) {
    $candidates = $candidates | Select-Object -First $Only
    Write-Host "-Only ${Only}: truncated to $($candidates.Count) candidates for a dry run"
}

# ---- Train every candidate (groups run in enumeration order; C's generations
#      depend on the prior generation's file, already enumerated first).
#      -RateOnly skips this and rates the slot files left by an earlier pass. ----
if (-not $RateOnly) {
$n = 0
foreach ($cand in $candidates) {
    $n++
    Write-Host ""
    Write-Host "=== [$n/$($candidates.Count)] group $($cand.Group) slot $($cand.Slot) ==="
    if ($cand.PSObject.Properties.Name -contains "ExtractArgs") {
        Write-Host "  extract: $($cand.ExtractArgs -join ' ')"
        & $RankExe @($cand.ExtractArgs) 2>&1 | ForEach-Object { Write-Host "    $_" }
    }
    Write-Host "  train: $($cand.Args -join ' ')"
    $trainOut = & $TrainExe @($cand.Args) 2>&1
    $trainOut | ForEach-Object { Write-Host "    $_" }
    # Capture the theory-24 stratified-loss printout (loss by |material diff|), so
    # the report shows equal-material calibration next to pooled Elo.
    $sl = $trainOut | Select-String 'Stratified loss by \|matDiff\|:\s+==0 n=\d+ loss=([0-9.]+)\s+==1 n=\d+ loss=([0-9.]+)\s+>=2 n=\d+ loss=([0-9.]+)'
    if ($sl) {
        $cand | Add-Member -NotePropertyName Loss0 -NotePropertyValue ([double]$sl.Matches[0].Groups[1].Value) -Force
        $cand | Add-Member -NotePropertyName Loss1 -NotePropertyValue ([double]$sl.Matches[0].Groups[2].Value) -Force
        $cand | Add-Member -NotePropertyName Loss2 -NotePropertyValue ([double]$sl.Matches[0].Groups[3].Value) -Force
    }
}
} else {
    Write-Host "-RateOnly: skipping training; rating existing models/sweep/slot*.txt files."
}

# ---- Hash every trained slot in one pass, build roster ids ----
Write-Host ""
Write-Host "=== Hashing + registering roster entries ==="
$checkOut = & $RankExe check 2>&1
$hashes = @{}
$checkOut | Select-String 'models/sweep/slot(\d+)\.txt = ([0-9a-f]+) \(slot \d+\)' | ForEach-Object {
    $hashes[[int]$_.Matches[0].Groups[1].Value] = $_.Matches[0].Groups[2].Value
}

# Build each candidate's canonical id (needed for the report regardless of rating).
foreach ($cand in $candidates) {
    $s = $cand.Slot
    if (-not $hashes.ContainsKey($s)) {
        Write-Warning "  slot $s has no model file / hash (training likely failed); skipping id"
        continue
    }
    $w = if ($cand.PSObject.Properties.Name -contains "Wrapper") { $cand.Wrapper } else { $Wrapper }
    $cand | Add-Member -NotePropertyName Id -NotePropertyValue "$w.learned(s$s,$($hashes[$s]))@1"
}

# -NoRate: stop after training + hashing. Reports the theory-24 stratified loss
# (search-free) without the expensive pool rating or the permanent roster/matches
# append, so the calibration comparison is cheap and reversible. Run the full
# rating later (or without -NoRate) when the Elo is wanted.
if (-not $NoRate) {
    # Idempotent: skip any id already present in the roster (e.g. a prior dry run
    # or a re-run after a partial failure), so this step is safe to repeat.
    $existing = if (Test-Path $Roster) { Get-Content $Roster } else { @() }
    $newLines = @()
    foreach ($cand in $candidates) {
        if (-not ($cand.PSObject.Properties.Name -contains "Id")) { continue }
        if ($existing -match [regex]::Escape($cand.Id)) { continue }   # already registered
        $newLines += "on $($cand.Id)"
    }
    if ($newLines.Count -gt 0) { Add-Content -Path $Roster -Value $newLines -Encoding Ascii }
    Write-Host "Appended $($newLines.Count) new 'on' lines to $Roster (of $($candidates.Count) candidates)"

    # ---- Rate everyone together in one pass ----
    Write-Host ""
    Write-Host "=== Rating the enlarged pool ($RateGames games/pair) ==="
    if ($Workers -gt 1) {
        & .\tools\run_rank.ps1 -Workers $Workers --games $RateGames --roster $Roster --in $Store
    } else {
        & $RankExe run --games $RateGames --roster $Roster --in $Store
    }
} else {
    Write-Host "-NoRate: skipped roster append + pool rating (training + stratified loss only)."
}

# ---- Pull each candidate's final Elo from ratings.tsv, write a findings table ----
Write-Host ""
Write-Host "=== Building findings report ==="
$ratings = @{}
Get-Content "ranking/ratings.tsv" | Select-Object -Skip 1 | ForEach-Object {
    $f = $_ -split "`t"
    if ($f.Count -ge 17) { $ratings[$f[16]] = @{ elo = [int]$f[1]; pm = [int]$f[2]; games = [int]$f[3] } }
}

$rows = @()
foreach ($cand in $candidates) {
    if (-not $cand.PSObject.Properties.Name -contains "Id") { continue }
    $r = $ratings[$cand.Id]
    $has = { param($nm) $cand.PSObject.Properties.Name -contains $nm }
    $rows += [PSCustomObject]@{
        Group = $cand.Group; Slot = $cand.Slot
        Elo = if ($r) { $r.elo } else { $null }
        EloErr = if ($r) { $r.pm } else { $null }
        # Theory-24 measure: logistic loss on equal-material (Loss0), 1-off (Loss1),
        # and >=2 (Loss2) positions. Lower Loss0 for the residual cell is the target.
        Loss0 = if (& $has "Loss0") { $cand.Loss0 } else { $null }
        Loss1 = if (& $has "Loss1") { $cand.Loss1 } else { $null }
        Loss2 = if (& $has "Loss2") { $cand.Loss2 } else { $null }
        Meta = ($cand.Meta.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ","
        Id = $cand.Id
    }
}
$rows | Sort-Object Group, { -$_.Elo } | Format-Table Group, Slot, Elo, EloErr, Loss0, Loss1, Loss2, Meta -AutoSize
$rows | Export-Csv -Path "models/sweep/report_v2.csv" -NoTypeInformation
Write-Host "Report written to models/sweep/report_v2.csv"
