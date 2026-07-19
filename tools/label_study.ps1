# label_study.ps1 - Multi-day position-oracle labeling campaign orchestrator.
#
# Drives the full pipeline: prep (rate the strong diluted-d8 ladder rungs and
# freeze the ratings snapshot), posgen (position pools), label-train and
# label-eval (sharded fresh-game playouts per position, chunked and resumable),
# fit (per-position mu/sigma labels), train (the dist-model configs), eval
# (dist-eval vs the calibrated d8 oracle baseline), rate (roster Elo for the
# shipped models). Every phase records done cells in a CSV ledger and returns
# early on re-run, so the study survives interruption at any point; the label
# phases additionally chunk their work so at most one chunk is in flight.
#
# Usage (from project root; build rank.exe and train.exe first):
#   .\tools\label_study.ps1 -DryRun          # tiny end-to-end pipeline check (~minutes)
#   .\tools\label_study.ps1 -Workers 12      # the real campaign (about 2 wall-days)
#   .\tools\label_study.ps1 -Phases "train","eval"   # rerun later phases only
#
# The ladder spec files (data/labels/ladder_train.txt, ladder_eval.txt) are
# written once with the default design and are hand-editable afterwards; rung
# Elo comments are informational, the authoritative Elos come from the frozen
# snapshot at fit/train time.

param(
    [int]$Workers = 12,
    [string[]]$Phases = @("prep","posgen","label-train","label-eval","fit","train","eval","rate"),
    [switch]$DryRun,
    [int]$Seed = 20260718,
    [int]$ChunkPositions = 2000,
    [string]$Csv = ""
)

$ErrorActionPreference = "Stop"
if (-not (Test-Path ".\rank.exe"))  { Write-Error "rank.exe not found. Build it: .\build_rank.bat"; exit 1 }
if (-not (Test-Path ".\train.exe")) { Write-Error "train.exe not found. Build it: .\build_train.bat"; exit 1 }

# ---- Study layout (the dry run lives in its own directory) ----
$Dir = "data/labels"
$TrainTarget = 24000; $EvalTarget = 1500
$Epochs = 40; $CalibN = 800; $OracleDepth = 8; $OracleNb = 2000000
if ($DryRun) {
    $Dir = "data/labels/dry"
    $Workers = 1
    $ChunkPositions = 50
    $TrainTarget = 20; $EvalTarget = 6
    $Epochs = 3; $CalibN = 8; $OracleDepth = 2; $OracleNb = 4000
    $Phases = @("prep","posgen","label-train","label-eval","fit","train","eval")
}
if ($Csv -eq "") { $Csv = "$Dir/study.csv" }
New-Item -ItemType Directory -Force $Dir | Out-Null
New-Item -ItemType Directory -Force "$Dir/logs" | Out-Null

$Snapshot    = "$Dir/ratings_snapshot.tsv"
$PoolTrain   = "$Dir/pool_train.jsonl"
$PoolEval    = "$Dir/pool_eval.jsonl"
$LadderTrain = "$Dir/ladder_train.txt"
$LadderEval  = "$Dir/ladder_eval.txt"
$RawTrain    = "$Dir/raw_train.jsonl"
$RawEval     = "$Dir/raw_eval.jsonl"
$LabelsTrain = "$Dir/labels_train.jsonl"
$LabelsEval  = "$Dir/labels_eval.jsonl"

# The two strong stochastic rungs the prep phase adds and rates: depth-diluted
# d8 (15% or 30% of moves fall back to a depth-6 search; the per-move dilution
# roll consumes rand(), so playouts from a fixed position genuinely vary while
# the agent stays near oracle strength). Tie-jitter agents do NOT work here:
# jitter never draws from rand(), so a jitter pairing replays one game.
$PrepRungs = @(
    "ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.dil(r15,d6)@1",
    "ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2.dil(r30,d6)@1"
)

# ---- Ledger ----
$done = @{}
if (Test-Path $Csv) {
    foreach ($line in Get-Content $Csv) {
        $c = $line -split ","
        if ($c.Count -ge 4 -and $c[3] -eq "done") { $done["$($c[0])|$($c[1])"] = $true }
    }
}
function Is-Done([string]$phase, [string]$cell) { return $done.ContainsKey("$phase|$cell") }
function Mark-Done([string]$phase, [string]$cell, [string]$detail) {
    $stamp = [DateTime]::UtcNow.ToString("yyyyMMddTHHmmssZ")
    Add-Content -Path $Csv -Value "$phase,$cell,$detail,done,$stamp"
    $done["$phase|$cell"] = $true
}
function Run-Tool([string]$exe, [string[]]$toolArgs, [string]$log) {
    Write-Host ">> $exe $($toolArgs -join ' ')"
    # No stderr redirect: the exes are stdout-only, and native 2>&1 under
    # ErrorActionPreference Stop turns any stderr line into a terminating error.
    & $exe @toolArgs | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0) { Write-Error "$exe failed (exit $LASTEXITCODE), log: $log"; exit 1 }
}

# ---- Default ladder specs (written once, hand-editable afterwards) ----
function Write-Ladders {
    if ($DryRun) {
        if (-not (Test-Path $LadderTrain)) {
            @(
                "rung 0 ab(d2)@1.classic(t1,c4,w0,l0)@2",
                "rung 1 ab(d2)@1.classic(t1,c4,w0,l0)@2.dil(r25)@1",
                "pair 1 0 4",
                "pair 0 1 4",
                "pair 1 1 2 mod 2 0"
            ) | Set-Content -Encoding Ascii $LadderTrain
        }
        if (-not (Test-Path $LadderEval)) {
            @(
                "rung 0 ab(d2)@1.classic(t1,c4,w0,l0)@2",
                "rung 1 ab(d2)@1.classic(t1,c4,w0,l0)@2.dil(r25)@1",
                "pair 1 0 6",
                "pair 0 1 6",
                "pair 1 1 4"
            ) | Set-Content -Encoding Ascii $LadderEval
        }
        # Scratch ratings for the dry rungs (fabricated Elos; the point is the
        # pipeline, not the numbers).
        if (-not (Test-Path $Snapshot)) {
            @(
                "rank`telo`tpm`tgames`tid",
                "1`t500`t15`t100`tab(d2)@1.classic(t1,c4,w0,l0)@2",
                "2`t350`t18`t100`tab(d2)@1.classic(t1,c4,w0,l0)@2.dil(r25)@1"
            ) | Set-Content -Encoding Ascii $Snapshot
        }
        return
    }
    $H6 = "ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2"
    if (-not (Test-Path $LadderTrain)) {
        @(
            "# Train-tier labeling ladder: 8 pairings x 8 games + a d8 premium pair on hash%4==0.",
            "# Rung Elo comments are from the fit at design time; the frozen snapshot is authoritative.",
            "rung 0 $H6.dil(r15,d4)@1    # ~910",
            "rung 1 $H6.dil(r10)@1       # ~758",
            "rung 2 $H6.dil(r18)@1       # ~616",
            "rung 3 $H6.dil(r22)@1       # ~566",
            "rung 4 $H6.dil(r35)@1       # ~377",
            "rung 5 $($PrepRungs[0])     # rated in prep",
            "rung 6 $($PrepRungs[1])     # rated in prep",
            "pair 0 0 8",
            "pair 0 1 8",
            "pair 1 0 8",
            "pair 0 2 8",
            "pair 2 0 8",
            "pair 5 3 8",
            "pair 3 5 8",
            "pair 5 0 8",
            "pair 5 6 2 mod 4 0",
            "pair 6 5 2 mod 4 0"
        ) | Set-Content -Encoding Ascii $LadderTrain
    }
    if (-not (Test-Path $LadderEval)) {
        @(
            "# Eval-tier ladder: a dense gap grid, both color orders, d8 premium pairs on ALL positions.",
            "rung 0 $H6.dil(r15,d4)@1    # ~910",
            "rung 1 $H6.dil(r10)@1       # ~758",
            "rung 2 $H6.dil(r18)@1       # ~616",
            "rung 3 $H6.dil(r22)@1       # ~566",
            "rung 4 $H6.dil(r35)@1       # ~377",
            "rung 5 $($PrepRungs[0])     # rated in prep",
            "rung 6 $($PrepRungs[1])     # rated in prep",
            "pair 0 0 40",
            "pair 5 5 40",
            "pair 0 1 40",
            "pair 1 0 40",
            "pair 1 2 40",
            "pair 2 1 40",
            "pair 0 2 40",
            "pair 2 0 40",
            "pair 5 1 40",
            "pair 1 5 40",
            "pair 0 3 40",
            "pair 3 0 40",
            "pair 5 2 40",
            "pair 2 5 40",
            "pair 0 4 40",
            "pair 4 0 40",
            "pair 5 4 40",
            "pair 4 5 40",
            "pair 6 0 40",
            "pair 0 6 40",
            "pair 5 6 12",
            "pair 6 5 12"
        ) | Set-Content -Encoding Ascii $LadderEval
    }
}

# ---- Phase: prep ----
function Phase-Prep {
    if ($DryRun) { Write-Ladders; return }
    if (-not (Is-Done "prep" "rungs-rated")) {
        $roster = Get-Content "ranking/roster.txt" -Raw
        $added = $false
        foreach ($r in $PrepRungs) {
            if ($roster -notmatch [regex]::Escape($r)) {
                Add-Content "ranking/roster.txt" "on $r"
                Write-Host "prep: appended roster rung $r"
                $added = $true
            }
        }
        Run-Tool ".\rank.exe" @("check") "$Dir/logs/prep_check.log"
        Write-Host "prep: playing the new rungs into the pool (run_rank -Workers $Workers)..."
        & .\tools\run_rank.ps1 -Workers $Workers
        if ($LASTEXITCODE -ne 0) { Write-Error "run_rank failed during prep"; exit 1 }
        Mark-Done "prep" "rungs-rated" "$($PrepRungs.Count) rungs"
    }
    if (-not (Is-Done "prep" "snapshot")) {
        if (-not (Test-Path $Snapshot)) {
            Copy-Item "ranking/ratings.tsv" $Snapshot
            Write-Host "prep: froze $Snapshot (the study's fixed Elo basis; never overwritten)"
        } else {
            Write-Host "prep: $Snapshot already exists, keeping the frozen basis"
        }
        # Sanity: every ladder rung must be rated in the snapshot.
        $snap = Get-Content $Snapshot -Raw
        foreach ($r in $PrepRungs) {
            if ($snap -notmatch [regex]::Escape($r)) { Write-Error "prep: $r is not in $Snapshot; top up games and refit before freezing"; exit 1 }
        }
        Mark-Done "prep" "snapshot" $Snapshot
    }
    Write-Ladders
}

# ---- Phase: posgen ----
function Phase-Posgen {
    if (Is-Done "posgen" "pools") { Write-Host "posgen: already done"; return }
    Run-Tool ".\rank.exe" @("posgen", "--out-train", $PoolTrain, "--out-eval", $PoolEval,
                            "--train", $TrainTarget, "--eval", $EvalTarget,
                            "--seed", $Seed) "$Dir/logs/posgen.log"
    Mark-Done "posgen" "pools" "$TrainTarget train / $EvalTarget eval"
}

# ---- Phase: label (shared by both tiers) ----
function Phase-Label([string]$phase, [string]$pool, [string]$ladder, [string]$master) {
    $chunk = 0
    while ($true) {
        $chunk++
        if ($chunk -gt 500) { Write-Error "$phase runaway chunk loop"; exit 1 }
        if (Is-Done $phase "chunk$chunk") { continue }
        $procs = @(); $shardFiles = @(); $metaFiles = @()
        for ($s = 0; $s -lt $Workers; $s++) {
            $sf = "$master.s$s"
            if (Test-Path $sf) { Remove-Item $sf -Force }
            $shardFiles += $sf
            $metaFiles += "$sf.meta.json"
            $log = "$Dir/logs/$phase.c$chunk.s$s.log"
            $shardArgs = @("label", "--pool", $pool, "--ladder", $ladder, "--out", $sf,
                           "--seed", $Seed, "--shard", $s, "--of", $Workers,
                           "--resume", "--done", $master, "--max-positions", $ChunkPositions)
            $procs += Start-Process -FilePath ".\rank.exe" -ArgumentList $shardArgs `
                        -RedirectStandardOutput $log -PassThru -WindowStyle Hidden
        }
        Write-Host "$phase chunk ${chunk}: $($procs.Count) shards running..."
        $procs | Wait-Process
        foreach ($p in $procs) { if ($p.ExitCode -ne 0) { Write-Error "$phase chunk $chunk shard failed (exit $($p.ExitCode)); shard files kept for inspection"; exit 1 } }
        # Merge shard rows into the master, keep shard 0's meta as the master's
        # rung-id mapping (identical across shards of one design).
        $touched = 0
        foreach ($mf in $metaFiles) {
            if (Test-Path $mf) {
                $m = Get-Content $mf -Raw
                if ($m -match '"positions_touched":(\d+)') { $touched += [int]$Matches[1] }
            }
        }
        $existing = $shardFiles | Where-Object { (Test-Path $_) -and ((Get-Item $_).Length -gt 0) }
        if ($existing) { Get-Content $existing | Add-Content $master }
        if (Test-Path $metaFiles[0]) { Copy-Item $metaFiles[0] "$master.meta.json" -Force }
        Remove-Item $shardFiles -Force -ErrorAction SilentlyContinue
        Remove-Item $metaFiles -Force -ErrorAction SilentlyContinue
        Mark-Done $phase "chunk$chunk" "touched=$touched"
        if ($touched -eq 0) { Write-Host "$phase complete after $chunk chunks."; break }
    }
}

# ---- Phase: fit ----
function Phase-Fit {
    if (-not (Is-Done "fit" "train")) {
        Run-Tool ".\rank.exe" @("labelfit", "--in", $RawTrain, "--pool", $PoolTrain,
                                "--ratings", $Snapshot, "--out", $LabelsTrain, "--min-rows", 8) "$Dir/logs/fit_train.log"
        Mark-Done "fit" "train" $LabelsTrain
    }
    if (-not (Is-Done "fit" "eval")) {
        Run-Tool ".\rank.exe" @("labelfit", "--in", $RawEval, "--pool", $PoolEval,
                                "--ratings", $Snapshot, "--out", $LabelsEval, "--min-rows", 8) "$Dir/logs/fit_eval.log"
        Mark-Done "fit" "eval" $LabelsEval
    }
}

# ---- Phase: train ----
function Get-TrainConfigs {
    if ($DryRun) {
        return @(
            @{ Name = "lin"; Out = "$Dir/dist_lin"; Extra = @("--mu-type","linear","--s-type","linear","--seed","1001") }
        )
    }
    return @(
        @{ Name = "lin";       Out = "models/dist_lin";       Extra = @("--mu-type","linear","--s-type","linear","--seed","1001") },
        @{ Name = "mlp_s1001"; Out = "models/dist_mlp_s1001"; Extra = @("--mu-type","mlp","--mu-hidden","128,64","--s-type","mlp","--s-hidden","32","--seed","1001") },
        @{ Name = "mlp_s2002"; Out = "models/dist_mlp_s2002"; Extra = @("--mu-type","mlp","--mu-hidden","128,64","--s-type","mlp","--s-hidden","32","--seed","2002") },
        @{ Name = "mlp_wide";  Out = "models/dist_mlp_wide";  Extra = @("--mu-type","mlp","--mu-hidden","256,128","--s-type","mlp","--s-hidden","64","--seed","3003") }
    )
}
function Phase-Train {
    $configs = Get-TrainConfigs
    $procs = @()
    foreach ($c in $configs) {
        if (Is-Done "train" $c.Name) { Write-Host "train: $($c.Name) already done"; continue }
        $log = "$Dir/logs/train_$($c.Name).log"
        $trainArgs = @("dist-value", "--raw", $RawTrain, "--pool", $PoolTrain,
                       "--ratings", $Snapshot, "--out", $c.Out,
                       "--epochs", $Epochs, "--lr", "0.02", "--lr-sigma", "0.004",
                       "--val-split", "0.1", "--early-stop", "--ckpt-every", 5) + $c.Extra
        Write-Host "train: launching $($c.Name) -> $($c.Out).txt (log: $log)"
        $procs += @{ Proc = (Start-Process -FilePath ".\train.exe" -ArgumentList $trainArgs `
                              -RedirectStandardOutput $log -PassThru -WindowStyle Hidden); Cfg = $c }
    }
    foreach ($e in $procs) {
        $e.Proc | Wait-Process
        if ($e.Proc.ExitCode -ne 0) { Write-Error "train: $($e.Cfg.Name) failed, log: $Dir/logs/train_$($e.Cfg.Name).log"; exit 1 }
        Mark-Done "train" $e.Cfg.Name "$($e.Cfg.Out).txt"
    }
}

# ---- Phase: eval ----
function Phase-Eval {
    foreach ($c in Get-TrainConfigs) {
        if (Is-Done "eval" $c.Name) { Write-Host "eval: $($c.Name) already done"; continue }
        $log = "$Dir/logs/eval_$($c.Name).log"
        Run-Tool ".\train.exe" @("dist-eval", "--model", "$($c.Out).txt",
                                 "--labels-eval", $LabelsEval, "--raw-eval", $RawEval,
                                 "--pool-eval", $PoolEval, "--ratings", $Snapshot,
                                 "--labels-train", $LabelsTrain, "--raw-train", $RawTrain,
                                 "--calib", $CalibN, "--oracle-depth", $OracleDepth,
                                 "--oracle-nb", $OracleNb) $log
        Mark-Done "eval" $c.Name $log
    }
}

# ---- Phase: rate (roster Elo for the shipped models; slots 76..79) ----
function Phase-Rate {
    if ($DryRun) { return }
    if (Is-Done "rate" "roster") { Write-Host "rate: already done"; return }
    Copy-Item "models/dist_lin.txt"       "models/sweep/slot76.txt" -Force
    Copy-Item "models/dist_mlp_s1001.txt" "models/sweep/slot77.txt" -Force
    Copy-Item "models/dist_mlp_s2002.txt" "models/sweep/slot78.txt" -Force
    $check = & .\rank.exe check
    $lines = @()
    foreach ($slot in 76, 77, 78) {
        $hash = ""
        foreach ($l in $check) {
            if ($l -match "slot$slot\.txt = ([0-9a-f]{8}) \(slot $slot\)") { $hash = $Matches[1] }
        }
        if ($hash -eq "") { Write-Error "rate: no model hash for slot $slot in rank.exe check output"; exit 1 }
        if ($slot -eq 76) {
            # The linear variant keeps the incremental leaf: both standard heads.
            $lines += "on ab(d4)@1.learned(s$slot,$hash)@2"
            $lines += "on ab(d6,tt,ord,nb200k)@1.learned(s$slot,$hash)@2"
        } else {
            # MLP mu heads full-scan every leaf; a d6/nb200k head would cost
            # seconds per move, so the mlp variants rate at the d4 head only.
            $lines += "on ab(d4)@1.learned(s$slot,$hash)@2"
        }
    }
    $roster = Get-Content "ranking/roster.txt" -Raw
    foreach ($l in $lines) {
        $id = $l.Substring(3)
        if ($roster -notmatch [regex]::Escape($id)) { Add-Content "ranking/roster.txt" $l; Write-Host "rate: appended $l" }
    }
    & .\tools\run_rank.ps1 -Workers $Workers
    if ($LASTEXITCODE -ne 0) { Write-Error "rate: run_rank failed"; exit 1 }
    Mark-Done "rate" "roster" ($lines -join ";")
}

# ---- Main ----
Write-Host "=== label_study ($(if ($DryRun) { 'DRY RUN' } else { 'campaign' })): phases = $($Phases -join ', ') ==="
foreach ($ph in $Phases) {
    Write-Host ""
    Write-Host "=== Phase: $ph ==="
    switch ($ph) {
        "prep"        { Phase-Prep }
        "posgen"      { Phase-Posgen }
        "label-train" { Phase-Label "label-train" $PoolTrain $LadderTrain $RawTrain }
        "label-eval"  { Phase-Label "label-eval"  $PoolEval  $LadderEval  $RawEval }
        "fit"         { Phase-Fit }
        "train"       { Phase-Train }
        "eval"        { Phase-Eval }
        "rate"        { Phase-Rate }
        default       { Write-Error "unknown phase $ph"; exit 1 }
    }
}
Write-Host ""
Write-Host "=== label_study finished ==="
