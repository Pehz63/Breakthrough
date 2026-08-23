<#
.SYNOPSIS
  Opener-division follow-up to the mlp/conv architecture sweep (round 6,
  plans/gumbel-mcts-arch-results-6-silver-thistle.md): wraps every non-REF
  round-6 checkpoint (45 mlp + 45 conv, rung=4000) with each of the 4
  non-openless CHAMPION.md categories' openers and screens every variant
  against the standard pool, to check whether the checkpoints that led the
  openless screen (M34/M14/M10/M31, see todo.md's Agent Track) are also
  strong under a book/random opener, or whether different checkpoints lead
  in those divisions instead.

.DESCRIPTION
  No retraining: every checkpoint already exists on disk (round 6's slots
  1402..1769). This script only constructs new roster IDs -- a round-6 base
  id plus one of 4 .opener(...) segments -- and gauntlet-screens them
  against the existing ranking/roster_screening_pool.txt, same method as
  the base sweep. Base ids are read directly from round 6's own
  full_agent_id_rung4000 column (plans/gumbel-mcts-arch-sweep-agents-6-
  silver-thistle.wide.tsv), never hand-transcribed, filtered to non-REF
  rows.

  Openers match ranking/CHAMPION.md's category definitions: book=15
  (4-ply, mined for learned(model=98) vs classic@2) for 4-book, book=16
  (8-ply, same pair) for 8-book, rand moves=4 for 4-random, rand moves=8
  for 8-random. book15/16 are already roster_screening_pool.txt members --
  any book at the matching ply depth is category-eligible, the category
  rule keys on ply depth, not which pair the book was mined for.

  Caveat: gaz(...) is not ab(deep=6,tt,ord,nodes=200k)@1, so per
  CHAMPION.md's one-head rule a gaz agent is never eligible for a category
  TITLE regardless of its score here. This is a screening exploration to
  find promising checkpoint/opener combinations, not a title challenge.

  export_cohort_results.ps1 is not reused for the results table: it joins
  a ledger to standings by model SLOT alone, but here 4 opener variants
  share one base slot, so a slot-keyed join would silently collapse to one
  of the four. ExportResults below joins by the full canonical id instead.

.PARAMETER Workers
.PARAMETER Phase
  all | roster | play | screen | export.
#>
param(
    [int]$Workers = [Math]::Max(1, [Environment]::ProcessorCount - 2),
    [ValidateSet("all","roster","play","screen","export")]
    [string]$Phase = "all",
    [int]$GamesPerPair = 16
)

$ErrorActionPreference = "Stop"
$Root        = Split-Path -Parent $PSScriptRoot
$RankExe     = Join-Path $Root "rank.exe"
$SweepDir    = Join-Path $Root "models\sweep"
$SourceWide  = Join-Path $Root "plans\gumbel-mcts-arch-sweep-agents-6-silver-thistle.wide.tsv"
$RosterOut   = Join-Path $Root "ranking\roster_gumbelzero_arch_opener.txt"
$CohortOut   = Join-Path $Root "ranking\cohort_gumbelzero_arch_opener.txt"
$BaseRoster  = Join-Path $Root "ranking\roster_screening_pool.txt"
$Standings   = Join-Path $Root "ranking\standings.tsv"
$PinnedStandings = Join-Path $Root "ranking\standings_screen_gzarchopener_pinned.tsv"
$PinnedReport    = Join-Path $Root "ranking\report_screen_gzarchopener_pinned.md"
$ScreenStore = Join-Path $Root "ranking\matches_screen_gzarchopener.jsonl"
$ResultsOut  = Join-Path $Root "plans\gumbel-mcts-arch-opener-agents-6-silver-thistle.tsv"

# One representative opener per CHAMPION.md non-openless category.
$Openers = @(
    [ordered]@{ Cat="4book";   Seg=".opener(book,book=15)@1" }
    [ordered]@{ Cat="8book";   Seg=".opener(book,book=16)@1" }
    [ordered]@{ Cat="4random"; Seg=".opener(rand,moves=4)@1" }
    [ordered]@{ Cat="8random"; Seg=".opener(rand,moves=8)@1" }
)

function LoadBaseIds {
    $lines = Get-Content $SourceWide | Where-Object { $_ -notmatch '^#' }
    $wide = $lines | ConvertFrom-Csv -Delimiter "`t"
    $cands = $wide | Where-Object { $_.block -notlike "REF*" }
    $out = @()
    foreach ($row in $cands) {
        $out += [PSCustomObject]@{ Block = $row.block; ModelType = $row.modeltype; Id = $row.full_agent_id_rung4000 }
    }
    return $out
}

function BuildRoster {
    $base = LoadBaseIds
    $mlpCount  = @($base | Where-Object { $_.ModelType -eq 'mlp' }).Count
    $convCount = @($base | Where-Object { $_.ModelType -eq 'conv' }).Count
    Write-Host "  base checkpoints: $($base.Count) ($mlpCount mlp, $convCount conv) x $($Openers.Count) openers"

    $rows = @()
    foreach ($b in $base) {
        foreach ($o in $Openers) {
            $rows += [PSCustomObject]@{ Block = $b.Block; ModelType = $b.ModelType; Category = $o.Cat; Id = "$($b.Id)$($o.Seg)" }
        }
    }

    $marker = "# ---- Gumbel-Zero arch-sweep opener-division follow-up ($($rows.Count) agents), added $(Get-Date -Format yyyy-MM-dd) ----"
    Get-Content $BaseRoster | Out-File -FilePath $RosterOut -Encoding ascii
    "" | Add-Content -Path $RosterOut -Encoding ascii
    $marker | Add-Content -Path $RosterOut -Encoding ascii
    ($rows | ForEach-Object { "on $($_.Id)" }) | Add-Content -Path $RosterOut -Encoding ascii

    $canonLines = & $RankExe canon --roster $RosterOut
    $canonLines | Out-File -FilePath $RosterOut -Encoding ascii

    $afterMarker = $false
    $canonLines | ForEach-Object {
        if ($_ -eq $marker) { $script:afterMarker = $true; return }
        if ($script:afterMarker -and $_ -match '^on\s+(\S.*)$') { $Matches[1] }
    } | Out-File -FilePath $CohortOut -Encoding ascii

    # Side table for ExportResults (block/modeltype/category survive canon).
    $rows | Export-Csv -Path (Join-Path $SweepDir "gza_opener_rows.csv") -NoTypeInformation -Encoding ascii

    Write-Host "  roster -> $RosterOut ($($rows.Count) cohort agents appended, base = $BaseRoster)"
    Write-Host "  cohort -> $CohortOut"
    & $RankExe check --roster $RosterOut 2>&1 | Select-String -NotMatch '^model hash' | Select-Object -Last 4
}

function PlayCohort {
    $ids = Get-Content $CohortOut | Where-Object { $_.Trim() -ne "" }
    Write-Host "  gauntlet-playing $($ids.Count) checkpoints at $GamesPerPair games/opponent vs roster_screening_pool.txt, $Workers shards"

    $shardFiles = @()
    $buckets = New-Object 'System.Collections.Generic.List[System.Collections.Generic.List[string]]'
    for ($w = 0; $w -lt $Workers; $w++) { $buckets.Add((New-Object 'System.Collections.Generic.List[string]')) }
    for ($i = 0; $i -lt $ids.Count; $i++) { $buckets[$i % $Workers].Add($ids[$i]) }

    $jobs = @()
    for ($w = 0; $w -lt $Workers; $w++) {
        if ($buckets[$w].Count -eq 0) { continue }
        $shardStore = "$ScreenStore.$w"
        $shardFiles += $shardStore
        $bat = Join-Path $SweepDir "gza_op_play_worker$w.bat"
        $lines = @("@echo off")
        foreach ($id in $buckets[$w]) {
            $lines += "`"$RankExe`" gauntlet --roster `"$BaseRoster`" --in `"$shardStore`" --id `"$id`" --games $GamesPerPair --keep"
            $lines += "if errorlevel 1 exit /b 1"
        }
        $lines += "exit /b 0"
        $lines | Out-File -FilePath $bat -Encoding ascii
        $p = Start-Process -FilePath "cmd.exe" -ArgumentList "/c", "`"$bat`"" -PassThru -NoNewWindow `
                 -RedirectStandardOutput "$SweepDir\gza_op_play_worker$w.log"
        [void]$p.Handle
        $jobs += $p
    }
    Write-Host "  waiting for $($jobs.Count) shard(s)..."
    $jobs | ForEach-Object { $_.WaitForExit() }
    $bad = $jobs | Where-Object { $_.ExitCode -ne 0 }
    if ($bad) { throw "  $($bad.Count) gauntlet shard(s) exited non-zero; store NOT merged, inspect gza_op_play_worker*.log" }
    foreach ($sf in $shardFiles) {
        if (Test-Path $sf) {
            Get-Content $sf | Add-Content -Path $ScreenStore -Encoding Ascii
            Remove-Item $sf -Force
        }
    }
    Write-Host "  merged $($shardFiles.Count) shard(s) -> $ScreenStore"
}

function ScreenCohort {
    Write-Host "  pinned fit over $ScreenStore (screening pool frozen at ranking/standings.tsv)"
    & $RankExe rate --roster $RosterOut --in $ScreenStore --pin $Standings
}

function ExportResults {
    $rows = Import-Csv (Join-Path $SweepDir "gza_opener_rows.csv")
    $standingsLines = Get-Content $PinnedStandings | Where-Object { $_ -notmatch '^#' }
    $standingsRows = $standingsLines | ConvertFrom-Csv -Delimiter "`t"
    $byId = @{}
    foreach ($sr in $standingsRows) { $byId[$sr.id] = $sr }

    $out = @()
    $unmatched = 0
    foreach ($r in $rows) {
        if ($byId.ContainsKey($r.Id)) {
            $sr = $byId[$r.Id]
            $out += [PSCustomObject]@{
                block = $r.Block; modeltype = $r.ModelType; category = $r.Category
                elo = $sr.elo; elo_pm = $sr.pm; games = $sr.games; cpu_ms_move = $sr.cpu_ms_move
                full_agent_id = $r.Id
            }
        } else {
            $unmatched++
            $out += [PSCustomObject]@{
                block = $r.Block; modeltype = $r.ModelType; category = $r.Category
                elo = ""; elo_pm = ""; games = ""; cpu_ms_move = ""
                full_agent_id = $r.Id
            }
        }
    }
    if ($unmatched -gt 0) { Write-Warning "  $unmatched of $($rows.Count) row(s) have no matching standings row" }

    $sw = New-Object System.IO.StreamWriter($ResultsOut, $false, [System.Text.Encoding]::ASCII)
    try {
        $sw.WriteLine("# Gumbel-Zero mlp/conv architecture sweep (round 6) opener-division follow-up: every non-REF rung=4000 checkpoint (45 mlp + 45 conv) wrapped with each of the 4 non-openless CHAMPION.md categories' openers.")
        $sw.WriteLine("# elo/games/cpu_ms_move columns are from a PINNED screening fit against ranking/roster_screening_pool.txt: screening only, not certification. gaz(...) is not category-eligible for a title (CHAMPION.md one-head rule) -- this ranks candidates for a possible future certification push, per division.")
        $sw.WriteLine("block`tmodeltype`tcategory`telo`telo_pm`tgames`tcpu_ms_move`tfull_agent_id")
        foreach ($row in $out) {
            $sw.WriteLine("$($row.block)`t$($row.modeltype)`t$($row.category)`t$($row.elo)`t$($row.elo_pm)`t$($row.games)`t$($row.cpu_ms_move)`t$($row.full_agent_id)")
        }
    } finally { $sw.Close() }
    Write-Host "wrote $($out.Count) rows to $ResultsOut"
}

switch ($Phase) {
    "roster" { BuildRoster }
    "play"   { PlayCohort }
    "screen" { ScreenCohort }
    "export" { ExportResults }
    "all"    { BuildRoster; PlayCohort; ScreenCohort; ExportResults }
}
