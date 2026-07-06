# sweep_pst.ps1 - Train a grid of sparse piece-square (feature-version 2) value
# models varying teacher depth and self-play game count, then rank each one on
# the real Elo ladder via rank.exe gauntlet, so "does a stronger teacher help"
# and "does more training data help" become measured answers instead of guesses.
#
# Usage (always run from the project root):
#   .\tools\sweep_pst.ps1 -Build
#   .\tools\sweep_pst.ps1 -Depths 2,4,6 -Games 100,250 -GauntletGames 4
#
# Mechanics: each grid cell trains a model to models/sweep/pst_d<D>_g<G>.txt,
# then (since rank.exe's learned-agent identity is the content hash of the FIXED
# convention path models/pst_value.txt for slot 2) copies that file over
# models/pst_value.txt, reads its hash back via "rank.exe check", and gauntlets
# "<SearchWrap>.learned(s2,<hash>)@1" against the frozen roster. Elo, cpu/move,
# and nodes/move come out of the gauntlet's own output and its scratch
# ranking/gauntlet.jsonl. All models/sweep/*.txt are throwaway and gitignored;
# only the printed/report table is the durable artifact.

param(
    [switch]$Build,
    [int[]]$Depths = @(2, 4, 6),
    [int[]]$Games = @(100, 250),
    [int]$Epochs = 6,
    [double]$Lr = 0.05,
    [double]$GenRandom = 0.2,
    [string]$GenEval = "Classic",
    [int]$Seed = 20260706,
    [int]$GauntletGames = 4,
    [string]$SearchWrap = "ab(d6,tt,ord,nb200k)@1",
    [string]$OutDir = "models/sweep",
    [string]$Report = "models/sweep/report.md"
)

$ErrorActionPreference = "Stop"
$TrainExe = ".\train.exe"
$RankExe = ".\rank.exe"

if ($Build) {
    Write-Host "Building train.exe..."
    if (Test-Path $TrainExe) { Remove-Item $TrainExe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_train.bat"
    if (-not (Test-Path $TrainExe)) { Write-Error "Build did not produce $TrainExe."; exit 1 }
    Write-Host "Building rank.exe..."
    if (Test-Path $RankExe) { Remove-Item $RankExe -Force -ErrorAction SilentlyContinue }
    cmd /c ".\build_rank.bat"
    if (-not (Test-Path $RankExe)) { Write-Error "Build did not produce $RankExe."; exit 1 }
}
if (-not (Test-Path $TrainExe)) { Write-Error "$TrainExe not found. Run with -Build first."; exit 1 }
if (-not (Test-Path $RankExe))  { Write-Error "$RankExe not found. Run with -Build first."; exit 1 }
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force -Path $OutDir | Out-Null }

$slotFile = "models/pst_value.txt"
$results = @()
$cellNum = 0
$cellTotal = $Depths.Count * $Games.Count

foreach ($d in $Depths) {
    foreach ($g in $Games) {
        $cellNum++
        $tag = "d${d}_g${g}"
        $outBase = "$OutDir/pst_$tag"
        $modelFile = "$outBase.txt"
        Write-Host ""
        Write-Host "=== [$cellNum/$cellTotal] teacher depth=$d  games=$g  epochs=$Epochs ==="

        $trainOut = & $TrainExe selfplay-supervised `
            --out $outBase --feature-version 2 --games $g --epochs $Epochs --lr $Lr `
            --gen-depth $d --gen-random $GenRandom --gen-eval $GenEval --seed $Seed 2>&1
        $trainOut | ForEach-Object { Write-Host "  $_" }

        $lossLines = $trainOut | Select-String 'loss=([0-9.]+)'
        $finalLoss = $null
        if ($lossLines.Count -gt 0) {
            $finalLoss = [double]$lossLines[$lossLines.Count - 1].Matches[0].Groups[1].Value
        }
        $wrLine = $trainOut | Select-String 'winrate_vs_random=([0-9.]+)\s+elo~(-?\d+)' | Select-Object -Last 1
        $trainWinrate = $null; $trainEloProxy = $null
        if ($wrLine) {
            $trainWinrate = [double]$wrLine.Matches[0].Groups[1].Value
            $trainEloProxy = [int]$wrLine.Matches[0].Groups[2].Value
        }

        if (-not (Test-Path $modelFile)) {
            Write-Warning "  no model produced at $modelFile, skipping gauntlet for this cell"
            continue
        }
        Copy-Item -Path $modelFile -Destination $slotFile -Force

        $checkOut = & $RankExe check 2>&1
        $hashLine = $checkOut | Select-String 'models/pst_value\.txt = ([0-9a-f]+) \(slot 2\)'
        if (-not $hashLine) { Write-Warning "  could not read model hash, skipping gauntlet"; continue }
        $hash = $hashLine.Matches[0].Groups[1].Value
        $candId = "$SearchWrap.learned(s2,$hash)@1"
        Write-Host "  candidate: $candId"

        $gOut = & $RankExe gauntlet --id $candId --games $GauntletGames --seed $Seed 2>&1
        $gOut | ForEach-Object { Write-Host "  $_" }
        $eloLine = $gOut | Select-String 'Elo\s+(-?\d+)\s+\+/-\s+(\d+)'
        if (-not $eloLine) { Write-Warning "  gauntlet did not report an Elo, skipping"; continue }
        $elo = [int]$eloLine.Matches[0].Groups[1].Value
        $eloErr = [int]$eloLine.Matches[0].Groups[2].Value

        # Sum the candidate's own cpu/moves/nodes out of the gauntlet scratch file
        # (same fields rank.exe's own report uses: wcpu/bcpu, wmv/bmv, wnod/bnod).
        $cpuMs = 0.0; $moves = 0L; $nodes = 0L
        Get-Content "ranking/gauntlet.jsonl" | ForEach-Object {
            $row = $_ | ConvertFrom-Json
            if ($row.w -eq $candId) { $cpuMs += $row.wcpu; $moves += $row.wmv; $nodes += $row.wnod }
            elseif ($row.b -eq $candId) { $cpuMs += $row.bcpu; $moves += $row.bmv; $nodes += $row.bnod }
        }
        $cpuMsPerMove = if ($moves -gt 0) { $cpuMs / $moves } else { 0.0 }
        $nodesPerMove = if ($moves -gt 0) { $nodes / $moves } else { 0.0 }
        $cpuUsPerMove = $cpuMsPerMove * 1000.0
        $eff = if ($cpuUsPerMove -gt 0) { $elo / ([Math]::Log(1.0 + $cpuUsPerMove) / [Math]::Log(2.0)) } else { 0.0 }

        $results += [PSCustomObject]@{
            TeacherDepth   = $d
            Games          = $g
            Epochs         = $Epochs
            FinalLoss      = $finalLoss
            TrainWinrate   = $trainWinrate
            TrainEloProxy  = $trainEloProxy
            GauntletElo    = $elo
            EloErr         = $eloErr
            CpuMsPerMove   = [math]::Round($cpuMsPerMove, 3)
            NodesPerMove   = [math]::Round($nodesPerMove, 0)
            Eff            = [math]::Round($eff, 1)
            ModelHash      = $hash
            ModelFile      = $modelFile
        }
    }
}

Write-Host ""
Write-Host "=== Sweep results ==="
$results | Format-Table -AutoSize

$results | Export-Csv -Path "$OutDir/report.csv" -NoTypeInformation

$md = @()
$md += "# PST training sweep ($([DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')))"
$md += ""
$md += "Search wrapper: ``$SearchWrap.learned(s2,<hash>)@1``, $GauntletGames games/opponent, seed $Seed."
$md += ""
$md += "| depth | games | loss | train winrate | gauntlet Elo | +/- | cpu ms/move | nodes/move | eff |"
$md += "|---|---|---|---|---|---|---|---|---|"
foreach ($r in $results) {
    $md += "| $($r.TeacherDepth) | $($r.Games) | $($r.FinalLoss) | $($r.TrainWinrate) | $($r.GauntletElo) | $($r.EloErr) | $($r.CpuMsPerMove) | $($r.NodesPerMove) | $($r.Eff) |"
}
$md -join "`n" | Out-File -FilePath $Report -Encoding utf8

Write-Host ""
Write-Host "Report written to $Report and $OutDir/report.csv"
