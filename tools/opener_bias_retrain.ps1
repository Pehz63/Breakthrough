# opener_bias_retrain.ps1 -- Theory 6, Layer 3 (Docs/theories.md): retrain the
# oracle arm on ASYMMETRIC-opener data and compare to the symmetric baseline.
#
# The vs-champion study trained the oracle arm on pg_oracle_champ.jsonl, generated
# with --open-plies 6 on BOTH sides, so the champion played 6 random moves in every
# training game. This regenerates that data with --open-side a (only the oracle
# plays the random opener; the champion plays its own true policy throughout,
# including the opening), retrains the same 3-seed oracle cell, gauntlet-screens at
# the d4 wrapper, and d6-confirms the best. If the numbers drop meaningfully versus
# the symmetric baseline (screen mean ~785 / d6 confirm 1137), the headline oracle
# result was partly an artifact of training against a handicapped champion.
#
# Non-mutating: writes only to models/sweep/vsc_oracle-asym_*.txt + the cycling
# screening slots + its own CSV. It does NOT touch roster.txt or matches.jsonl
# (gauntlet uses scratch), and does NOT overwrite the symmetric baseline models.
#
# Usage: .\tools\opener_bias_retrain.ps1 [-GamesOracle 2000] [-Workers 12]
#        .\tools\opener_bias_retrain.ps1 -DryRun     (tiny sizes, 1 seed)
param(
    [int]$GamesOracle = 2000,
    [int[]]$Seeds = @(1001, 2002, 3003),
    [int]$Epochs = 6,
    [double]$Lr = 0.05,
    [int]$GauntletGames = 4,
    [int]$Workers = 12,
    [int]$OpenPlies = 6,
    [string]$Wrapper = "ab(d4,tt,ord,nb200k)@1",
    [string]$D6Wrapper = "ab(d6,tt,ord,nb200k)@1",
    [string]$Csv = "models/sweep/opener_bias_retrain.csv",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$TrainExe = ".\train.exe"
$RankExe = ".\rank.exe"
foreach ($e in $TrainExe, $RankExe) { if (-not (Test-Path $e)) { Write-Error "$e not found (build it first)."; exit 1 } }
New-Item -ItemType Directory -Force -Path "models/sweep" | Out-Null

$Champion = "ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2"
$OracleId = "ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2"
$DataAsym = "data/pg_oracle_champ_asym.jsonl"

# Symmetric baseline (from models/sweep/vs_champ.csv, for the printed comparison).
$BaseScreen = @(792, 738, 825); $BaseScreenMean = 785; $BaseD6 = 1137

if ($DryRun) { $GamesOracle = 40; $Seeds = @(1001); $GauntletGames = 2; $Workers = 2 }

# Resumable CSV.
$done = @{}
if (Test-Path $Csv) {
    Import-Csv $Csv | ForEach-Object { $done["$($_.phase)|$($_.seed)"] = $_ }
    Write-Host "Resuming: $($done.Count) cell(s) already in $Csv"
} else {
    "phase,seed,epochs,slot,hash,elo,pm,id" | Out-File -FilePath $Csv -Encoding ascii
}

$script:slotCycle = 81
function NextSlot { $s = $script:slotCycle; $script:slotCycle++; if ($script:slotCycle -gt 92) { $script:slotCycle = 81 }; $s }
function SlotHash([int]$slot) {
    $o = & $RankExe check 2>&1
    $line = $o | Select-String ("models/sweep/slot" + $slot + "\.txt = ([0-9a-f]+) \(slot " + $slot + "\)")
    if (-not $line) { return $null }
    $line.Matches[0].Groups[1].Value
}

# --- Phase 1: generate asymmetric-opener oracle data (sharded) ---
if (Test-Path $DataAsym) {
    Write-Host "[gen] $DataAsym exists, skipping (delete to regenerate)"
} else {
    $w = [Math]::Max(1, [Math]::Min($Workers, [int][Math]::Floor($GamesOracle / 4)))
    Write-Host "[gen] asymmetric oracle data: $GamesOracle games across $w worker(s) -> $DataAsym"
    $exe = (Resolve-Path $RankExe).Path; $cwd = (Get-Location).Path; $procs = @()
    for ($i = 0; $i -lt $w; $i++) {
        $a = @("pairgen", "--a", $OracleId, "--b", $Champion, "--seed", "104",
               "--open-plies", "$OpenPlies", "--open-side", "a",
               "--games", "$GamesOracle", "--out", "$DataAsym.$i", "--shard", "$i", "--of", "$w")
        $p = Start-Process -FilePath $exe -ArgumentList $a -WorkingDirectory $cwd -NoNewWindow -PassThru -RedirectStandardOutput "$DataAsym.$i.log"
        $null = $p.Handle; $procs += ,@($p, $i)
    }
    foreach ($pi in $procs) {
        $pi[0].WaitForExit()
        if ($pi[0].ExitCode -ne 0) { Get-Content "$DataAsym.$($pi[1]).log" -Tail 5 | ForEach-Object { Write-Host "    $_" }; Write-Error "shard $($pi[1]) failed"; exit 1 }
    }
    $fs = [System.IO.File]::Create((Join-Path $cwd $DataAsym))
    for ($i = 0; $i -lt $w; $i++) { $s = [System.IO.File]::OpenRead((Join-Path $cwd "$DataAsym.$i")); $s.CopyTo($fs); $s.Close() }
    $fs.Close()
    $tally = $null
    for ($i = 0; $i -lt $w; $i++) {
        $m = Get-Content "$DataAsym.$i.meta.json" -Raw | ConvertFrom-Json
        if ($null -eq $tally) { $tally = $m } else { foreach ($f in "played","kept","a_wins","b_wins","draws","positions") { $tally.$f += $m.$f } }
        Remove-Item "$DataAsym.$i", "$DataAsym.$i.meta.json", "$DataAsym.$i.log" -Force -ErrorAction SilentlyContinue
    }
    $tally.shard = 0; $tally.of = 1
    ($tally | ConvertTo-Json -Compress) | Out-File "$DataAsym.meta.json" -Encoding ascii
    Write-Host "[gen] played $($tally.played), oracle record $($tally.a_wins)-$($tally.b_wins)-$($tally.draws), $($tally.positions) positions"
}

# --- Phase 2: train + screen each seed replica ---
Write-Host "`n=== Retrain oracle arm on asymmetric-opener data ==="
foreach ($seed in $Seeds) {
    $key = "oracle-asym|$seed"
    if ($done.ContainsKey($key)) { Write-Host "  [skip] seed $seed done (Elo $($done[$key].elo))"; continue }
    $s = NextSlot
    $out = "models/sweep/slot$s"
    Write-Host "  train: seed=$seed -> slot $s"
    & $TrainExe selfplay-supervised --out $out --epochs $Epochs --seed $seed --feature-version 2 --lr $Lr --from-data $DataAsym --games 0 2>&1 |
        Select-Object -Last 3 | ForEach-Object { Write-Host "    $_" }
    if (-not (Test-Path "$out.txt")) { Write-Warning "  no model produced"; continue }
    $hash = SlotHash $s
    if (-not $hash) { Write-Warning "  no hash for slot $s"; continue }
    $id = "$Wrapper.learned(s$s,$hash)@1"
    Write-Host "  gauntlet (d4): $id"
    $g = & $RankExe gauntlet --id $id --games $GauntletGames --seed $seed 2>&1
    $line = $g | Select-String 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)'
    if (-not $line) { Write-Warning "  gauntlet reported no Elo"; continue }
    $elo = [int]$line.Matches[0].Groups[1].Value; $pm = [int]$line.Matches[0].Groups[2].Value
    Write-Host "  -> screen Elo $elo +/- $pm"
    Copy-Item "$out.txt" "models/sweep/vsc_oracle-asym_$seed.txt" -Force
    "oracle-asym,$seed,$Epochs,$s,$hash,$elo,$pm,$id" | Add-Content -Path $Csv -Encoding Ascii
    $done[$key] = [PSCustomObject]@{ phase="oracle-asym"; seed=$seed; epochs=$Epochs; slot=$s; hash=$hash; elo=$elo; pm=$pm; id=$id }
}

# --- Phase 3: d6-confirm the best seed ---
$screenRows = $done.Values | Where-Object { $_.phase -eq "oracle-asym" }
if (-not $screenRows) { Write-Warning "no screened cells; stopping before d6 confirm"; exit 1 }
$best = $screenRows | Sort-Object { [int]$_.elo } -Descending | Select-Object -First 1
$screenMean = [math]::Round((($screenRows | ForEach-Object { [int]$_.elo } | Measure-Object -Average).Average), 0)

$dkey = "d6-oracle-asym|$($best.seed)"
if ($done.ContainsKey($dkey)) {
    $d6 = $done[$dkey]
    Write-Host "`n[skip] d6 confirm already done (Elo $($d6.elo))"
} else {
    Copy-Item "models/sweep/vsc_oracle-asym_$($best.seed).txt" "models/sweep/slot93.txt" -Force   # 93 = confirm scratch
    $h = SlotHash 93
    $d6id = "$D6Wrapper.learned(s93,$h)@1"
    Write-Host "`n=== d6 confirm (best seed $($best.seed), screen Elo $($best.elo)) ==="
    $g = & $RankExe gauntlet --id $d6id --games $GauntletGames --seed 42 2>&1
    $line = $g | Select-String 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)'
    if ($line) {
        $e = [int]$line.Matches[0].Groups[1].Value; $p = [int]$line.Matches[0].Groups[2].Value
        Write-Host "  d6 confirm: Elo $e +/- $p"
        "d6-oracle-asym,$($best.seed),$Epochs,93,$h,$e,$p,$d6id" | Add-Content -Path $Csv -Encoding Ascii
        $done[$dkey] = [PSCustomObject]@{ phase="d6-oracle-asym"; seed=$best.seed; elo=$e; pm=$p; id=$d6id }
        $d6 = $done[$dkey]
    } else { Write-Warning "  d6 confirm produced no Elo" }
}

# --- Comparison ---
Write-Host "`n=== Asymmetric vs symmetric oracle arm ==="
Write-Host ("  screening (d4):  symmetric mean {0}  ({1})  ->  asymmetric mean {2}  ({3})" -f `
    $BaseScreenMean, ($BaseScreen -join "/"), $screenMean, (($screenRows | ForEach-Object { $_.elo }) -join "/"))
if ($done.ContainsKey($dkey)) {
    Write-Host ("  d6 confirm:      symmetric {0}  ->  asymmetric {1}" -f $BaseD6, $done[$dkey].elo)
    $delta = [int]$done[$dkey].elo - $BaseD6
    Write-Host ("  d6 delta:        {0:+0;-0} Elo" -f $delta)
    if ($delta -lt -50) {
        Write-Host "  NOTE: a large drop here is NOT on its own evidence that the original number was"
        Write-Host "  opener-inflated -- it is confounded with training-data label skew (the asymmetric"
        Write-Host "  recipe removes 6 plies of the champion's own randomness, which changes the win/loss"
        Write-Host "  ratio in the generated data, a separately-documented failure mode for this project's"
        Write-Host "  linear value models). Compare the oracle win rate in the two .meta.json files before"
        Write-Host "  concluding anything; see plans/opener-bias-results-1-synchronous-stearns.md Layer 3."
    } else {
        Write-Host "  no meaningful drop: oracle result robust to the opener"
    }
}
