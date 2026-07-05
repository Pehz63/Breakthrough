# hill_climb.ps1 - Stochastic hill climber over the Experimental evaluator's weight mix,
# optimizing agent Elo at a fixed search depth. Fitness is rank.exe's gauntlet: it rates
# one candidate id against the frozen ranking/ratings.tsv pool and prints an anchored Elo.
#
#   .\tools\hill_climb.ps1 -Build -Iters 4 -Games 2 -Depth 2      # quick smoke
#   .\tools\hill_climb.ps1 -Iters 40 -Games 4                     # real climb at d4
#   .\tools\hill_climb.ps1 -Iters 20 -Promote -PromoteTop 2       # then rank the winners
#
# Turn is pinned at -Turn (20) and chip+wall+column+forward are renormalized to sum to
# -Sum minus -Turn (80). This pins the scale (the evaluator is scale-invariant for move
# selection), so scalar-duplicate candidates collapse to one canonical simplex point that
# dedupes the cache. The mutation shifts the relative mix, with an occasional drastic reset
# of the chip weight. Greedy acceptance from the best-so-far; drastic jumps provide escape.
#
# gauntlet writes only scratch ranking/gauntlet.jsonl (truncated each call), so the climber
# is serial and never touches the permanent store ranking/matches.jsonl. Opponent ratings
# come from ranking/ratings.tsv, so run a full `.\tools\run_rank.ps1 run` first if the
# roster changed. -Promote appends the top finds to ranking/roster.txt and does a full
# `rank.exe run` so they are rated on the shared scale.

[CmdletBinding(PositionalBinding = $false)]
param(
    [switch]$Build,
    [int]$Depth = 4,
    [string]$Head = "",                              # override the ab(...) head, e.g. "d6,tt,ord,nb200k"
    [string]$Roster = "ranking/climb_roster.txt",
    [int]$Games = 4,
    [int]$Iters = 40,
    [int]$Seed = 1,
    [double]$Drastic = 0.3,
    [int]$Sum = 100,
    [int]$Turn = 20,
    [string]$Start = "c4,w0,l0,f1",
    [switch]$Promote,
    [int]$PromoteTop = 3,
    [int]$PromoteGames = 8
)

$ErrorActionPreference = "Stop"
$Exe = ".\rank.exe"
$RosterFile = "ranking/roster.txt"
$RatingsFile = "ranking/ratings.tsv"

if ($Build) {
    Write-Host "Building rank.exe..."
    if (Test-Path $Exe) { Remove-Item $Exe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_rank.bat"
    if (-not (Test-Path $Exe)) { Write-Error "Build did not produce $Exe."; exit 1 }
}
if (-not (Test-Path $Exe)) { Write-Error "$Exe not found. Run with -Build first."; exit 1 }
if (-not (Test-Path $RatingsFile)) {
    Write-Error "$RatingsFile not found. Run '.\tools\run_rank.ps1 run' first to build the pool."
    exit 1
}

$PosSum = $Sum - $Turn
if ($PosSum -lt 4) { Write-Error "-Sum ($Sum) minus -Turn ($Turn) must leave at least 4 for the positional weights."; exit 1 }

# ---- weight helpers (order: chip, wall, column, forward) ----

# Scale a 4-vector to sum exactly $PosSum as integers (largest-remainder rounding).
function Get-NormWeights([double[]]$v) {
    [double]$total = 0.0
    for ($i = 0; $i -lt 4; $i++) { $total += $v[$i] }
    if ($total -le 0) { return @([int]$PosSum, 0, 0, 0) }
    $ints = New-Object 'int[]' 4
    $rem = New-Object 'double[]' 4
    [int]$used = 0
    for ($i = 0; $i -lt 4; $i++) {
        [double]$scaled = $v[$i] * $PosSum / $total
        [int]$fl = [math]::Floor($scaled)
        $ints[$i] = $fl
        $rem[$i] = $scaled - $fl
        $used += $fl
    }
    [int]$deficit = [int]$PosSum - $used
    # hand the leftover units to the components with the largest fractional remainder
    for ($k = 0; $k -lt $deficit; $k++) {
        [int]$bi = 0
        for ($i = 1; $i -lt 4; $i++) { if ($rem[$i] -gt $rem[$bi]) { $bi = $i } }
        $ints[$bi] += 1
        $rem[$bi] = -1.0
    }
    return @([int]$ints[0], [int]$ints[1], [int]$ints[2], [int]$ints[3])
}

function Get-StartWeights([string]$s) {
    $map = @{ c = 0.0; w = 0.0; l = 0.0; f = 0.0 }
    foreach ($tok in $s.Split(",")) {
        $t = $tok.Trim()
        if ($t.Length -lt 2) { continue }
        $letter = $t.Substring(0, 1)
        $num = [double]$t.Substring(1)
        if ($map.ContainsKey($letter)) { $map[$letter] = $num }
    }
    return @($map.c, $map.w, $map.l, $map.f)
}

function Get-CandidateId([int[]]$wts) {
    if ($Head -ne "") { $h = $Head } else { $h = "d$Depth" }
    return "ab($h)@1.exp(t$Turn,c$($wts[0]),w$($wts[1]),l$($wts[2]),f$($wts[3]))@1"
}

# ---- fitness: one gauntlet, parse the anchored Elo ----
$cache = @{}
function Get-Elo([int[]]$wts) {
    $id = Get-CandidateId $wts
    if ($cache.ContainsKey($id)) { return $cache[$id] }
    $out = & $Exe gauntlet --id $id --games $Games --roster $Roster --seed $Seed 2>&1
    $elo = $null; $se = $null
    foreach ($line in $out) {
        if ($line -match 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)') { $elo = [int]$Matches[1]; $se = [int]$Matches[2]; break }
    }
    if ($null -eq $elo) {
        Write-Host ($out -join "`n")
        Write-Error "Could not parse an Elo for id: $id"
        exit 1
    }
    $r = [pscustomobject]@{ id = $id; elo = $elo; se = $se; wts = $wts }
    $cache[$id] = $r
    return $r
}

# ---- mutation on the sum-$PosSum simplex ----
function Mutate([int[]]$b) {
    if ((Get-Random -Minimum 0.0 -Maximum 1.0) -lt $Drastic) {
        # drastic: reseed the chip weight, spread the remainder over wall/column/forward
        [int]$chip = Get-Random -Minimum 0 -Maximum ($PosSum + 1)
        [double]$rem = $PosSum - $chip
        [double]$r0 = Get-Random -Minimum 0.0 -Maximum 1.0
        [double]$r1 = Get-Random -Minimum 0.0 -Maximum 1.0
        [double]$r2 = Get-Random -Minimum 0.0 -Maximum 1.0
        [double]$rs = $r0 + $r1 + $r2
        if ($rs -le 0) { $r0 = 1.0; $r1 = 1.0; $r2 = 1.0; $rs = 3.0 }
        $v = New-Object 'double[]' 4
        $v[0] = [double]$chip
        $v[1] = $rem * $r0 / $rs
        $v[2] = $rem * $r1 / $rs
        $v[3] = $rem * $r2 / $rs
        return Get-NormWeights $v
    }
    # small step: move a delta of {1,3,5} units from one component to another. Pick the
    # source among components that actually have weight, so the step always moves something.
    $deltas = @(1, 3, 5)
    [int]$delta = $deltas[(Get-Random -Minimum 0 -Maximum $deltas.Count)]
    $nonzero = @(0..3 | Where-Object { $b[$_] -gt 0 })
    [int]$src = $nonzero[(Get-Random -Minimum 0 -Maximum $nonzero.Count)]
    [int]$dst = Get-Random -Minimum 0 -Maximum 4
    while ($dst -eq $src) { $dst = Get-Random -Minimum 0 -Maximum 4 }
    $n = New-Object 'int[]' 4
    for ($i = 0; $i -lt 4; $i++) { $n[$i] = [int]$b[$i] }
    [int]$moved = [math]::Min($delta, $n[$src])
    $n[$src] -= $moved
    $n[$dst] += $moved
    return @([int]$n[0], [int]$n[1], [int]$n[2], [int]$n[3])
}

# ---- run ----
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$log = "ranking/climb_exp_d${Depth}_$stamp.tsv"
"iter`taccepted`telo`tse`tturn`tchip`twall`tcolumn`tforward`tid" | Out-File -FilePath $log -Encoding Ascii
function Write-LogRow([int]$iter, [int]$accepted, $r) {
    "$iter`t$accepted`t$($r.elo)`t$($r.se)`t$Turn`t$($r.wts[0])`t$($r.wts[1])`t$($r.wts[2])`t$($r.wts[3])`t$($r.id)" |
        Add-Content -Path $log -Encoding Ascii
}

Write-Host "Hill climb: exp weights at head 'ab($(if ($Head -ne '') { $Head } else { "d$Depth" }))', turn=$Turn, positional sum=$PosSum"
Write-Host "  roster=$Roster games=$Games iters=$Iters seed=$Seed drastic=$Drastic  log=$log`n"

$best = Get-Elo (Get-NormWeights (Get-StartWeights $Start))
Write-LogRow 0 1 $best
Write-Host ("[  0] start   Elo {0,5} +/-{1,3}  ({2})  BEST" -f $best.elo, $best.se, $best.id)

for ($i = 1; $i -le $Iters; $i++) {
    $cand = Mutate $best.wts
    $r = Get-Elo $cand
    $accept = $r.elo -gt $best.elo
    if ($accept) { $best = $r }
    Write-LogRow $i ([int][bool]$accept) $r
    $tag = if ($accept) { "BEST" } else { "" }
    Write-Host ("[{0,3}] {1,-7} Elo {2,5} +/-{3,3}  (c{4} w{5} l{6} f{7})  {8}" -f `
        $i, $(if ($accept) { "accept" } else { "reject" }), $r.elo, $r.se, `
        $r.wts[0], $r.wts[1], $r.wts[2], $r.wts[3], $tag)
}

Write-Host "`nBest: $($best.id)  Elo $($best.elo) +/- $($best.se)"
$top = $cache.Values | Sort-Object -Property elo -Descending | Select-Object -First 5
Write-Host "Top candidates:"
$top | ForEach-Object { Write-Host ("  Elo {0,5}  {1}" -f $_.elo, $_.id) }

if ($Promote) {
    Write-Host "`nPromoting top $PromoteTop distinct candidate(s) into $RosterFile..."
    $existing = @(Get-Content $RosterFile | ForEach-Object { ($_ -replace '#.*$', '').Trim() } |
        Where-Object { $_ -ne "" } | ForEach-Object { ($_ -split '\s+', 2)[1] })
    $promoted = $cache.Values | Sort-Object -Property elo -Descending | Select-Object -First $PromoteTop
    $added = 0
    foreach ($p in $promoted) {
        if ($existing -contains $p.id) { Write-Host "  already in roster: $($p.id)"; continue }
        "on      $($p.id)" | Add-Content -Path $RosterFile -Encoding Ascii
        Write-Host "  added: on $($p.id)  (climb Elo $($p.elo))"
        $added++
    }
    if ($added -gt 0) {
        Write-Host "`nFull refit including the promoted agents (rank.exe run --games $PromoteGames)..."
        & $Exe run --games $PromoteGames
        Write-Host "`nTop of ${RatingsFile}:"
        Get-Content $RatingsFile | Select-Object -First 10
    } else {
        Write-Host "Nothing new to promote."
    }
}
