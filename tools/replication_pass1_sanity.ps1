# Replication study Stage 1, Pass 1 (sanity): plans/replication-study-plan-1-brass-lectern.md.
#
# Every arm trains at the study head from scratch to a two-rung ladder, and the
# script checks what Pass 1 asks for:
#   1. each rung stops where it should and its provenance is truthful
#      (games= per rung, cpu= and nodes= present and growing, the arm's switch
#      token present, no switch token on the baseline, init:scratch)
#   2. the same seed twice gives the same model (every byte except the
#      measured secs= and cpu= values)
#   3. at lr 0 no switch changes what the search plays (nodes, searched moves
#      and results equal to the baseline's), A7 excluded since exploring is
#      supposed to change play
#   4. the TD-Leaf arms' mean PV depth and the TreeStrap coverage, read from
#      the run logs
#   5. every rung-20 checkpoint loads through the real search path: published
#      to its slot, then played against the baseline's rung 20 by rank.exe
#      pairgen
# Panel-game independence waits for the panel (Round 4's fit).
#
# Phases: train, check, play, all. Outputs under models/sweep/rep1_p1/,
# published slots recorded in models/sweep/replication_stage1_pass1.csv.

param(
    [string]$Phase = "all",
    [int]$Workers = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [int]$Seed = 2001,
    [int]$SlotBase = 1858
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root
$Train = Join-Path $Root "train.exe"
$Rank = Join-Path $Root "rank.exe"
$Work = "models/sweep/rep1_p1"
$Ledger = "models/sweep/replication_stage1_pass1.csv"
$Summary = "$Work/pass1_summary.txt"
foreach ($d in @($Work, "$Work/again", "$Work/lr0", "$Work/play")) { New-Item -ItemType Directory -Force $d | Out-Null }

$HeadFlags = @("--depth", "12", "--node-budget", "100000", "--rem", "70", "--retain", "--open-plies", "4", "--report-every", "0")
$HeadId = "ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3"
$Rungs = @(10, 20)
$SwitchTokens = @("backup=", "dmin=", "terminal=", "augment=", "ordinal=")

# Learning rates are placeholders until Pass 2 tunes them.
$Arms = [ordered]@{
    B0 = @{ Args = @("--lr", "0.01");                                                   Token = "" }
    A1 = @{ Args = @("--lr", "0.01", "--backup", "td-directed");                        Token = "backup=td-directed" }
    A2 = @{ Args = @("--lr", "0.01", "--backup", "rootstrap");                          Token = "backup=rootstrap" }
    A3 = @{ Args = @("--lr", "0.0005", "--backup", "treestrap");                        Token = "backup=treestrap" }
    A4 = @{ Args = @("--lr", "0.01", "--lambda", "1");                                  Token = "lambda=1," }
    A5 = @{ Args = @("--lr", "0.01", "--terminal", "depth");                            Token = "terminal=depth" }
    A6 = @{ Args = @("--lr", "0.01", "--augment", "mirror");                            Token = "augment=mirror" }
    A7 = @{ Args = @("--lr", "0.01", "--explore", "0.1");                               Token = "explore=0.1" }
    A8 = @{ Args = @("--lr", "0.01", "--explore-dist", "ordinal", "--ordinal-games", "20"); Token = "ordinal=0->1/20g" }
}

$out = New-Object System.Collections.Generic.List[string]
function Say([string]$s) { Write-Host $s; $out.Add($s) }

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

function Remove-LrArg($a) {
    $r = @(); for ($i = 0; $i -lt $a.Count; $i++) { if ($a[$i] -eq "--lr") { $i++; continue }; $r += $a[$i] }; return $r
}

# ---- Phase: train ----
function Train {
    $jobs = @()
    $ladder = ($Rungs -join ",")
    foreach ($k in $Arms.Keys) {
        # --games is required: without it the trainer runs to max(its default
        # 500, the top rung), so a ladder below 500 games does not stop there.
        $base = @("tdleaf") + $HeadFlags + @("--seed", "$Seed", "--games", "$($Rungs[-1])") + $Arms[$k].Args
        $jobs += @{ Args = $base + @("--ckpt-at", $ladder, "--out", "$Work/$k");       Log = "$Work/$k.log" }
        $jobs += @{ Args = $base + @("--ckpt-at", $ladder, "--out", "$Work/again/$k"); Log = "$Work/again/$k.log" }
        if ($k -ne "A7") {
            $z = @("tdleaf") + $HeadFlags + @("--seed", "$Seed", "--games", "3", "--out", "$Work/lr0/$k") + (Remove-LrArg $Arms[$k].Args) + @("--lr", "0")
            if ($k -eq "A8") { $z += @("--ordinal-start", "1", "--ordinal-end", "1") }
            $jobs += @{ Args = $z; Log = "$Work/lr0/$k.log" }
        }
    }
    Invoke-TrainJobs $jobs
}

function Teacher($path) { (Get-Content $path | Where-Object { $_ -like "teacher=*" } | Select-Object -First 1) }
function Num($s, $key) { if ($s -match "(?:^|[,(])$key=([0-9.eE+-]+)") { return [double]$Matches[1] }; return $null }
function Norm($path) { ((Get-Content $path) -replace ',secs=[^,)]*', '' -replace ',cpu=[^,)]*', '') -join "`n" }

# ---- Phase: check ----
function Check {
    $fail = 0
    Say "== 1. rungs and provenance (seed $Seed, head $HeadId, scratch init)"
    Say ("{0,-4} {1,5} {2,6} {3,10} {4,14} {5,6}  {6}" -f "arm", "rung", "games", "cpu s", "nodes", "token", "init")
    foreach ($k in $Arms.Keys) {
        $prevCpu = 0; $prevNodes = 0
        foreach ($r in $Rungs) {
            $f = "$Work/${k}_g$r.txt"
            if (-not (Test-Path $f)) { Say "FAIL $k rung $r missing"; $fail++; continue }
            $t = Teacher $f
            $g = Num $t "games"; $c = Num $t "cpu"; $n = Num $t "nodes"
            $tok = $Arms[$k].Token
            $tokOk = if ($tok -eq "") { -not ($SwitchTokens | Where-Object { $t.Contains($_) }) } else { $t.Contains($tok) }
            $init = if ($t -match "init:(\S+)") { $Matches[1] } else { "?" }
            $ok = ($g -eq $r) -and ($c -gt $prevCpu) -and ($n -gt $prevNodes) -and $tokOk -and ($init -eq "scratch")
            if (-not $ok) { $fail++ }
            Say ("{0,-4} {1,5} {2,6} {3,10:N2} {4,14:N0} {5,6}  {6}{7}" -f $k, $r, $g, $c, $n, $tokOk, $init, $(if ($ok) { "" } else { "   FAIL: $t" }))
            $prevCpu = $c; $prevNodes = $n
        }
    }

    Say ""
    Say "== 2. same seed twice (every byte except secs= and cpu=)"
    foreach ($k in $Arms.Keys) {
        $same = @()
        foreach ($r in $Rungs) {
            $a = "$Work/${k}_g$r.txt"; $b = "$Work/again/${k}_g$r.txt"
            $eq = (Test-Path $a) -and (Test-Path $b) -and ((Norm $a) -ceq (Norm $b))
            if (-not $eq) { $fail++ }
            $same += "rung $r " + $(if ($eq) { "identical" } else { "DIFFERS" })
        }
        Say ("{0,-4} {1}" -f $k, ($same -join ", "))
    }

    Say ""
    Say "== 3. lr 0: does the switch change what the search plays? (3 games, vs B0 at lr 0)"
    $ref = $null
    Say ("{0,-4} {1,14} {2,9} {3,8}  {4}" -f "arm", "search nodes", "searched", "W-B-D", "vs B0")
    foreach ($k in $Arms.Keys) {
        if ($k -eq "A7") { continue }
        $log = Get-Content "$Work/lr0/$k.log" -Raw
        $nodes = if ($log -match "s cpu, (\d+) search nodes") { [long]$Matches[1] } else { -1 }
        $srch = if ($log -match "searched moves (\d+)") { [int]$Matches[1] } else { -1 }
        $wbd = if ($log -match "\((\d+) W / (\d+) B / (\d+) draw\)") { "$($Matches[1])-$($Matches[2])-$($Matches[3])" } else { "?" }
        if ($k -eq "B0") { $ref = @($nodes, $srch, $wbd) }
        $same = ($nodes -eq $ref[0]) -and ($srch -eq $ref[1]) -and ($wbd -eq $ref[2]) -and ($nodes -gt 0)
        if (-not $same) { $fail++ }
        Say ("{0,-4} {1,14:N0} {2,9} {3,8}  {4}" -f $k, $nodes, $srch, $wbd, $(if ($same) { "same" } else { "DIFFERS" }))
    }

    Say ""
    Say "== 4. instrument readings from each arm's run log (the whole run, to its last rung)"
    foreach ($k in $Arms.Keys) {
        $lines = Get-Content "$Work/$k.log" | Select-String -Pattern "training compute|mean PV depth|backup |treestrap|mirror:|ordinal:|trained positions"
        Say "$k"
        foreach ($l in $lines) { Say "    $($l.Line.Trim())" }
    }
    Say ""
    Say "check failures: $fail"
    return $fail
}

# ---- Phase: play ----
function Invoke-PublishAndPlay {
    $rows = @{}
    if (Test-Path $Ledger) { Import-Csv $Ledger | ForEach-Object { $rows["$($_.arm)|$($_.rung)"] = $_ } }
    $slot = $SlotBase
    $plan = @()
    foreach ($k in $Arms.Keys) { foreach ($r in $Rungs) { $plan += @{ Arm = $k; Rung = $r; Slot = $slot }; $slot++ } }
    foreach ($p in $plan) {
        $dst = "models/sweep/slot$($p.Slot).txt"
        if ((Test-Path $dst) -and -not $rows.ContainsKey("$($p.Arm)|$($p.Rung)")) {
            throw "$dst exists and is not in ${Ledger}: refusing to overwrite a slot this study did not write"
        }
        Copy-Item "$Work/$($p.Arm)_g$($p.Rung).txt" $dst -Force
    }
    "arm,seed,rung,slot,model" | Out-File -FilePath $Ledger -Encoding ascii
    foreach ($p in $plan) { "$($p.Arm),$Seed,$($p.Rung),$($p.Slot),models/sweep/slot$($p.Slot).txt" | Add-Content -Path $Ledger -Encoding ascii }

    $hashes = @{}
    & $Rank check 2>&1 | Select-String 'models/sweep/slot(\d+)\.txt = ([0-9a-f]{8})' | ForEach-Object {
        $hashes[[int]$_.Matches[0].Groups[1].Value] = $_.Matches[0].Groups[2].Value
    }
    $idOf = @{}
    foreach ($p in $plan) { $idOf["$($p.Arm)|$($p.Rung)"] = "$HeadId.learned(s$($p.Slot),$($hashes[$p.Slot]))@1" }

    Say ""
    Say "== 5. real search path: each rung-20 checkpoint vs B0 rung 20 (rank.exe pairgen, 2 games, 4 random opener plies)"
    Say ("{0,-4} {1,6} {2,6} {3,6}  {4}" -f "arm", "a wins", "b wins", "draws", "id (a)")
    $fail = 0
    $b = $idOf["B0|20"]
    foreach ($k in $Arms.Keys) {
        $a = if ($k -eq "B0") { $idOf["B0|10"] } else { $idOf["$k|20"] }
        $o = "$Work/play/$k.jsonl"
        $txt = & $Rank pairgen --a $a --b $b --games 2 --open-plies 4 --seed $Seed --out $o 2>&1 | Out-String
        $m = if (Test-Path "$o.meta.json") { Get-Content "$o.meta.json" -Raw | ConvertFrom-Json } else { $null }
        if ($LASTEXITCODE -ne 0 -or $null -eq $m) { $fail++; Say "FAIL $k`n$txt"; continue }
        Say ("{0,-4} {1,6} {2,6} {3,6}  {4}" -f $k, $m.a_wins, $m.b_wins, $m.draws, $a)
    }
    Say "play failures: $fail"
    return $fail
}

$total = 0
if ($Phase -in @("train", "all")) { Train }
if ($Phase -in @("check", "all")) { $total += Check }
if ($Phase -in @("play", "all")) { $total += Invoke-PublishAndPlay }
$out | Out-File -FilePath $Summary -Encoding utf8
Write-Host "summary: $Summary"
if ($total -gt 0) { exit 2 }
