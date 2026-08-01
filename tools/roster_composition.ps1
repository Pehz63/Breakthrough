<#
.SYNOPSIS
  Break the MAIN roster (ranking/roster.txt) down by agent theme, with counts
  that sum to the total, plus in-fit Elo reference points.

.DESCRIPTION
  "The roster" in this project means ONE file: ranking/roster.txt. It is the
  fixed-start pool whose ratings are published in ranking/standings.tsv and
  ranking/ratings.tsv. Everything else that looks like a roster is NOT it:

    ranking/roster.txt        THE roster. Rated into standings.tsv/ratings.tsv.
    ranking/roster_open.txt   A SEPARATE POOL (diversified openings), its own
                              store (matches_open.jsonl) and its own outputs
                              (standings_open.tsv). Incompatible scale -- never
                              mix its numbers with the main roster's.
    ranking/roster_top.txt    A boost list: the same agents as the main roster,
                              replayed to more games/pair to resolve the top.
                              Not a separate population.
    ranking/roster_<study>.txt  Disposable per-study copies (main roster + a
                              temporary cohort) used for pinned screening. These
                              never define the roster; they are scratch.

  Themes group by TRAINING REGIME (how the agent was produced), not by model
  architecture -- two linear v2 models fit by different regimes have little in
  common, while a dist model is the same pipeline whether its mu head is linear
  or MLP. For learned agents the regime is read from the model file's own
  `teacher=` provenance line, because the canonical id cannot distinguish them
  (TD-Leaf and replay-supervised models both carry recipe=value). Every active
  agent lands in exactly one theme and the counts sum to the active total
  (asserted at the end).

  Also prints Elo reference points from the CURRENT fit -- top, champion-class,
  median, and bottom -- because a cohort number quoted without in-fit neighbours
  invites comparing it against some other fit's numbers, which is the single
  most repeated measurement error in this project (Docs/benchmarking.md,
  "A pinned fit reports on the PIN FILE'S scale, not today's").

.PARAMETER Roster
  Roster file to analyse. Defaults to the main roster; pass another only if you
  genuinely mean a different pool, and read its Elo against its OWN outputs.

.PARAMETER Standings
  Standings file for the Elo reference points. Must be the fit that rates
  -Roster, or the numbers will not correspond.
#>
param(
    [string]$Roster = "ranking/roster.txt",
    [string]$Standings = "ranking/standings.tsv",
    [switch]$IncludeOff
)

$ErrorActionPreference = "Stop"
Set-Location (Split-Path -Parent $PSScriptRoot)

if (-not (Test-Path $Roster)) { Write-Error "$Roster not found"; exit 1 }

# ---- parse roster lines ----
$agents = @()
foreach ($line in Get-Content $Roster) {
    # Strip inline comments FIRST, exactly as rankLoadRoster does in
    # src/ranking.cpp -- roster lines carry trailing "# note" text that would
    # otherwise be glued onto the id and corrupt every downstream match.
    $t = $line
    $h = $t.IndexOf('#')
    if ($h -ge 0) { $t = $t.Substring(0, $h) }
    $t = $t.Trim()
    if ($t -eq '') { continue }
    $parts = $t -split '\s+', 2
    if ($parts.Count -lt 2) { continue }
    $state = $parts[0]; $id = $parts[1].Trim()
    if ($state -notin @('on','off','anchor')) { continue }
    if (-not $IncludeOff -and $state -eq 'off') { continue }
    $agents += [PSCustomObject]@{ State = $state; Id = $id }
}

# Legacy short-form learned ids (slot+hash, no architecture fields) carry no
# theme information on their own. rank.exe canonicalizes them on load, so the
# rich form is recoverable from the standings file; build a slot -> canonical-id
# map and use it to resolve those before classifying.
$canonBySlot = @{}
if (Test-Path $Standings) {
    $sHdr = $null; $sIdI = -1
    foreach ($line in Get-Content $Standings) {
        if ($line -match '^\s*$' -or $line.StartsWith('#')) { continue }
        $c = $line -split "`t"
        if ($null -eq $sHdr) { $sHdr = $c; $sIdI = [array]::IndexOf($c,'id'); continue }
        # Use the 'id' COLUMN specifically. standings.tsv also has bare
        # 'evaluator'/'eff_evaluator' columns holding just the learned(...)
        # segment; matching the first field that looks right picks one of those
        # and silently replaces the full id with a fragment.
        if ($sIdI -lt 0 -or $c.Count -le $sIdI) { continue }
        $fullId = $c[$sIdI]
        if ($fullId -match '\.learned\(s(\d+),[0-9a-f]{8},') { $canonBySlot[$Matches[1]] = $fullId }
    }
}
foreach ($a in $agents) {
    if ($a.Id -match '\.learned\(s(\d+),[0-9a-f]{8}\)@') {
        $slot = $Matches[1]
        if ($canonBySlot.ContainsKey($slot)) { $a.Id = $canonBySlot[$slot] }
    }
}

# ---- theme classification (exactly one theme per agent) ----
# Themes group by HOW AN AGENT WAS PRODUCED (its training regime), not by its
# architecture. Two models can share an architecture (both linear v2) and have
# nothing else in common if one was fit on outcome labels and the other by an
# online bootstrap; conversely a dist model's mu head may be linear or MLP and
# it is the same pipeline either way. The regime is what a roster needs spread
# across, so that is what is counted.
#
# The canonical id CANNOT distinguish these: a TD-Leaf model and a
# replay-supervised model both carry recipe=value. The authoritative source is
# the model file's own `teacher=` provenance line, which every regime writes.
# slotFile() conventions: 0=lin_value, 1=lin_policy, 2=pst_value, 3+=sweep.
function Get-SlotFile([string]$slot) {
    switch ($slot) {
        '0' { return 'models/lin_value.txt' }
        '1' { return 'models/lin_policy.txt' }
        '2' { return 'models/pst_value.txt' }
        default { return "models/sweep/slot$slot.txt" }
    }
}
$teacherCache = @{}
function Get-Teacher([string]$slot) {
    if ($teacherCache.ContainsKey($slot)) { return $teacherCache[$slot] }
    $f = Get-SlotFile $slot
    $t = ''
    if (Test-Path $f) {
        $line = Select-String -Path $f -Pattern '^teacher=' -SimpleMatch:$false | Select-Object -First 1
        if ($line) { $t = $line.Line.Substring(8) }
    }
    $teacherCache[$slot] = $t
    return $t
}

function Get-Theme($id) {
    if ($id -match '\.learned\(s(?<slot>\d+),') {
        $slot = $Matches['slot']
        $teacher = Get-Teacher $slot
        if ($teacher -match '^tdleaf\(')     { return 'Learned: TD-Leaf (online bootstrap)' }
        if ($teacher -match '^labelstore:')  { return 'Learned: dist / position-oracle' }
        if ($teacher -match '^replay:')      { return 'Learned: supervised on replayed pool games' }
        if ($teacher -match '^ensemble\(')   { return 'Learned: ensemble (weight-averaged / mirrored)' }
        if ($teacher -match 'self-play-bootstrap') { return 'Learned: self-play bootstrap (offline)' }
        if ($teacher -match '^AlphaBeta\(') { return 'Learned: supervised on teacher self-play' }
        if ($teacher -eq '')                 { return 'Learned: regime unknown (no teacher= line)' }
        return 'Learned: other regime'
    }
    if ($id -match '\.linpol\(')  { return 'Learned: policy move-rater' }
    if ($id -match '\.adv\(')     { return 'Heuristic: Advanced evaluator' }
    if ($id -match '\.exp\(')     { return 'Heuristic: Experimental evaluator' }
    if ($id -match '\.classic\(') { return 'Heuristic: Classic chip-counter' }
    if ($id -match '^(rand|tiered|smart\(\d+\))@') { return 'Random family (no search)' }
    if ($id -match '^greedy@')    { return 'Greedy 1-ply (no evaluator segment)' }
    return 'Other / unclassified'
}

foreach ($a in $agents) { $a | Add-Member -NotePropertyName Theme -NotePropertyValue (Get-Theme $a.Id) }

$total = $agents.Count
Write-Host ""
Write-Host "MAIN ROSTER COMPOSITION -- $Roster"
Write-Host ("  {0} agents ({1})" -f $total, $(if ($IncludeOff) { "including 'off'" } else { "active only: 'on' + 'anchor'" }))
Write-Host ""

$groups = $agents | Group-Object Theme | Sort-Object Count -Descending
$w = ($groups.Name | Measure-Object -Property Length -Maximum).Maximum
Write-Host ("  {0,-$w}  {1,5}  {2,7}" -f "Theme", "Count", "Share")
Write-Host ("  {0}  {1}  {2}" -f ('-' * $w), '-----', '-------')
foreach ($g in $groups) {
    Write-Host ("  {0,-$w}  {1,5}  {2,6:N1}%" -f $g.Name, $g.Count, (100.0 * $g.Count / $total))
}
Write-Host ("  {0}  {1}  {2}" -f ('-' * $w), '-----', '-------')
Write-Host ("  {0,-$w}  {1,5}  {2,6:N1}%" -f "TOTAL", ($groups | Measure-Object Count -Sum).Sum, 100.0)

# Sum assertion: every agent must be counted exactly once.
$sum = ($groups | Measure-Object Count -Sum).Sum
if ($sum -ne $total) { Write-Warning "Theme counts sum to $sum but there are $total agents -- classification is dropping or double-counting." }

# ---- search-head breakdown (an agent is search + evaluator; heads are not comparable) ----
Write-Host ""
Write-Host "  By search head (never compare Elo across heads):"
$agents | ForEach-Object {
    $h = if ($_.Id -match '^([^.]+)') { $Matches[1] } else { '?' }
    [PSCustomObject]@{ Head = $h }
} | Group-Object Head | Sort-Object Count -Descending | ForEach-Object {
    Write-Host ("    {0,5}  {1}" -f $_.Count, $_.Name)
}

# ---- Elo reference points from the SAME fit ----
if (Test-Path $Standings) {
    $rows = @()
    $hdr = $null; $eloI = -1; $idI = -1; $headI = -1; $gI = -1
    foreach ($line in Get-Content $Standings) {
        if ($line -match '^\s*$' -or $line.StartsWith('#')) { continue }
        $c = $line -split "`t"
        if ($null -eq $hdr) {
            $hdr = $c
            $eloI = [array]::IndexOf($c,'elo'); $idI = [array]::IndexOf($c,'id')
            $headI = [array]::IndexOf($c,'head'); $gI = [array]::IndexOf($c,'games')
            continue
        }
        if ($c.Count -le [math]::Max($eloI,$idI)) { continue }
        $rows += [PSCustomObject]@{ Elo=[int][double]$c[$eloI]; Id=$c[$idI]; Head=$c[$headI]; Games=[int][double]$c[$gI] }
    }
    $std = $rows | Where-Object { $_.Head -eq 'ab(d6,tt,ord,nb200k)@1' } | Sort-Object Elo -Descending
    if ($std.Count -gt 0) {
        Write-Host ""
        Write-Host "  Elo reference points -- ALL from $Standings, head ab(d6,tt,ord,nb200k)@1."
        Write-Host "  Quote these ONLY against numbers from this same fit (Docs/benchmarking.md)."
        $picks = @(
            @{ Label='top';        Row=$std[0] },
            @{ Label='2nd';        Row=$std[1] },
            @{ Label='median';     Row=$std[[int]($std.Count/2)] },
            @{ Label='bottom';     Row=$std[-1] }
        )
        foreach ($p in $picks) {
            if ($p.Row) { Write-Host ("    {0,-8} {1,5}  {2}" -f $p.Label, $p.Row.Elo, $p.Row.Id) }
        }
    }
} else {
    Write-Warning "$Standings not found -- skipping Elo reference points."
}
Write-Host ""
