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

param(
    [Parameter(Mandatory = $true)][ValidateSet("curve", "noise", "tune")][string]$Step,
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
    [double]$BaseLambda = -1
)
$ErrorActionPreference = "Stop"
$Inv = [Globalization.CultureInfo]::InvariantCulture
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Train = Join-Path $Root "train.exe"
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

# The arm's cumulative CPU seconds reach $cpu at this many games, read off its
# curve run (linear between rungs, the last rung's rate past the top).
function Get-ArmGames([string]$arm, [double]$cpu, [int]$fixed) {
    if ($fixed -gt 0) { return $fixed }
    if ($cpu -le 0) { throw "give -NoiseCpu/-TuneCpu (matched compute) or -NoiseGames/-TuneGames" }
    $pts = @(@{ G = 0; C = 0.0 })
    foreach ($r in $CurveRungList) {
        $f = "$Work/curve/curve_${arm}_s${CurveSeed}_g$r.txt"
        if (Test-Path $f) { $pts += @{ G = $r; C = (StampNum (Teacher $f) "cpu") } }
    }
    if ($pts.Count -lt 2) { throw "no curve checkpoints for $arm under $Work/curve: run -Step curve first" }
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
    for ($d = 1; $d -le 2 * $Draws; $d++) { $rows += @{ D = $d; U = $rng.NextDouble(); V = $rng.NextDouble() } }
    "draw,u,v" | Out-File -FilePath "$Work/tune_draws.csv" -Encoding ascii
    foreach ($r in $rows) { "$($r.D),$($r.U.ToString('R', $Inv)),$($r.V.ToString('R', $Inv))" | Add-Content -Path "$Work/tune_draws.csv" -Encoding ascii }
    return $rows
}

function New-Run($arm, $seed, $draw, [double]$lrExp, $hName, $hVal, [int]$games, $rungs, $extra) {
    $key = if ($draw -gt 0) { "{0}_{1}_d{2:D2}" -f $Step, $arm, $draw } else { "{0}_{1}_s{2}" -f $Step, $arm, $seed }
    $a = @("tdleaf") + $HeadFlags + @("--seed", "$seed", "--games", "$games", "--ckpt-at", ($rungs -join ","),
        "--lr", (Lr $lrExp), "--out", "$StepDir/$key") + $ArmSwitch[$arm] + $extra
    if ($arm -eq "A8") { $a += @("--ordinal-games", "$games") }
    $a = @($a | Where-Object { $null -ne $_ })   # an empty @() assigned from an if expression arrives as $null
    return [pscustomobject]@{ Key = $key; Arm = $arm; Seed = $seed; Draw = $draw; LrExp = $lrExp; Lr = (Lr $lrExp)
        HName = $hName; HVal = $hVal; Games = $games; Rungs = $rungs; Args = $a }
}

function Get-Runs {
    $runs = @()
    if ($Step -eq "curve") {
        foreach ($k in $ArmList) {
            $extra = if ($k -eq "A7") { @("--explore", "0.1") } else { @() }
            $h = @{ B0 = "lambda"; A1 = "lambda"; A3 = "dmin"; A7 = "epsilon"; A8 = "ordinal_start" }[$k]
            $hv = @{ B0 = "0.7"; A1 = "0.7"; A3 = "1"; A7 = "0.1"; A8 = "0" }[$k]
            $runs += New-Run $k $CurveSeed 0 ($RangeLo[$k] + $Width / 2) $h $hv $CurveRungList[-1] $CurveRungList $extra
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
            $g = Get-ArmGames $k $TuneCpu $TuneGames
            $joint = @("B0", "A1", "A3", "A7", "A8") -contains $k
            $n = if ($joint) { 2 * $Draws } else { $Draws }
            foreach ($dr in ($drawRows | Select-Object -First $n)) {
                $e = $RangeLo[$k] + $Width * $dr.U
                $extra = @(); $h = ""; $hv = ""
                if ($InheritLambda -contains $k) { $extra += @("--lambda", $BaseLambda.ToString("R", $Inv)) }
                switch ($k) {
                    { $_ -in @("B0", "A1") } { $h = "lambda"; $hv = $dr.V.ToString("0.####", $Inv); $extra += @("--lambda", $hv) }
                    "A3" { $h = "dmin"; $hv = "$($DminSet[[int][Math]::Floor($dr.V * $DminSet.Count)])"; $extra += @("--tree-min-depth", $hv) }
                    "A7" { $h = "epsilon"; $hv = [Math]::Pow(10, -2 + $dr.V * [Math]::Log10(30)).ToString("0.#####", $Inv); $extra += @("--explore", $hv) }
                    "A8" { $h = "ordinal_start"; $hv = $dr.V.ToString("0.####", $Inv); $extra += @("--ordinal-start", $hv, "--ordinal-end", "1") }
                }
                $runs += New-Run $k ($TuneSeedBase + $dr.D - 1) $dr.D $e $h $hv $g @($g) $extra
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
    foreach ($r in (@($runs | Where-Object { $_.Arm -eq "A3" }) + @($runs | Where-Object { $_.Arm -ne "A3" }))) {
        if (Test-Path "$StepDir/$($r.Key)_g$($r.Games).txt") { continue }
        $jobs += @{ Exe = $Train; Args = $r.Args; Log = "$StepDir/$($r.Key).log" }
    }
    Say "train: $($jobs.Count) run(s) to train, $(@($runs).Count - $jobs.Count) already done"
    Invoke-TrainJobs $jobs
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
        $arms = @($rows | ForEach-Object { $_.arm } | Select-Object -Unique)
        $rungs = @($rows | ForEach-Object { [int]$_.rung } | Sort-Object -Unique)
        $cell = @{}; foreach ($x in $rows) { $cell["$($x.arm)|$($x.rung)"] = $x }
        $hdr = ("{0,7}" -f "games") + (($arms | ForEach-Object { "{0,12}" -f $_ }) -join "")
        Say "Elo (pm) at each rung, 1 seed ($CurveSeed), each arm at the middle of its locked range:"
        Say (("{0,7}" -f "lr") + (($arms | ForEach-Object { "{0,12}" -f (F ([Math]::Pow(10, $RangeLo[$_] + $Width / 2))) }) -join ""))
        Say $hdr
        foreach ($g in $rungs) {
            Say (("{0,7}" -f $g) + (($arms | ForEach-Object { $c = $cell["$_|$g"]; if ($c -and $null -ne $c.elo) { "{0,12}" -f ("{0:N0} ({1:N0})" -f $c.elo, $c.pm) } else { "{0,12}" -f "" } }) -join ""))
        }
        Say ""
        Say "Cumulative training CPU seconds at each rung:"
        Say $hdr
        foreach ($g in $rungs) { Say (("{0,7}" -f $g) + (($arms | ForEach-Object { $c = $cell["$_|$g"]; "{0,12}" -f $(if ($c) { "{0:N1}" -f $c.cpu } else { "" }) }) -join "")) }
        Say ""
        Say "CPU seconds per game between consecutive rungs (the cost output):"
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
            $lo = $RangeLo[$a]
            Say ("$a, range [{0}, {1}], {2} draws, {3} games each:" -f (Lr $lo), (Lr ($lo + $Width)), $ar.Count, $ar[0].rung)
            Say ("{0,6}{1,12}{2,16}{3,10}{4,14}" -f "draw", "lr", $(if ($ar[0].hname) { $ar[0].hname } else { "" }), "cpu s", "Elo (pm)")
            foreach ($x in $ar) { Say ("{0,6}{1,12}{2,16}{3,10:N1}{4,14}" -f $x.draw, $x.lr, $x.hval, $x.cpu, $(if ($null -ne $x.elo) { "{0:N0} ({1:N0})" -f $x.elo, $x.pm } else { "" })) }
            $best = $ar | Where-Object { $null -ne $_.elo } | Sort-Object { - $_.elo } | Select-Object -First 1
            if ($best) {
                $pos = ([Math]::Log10([double]::Parse($best.lr, $Inv)) - $lo) / $Width
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

$runs = Get-Runs
if ($Phase -in @("train", "all")) { Invoke-Train $runs }
if ($Phase -in @("publish", "all")) { [void](Assert-Panel); Invoke-Publish $runs }
if ($Phase -in @("play", "all")) { Invoke-Play }
if ($Phase -in @("rate", "all")) { Invoke-Rate }
if ($Phase -in @("report", "all")) { Invoke-Report }
$sum = "$Work/${Step}_summary.txt"
$out | Out-File -FilePath $sum -Encoding utf8
Write-Host "summary: $sum"
