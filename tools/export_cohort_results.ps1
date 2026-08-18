<#
.SYNOPSIS
  Generic "agents trained x results" export: joins a training ledger CSV
  against a rank.exe pinned-fit rating output, so a cohort study's full
  training configuration and its measured results live in one flat table.

.DESCRIPTION
  Every cohort-study driver in this project (gumbelzero_joint_study.ps1,
  gumbelzero_study.ps1, gumbelzero_study_roundb.ps1, tdleaf_study.ps1, ...)
  writes a resumable per-checkpoint ledger to models/sweep/*_study.csv with a
  `slot` column plus whatever training-config columns that study swept, then
  screens the cohort into ranking/standings_screen_<name>_pinned.tsv +
  ranking/report_screen_<name>_pinned.md (Docs/ranking-workflow.md Workflow
  A). This script is deliberately ledger-schema-agnostic: it does not know or
  assume which axes a given study swept, so it works unchanged for any future
  cohort study that follows the same slot-keyed-ledger + pinned-fit pattern.
  It reads the ledger's columns verbatim (whatever they are) and appends a
  fixed set of measured-result columns, joined on the model slot number
  embedded in each agent's canonical id (`learned(model=<slot>,...)`).

  Playstyle columns (avg_plies, end_piece_margin, eff_elo_per_log2cpu,
  nodes_per_move, wl_as_white/black) come from the pinned report.md's
  Ratings table, which rank.exe already aggregates per agent from the
  underlying games -- this script does not recompute them.

  A ledger row whose slot has no matching standings row (e.g. training
  finished but the play/screen phase has not reached it yet) is still
  written, with the result columns left blank, rather than silently dropped
  -- a partial run should stay visible, not disappear.

  Output is a plain tab-separated file (no CSV quoting), matching the
  project's other *.tsv outputs (ranking/standings.tsv, ranking/games.tsv).

.PARAMETER Ledger
  Path to the study's ledger CSV. Must have a `slot` column (int); every
  other column is passed through unchanged, in its original order.

.PARAMETER PinnedStandings
  Path to the pinned-fit standings tsv (rank.exe rate --pin output).

.PARAMETER PinnedReport
  Path to the matching pinned report.md (for playstyle columns). Defaults to
  PinnedStandings with "standings" -> "report" and extension -> .md, which
  is what every study driver's ScreenCohort phase actually writes. Pass an
  explicit path, or "" to skip playstyle columns entirely, if that default
  does not apply.

.PARAMETER Out
  Output tsv path.

.PARAMETER HeaderComment
  Optional extra `#`-prefixed lines written at the top of the output, for
  study-specific caveats (e.g. "training search shape is fixed at engine
  defaults for every row"). One array element per line; do not include the
  leading `#` yourself.

.EXAMPLE
  .\tools\export_cohort_results.ps1 `
      -Ledger models/sweep/gumbelzero_joint_study.csv `
      -PinnedStandings ranking/standings_screen_gzjoint_pinned.tsv `
      -Out plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv
#>
param(
    [Parameter(Mandatory=$true)][string]$Ledger,
    [Parameter(Mandatory=$true)][string]$PinnedStandings,
    [string]$PinnedReport = $null,
    [Parameter(Mandatory=$true)][string]$Out,
    [string[]]$HeaderComment = @()
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $Ledger)) { throw "Ledger not found: $Ledger" }
if (-not (Test-Path $PinnedStandings)) { throw "PinnedStandings not found: $PinnedStandings" }

# [string] params can never actually hold $null (PowerShell coerces a $null
# bind to ""), so an explicit -PinnedReport "" (skip playstyle columns) is
# indistinguishable from "not passed" unless we check PSBoundParameters.
if (-not $PSBoundParameters.ContainsKey('PinnedReport')) {
    $PinnedReport = $PinnedStandings -replace 'standings', 'report' -replace '\.tsv$', '.md'
}

# ---- load ledger, preserving column order ----
$ledgerRows = @(Import-Csv $Ledger)
if ($ledgerRows.Count -eq 0) { throw "Ledger is empty: $Ledger" }
if (-not ($ledgerRows[0].PSObject.Properties.Name -contains "slot")) {
    throw "Ledger has no 'slot' column: $Ledger"
}
$ledgerCols = @($ledgerRows[0].PSObject.Properties.Name)
$ledgerSlots = @{}
foreach ($lr in $ledgerRows) { $ledgerSlots[[int]$lr.slot] = $true }

# ---- load pinned standings, keyed by model slot parsed from id ----
# Only slots the ledger actually asks about are indexed: a low slot number
# (e.g. model=98) can appear in many screening-pool agent ids (the champion
# itself, plus every book variant wearing it), and those collide harmlessly
# on the regex below since this study's own slots never reuse pool numbers --
# indexing only ledger slots keeps that collision out of the output entirely.
$standingsLines = Get-Content $PinnedStandings | Where-Object { $_ -notmatch '^#' }
$standingsRows = $standingsLines | ConvertFrom-Csv -Delimiter "`t"
$standingsBySlot = @{}
foreach ($row in $standingsRows) {
    if ($row.id -match 'learned\(model=(\d+),') {
        $slot = [int]$Matches[1]
        if (-not $ledgerSlots.ContainsKey($slot)) { continue }
        if ($standingsBySlot.ContainsKey($slot)) {
            Write-Warning "  duplicate standings row for ledger slot $slot -- keeping the last one seen"
        }
        $standingsBySlot[$slot] = $row
    }
}

# ---- load pinned report's Ratings table for playstyle columns (optional) ----
$playstyleById = @{}
$havePlaystyle = $false
if ($PinnedReport -and (Test-Path $PinnedReport)) {
    $havePlaystyle = $true
    $inTable = $false
    foreach ($line in Get-Content $PinnedReport) {
        if ($line.StartsWith("| rank |")) { $inTable = $true; continue }
        if ($inTable -and $line.StartsWith("|---")) { continue }
        if ($inTable -and -not $line.StartsWith("|")) { break }
        if ($inTable) {
            $cells = ($line.Trim().Trim('|') -split '\|') | ForEach-Object { $_.Trim() }
            # rank,Elo,+/-,games,W-L White,W-L Black,avg plies,margin,cpu/mv,eff,wall/mv,nodes/mv,state,id
            if ($cells.Count -lt 14) { continue }
            $agentId = $cells[13].Trim('`')
            $playstyleById[$agentId] = [ordered]@{
                wl_as_white          = $cells[4]
                wl_as_black          = $cells[5]
                avg_plies            = $cells[6]
                end_piece_margin     = $cells[7]
                eff_elo_per_log2cpu  = $cells[9]
                nodes_per_move       = $cells[11]
            }
        }
    }
} else {
    Write-Warning "  no pinned report found at '$PinnedReport' -- playstyle columns will be blank"
}

$resultCols = @(
    "elo", "elo_pm", "games", "cpu_ms_move",
    "wl_as_white", "wl_as_black", "avg_plies", "end_piece_margin",
    "eff_elo_per_log2cpu", "nodes_per_move",
    "search_head_id", "model_id", "full_agent_id"
)

$unmatched = 0
$outRows = @()
foreach ($lr in $ledgerRows) {
    $slot = [int]$lr.slot
    $vals = [ordered]@{}
    foreach ($c in $ledgerCols) { $vals[$c] = $lr.$c }

    if ($standingsBySlot.ContainsKey($slot)) {
        $sr = $standingsBySlot[$slot]
        $ps = if ($playstyleById.ContainsKey($sr.id)) { $playstyleById[$sr.id] } else { @{} }
        $vals["elo"]                  = $sr.elo
        $vals["elo_pm"]               = $sr.pm
        $vals["games"]                = $sr.games
        $vals["cpu_ms_move"]          = $sr.cpu_ms_move
        $vals["wl_as_white"]          = $ps["wl_as_white"]
        $vals["wl_as_black"]          = $ps["wl_as_black"]
        $vals["avg_plies"]            = $ps["avg_plies"]
        $vals["end_piece_margin"]     = $ps["end_piece_margin"]
        $vals["eff_elo_per_log2cpu"]  = $ps["eff_elo_per_log2cpu"]
        $vals["nodes_per_move"]       = $ps["nodes_per_move"]
        $vals["search_head_id"]       = $sr.head
        $vals["model_id"]             = $sr.evaluator
        $vals["full_agent_id"]        = $sr.id
    } else {
        $unmatched++
        foreach ($c in $resultCols) { $vals[$c] = "" }
    }
    $outRows += [PSCustomObject]$vals
}

if ($unmatched -gt 0) {
    Write-Warning "  $unmatched of $($ledgerRows.Count) ledger row(s) have no matching standings row (left blank) -- training ran ahead of rating, or the pinned fit is stale"
}

$allCols = $ledgerCols + $resultCols
$sw = New-Object System.IO.StreamWriter($Out, $false, [System.Text.Encoding]::ASCII)
try {
    foreach ($c in $HeaderComment) { $sw.WriteLine("# $c") }
    $sw.WriteLine("# Generated by tools/export_cohort_results.ps1 from:")
    $sw.WriteLine("#   ledger:           $Ledger")
    $sw.WriteLine("#   pinned standings: $PinnedStandings")
    if ($havePlaystyle) { $sw.WriteLine("#   pinned report:    $PinnedReport") }
    $sw.WriteLine(($allCols -join "`t"))
    foreach ($row in $outRows) {
        $sw.WriteLine((($allCols | ForEach-Object { $row.$_ }) -join "`t"))
    }
} finally {
    $sw.Close()
}

Write-Host "wrote $($outRows.Count) rows ($($allCols.Count) columns) to $Out"
