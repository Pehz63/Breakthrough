# opener_bias_study.ps1 -- Theory 6 test (Docs/theories.md): do symmetric random
# openers inflate "beats the champion" results?
#
# For each promoted challenger vs the champion, play the d6 head-to-head under
# three opener configs and compare the challenger's win rate:
#   S  symmetric   --open-plies 6 --open-side both   (the original study's setup)
#   C  challenger  --open-plies 6 --open-side a       (challenger random, champion true policy)
#   P  champion    --open-plies 6 --open-side b       (champion random, challenger true policy)
# S is what every prior "beats the champion" number used. If the win survives in C
# (the challenger is the one handicapped, the champion plays perfectly), the win is
# real; if it collapses from S to C, the symmetric win was a shared-handicap artifact.
# P vs C brackets how much the random opener matters.
#
# Then runs the Layer-2 mechanism measurement (rank.exe opener-bias) with a
# positionally-aware learned judge, quantifying how much the random opener degrades
# the champion's position (the champion's own coarse eval cannot see it).
#
# Pairgen doubles as an evaluation tool here: its .meta.json carries the win tally
# plus the White/Black split, which is all this study reads. Games land under
# data/opener_bias/ (gitignored training-data pattern) and are otherwise unused.
#
# Usage: .\tools\opener_bias_study.ps1 [-Games 80] [-Workers 8] [-Seed 220]
param(
    [int]$Games = 80,
    [int]$Workers = 8,
    [int]$Seed = 220,
    [int]$OpenPlies = 6,
    [int]$MechGames = 60,
    [string]$Board = "boards/board1.txt"
)

$ErrorActionPreference = "Stop"
$RankExe = ".\rank.exe"
if (-not (Test-Path $RankExe)) { Write-Error "$RankExe not found (build with .\build_rank.bat)."; exit 1 }
New-Item -ItemType Directory -Force -Path "data/opener_bias" | Out-Null

# Pinned agents (roster IDs; models already on disk).
$Champion = "ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2"
$Models = @(
    @{ name = "champdil-s96"; id = "ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7)@1"; note = "62.5% d6 in symmetric study (Theory 2)" },
    @{ name = "oracle-s98";   id = "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1"; note = "tied champion at d6 in symmetric study (headline)" }
)
$Configs = @(
    @{ tag = "S"; side = "both"; desc = "symmetric (both random)" },
    @{ tag = "C"; side = "a";    desc = "challenger random, champion true policy" },
    @{ tag = "P"; side = "b";    desc = "champion random, challenger true policy" }
)

# Play one config sharded across workers, return the merged meta tally object.
function PlayConfig([string]$outBase, [string]$challengerId, [string]$openSide) {
    $out = "$outBase.jsonl"
    $w = [Math]::Max(1, [Math]::Min($Workers, [int][Math]::Floor($Games / 4)))
    $exe = (Resolve-Path $RankExe).Path
    $cwd = (Get-Location).Path
    $procs = @()
    for ($i = 0; $i -lt $w; $i++) {
        $args = @("pairgen", "--a", $challengerId, "--b", $Champion, "--board", $Board,
                  "--games", "$Games", "--seed", "$Seed",
                  "--open-plies", "$OpenPlies", "--open-side", $openSide,
                  "--out", "$out.$i", "--shard", "$i", "--of", "$w")
        $p = Start-Process -FilePath $exe -ArgumentList $args -WorkingDirectory $cwd -NoNewWindow -PassThru -RedirectStandardOutput "$out.$i.log"
        $null = $p.Handle
        $procs += ,@($p, $i)
    }
    foreach ($pi in $procs) {
        $pi[0].WaitForExit()
        if ($pi[0].ExitCode -ne 0) {
            Write-Warning "shard $($pi[1]) exited $($pi[0].ExitCode):"
            Get-Content "$out.$($pi[1]).log" -Tail 5 | ForEach-Object { Write-Host "    $_" }
            Write-Error "generation failed for $out"; exit 1
        }
    }
    # Merge only the tallies we report (win/color counts), keeping shard 0's config.
    $tally = $null
    for ($i = 0; $i -lt $w; $i++) {
        $m = Get-Content "$out.$i.meta.json" -Raw | ConvertFrom-Json
        if ($null -eq $tally) { $tally = $m }
        else {
            foreach ($f in "played","a_wins","b_wins","draws","a_white_games","a_white_wins","a_white_draws","a_black_games","a_black_wins","a_black_draws") {
                $tally.$f = $tally.$f + $m.$f
            }
        }
        Remove-Item "$out.$i", "$out.$i.meta.json", "$out.$i.log" -ErrorAction SilentlyContinue
    }
    return $tally
}

Write-Host "=== Layer 1: head-to-head sensitivity sweep (d6, $Games games/config, seed $Seed) ==="
Write-Host "challenger win% vs champion under each opener config`n"
$rows = @()
foreach ($m in $Models) {
    foreach ($c in $Configs) {
        $base = "data/opener_bias/$($m.name)_$($c.tag)"
        $t = PlayConfig $base $m.id $c.side
        $wr = if ($t.played -gt 0) { 100.0 * $t.a_wins / $t.played } else { 0 }
        $wWr = if ($t.a_white_games -gt 0) { 100.0 * $t.a_white_wins / $t.a_white_games } else { 0 }
        $bWr = if ($t.a_black_games -gt 0) { 100.0 * $t.a_black_wins / $t.a_black_games } else { 0 }
        $rows += [pscustomobject]@{
            Model = $m.name; Config = $c.tag; Opener = $c.desc
            Games = $t.played; WinPct = [math]::Round($wr,1)
            AsWhite = "$($t.a_white_wins)/$($t.a_white_games) ($([math]::Round($wWr,0))%)"
            AsBlack = "$($t.a_black_wins)/$($t.a_black_games) ($([math]::Round($bWr,0))%)"
        }
        Write-Host ("  {0,-13} {1}  {2,5}%  ({3} games)  [{4}]" -f $m.name, $c.tag, [math]::Round($wr,1), $t.played, $c.desc)
    }
    Write-Host ""
}
$rows | Format-Table -AutoSize | Out-String | Write-Host

# Persist a CSV for the results doc.
$csv = "data/opener_bias/sensitivity_sweep.csv"
$rows | Export-Csv -Path $csv -NoTypeInformation
Write-Host "wrote $csv`n"

Write-Host "=== Layer 2: mechanism measurement (does the random opener degrade the champion's position?) ==="
Write-Host "judge = a positionally-aware learned agent; the champion's own eval is too coarse to see opener differences`n"
$Judge = "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1"
& $RankExe opener-bias --a $Champion --b $Judge --judge $Judge --board $Board --games $MechGames --open-plies $OpenPlies --seed $Seed
