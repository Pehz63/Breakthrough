# hill_climb.ps1 - Stochastic hill climber over the Advanced evaluator's weight mix,
# optimizing agent Elo at a fixed search depth. Fitness is rank.exe's gauntlet: it rates
# one candidate id against the frozen ranking/ratings.tsv pool and prints an anchored Elo.
#
#   .\tools\hill_climb.ps1 -Build -Iters 4 -Games 2 -Depth 2      # quick smoke
#   .\tools\hill_climb.ps1 -Iters 60 -Games 4                     # real climb at d4
#   .\tools\hill_climb.ps1 -Iters 60 -Games 4 -AllowNegative      # signed-weight climb
#   .\tools\hill_climb.ps1 -Iters 20 -Promote -PromoteTop 2       # then rank the winners
#
# Climbed weights (13): chip, wall, column, forward, support, center, mobility, hole,
# control, open, race, overext, noise. Turn is pinned at -Turn (20), the noise seed at
# -NoiseSeed (1), and the RaceWin detector at -RaceWin (1; it is a proven-sound win
# check, not a mix weight). The 13 weights are renormalized so their ABSOLUTE values
# sum to -Sum minus -Turn (80). This pins the scale (the evaluator is scale-invariant
# for move selection), so scalar-duplicate candidates collapse to one canonical simplex
# point that dedupes the cache.
#
# -AllowNegative lets weights go negative (sign flips, signed drastic resets), which is
# the only way the climber can reach e.g. the pure-capacity direction (forward positive,
# chip negative in a 1:-7 ratio) or the negative wall/column hypothesis. Default off:
# the historical non-negative search, for A/B comparison against the signed mode.
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
    [int]$Iters = 60,
    [int]$Seed = 1,
    [double]$Drastic = 0.3,
    [double]$FlipProb = 0.15,                        # sign-flip mutation share (AllowNegative only)
    [int]$Sum = 100,
    [int]$Turn = 20,
    [int]$NoiseSeed = 1,
    [int]$RaceWin = 1,
    [string]$Start = "c80",
    [switch]$AllowNegative,
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

# Seed PowerShell's RNG so the mutation stream is reproducible per -Seed (the
# gauntlet games are already seeded; this makes the climb itself replayable and
# lets two modes at the same seed draw comparable mutation sequences).
$null = Get-Random -SetSeed $Seed

# ---- weight vector: 13 climbed components, ID letters in adv(...) order ----
$Letters = @('c', 'w', 'l', 'f', 'd', 'e', 'm', 'h', 'b', 'o', 'r', 'x', 'n')
$NW = $Letters.Count

# Scale a 13-vector so the ABSOLUTE values sum exactly to $PosSum as integers
# (largest-remainder rounding on the magnitudes, signs preserved).
function Get-NormWeights([double[]]$v) {
    [double]$total = 0.0
    for ($i = 0; $i -lt $NW; $i++) { $total += [math]::Abs($v[$i]) }
    if ($total -le 0) {
        $out = New-Object 'int[]' $NW
        $out[0] = [int]$PosSum
        return ,$out
    }
    $ints = New-Object 'int[]' $NW
    $sign = New-Object 'int[]' $NW
    $rem = New-Object 'double[]' $NW
    [int]$used = 0
    for ($i = 0; $i -lt $NW; $i++) {
        if ($v[$i] -lt 0) { $sign[$i] = -1 } else { $sign[$i] = 1 }
        [double]$scaled = [math]::Abs($v[$i]) * $PosSum / $total
        [int]$fl = [math]::Floor($scaled)
        $ints[$i] = $fl
        $rem[$i] = $scaled - $fl
        $used += $fl
    }
    [int]$deficit = [int]$PosSum - $used
    # hand the leftover units to the components with the largest fractional remainder
    for ($k = 0; $k -lt $deficit; $k++) {
        [int]$bi = 0
        for ($i = 1; $i -lt $NW; $i++) { if ($rem[$i] -gt $rem[$bi]) { $bi = $i } }
        $ints[$bi] += 1
        $rem[$bi] = -1.0
    }
    $out = New-Object 'int[]' $NW
    for ($i = 0; $i -lt $NW; $i++) { $out[$i] = $ints[$i] * $sign[$i] }
    return ,$out
}

function Get-StartWeights([string]$s) {
    $v = New-Object 'double[]' $NW
    foreach ($tok in $s.Split(",")) {
        $t = $tok.Trim()
        if ($t.Length -lt 2) { continue }
        $letter = $t.Substring(0, 1)
        $num = [double]$t.Substring(1)
        [int]$idx = [array]::IndexOf($Letters, $letter)
        if ($idx -ge 0) { $v[$idx] = $num }
    }
    return ,$v
}

function Get-CandidateId([int[]]$w) {
    if ($Head -ne "") { $h = $Head } else { $h = "d$Depth" }
    $parts = New-Object System.Collections.Generic.List[string]
    $parts.Add("t$Turn") | Out-Null
    for ($i = 0; $i -lt $NW; $i++) { $parts.Add("$($Letters[$i])$($w[$i])") | Out-Null }
    $parts.Add("s$NoiseSeed") | Out-Null
    $parts.Add("g$RaceWin") | Out-Null
    return "ab($h)@1.adv(" + ($parts -join ",") + ")@1"
}

# ---- fitness: one gauntlet, parse the anchored Elo ----
$cache = @{}
function Get-Elo([int[]]$w) {
    $id = Get-CandidateId $w
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
    $r = [pscustomobject]@{ id = $id; elo = $elo; se = $se; wts = $w }
    $cache[$id] = $r
    return $r
}

# ---- mutation on the |sum| = $PosSum simplex ----
function Mutate([int[]]$b) {
    [double]$roll = Get-Random -Minimum 0.0 -Maximum 1.0
    if ($roll -lt $Drastic) {
        # drastic: reseed the chip weight, spread the remainder over the other 12.
        # In signed mode chip and each spread component get a random sign.
        [int]$chip = Get-Random -Minimum 0 -Maximum ($PosSum + 1)
        [double]$rem = $PosSum - $chip
        $r = New-Object 'double[]' ($NW - 1)
        [double]$rs = 0.0
        for ($i = 0; $i -lt $NW - 1; $i++) { $r[$i] = Get-Random -Minimum 0.0 -Maximum 1.0; $rs += $r[$i] }
        if ($rs -le 0) { for ($i = 0; $i -lt $NW - 1; $i++) { $r[$i] = 1.0 }; $rs = [double]($NW - 1) }
        $v = New-Object 'double[]' $NW
        $v[0] = [double]$chip
        if ($AllowNegative) {
            if ((Get-Random -Minimum 0.0 -Maximum 1.0) -lt 0.25) { $v[0] = -$v[0] }
        }
        for ($i = 1; $i -lt $NW; $i++) {
            $v[$i] = $rem * $r[$i - 1] / $rs
            if ($AllowNegative) {
                if ((Get-Random -Minimum 0.0 -Maximum 1.0) -lt 0.3) { $v[$i] = -$v[$i] }
            }
        }
        return Get-NormWeights $v
    }
    $nonzero = @(0..($NW - 1) | Where-Object { $b[$_] -ne 0 })
    if ($AllowNegative -and $roll -lt ($Drastic + $FlipProb) -and $nonzero.Count -gt 0) {
        # sign flip: negate one nonzero component (|sum| unchanged)
        [int]$fi = $nonzero[(Get-Random -Minimum 0 -Maximum $nonzero.Count)]
        $n = New-Object 'int[]' $NW
        for ($i = 0; $i -lt $NW; $i++) { $n[$i] = [int]$b[$i] }
        $n[$fi] = -$n[$fi]
        return ,$n
    }
    # small step: move a delta of {1,3,5} magnitude units from one component to another.
    # The source shrinks toward 0; the destination grows away from 0 in its own sign
    # direction (a zero destination grows positive, or randomly signed in signed mode).
    $deltas = @(1, 3, 5)
    [int]$delta = $deltas[(Get-Random -Minimum 0 -Maximum $deltas.Count)]
    [int]$src = $nonzero[(Get-Random -Minimum 0 -Maximum $nonzero.Count)]
    [int]$dst = Get-Random -Minimum 0 -Maximum $NW
    while ($dst -eq $src) { $dst = Get-Random -Minimum 0 -Maximum $NW }
    $n = New-Object 'int[]' $NW
    for ($i = 0; $i -lt $NW; $i++) { $n[$i] = [int]$b[$i] }
    [int]$moved = [math]::Min($delta, [math]::Abs($n[$src]))
    if ($n[$src] -lt 0) { $n[$src] += $moved } else { $n[$src] -= $moved }
    [int]$dstSign = 1
    if ($n[$dst] -lt 0) { $dstSign = -1 }
    elseif ($n[$dst] -eq 0 -and $AllowNegative) {
        if ((Get-Random -Minimum 0.0 -Maximum 1.0) -lt 0.3) { $dstSign = -1 }
    }
    $n[$dst] += $moved * $dstSign
    return ,$n
}

# ---- run ----
$mode = "nonneg"
if ($AllowNegative) { $mode = "signed" }
$stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$log = "ranking/climb_adv_${mode}_d${Depth}_$stamp.tsv"
$hdrW = ($Letters | ForEach-Object { $_ }) -join "`t"
"iter`taccepted`telo`tse`tturn`t$hdrW`tid" | Out-File -FilePath $log -Encoding Ascii
function Write-LogRow([int]$iter, [int]$accepted, $r) {
    $wcols = ($r.wts | ForEach-Object { $_ }) -join "`t"
    "$iter`t$accepted`t$($r.elo)`t$($r.se)`t$Turn`t$wcols`t$($r.id)" |
        Add-Content -Path $log -Encoding Ascii
}

$headTxt = "d$Depth"
if ($Head -ne "") { $headTxt = $Head }
Write-Host "Hill climb: adv weights at head 'ab($headTxt)', mode=$mode, turn=$Turn, |positional| sum=$PosSum"
Write-Host "  roster=$Roster games=$Games iters=$Iters seed=$Seed drastic=$Drastic noiseseed=$NoiseSeed racewin=$RaceWin  log=$log`n"

$best = Get-Elo (Get-NormWeights (Get-StartWeights $Start))
Write-LogRow 0 1 $best
Write-Host ("[  0] start   Elo {0,5} +/-{1,3}  ({2})  BEST" -f $best.elo, $best.se, $best.id)

for ($i = 1; $i -le $Iters; $i++) {
    $cand = Mutate $best.wts
    $r = Get-Elo $cand
    $accept = $r.elo -gt $best.elo
    if ($accept) { $best = $r }
    Write-LogRow $i ([int][bool]$accept) $r
    $tag = ""
    if ($accept) { $tag = "BEST" }
    $verb = "reject"
    if ($accept) { $verb = "accept" }
    $wtxt = ""
    for ($k = 0; $k -lt $NW; $k++) { $wtxt += "$($Letters[$k])$($r.wts[$k]) " }
    Write-Host ("[{0,3}] {1,-7} Elo {2,5} +/-{3,3}  ({4}) {5}" -f $i, $verb, $r.elo, $r.se, $wtxt.TrimEnd(), $tag)
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
