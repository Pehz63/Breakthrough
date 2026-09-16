# Replication study Stage 1, Pass 2 (calibration): plans/replication-study-plan-1-brass-lectern.md.
#
# Three rated steps, each against a frozen reference panel:
#   curve  B0 and every arm, 1 seed, on a generous game ladder, at the middle
#          of the arm's locked learning-rate range. Where the curves flatten
#          places T_max, and each checkpoint's cpu= stamp gives the arm's CPU
#          seconds per game (Pass 2's cost output).
#   noise  several seeds of B0 and of one contrasting arm at the candidate
#          T_max (-NoiseCpu, matched compute), every checkpoint on the way
#          rated: the spread over seeds is sigma_seed, the fit's pm sigma_meas.
#   tune   the equal-budget random search. Every arm gets the same draws over
#          its own locked range. Arms with an arm-specific hyperparameter
#          (lambda for B0 and A1, d_min for A3, epsilon for A7, the ordinal
#          start e for A8) get the same budget again, drawn jointly with the
#          rate. A5 to A8 differ from B0 in one switch, so they train at B0's
#          tuned lambda (-BaseLambda).
#          Draws are matched by PRICED SECONDS, per draw: A3's cost per game
#          depends on d_min, so each A3 draw's game count comes from the priced
#          curve ladder of its own d_min (arm names A3d2, A3d4, A3d8, which
#          -Step curve trains). -WidenArms adds -WidenDraws more draws in the
#          -WidenUp decades above an arm's locked range, for an arm whose best
#          draw sat at the top of it.
#   cost   prices each arm's training work from counts that do not depend on
#          machine load. Every job resumes a published curve checkpoint for a
#          few games with exactly -CostSlots trainings running (filler runs
#          hold the count while the queue drains). The fit gives each arm CPU
#          microseconds per search node, plus per TreeStrap probe move for A3,
#          in cost_prices.tsv. -NoiseCpu and -TuneCpu are priced seconds.
#
# Rating. Every rated checkpoint is published to a model slot and plays ONLY
# the panel: one rank.exe process per agent, each writing its own part file
# under ranking/<store stem>/, listed in <store stem>.index.txt, with paired
# common openings (every agent meets a panel opponent on the same opening
# couples). No study agent meets another, so one pinned fit over the whole
# study store gives each agent the rating it would get alone. The panel file
# is a roster (anchor/on lines) and every panel id must be in -PanelPin.
#
# Phases: train, publish, play, rate, report, all. Everything resumes: a
# finished run is not retrained, a published checkpoint keeps its slot, and
# play is deficit-scheduled. Slots come from the study's block (1858..3857),
# recorded in -Ledger, and a slot file the ledger does not list is never
# overwritten.
#
# -CurveTag names a second curve ladder under its own run keys (for example a
# longer ladder, "-CurveTag _T10240 -CurveRungs 5120,10240"), so it neither
# collides with the published ladder nor retrains it. The curve report gives
# each ladder its own column.
#
# cpu= stamps are load-sensitive: on the 6-core, 12-thread dev machine the same
# 20 games (identical weights and node counts) cost 18 to 22% fewer CPU seconds
# with 4 training processes running than with 9. Compare CPU across runs only
# when they ran at the same concurrency.

param(
    [Parameter(Mandatory = $true)][ValidateSet("curve", "noise", "tune", "cost")][string]$Step,
    [ValidateSet("train", "publish", "play", "rate", "report", "all")][string]$Phase = "all",
    [string]$Panel = "",
    [string]$PanelPin = "ranking/standings.tsv",
    [string]$Opener = ".opener(rand,moves=8)@1",
    [string]$Store = "ranking/matches_rep1.jsonl",
    [string]$Work = "models/sweep/rep1_p2",
    [string]$Ledger = "models/sweep/replication_stage1_pass2.csv",
    [int]$SlotBase = 1876,
    [int]$SlotMax = 3857,
    [int]$Workers = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [int]$GamesPerPair = 32,
    [string]$Arms = "B0,A1,A2,A3,A4,A5,A6,A7,A8",
    [string]$CurveRungs = "10,20,40,80,160,320,640,1280,2560,5120",
    [int]$CurveSeed = 4001,
    [string]$CurveTag = "",
    [string]$TrainExe = "train.exe",
    [int]$CostSlots = 6,
    [int]$CostReps = 2,
    [string]$CostFrom = "0,80,640,1280,2560,5120",
    [ValidateSet("shared", "perarm")][string]$CostPricing = "shared",
    [switch]$CostAllowBusy,   # stand-in tests only: prices measured beside other trainings are not load-free
    [string]$NoiseArms = "",
    [int]$NoiseSeedBase = 4101,
    [int]$NoiseSeeds = 5,
    [double]$NoiseCpu = 0,
    [int]$NoiseGames = 0,
    [int]$Draws = 16,
    [int]$DrawSeed = 7,
    [int]$TuneSeedBase = 5001,
    [double]$TuneCpu = 0,
    [int]$TuneGames = 0,
    [double]$BaseLambda = -1,
    [string]$WidenArms = "",
    [double]$WidenUp = 0.5,
    [int]$WidenDraws = 8
)
$ErrorActionPreference = "Stop"
$Inv = [Globalization.CultureInfo]::InvariantCulture
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Train = Join-Path $Root $TrainExe
$Rank = Join-Path $Root "rank.exe"
$StepDir = "$Work/$Step"
foreach ($d in @($Work, $StepDir, "$StepDir/rosters", "$StepDir/play_logs")) { New-Item -ItemType Directory -Force $d | Out-Null }

$HeadFlags = @("--depth", "12", "--node-budget", "100000", "--rem", "70", "--retain", "--open-plies", "4", "--report-every", "0")
$HeadId = "ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3"
$StoreStem = [IO.Path]::GetFileNameWithoutExtension($Store)
$StoreDir = Split-Path -Parent $Store
$PartDir = "$StoreDir/$StoreStem"
$IndexFile = "$StoreDir/$StoreStem.index.txt"
$Suffix = if ($StoreStem.StartsWith("matches") -and $StoreStem.Length -gt 7) { $StoreStem.Substring(7) } else { "" }
$PinnedStandings = "ranking/standings${Suffix}_pinned.tsv"
$ArmList = @($Arms.Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ })

# Locked learning-rate ranges (results doc, Pass 2 probe): the low end's
# decimal exponent. Every range is 2 decades wide, [D / 10^2.5, D / 10^0.5].
$RangeLo = @{ B0 = -3; A1 = -2.5; A2 = -3; A3 = -6.5; A4 = -4; A5 = -2.5; A6 = -3; A7 = -2.5; A8 = -2.5 }
$Width = 2.0
# Each arm's one switch against B0. A7's epsilon and A8's schedule are set per run.
$ArmSwitch = @{
    B0 = @(); A1 = @("--backup", "td-directed"); A2 = @("--backup", "rootstrap"); A3 = @("--backup", "treestrap")
    A4 = @("--lambda", "1"); A5 = @("--terminal", "depth"); A6 = @("--augment", "mirror")
    A7 = @(); A8 = @("--explore-dist", "ordinal")
}
# Arms whose TD-Leaf backup reads lambda and that differ from B0 in one switch.
$InheritLambda = @("A5", "A6", "A7", "A8")
$DminSet = @(1, 2, 4, 8)
# A3's cost per game falls as d_min rises, so matching draws by priced seconds
# needs one priced curve ladder per d_min. A3dN is A3's recipe at d_min N, an
# arm name the curve step accepts (-Arms A3d2,A3d4,A3d8) and the tune step reads
# to size each A3 draw's game count. d_min 1 is A3's own published ladder.
$VariantBase = @{}; $VariantExtra = @{}
foreach ($d in $DminSet) {
    if ($d -eq 1) { continue }
    $VariantBase["A3d$d"] = "A3"; $VariantExtra["A3d$d"] = @("--tree-min-depth", "$d")
}
function BaseArm([string]$a) { if ($VariantBase.ContainsKey($a)) { return $VariantBase[$a] } return $a }
# The curve ladder that prices arm $a's games, at hyperparameter value $hv.
function LadderOf([string]$a, $hv) { if ($a -eq "A3" -and "$hv" -ne "" -and [int]"$hv" -ne 1) { return "A3d$hv" } return $a }
$WidenList = @($WidenArms.Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ })
function RangeWidth([string]$a) { if ($WidenList -contains $a) { return $Width + $WidenUp } return $Width }

$out = New-Object System.Collections.Generic.List[string]
function Say([string]$s) { Write-Host $s; $out.Add($s) }
function F([double]$x) { $x.ToString("G4", $Inv) }
function Lr([double]$e) { [Math]::Pow(10, $e).ToString("G6", $Inv) }

Add-Type -TypeDefinition @"
public static class RepFnv {
    public static string Hash8(byte[] b) {
        ulong h = 1469598103934665603UL;
        foreach (byte x in b) { h ^= x; h *= 1099511628211UL; }
        return h.ToString("x16").Substring(0, 8);
    }
}
"@

function Teacher($path) { (Get-Content $path | Where-Object { $_ -like "teacher=*" } | Select-Object -First 1) }
function StampNum($t, $key) { if ($t -match "(?:^|[,(])$key=([0-9.eE+-]+)") { return [double]::Parse($Matches[1], $Inv) }; return $null }

# Priced training seconds: a checkpoint's stamped work counts at the prices
# -Step cost measured, which unlike cpu= stamps do not depend on machine load.
function Read-Prices {
    $f = "$Work/cost_prices.tsv"
    if (-not (Test-Path $f)) { return $null }
    $m = @{}
    foreach ($r in (Import-Csv $f -Delimiter "`t")) {
        $m[$r.arm] = @{ A = [double]::Parse($r.us_per_node, $Inv); B = [double]::Parse($r.us_per_treemove, $Inv) }
    }
    return $m
}

function Weights($path) { @(Get-Content $path | Where-Object { $_ -notlike "teacher=*" }) -join "`n" }

# Priced seconds of the checkpoint at $path, $arm's after $games games. A
# TreeStrap checkpoint from before the tree= stamp borrows the counts of another
# ladder of the same arm and seed whose checkpoint at that game count holds
# identical weights: training is deterministic, so that is the same run.
function Get-Priced([string]$arm, [string]$path, [int]$games) {
    $pk = BaseArm $arm
    if ($null -eq $Prices -or -not $Prices.ContainsKey($pk)) { return $null }
    $p = $Prices[$pk]
    $t = Teacher $path
    $n = StampNum $t "nodes"; $m = StampNum $t "treemoves"
    if ($p.B -gt 0 -and $null -eq $m) {
        $w = $null
        foreach ($f in @(Get-ChildItem "$Work/curve" -Filter "curve_${arm}_s${CurveSeed}*_g$games.txt" -ErrorAction SilentlyContinue)) {
            $t2 = Teacher $f.FullName
            if ($null -eq (StampNum $t2 "treemoves")) { continue }
            if ($null -eq $w) { $w = Weights $path }
            if ((Weights $f.FullName) -eq $w) { $n = StampNum $t2 "nodes"; $m = StampNum $t2 "treemoves"; break }
        }
        if ($null -eq $m) { return $null }
    }
    if ($null -eq $m) { $m = 0.0 }
    return ($p.A * $n + $p.B * $m) / 1e6
}

# The arm's priced seconds reach $cpu at this many games, read off its curve
# ladders (linear between rungs, the last rung's rate past the top). At a game
# count two ladders share the published one wins, since a tagged ladder can be
# a different recipe (A8's anneal spans its own run).
function Get-ArmGames([string]$arm, [double]$cpu, [int]$fixed) {
    if ($fixed -gt 0) { return $fixed }
    if ($cpu -le 0) { throw "give -NoiseCpu/-TuneCpu (priced seconds) or -NoiseGames/-TuneGames" }
    if ($null -eq $Prices) { throw "no $Work/cost_prices.tsv: run -Step cost first" }
    $byG = @{}
    $files = @(Get-ChildItem "$Work/curve" -Filter "curve_${arm}_s${CurveSeed}*_g*.txt" -ErrorAction SilentlyContinue |
        Sort-Object { $_.Name -notmatch "_s${CurveSeed}_g\d+\.txt$" }, Name)
    foreach ($f in $files) {
        if ($f.Name -notmatch "_g(\d+)\.txt$") { continue }
        $g = [int]$Matches[1]
        if ($byG.ContainsKey($g)) { continue }
        $c = Get-Priced $arm $f.FullName $g
        if ($null -ne $c) { $byG[$g] = $c }
    }
    $pts = @(@{ G = 0; C = 0.0 }) + @($byG.Keys | Sort-Object | ForEach-Object { @{ G = $_; C = $byG[$_] } })
    if ($pts.Count -lt 2) { throw "no priced curve checkpoints for $arm under $Work/curve: run -Step curve and -Step cost first" }
    for ($i = 1; $i -lt $pts.Count; $i++) {
        if ($pts[$i].C -ge $cpu) {
            $a = $pts[$i - 1]; $b = $pts[$i]
            return [int][Math]::Max(1, [Math]::Round($a.G + ($cpu - $a.C) * ($b.G - $a.G) / ($b.C - $a.C)))
        }
    }
    $last = $pts[-1]
    return [int][Math]::Round($cpu * $last.G / $last.C)
}

$CurveRungList = @($CurveRungs.Split(",") | ForEach-Object { [int]$_ })

# Random-search draws, the same for every arm: (u, v) uniform on [0,1), u
# places the rate in the arm's range, v the arm-specific hyperparameter.
function Get-Draws {
    $rng = New-Object System.Random($DrawSeed)
    $rows = @()
    for ($d = 1; $d -le 2 * $Draws + $WidenDraws; $d++) { $rows += @{ D = $d; U = $rng.NextDouble(); V = $rng.NextDouble() } }
    "draw,u,v" | Out-File -FilePath "$Work/tune_draws.csv" -Encoding ascii
    foreach ($r in $rows) { "$($r.D),$($r.U.ToString('R', $Inv)),$($r.V.ToString('R', $Inv))" | Add-Content -Path "$Work/tune_draws.csv" -Encoding ascii }
    return $rows
}

function New-Run($arm, $seed, $draw, [double]$lrExp, $hName, $hVal, [int]$games, $rungs, $extra) {
    $key = if ($draw -gt 0) { "{0}_{1}_d{2:D2}" -f $Step, $arm, $draw } else { "{0}_{1}_s{2}" -f $Step, $arm, $seed }
    if ($Step -eq "curve") { $key += $CurveTag }
    $a = @("tdleaf") + $HeadFlags + @("--seed", "$seed", "--games", "$games", "--ckpt-at", ($rungs -join ","),
        "--lr", (Lr $lrExp), "--out", "$StepDir/$key") + $ArmSwitch[(BaseArm $arm)] + $(if ($VariantExtra.ContainsKey($arm)) { $VariantExtra[$arm] } else { @() }) + $extra
    if ($arm -eq "A8") { $a += @("--ordinal-games", "$games") }
    $a = @($a | Where-Object { $null -ne $_ })   # an empty @() assigned from an if expression arrives as $null
    return [pscustomobject]@{ Key = $key; Arm = $arm; Seed = $seed; Draw = $draw; LrExp = $lrExp; Lr = (Lr $lrExp)
        HName = $hName; HVal = $hVal; Games = $games; Rungs = $rungs; Args = $a }
}

function Get-Runs {
    $runs = @()
    if ($Step -eq "curve") {
        foreach ($k in $ArmList) {
            $b = BaseArm $k
            $extra = if ($b -eq "A7") { @("--explore", "0.1") } else { @() }
            $h = @{ B0 = "lambda"; A1 = "lambda"; A3 = "dmin"; A7 = "epsilon"; A8 = "ordinal_start" }[$b]
            $hv = @{ B0 = "0.7"; A1 = "0.7"; A3 = "1"; A7 = "0.1"; A8 = "0" }[$b]
            if ($VariantBase.ContainsKey($k)) { $hv = $k.Substring($b.Length + 1) }
            $runs += New-Run $k $CurveSeed 0 ($RangeLo[$b] + $Width / 2) $h $hv $CurveRungList[-1] $CurveRungList $extra
        }
    } elseif ($Step -eq "noise") {
        $na = @($NoiseArms.Split(",") | ForEach-Object { $_.Trim() } | Where-Object { $_ })
        if ($na.Count -lt 2 -or $na[0] -ne "B0") { throw "-NoiseArms must be B0 plus the contrasting arm, e.g. B0,A3" }
        foreach ($k in $na) {
            $g = Get-ArmGames $k $NoiseCpu $NoiseGames
            $rungs = @($CurveRungList | Where-Object { $_ -lt $g }) + @($g)
            $extra = if ($k -eq "A7") { @("--explore", "0.1") } else { @() }
            for ($s = 0; $s -lt $NoiseSeeds; $s++) {
                $runs += New-Run $k ($NoiseSeedBase + $s) 0 ($RangeLo[$k] + $Width / 2) "" "" $g $rungs $extra
            }
        }
    } else {
        $drawRows = Get-Draws   # not $draws: PowerShell names are case-blind and -Draws is a parameter
        if (($ArmList | Where-Object { $InheritLambda -contains $_ }) -and $BaseLambda -lt 0) {
            throw "tuning A5..A8 needs -BaseLambda: B0's tuned lambda, since they differ from B0 in one switch"
        }
        foreach ($k in $ArmList) {
            $joint = @("B0", "A1", "A3", "A7", "A8") -contains $k
            $n = if ($joint) { 2 * $Draws } else { $Draws }
            # The range's draws, then the widening draws: the same random rows,
            # placed in the half decade (-WidenUp) above the locked range and
            # numbered from 100 so they get their own run keys and seeds.
            $rowsFor = @($drawRows | Select-Object -First $n | ForEach-Object { @{ R = $_; D = $_.D; E = $RangeLo[$k] + $Width * $_.U } })
            if ($WidenList -contains $k) {
                $rowsFor += @($drawRows | Select-Object -Last $WidenDraws | ForEach-Object { @{ R = $_; D = 100 + $_.D; E = $RangeLo[$k] + $Width + $WidenUp * $_.U } })
            }
            foreach ($row in $rowsFor) {
                $dr = $row.R; $e = $row.E
                $extra = @(); $h = ""; $hv = ""
                if ($InheritLambda -contains $k) { $extra += @("--lambda", $BaseLambda.ToString("R", $Inv)) }
                switch ($k) {
                    { $_ -in @("B0", "A1") } { $h = "lambda"; $hv = $dr.V.ToString("0.####", $Inv); $extra += @("--lambda", $hv) }
                    "A3" { $h = "dmin"; $hv = "$($DminSet[[int][Math]::Floor($dr.V * $DminSet.Count)])"; $extra += @("--tree-min-depth", $hv) }
                    "A7" { $h = "epsilon"; $hv = [Math]::Pow(10, -2 + $dr.V * [Math]::Log10(30)).ToString("0.#####", $Inv); $extra += @("--explore", $hv) }
                    "A8" { $h = "ordinal_start"; $hv = $dr.V.ToString("0.####", $Inv); $extra += @("--ordinal-start", $hv, "--ordinal-end", "1") }
                }
                # Compute-matched per draw, not per arm: A3's game count comes
                # from the priced ladder of the draw's own d_min.
                $g = Get-ArmGames (LadderOf $k $hv) $TuneCpu $TuneGames
                $runs += New-Run $k ($TuneSeedBase + $row.D - 1) $row.D $e $h $hv $g @($g) $extra
            }
        }
    }
    return $runs
}

# ---- Phase: train ----
function Invoke-TrainJobs($jobs) {
    $running = @()
    foreach ($j in $jobs) {
        while (@($running | Where-Object { -not $_.HasExited }).Count -ge $Workers) { Start-Sleep -Milliseconds 500 }
        $p = Start-Process -FilePath $j.Exe -ArgumentList $j.Args -WorkingDirectory $Root -NoNewWindow -PassThru `
            -RedirectStandardOutput $j.Log -RedirectStandardError "$($j.Log).err"
        [void]$p.Handle
        $running += $p
    }
    $running | ForEach-Object { $_.WaitForExit() }
    $bad = @($running | Where-Object { $_.ExitCode -ne 0 })
    if ($bad.Count -gt 0) { throw "$($bad.Count) process(es) failed, see the .log/.err files under $StepDir" }
}

function Invoke-Train($runs) {
    $jobs = @()
    # Slowest arm first so it does not become the tail.
    foreach ($r in (@($runs | Where-Object { (BaseArm $_.Arm) -eq "A3" }) + @($runs | Where-Object { (BaseArm $_.Arm) -ne "A3" }))) {
        if (Test-Path "$StepDir/$($r.Key)_g$($r.Games).txt") { continue }
        $jobs += @{ Exe = $Train; Args = $r.Args; Log = "$StepDir/$($r.Key).log" }
    }
    Say "train: $($jobs.Count) run(s) to train, $(@($runs).Count - $jobs.Count) already done"
    Invoke-TrainJobs $jobs
}

# ---- Step: cost ----
# Games per benchmark job, sized so every job takes a similar time.
$CostGames = @{ B0 = 40; A1 = 40; A2 = 40; A3 = 8; A4 = 40; A5 = 40; A6 = 40; A7 = 40; A8 = 40 }

function Stamp-Counts($path) {
    $r = @{}
    if (-not $path) { foreach ($k in "games", "cpu", "nodes", "tree", "treemoves") { $r[$k] = 0.0 }; return $r }
    $t = Teacher $path
    foreach ($k in "games", "cpu", "nodes", "tree", "treemoves") { $v = StampNum $t $k; $r[$k] = $(if ($null -eq $v) { 0.0 } else { $v }) }
    return $r
}

function Invoke-Cost {
    $bench = "$Work/cost_bench.tsv"
    if ($Phase -eq "report") {
        # Refit the prices from the stored benchmark without rerunning it.
        $rows = @(Import-Csv $bench -Delimiter "`t" | ForEach-Object { [pscustomobject]@{ arm = $_.arm; from = [int]$_.from; rep = [int]$_.rep
            games = [double]::Parse($_.games, $Inv); cpu = [double]::Parse($_.cpu, $Inv); nodes = [double]::Parse($_.nodes, $Inv)
            tree = [double]::Parse($_.tree, $Inv); treemoves = [double]::Parse($_.treemoves, $Inv) } })
        Say "cost: refit from $bench ($($rows.Count) jobs), pricing $CostPricing"
    } else {
    $froms = @($CostFrom.Split(",") | ForEach-Object { [int]$_ })
    $jobs = @()
    for ($rep = 1; $rep -le $CostReps; $rep++) {
        foreach ($k in (@($ArmList | Where-Object { $_ -eq "A3" }) + @($ArmList | Where-Object { $_ -ne "A3" }))) {
            foreach ($f in $froms) {
                $key = "cost_${k}_f${f}_r$rep"
                $extra = if ($k -eq "A7") { @("--explore", "0.1") } else { @() }
                $a = @("tdleaf") + $HeadFlags + @("--seed", "$CurveSeed", "--games", "$($f + $CostGames[$k])",
                    "--lr", (Lr ($RangeLo[$k] + $Width / 2)), "--out", "$StepDir/$key") + $ArmSwitch[$k] + $extra
                # The published ladder's recipe: A8 anneals over its last rung.
                if ($k -eq "A8") { $a += @("--ordinal-games", "$($CurveRungList[-1])") }
                $prior = $null
                if ($f -gt 0) {
                    $prior = "$Work/curve/curve_${k}_s${CurveSeed}_g$f.txt"
                    if (-not (Test-Path $prior)) { throw "$prior missing: the cost step resumes the published curve ladder" }
                    $a += @("--resume", $prior)
                }
                $a = @($a | Where-Object { $null -ne $_ })
                $jobs += [pscustomobject]@{ Key = $key; Arm = $k; From = $f; Rep = $rep; Prior = $prior; Args = $a }
            }
        }
    }
    $others = @(Get-Process -Name train, train_new -ErrorAction SilentlyContinue)
    if ($others.Count -gt 0 -and -not $CostAllowBusy) { throw "$($others.Count) trainer process(es) already running: the benchmark needs a machine running nothing else" }
    Say "cost: $($jobs.Count) job(s), exactly $CostSlots trainings at every moment ($Train)"
    $queue = New-Object System.Collections.Queue
    foreach ($j in $jobs) { $queue.Enqueue($j) }
    $procs = @(); $fillers = @(); $fi = 0
    while ($true) {
        $live = @($procs | Where-Object { -not $_.P.HasExited })
        $fillers = @($fillers | Where-Object { -not $_.HasExited })
        if ($queue.Count -eq 0 -and $live.Count -eq 0) { break }
        for ($free = $CostSlots - $live.Count - $fillers.Count; $free -gt 0; $free--) {
            if ($queue.Count -gt 0) {
                $j = $queue.Dequeue()
                $p = Start-Process -FilePath $Train -ArgumentList $j.Args -WorkingDirectory $Root -NoNewWindow -PassThru `
                    -RedirectStandardOutput "$StepDir/$($j.Key).log" -RedirectStandardError "$StepDir/$($j.Key).log.err"
                [void]$p.Handle
                $procs += [pscustomobject]@{ Job = $j; P = $p }
            } else {
                # Filler: B0 from scratch, never read, stopped when the last job ends.
                $fi++
                $fa = @("tdleaf") + $HeadFlags + @("--seed", "$(9000 + $fi)", "--games", "100000", "--lr", "0.01", "--out", "$StepDir/filler_$fi")
                $p = Start-Process -FilePath $Train -ArgumentList $fa -WorkingDirectory $Root -NoNewWindow -PassThru `
                    -RedirectStandardOutput "$StepDir/filler_$fi.log" -RedirectStandardError "$StepDir/filler_$fi.log.err"
                $fillers += $p
            }
        }
        Start-Sleep -Milliseconds 500
    }
    $fillers | ForEach-Object { Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue }
    $bad = @($procs | Where-Object { $_.P.ExitCode -ne 0 })
    if ($bad.Count -gt 0) { throw "$($bad.Count) benchmark job(s) failed, see the .log/.err files under $StepDir" }

    # Each job's own work: its stamp minus the stamp it resumed from. A treestrap
    # checkpoint from before the tree= stamp resumes with no walk prior, so its
    # job's stamp holds only the job's own walk.
    $rows = @()
    foreach ($x in $procs) {
        $j = $x.Job
        $s = Stamp-Counts "$StepDir/$($j.Key).txt"; $p0 = Stamp-Counts $j.Prior
        $rows += [pscustomobject]@{ arm = $j.Arm; from = $j.From; rep = $j.Rep; games = $s.games - $p0.games
            cpu = $s.cpu - $p0.cpu; nodes = $s.nodes - $p0.nodes; tree = $s.tree - $p0.tree; treemoves = $s.treemoves - $p0.treemoves }
    }
    @("arm`tfrom`trep`tgames`tcpu`tnodes`ttree`ttreemoves") + @($rows | ForEach-Object {
        "$($_.arm)`t$($_.from)`t$($_.rep)`t$($_.games)`t$($_.cpu.ToString('R', $Inv))`t$($_.nodes)`t$($_.tree)`t$($_.treemoves)" }) |
        Out-File -FilePath $bench -Encoding ascii
    }

    # Prices: CPU = a x nodes, by least squares. -CostPricing shared (the
    # default) pools every arm without a TreeStrap walk into one per-node price,
    # since the per-arm prices measured 2026-09-14 differed by no more than the
    # benchmark's own drift. perarm fits each arm alone. An arm whose walk made
    # probe moves (A3) runs B0's search, so it takes that per-node price and
    # fits only b, the price per probe move. The free two-price fit is printed
    # beside it as a check.
    $prices = @()
    $aB0 = $null
    Say ""
    Say ("{0,4}{1,7}{2,5}{3,7}{4,10}{5,15}{6,15}{7,11}{8,9}" -f "arm", "from", "rep", "games", "cpu s", "nodes", "treemoves", "us/node", "resid")
    $walkArms = @($ArmList | Where-Object { $k = $_; (@($rows | Where-Object { $_.arm -eq $k }) | ForEach-Object { $_.treemoves } | Measure-Object -Sum).Sum -gt 0 })
    $aShared = $null
    if ($CostPricing -eq "shared") {
        $pr = @($rows | Where-Object { $walkArms -notcontains $_.arm })
        $aShared = (($pr | ForEach-Object { $_.nodes * $_.cpu } | Measure-Object -Sum).Sum) / (($pr | ForEach-Object { $_.nodes * $_.nodes } | Measure-Object -Sum).Sum)
        $aB0 = $aShared
        Say ("  shared per-node price over {0} jobs: {1:N5} us" -f $pr.Count, (1e6 * $aShared))
    }
    foreach ($k in (@($ArmList | Where-Object { $walkArms -notcontains $_ }) + $walkArms)) {
        $r = @($rows | Where-Object { $_.arm -eq $k })
        $snn = 0.0; $snm = 0.0; $smm = 0.0; $snc = 0.0; $smc = 0.0
        foreach ($x in $r) { $snn += $x.nodes * $x.nodes; $snm += $x.nodes * $x.treemoves; $smm += $x.treemoves * $x.treemoves; $snc += $x.nodes * $x.cpu; $smc += $x.treemoves * $x.cpu }
        if ($smm -gt 0) {
            $det = $snn * $smm - $snm * $snm
            if ($det -gt 0) { Say ("  {0} free fit: {1:N5} us per node, {2:N5} us per probe move" -f $k, (1e6 * ($snc * $smm - $smc * $snm) / $det), (1e6 * ($smc * $snn - $snc * $snm) / $det)) }
            if ($null -ne $aB0) { $a = $aB0; $b = ($smc - $a * $snm) / $smm }
            else { $a = ($snc * $smm - $smc * $snm) / $det; $b = ($smc * $snn - $snc * $snm) / $det }
        } else { $a = $(if ($null -ne $aShared) { $aShared } else { $snc / $snn }); $b = 0.0 }
        $own = 1e6 * $snc / $snn
        if ($k -eq "B0" -and $null -eq $aShared) { $aB0 = $a }
        $maxr = 0.0
        foreach ($x in ($r | Sort-Object from, rep)) {
            $res = $x.cpu / ($a * $x.nodes + $b * $x.treemoves) - 1
            if ([Math]::Abs($res) -gt $maxr) { $maxr = [Math]::Abs($res) }
            Say ("{0,4}{1,7}{2,5}{3,7}{4,10:N2}{5,15:N0}{6,15:N0}{7,11:N4}{8,9:P1}" -f $k, $x.from, $x.rep, $x.games, $x.cpu, $x.nodes, $x.treemoves, (1e6 * $x.cpu / $x.nodes), $res)
        }
        $prices += [pscustomobject]@{ arm = $k; a = 1e6 * $a; b = 1e6 * $b; resid = 100 * $maxr; n = $r.Count; own = $own }
    }
    Say ""
    Say "Prices (CPU microseconds per search node, per TreeStrap probe move), largest residual over the arm's jobs:"
    foreach ($p in $prices) { Say ("  {0,-3} {1,9:N5} per node{2}   max |resid| {3:N1}%   {4} jobs   (arm alone, nodes only: {5:N5})" -f $p.arm, $p.a, $(if ($p.b -gt 0) { ", {0:N5} per probe move" -f $p.b } else { "" }), $p.resid, $p.n, $p.own) }
    @("arm`tus_per_node`tus_per_treemove`tmax_abs_resid_pct`tjobs") + @($prices | ForEach-Object {
        "$($_.arm)`t$($_.a.ToString('R', $Inv))`t$($_.b.ToString('R', $Inv))`t$($_.resid.ToString('R', $Inv))`t$($_.n)" }) |
        Out-File -FilePath "$Work/cost_prices.tsv" -Encoding ascii
    Say "cost: $bench, $Work/cost_prices.tsv"
}

# ---- Panel ----
function Read-RosterIds($path) {
    $ids = @()
    foreach ($line in Get-Content $path) {
        $body = ($line -split '#', 2)[0].Trim()
        if (-not $body) { continue }
        $t = $body -split '\s+', 2
        if ($t.Count -eq 2 -and @("anchor", "on") -contains $t[0]) { $ids += $t[1].Trim() }
    }
    return $ids
}

function Read-Pinned($path) {
    $m = @{}
    $lines = @(Get-Content $path | Where-Object { $_ -and -not $_.StartsWith("#") })
    $hdr = $lines[0].Split("`t")
    $ie = [Array]::IndexOf($hdr, "elo"); $ip = [Array]::IndexOf($hdr, "pm"); $ii = [Array]::IndexOf($hdr, "id"); $ig = [Array]::IndexOf($hdr, "games")
    foreach ($l in $lines[1..($lines.Count - 1)]) {
        $c = $l.Split("`t")
        $m[$c[$ii]] = @{ Elo = [double]::Parse($c[$ie], $Inv); Pm = [double]::Parse($c[$ip], $Inv); Games = [int]$c[$ig] }
    }
    return $m
}

function Assert-Panel {
    if (-not $Panel) { throw "-Panel <roster file> is required from the publish phase on" }
    $ids = Read-RosterIds $Panel
    $pin = Read-Pinned $PanelPin
    $missing = @($ids | Where-Object { -not $pin.ContainsKey($_) })
    if ($missing.Count -gt 0) { throw "panel ids not in ${PanelPin} (they would float in the fit): $($missing -join '; ')" }
    return $ids
}

# ---- Phase: publish ----
function Read-Ledger {
    if (-not (Test-Path $Ledger)) { return @() }
    return @(Import-Csv $Ledger)
}

function Invoke-Publish($runs) {
    $rows = Read-Ledger
    $have = @{}; foreach ($x in $rows) { $have["$($x.key)|$($x.rung)"] = $x }
    $bySlot = @{}; foreach ($x in $rows) { $bySlot[[int]$x.slot] = $x }
    $next = [Math]::Max($SlotBase, $(if ($rows.Count) { ($rows | ForEach-Object { [int]$_.slot } | Measure-Object -Maximum).Maximum + 1 } else { 0 }))
    $new = @()
    foreach ($r in $runs) {
        foreach ($g in $r.Rungs) {
            $src = "$StepDir/$($r.Key)_g$g.txt"
            if (-not (Test-Path $src)) { throw "$src missing: train first" }
            $k = "$($r.Key)|$g"
            if ($have.ContainsKey($k)) {
                $dst = $have[$k].model
                if ((Get-FileHash $src).Hash -ne (Get-FileHash $dst).Hash) { throw "${dst} differs from ${src}, the ledger's slot no longer holds this checkpoint" }
                continue
            }
            if ($next -gt $SlotMax) { throw "slot block exhausted at $SlotMax" }
            $dst = "models/sweep/slot$next.txt"
            if (Test-Path $dst) { throw "$dst exists and is not in ${Ledger}: refusing to overwrite a slot this study did not write" }
            Copy-Item $src $dst
            $hash = [RepFnv]::Hash8([IO.File]::ReadAllBytes((Resolve-Path $dst)))
            $new += [pscustomobject]@{ step = $Step; key = $r.Key; arm = $r.Arm; seed = $r.Seed; draw = $r.Draw; lr = $r.Lr
                hname = $r.HName; hval = $r.HVal; rung = $g; slot = $next; src = $src; model = $dst
                id = "$HeadId.learned(s$next,$hash)@1$Opener" }
            $next++
        }
    }
    if ($new.Count -gt 0) {
        # rank.exe owns the canonical spelling: the fit and the store key on it.
        $tmp = "$StepDir/canon_in.txt"
        $new | ForEach-Object { "on $($_.id)" } | Out-File -FilePath $tmp -Encoding ascii
        $canon = @(& $Rank canon --roster $tmp | Where-Object { $_ -match '^on\s' } | ForEach-Object { ($_ -split '\s+', 2)[1].Trim() })
        if ($canon.Count -ne $new.Count) { throw "rank.exe canon returned $($canon.Count) ids for $($new.Count)" }
        for ($i = 0; $i -lt $new.Count; $i++) { $new[$i].id = $canon[$i] }
        $all = @($rows) + $new
        $all | Export-Csv -Path $Ledger -NoTypeInformation -Encoding ascii
    }
    Say "publish: $($new.Count) checkpoint(s) published to slots, ledger $Ledger"
}

# ---- Phase: play ----
function PartOf($row) { "$PartDir/$($row.key)_g$($row.rung).jsonl" }

function Write-Index {
    New-Item -ItemType Directory -Force $PartDir | Out-Null
    $parts = @(Get-ChildItem $PartDir -Filter *.jsonl | Sort-Object Name | ForEach-Object { "$StoreStem/$($_.Name)" })
    @("# Study store parts, one per study agent (tools/replication_pass2.ps1).") + $parts | Out-File -FilePath $IndexFile -Encoding ascii
}

function Invoke-Play {
    $panelIds = Assert-Panel
    $panelLines = Get-Content $Panel
    New-Item -ItemType Directory -Force $PartDir | Out-Null
    $rows = @(Read-Ledger | Where-Object { $_.step -eq $Step })
    $jobs = @()
    foreach ($x in $rows) {
        $name = "$($x.key)_g$($x.rung)"
        $ros = "$StepDir/rosters/$name.txt"; $coh = "$StepDir/rosters/$name.cohort.txt"
        ($panelLines + @("", "on      $($x.id)")) | Out-File -FilePath $ros -Encoding ascii
        $x.id | Out-File -FilePath $coh -Encoding ascii
        $jobs += @{ Exe = $Rank; Log = "$StepDir/play_logs/$name.log"
            Args = @("play", "--roster", $ros, "--cohort", $coh, "--in", (PartOf $x), "--paired-openings", "--common-openings", "--games", "$GamesPerPair") }
    }
    Say "play: $($rows.Count) agent(s) x $($panelIds.Count) panel opponents, target $GamesPerPair games per pair"
    Invoke-TrainJobs $jobs
    Write-Index
}

# ---- Phase: rate ----
function Invoke-Rate {
    [void](Assert-Panel)
    Write-Index
    $all = "$Work/roster_all.txt"
    ((Get-Content $Panel) + @("") + @(Read-Ledger | ForEach-Object { "on      $($_.id)" })) | Out-File -FilePath $all -Encoding ascii
    & $Rank rate --roster $all --in $Store --pin $PanelPin | Select-Object -Last 3 | ForEach-Object { Say "  $_" }
    if ($LASTEXITCODE -ne 0) { throw "rank.exe rate failed" }
    Say "rate: pinned fit over $Store -> $PinnedStandings"
}

# ---- Phase: report ----
function Get-Distinct($part) {
    $n = 0; $keys = @{}
    if (Test-Path $part) {
        foreach ($l in [IO.File]::ReadLines((Resolve-Path $part))) {
            if (-not $l.Trim()) { continue }
            $n++
            $g = $l | ConvertFrom-Json
            $keys["$($g.w)|$($g.b)|$($g.r)|$($g.plies)|$($g.wnod)|$($g.bnod)"] = 1
        }
    }
    return @{ Rows = $n; Distinct = $keys.Count }
}

function Invoke-Report {
    $st = Read-Pinned $PinnedStandings
    $rows = @(Read-Ledger | Where-Object { $_.step -eq $Step })
    $panelIds = Read-RosterIds $Panel
    $pin = Read-Pinned $PanelPin
    foreach ($x in $rows) {
        $t = Teacher $x.model
        $x | Add-Member -Force NoteProperty cpu (StampNum $t "cpu")
        $s = $st[$x.id]
        $x | Add-Member -Force NoteProperty elo $(if ($s) { $s.Elo } else { $null })
        $x | Add-Member -Force NoteProperty pm $(if ($s) { $s.Pm } else { $null })
        $dd = Get-Distinct (PartOf $x)
        $x | Add-Member -Force NoteProperty nrows $dd.Rows
        $x | Add-Member -Force NoteProperty ndist $dd.Distinct
    }
    Say "== Pass 2 $Step. Head $HeadId, study agents wear $Opener, scratch init."
    Say "   Panel $Panel ($($panelIds.Count) agents, Elo pinned from $PanelPin), $GamesPerPair games per panel opponent,"
    Say "   paired common openings, pinned fit $PinnedStandings."
    Say ""
    Say "Panel:"
    foreach ($p in ($panelIds | Sort-Object { $pin[$_].Elo })) { Say ("  {0,6:N0}  {1}" -f $pin[$p].Elo, $p) }
    Say ""
    $unrated = @($rows | Where-Object { $null -eq $_.elo })
    if ($unrated.Count) { Say "NOT RATED ($($unrated.Count)): $(($unrated | ForEach-Object { "$($_.key)_g$($_.rung)" }) -join ', ')"; Say "" }
    $minD = ($rows | ForEach-Object { $_.ndist } | Measure-Object -Minimum).Minimum
    $minR = ($rows | ForEach-Object { $_.nrows } | Measure-Object -Minimum).Minimum
    Say "Games per agent: min $minR stored rows, min $minD distinct trajectories (colour, plies, result, both node totals)."
    Say ""

    if ($Step -eq "curve") {
        # One column per ladder: the arm, plus the -CurveTag of a ladder run under its own key.
        foreach ($x in $rows) { $x | Add-Member -Force NoteProperty col (($x.key -replace '^curve_', '') -replace "_s$($x.seed)", '') }
        $arms = @($rows | ForEach-Object { $_.col } | Select-Object -Unique)
        $colArm = @{}; foreach ($x in $rows) { $colArm[$x.col] = $x.arm }
        $rungs = @($rows | ForEach-Object { [int]$_.rung } | Sort-Object -Unique)
        $cell = @{}; foreach ($x in $rows) { $cell["$($x.col)|$($x.rung)"] = $x }
        $hdr = ("{0,7}" -f "games") + (($arms | ForEach-Object { "{0,12}" -f $_ }) -join "")
        Say "Elo (pm) at each rung, 1 seed ($CurveSeed), each arm at the middle of its locked range:"
        Say (("{0,7}" -f "lr") + (($arms | ForEach-Object { "{0,12}" -f (F ([Math]::Pow(10, $RangeLo[(BaseArm $colArm[$_])] + $Width / 2))) }) -join ""))
        Say $hdr
        foreach ($g in $rungs) {
            Say (("{0,7}" -f $g) + (($arms | ForEach-Object { $c = $cell["$_|$g"]; if ($c -and $null -ne $c.elo) { "{0,12}" -f ("{0:N0} ({1:N0})" -f $c.elo, $c.pm) } else { "{0,12}" -f "" } }) -join ""))
        }
        Say ""
        Say "Cumulative training CPU seconds at each rung:"
        Say $hdr
        foreach ($g in $rungs) { Say (("{0,7}" -f $g) + (($arms | ForEach-Object { $c = $cell["$_|$g"]; "{0,12}" -f $(if ($c) { "{0:N1}" -f $c.cpu } else { "" }) }) -join "")) }
        Say ""
        Say "CPU seconds per game between consecutive rungs (cpu= stamps, which depend on machine load):"
        Say $hdr
        $prev = @{}; foreach ($a in $arms) { $prev[$a] = @{ G = 0; C = 0.0 } }
        foreach ($g in $rungs) {
            $line = "{0,7}" -f $g
            foreach ($a in $arms) {
                $c = $cell["$a|$g"]
                if ($c) { $line += "{0,12}" -f ("{0:N2}" -f (($c.cpu - $prev[$a].C) / ($g - $prev[$a].G))); $prev[$a] = @{ G = $g; C = $c.cpu } } else { $line += "{0,12}" -f "" }
            }
            Say $line
        }
        Say ""
        if ($null -eq $Prices) { Say "Priced seconds: no $Work/cost_prices.tsv yet (run -Step cost)." }
        else {
            Say "Priced training seconds at each rung (the cost output: stamped counts at -Step cost's prices):"
            Say $hdr
            $pc = @{}
            foreach ($x in $rows) { $pc["$($x.col)|$($x.rung)"] = Get-Priced $x.arm $x.model ([int]$x.rung) }
            foreach ($g in $rungs) { Say (("{0,7}" -f $g) + (($arms | ForEach-Object { $v = $pc["$_|$g"]; "{0,12}" -f $(if ($null -ne $v) { "{0:N1}" -f $v } else { "" }) }) -join "")) }
            Say ""
            Say "Elo at matched priced seconds, log-linear between rungs, marks at B0's rungs:"
            Say (("{0,9}" -f "priced s") + (($arms | ForEach-Object { "{0,12}" -f $_ }) -join ""))
            $marks = @($rungs | Where-Object { $null -ne $pc["B0|$_"] } | ForEach-Object { $pc["B0|$_"] })
            foreach ($mk in $marks) {
                $line = "{0,9:N0}" -f $mk
                foreach ($a in $arms) {
                    $pts = @($rungs | Where-Object { $c = $cell["$a|$_"]; $c -and $null -ne $c.elo -and $null -ne $pc["$a|$_"] } |
                        ForEach-Object { @{ C = $pc["$a|$_"]; E = $cell["$a|$_"].elo } })
                    $v = $null
                    for ($i = 1; $i -lt $pts.Count; $i++) {
                        if ($pts[$i - 1].C -le $mk -and $mk -le $pts[$i].C) {
                            $fr = ([Math]::Log($mk) - [Math]::Log($pts[$i - 1].C)) / ([Math]::Log($pts[$i].C) - [Math]::Log($pts[$i - 1].C))
                            $v = $pts[$i - 1].E + $fr * ($pts[$i].E - $pts[$i - 1].E); break
                        }
                    }
                    $line += "{0,12}" -f $(if ($null -ne $v) { "{0:N0}" -f $v } else { "" })
                }
                Say $line
            }
        }
    } elseif ($Step -eq "noise") {
        foreach ($a in @($rows | ForEach-Object { $_.arm } | Select-Object -Unique)) {
            $ar = @($rows | Where-Object { $_.arm -eq $a })
            $seeds = @($ar | ForEach-Object { [int]$_.seed } | Sort-Object -Unique)
            Say "$a, lr $(Lr ($RangeLo[$a] + $Width / 2)): Elo (pm) by seed, then the spread over seeds (sigma_seed) and the mean pm (sigma_meas)"
            Say (("{0,7}" -f "games") + (($seeds | ForEach-Object { "{0,12}" -f $_ }) -join "") + ("{0,10}{1,10}{2,10}" -f "mean", "sd seed", "mean pm"))
            foreach ($g in @($ar | ForEach-Object { [int]$_.rung } | Sort-Object -Unique)) {
                $v = @(); $pms = @(); $line = "{0,7}" -f $g
                foreach ($s in $seeds) {
                    $c = $ar | Where-Object { [int]$_.seed -eq $s -and [int]$_.rung -eq $g } | Select-Object -First 1
                    if ($c -and $null -ne $c.elo) { $v += $c.elo; $pms += $c.pm; $line += "{0,12}" -f ("{0:N0} ({1:N0})" -f $c.elo, $c.pm) } else { $line += "{0,12}" -f "" }
                }
                if ($v.Count -ge 2) {
                    $m = ($v | Measure-Object -Average).Average
                    $sd = [Math]::Sqrt((($v | ForEach-Object { ($_ - $m) * ($_ - $m) } | Measure-Object -Sum).Sum) / ($v.Count - 1))
                    $line += "{0,10:N0}{1,10:N1}{2,10:N1}" -f $m, $sd, ($pms | Measure-Object -Average).Average
                }
                Say $line
            }
            $cpus = @($ar | Where-Object { [int]$_.rung -eq ($ar | ForEach-Object { [int]$_.rung } | Measure-Object -Maximum).Maximum } | ForEach-Object { $_.cpu })
            Say ("  final-rung CPU seconds over seeds: " + (($cpus | ForEach-Object { "{0:N1}" -f $_ }) -join ", "))
            Say ""
        }
    } else {
        foreach ($a in @($rows | ForEach-Object { $_.arm } | Select-Object -Unique)) {
            $ar = @($rows | Where-Object { $_.arm -eq $a } | Sort-Object { [double]::Parse($_.lr, $Inv) })
            $lo = $RangeLo[(BaseArm $a)]
            $wid = RangeWidth $a
            $gs = @($ar | ForEach-Object { [int]$_.rung } | Sort-Object -Unique)
            Say ("$a, range [{0}, {1}], {2} draws, {3} games each:" -f (Lr $lo), (Lr ($lo + $wid)), $ar.Count, $(if ($gs.Count -eq 1) { $gs[0] } else { "$($gs[0])..$($gs[-1]), compute-matched per draw" }))
            Say ("{0,6}{1,12}{2,16}{3,8}{4,10}{5,14}" -f "draw", "lr", $(if ($ar[0].hname) { $ar[0].hname } else { "" }), "games", "cpu s", "Elo (pm)")
            foreach ($x in $ar) { Say ("{0,6}{1,12}{2,16}{3,8}{4,10:N1}{5,14}" -f $x.draw, $x.lr, $x.hval, $x.rung, $x.cpu, $(if ($null -ne $x.elo) { "{0:N0} ({1:N0})" -f $x.elo, $x.pm } else { "" })) }
            $best = $ar | Where-Object { $null -ne $_.elo } | Sort-Object { - $_.elo } | Select-Object -First 1
            if ($best) {
                $pos = ([Math]::Log10([double]::Parse($best.lr, $Inv)) - $lo) / $wid
                $edge = if ($pos -lt 0.1 -or $pos -gt 0.9) { "  EDGE: best rate within 0.2 decades of the range end" } else { "" }
                Say ("  best draw {0}: lr {1} ({2:N2} of the way up the range){3} {4}" -f $best.draw, $best.lr, $pos, $(if ($best.hname) { ", $($best.hname) $($best.hval)" } else { "" }), $edge)
            }
            Say ""
        }
    }

    # Manifest for analysis/replication_stage1.py export (curve and noise feed analyze).
    $man = "$Work/${Step}_manifest.tsv"
    @("id`tarm`tseed`trung`tmodel") + @($rows | ForEach-Object { "$($_.id)`t$($_.arm)`t$(if ([int]$_.draw -gt 0) { "d$($_.draw)" } else { $_.seed })`t$($_.rung)`t$($_.model)" }) | Out-File -FilePath $man -Encoding ascii
    $exp = & python analysis/replication_stage1.py export --manifest $man --standings $PinnedStandings --out "$Work/${Step}_runs.tsv" 2>&1 | Out-String
    Say "export: $Work/${Step}_runs.tsv $($exp.Trim())"
    $coh = "$Work/${Step}_cohort.txt"
    $rows | ForEach-Object { $_.id } | Out-File -FilePath $coh -Encoding ascii
    $ver = & python analysis/replication_stage1.py verify-store --store $Store --cohort $coh 2>&1 | Out-String
    Say "verify-store ($Step agents): $($ver.Trim())"
    if ($LASTEXITCODE -ne 0) { Say "VERIFY-STORE FAILED" }
}

$Prices = Read-Prices
if ($Step -eq "cost") {
    Invoke-Cost
    $out | Out-File -FilePath "$Work/cost_summary.txt" -Encoding utf8
    Write-Host "summary: $Work/cost_summary.txt"
    return
}
$runs = Get-Runs
if ($Phase -in @("train", "all")) { Invoke-Train $runs }
if ($Phase -in @("publish", "all")) { [void](Assert-Panel); Invoke-Publish $runs }
if ($Phase -in @("play", "all")) { Invoke-Play }
if ($Phase -in @("rate", "all")) { Invoke-Rate }
if ($Phase -in @("report", "all")) { Invoke-Report }
$sum = "$Work/${Step}_summary.txt"
$out | Out-File -FilePath $sum -Encoding utf8
Write-Host "summary: $sum"
