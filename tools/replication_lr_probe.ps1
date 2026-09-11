# Replication study Stage 1, Pass 2: the learning-rate probe
# (plans/replication-study-plan-1-brass-lectern.md, "Hyperparameter fairness").
#
# Every arm trains at every half-decade learning rate from 1e-8 to 1, one
# calibration seed, 50 games at the study head, checkpoints every 10 games.
# Per arm:
#   D = lowest rate whose weights exceed max |w| $DivergeW at any checkpoint
#   L = lowest rate whose final weights move a mean of $MoveFloor from the
#       initialization (the 1e-8 run's rung 10 stands in for the init)
#   tuning range = [D / 10^2.5, D / 10^0.5]
# Weight-based only: the probe needs no panel and rates nothing.
#
# Phases: train, analyze, all. Models under models/sweep/rep1_lrprobe/ (never
# published to slots), per-checkpoint rows in models/sweep/replication_lr_probe.csv.

param(
    [string]$Phase = "all",
    [int]$Workers = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [int]$Seed = 3001,
    [int]$Games = 50,
    [double]$DivergeW = 5.0,
    [double]$MoveFloor = 0.01
)
$ErrorActionPreference = "Stop"
$Inv = [Globalization.CultureInfo]::InvariantCulture
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Train = Join-Path $Root "train.exe"
$Work = "models/sweep/rep1_lrprobe"
$Csv = "models/sweep/replication_lr_probe.csv"
$Summary = "$Work/probe_summary.txt"
New-Item -ItemType Directory -Force $Work | Out-Null

$HeadFlags = @("--depth", "12", "--node-budget", "100000", "--rem", "70", "--retain", "--open-plies", "4", "--report-every", "0")
$Rungs = @(10, 20, 30, 40, 50) | Where-Object { $_ -le $Games }
$Exps = @(); for ($e = -16; $e -le 0; $e++) { $Exps += $e / 2.0 }   # 10^-8 .. 10^0 in half decades

# Everything except the learning rate, which the probe sets.
$Arms = [ordered]@{
    B0 = @()
    A1 = @("--backup", "td-directed")
    A2 = @("--backup", "rootstrap")
    A3 = @("--backup", "treestrap")
    A4 = @("--lambda", "1")
    A5 = @("--terminal", "depth")
    A6 = @("--augment", "mirror")
    A7 = @("--explore", "0.1")
    A8 = @("--explore-dist", "ordinal", "--ordinal-games", "$Games")
}

$out = New-Object System.Collections.Generic.List[string]
function Say([string]$s) { Write-Host $s; $out.Add($s) }
function LrOf([double]$e) { [Math]::Pow(10, $e).ToString("G6", $Inv) }
function Tag([double]$e) { ("e{0}" -f $e.ToString("0.0", $Inv)).Replace("-", "m") }

function Invoke-TrainJobs($jobs) {
    $running = @()
    foreach ($j in $jobs) {
        while (@($running | Where-Object { -not $_.HasExited }).Count -ge $Workers) { Start-Sleep -Milliseconds 500 }
        $p = Start-Process -FilePath $Train -ArgumentList $j.Args -WorkingDirectory $Root -NoNewWindow -PassThru `
            -RedirectStandardOutput $j.Log -RedirectStandardError "$($j.Log).err"
        [void]$p.Handle
        $running += $p
    }
    $running | ForEach-Object { $_.WaitForExit() }
    $bad = @($running | Where-Object { $_.ExitCode -ne 0 })
    if ($bad.Count -gt 0) { throw "$($bad.Count) training process(es) failed, see the .log/.err files under $Work" }
}

function Invoke-Train {
    $jobs = @()
    # Slowest arm first so it does not become the tail.
    foreach ($k in @("A3") + @($Arms.Keys | Where-Object { $_ -ne "A3" })) {
        foreach ($e in $Exps) {
            $o = "$Work/${k}_$(Tag $e)"
            if (Test-Path "${o}_g$($Rungs[-1]).txt") { continue }
            $a = @("tdleaf") + $HeadFlags + @("--seed", "$Seed", "--games", "$Games", "--ckpt-at", ($Rungs -join ","),
                "--lr", (LrOf $e), "--out", $o) + $Arms[$k]
            $jobs += @{ Args = $a; Log = "$o.log" }
        }
    }
    Invoke-TrainJobs $jobs
}

function Read-Weights($path) {
    $w = @(); $b = 0.0
    foreach ($line in Get-Content $path) {
        if ($line -match '^w(\d+)=(.+)$') { $w += [double]::Parse($Matches[2], $Inv) }
        elseif ($line -match '^bias=(.+)$') { $b = [double]::Parse($Matches[1], $Inv) }
    }
    return @{ W = $w; Bias = $b }
}

function Invoke-Analyze {
    "arm,exp,lr,rung,max_abs_w,bias,mean_move,cpu_s" | Out-File -FilePath $Csv -Encoding ascii
    Say "== learning-rate probe: seed $Seed, $Games games, head ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3, scratch init"
    Say "   diverged = max |w| > $DivergeW at any checkpoint, moved = mean |w - init| >= $MoveFloor at the last checkpoint"
    Say ""
    $sumRows = @()
    foreach ($k in $Arms.Keys) {
        $init = Read-Weights "$Work/${k}_$(Tag $Exps[0])_g$($Rungs[0]).txt"
        $D = $null; $L = $null
        Say "$k"
        Say ("  {0,10} {1,12} {2,10} {3,12} {4,9}  {5}" -f "lr", "max|w| last", "bias last", "mean move", "cpu s", "max|w| by rung")
        foreach ($e in $Exps) {
            $byRung = @(); $div = $false; $last = $null; $cpu = 0
            foreach ($r in $Rungs) {
                $f = "$Work/${k}_$(Tag $e)_g$r.txt"
                if (-not (Test-Path $f)) { continue }
                $m = Read-Weights $f
                $mx = ($m.W | ForEach-Object { [Math]::Abs($_) } | Measure-Object -Maximum).Maximum
                $mv = 0.0; for ($i = 0; $i -lt $m.W.Count; $i++) { $mv += [Math]::Abs($m.W[$i] - $init.W[$i]) }; $mv /= [Math]::Max(1, $m.W.Count)
                $t = (Get-Content $f | Where-Object { $_ -like "teacher=*" } | Select-Object -First 1)
                $cpu = if ($t -match ',cpu=([0-9.eE+-]+)') { [double]::Parse($Matches[1], $Inv) } else { -1 }
                if ($mx -gt $DivergeW) { $div = $true }
                $byRung += $mx.ToString("0.###", $Inv)
                $last = @{ Max = $mx; Bias = $m.Bias; Move = $mv }
                "$k,$e,$(LrOf $e),$r,$mx,$($m.Bias),$mv,$cpu" | Add-Content -Path $Csv -Encoding ascii
            }
            if ($null -eq $last) { continue }
            if ($div -and $null -eq $D) { $D = $e }
            if (-not $div -and $last.Move -ge $MoveFloor -and $null -eq $L) { $L = $e }
            Say ("  {0,10} {1,12:N4} {2,10:N4} {3,12:N5} {4,9:N1}  {5}{6}" -f (LrOf $e), $last.Max, $last.Bias, $last.Move, $cpu, ($byRung -join " "), $(if ($div) { "  DIVERGED" } else { "" }))
        }
        $sumRows += @{ Arm = $k; D = $D; L = $L }
        Say ""
    }
    Say "== per-arm ranges: [D / 10^2.5, D / 10^0.5]"
    Say ("{0,-4} {1,10} {2,10} {3,12} {4,12}  {5}" -f "arm", "L", "D", "range low", "range high", "note")
    foreach ($s in $sumRows) {
        $Ls = if ($null -ne $s.L) { LrOf $s.L } else { "none" }
        if ($null -eq $s.D) { Say ("{0,-4} {1,10} {2,10} {3,12} {4,12}  {5}" -f $s.Arm, $Ls, "none", "", "", "no divergence up to 1"); continue }
        $lo = $s.D - 2.5; $hi = $s.D - 0.5
        $note = if ($null -eq $s.L) { "nothing moved the weights" } elseif ($lo -lt $s.L) { "range starts below L" } else { "" }
        Say ("{0,-4} {1,10} {2,10} {3,12} {4,12}  {5}" -f $s.Arm, $Ls, (LrOf $s.D), (LrOf $lo), (LrOf $hi), $note)
    }
}

if ($Phase -in @("train", "all")) { Invoke-Train }
if ($Phase -in @("analyze", "all")) { Invoke-Analyze }
$out | Out-File -FilePath $Summary -Encoding utf8
Write-Host "summary: $Summary"
