# sweep_pst_v2.ps1 - Large hyperparameter sweep for the sparse piece-square (v2)
# value model, rated as a single unified pool via rank.exe's proper Bradley-Terry
# system (not isolated one-off gauntlets), so candidates are compared against
# each other AND the existing 54-agent pool simultaneously.
#
# Candidate groups (78 total, see the design notes in the session's plan):
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
#
# Every candidate is trained straight to models/sweep/slot<N>.txt (N = its
# assigned ranking slot, see slotFile() in ranking.cpp), so no copy step is
# needed before hashing. All candidates are wrapped in the SAME search shell
# ($Wrapper, default ab(d4,tt,ord,nb200k)@1) for the screening pass; re-gauntlet
# the winners at d6 separately afterward for a final, more expensive check.
#
# Usage: .\tools\sweep_pst_v2.ps1 -Build
#        .\tools\sweep_pst_v2.ps1 -Workers 12      (shard the final rank.exe run)

param(
    [switch]$Build,
    [int]$Workers = 1,
    [int]$RateGames = 4,
    [string]$Wrapper = "ab(d4,tt,ord,nb200k)@1",
    [string]$Roster = "ranking/roster.txt",
    [string]$Store = "ranking/matches.jsonl",
    [int]$StartSlot = 3,
    [int]$Epochs = 6,
    [double]$Lr = 0.05,
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

$slot = $StartSlot
$candidates = @()   # each: Group, Slot, Meta (hashtable of axis values), TrainCmd (scriptblock-free arg array)

function NextSlot { $script:slot; $script:slot++ }

# ---- Group A: heuristic-teacher grid, constant dilution ----
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

# ---- Group B: dilution decay variants (depth=4, games=250) ----
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

# ---- Group C: self-play bootstrap chains (2 seeds x 3 generations) ----
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

# ---- Group D: replay-based training from existing match history ----
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

# ---- Group E: L2 regularization variants ----
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

Write-Host "Total candidates: $($candidates.Count) (slots $StartSlot..$($slot-1))"
$candidates | Group-Object Group | ForEach-Object { Write-Host "  group $($_.Name): $($_.Count)" }
if ($Only -gt 0) {
    $candidates = $candidates | Select-Object -First $Only
    Write-Host "-Only ${Only}: truncated to $($candidates.Count) candidates for a dry run"
}

# ---- Train every candidate (groups run in enumeration order; C's generations
#      depend on the prior generation's file, already enumerated first) ----
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
    & $TrainExe @($cand.Args) 2>&1 | ForEach-Object { Write-Host "    $_" }
}

# ---- Hash every trained slot in one pass, build roster ids ----
Write-Host ""
Write-Host "=== Hashing + registering roster entries ==="
$checkOut = & $RankExe check 2>&1
$hashes = @{}
$checkOut | Select-String 'models/sweep/slot(\d+)\.txt = ([0-9a-f]+) \(slot \d+\)' | ForEach-Object {
    $hashes[[int]$_.Matches[0].Groups[1].Value] = $_.Matches[0].Groups[2].Value
}

# Idempotent: skip any id already present in the roster (e.g. a prior dry run or
# a re-run after a partial failure), so this step is safe to repeat.
$existing = if (Test-Path $Roster) { Get-Content $Roster } else { @() }
$newLines = @()
foreach ($cand in $candidates) {
    $s = $cand.Slot
    if (-not $hashes.ContainsKey($s)) {
        Write-Warning "  slot $s has no model file / hash (training likely failed); skipping roster entry"
        continue
    }
    $id = "$Wrapper.learned(s$s,$($hashes[$s]))@1"
    $cand | Add-Member -NotePropertyName Id -NotePropertyValue $id
    if ($existing -match [regex]::Escape($id)) { continue }   # already registered
    $newLines += "on $id"
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
    $rows += [PSCustomObject]@{
        Group = $cand.Group; Slot = $cand.Slot
        Elo = if ($r) { $r.elo } else { $null }
        EloErr = if ($r) { $r.pm } else { $null }
        Meta = ($cand.Meta.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ","
        Id = $cand.Id
    }
}
$rows | Sort-Object Group, { -$_.Elo } | Format-Table Group, Slot, Elo, EloErr, Meta -AutoSize
$rows | Export-Csv -Path "models/sweep/report_v2.csv" -NoTypeInformation
Write-Host "Report written to models/sweep/report_v2.csv"
