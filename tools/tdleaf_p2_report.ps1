<#
.SYNOPSIS
  Join the TD-Leaf Pass-2 pinned-fit screening result back onto each draw's
  configuration and print the table sorted by Elo.

.DESCRIPTION
  Reads:
    - models/sweep/tdleaf_pass2_draws.csv    (the draw configurations)
    - models/sweep/tdleaf_pass2_study.csv    (block/seed/rung -> slot ledger)
    - ranking/ratings_pinned.tsv             (the pinned screening fit)
  and prints one row per rated cohort agent with Elo FIRST, so the config that
  produced each rating is readable without cross-referencing three files.

  Pinned fit = SCREENING ONLY (ranking/CHAMPION.md rule 1): the roster is held
  at its existing Elo and cannot move, so nothing here can dethrone anything.
  With 1 seed per draw these ratings also sit inside this project's measured
  50-150 Elo training-seed noise band, so the ORDER is a shortlist signal, not
  a ranking -- see the SE column and the gap warnings printed at the end.

.PARAMETER Top
  How many rows to flag as the shortlist for deeper evaluation.
#>
param(
    [int]$Top = 10,
    [string]$Ratings = "ranking/ratings_pinned.tsv"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

if (-not (Test-Path $Ratings)) { Write-Error "$Ratings not found -- run the pinned fit first."; exit 1 }

# ---- draw configs ----
$draws = @{}
Import-Csv "models/sweep/tdleaf_pass2_draws.csv" | ForEach-Object { $draws[$_.Draw] = $_ }

# ---- ledger: slot -> (block, seed, rung) ----
$slotInfo = @{}
Import-Csv "models/sweep/tdleaf_pass2_study.csv" | ForEach-Object {
    $slotInfo[[int]$_.slot] = @{ Block = $_.block; Seed = $_.seed; Rung = [int]$_.rung }
}

# ---- pinned fit ----
$rows = @()
$hdr = $null; $eloI = -1; $pmI = -1; $gI = -1; $idI = -1
foreach ($line in Get-Content $Ratings) {
    if ($line -match '^\s*$' -or $line.StartsWith('#')) { continue }
    $c = $line -split "`t"
    if ($null -eq $hdr) {
        $hdr = $c
        $eloI = [array]::IndexOf($c, 'elo'); $pmI = [array]::IndexOf($c, 'pm')
        $gI = [array]::IndexOf($c, 'games'); $idI = [array]::IndexOf($c, 'id')
        continue
    }
    if ($c.Count -le [math]::Max($eloI, $idI)) { continue }
    $id = $c[$idI]
    if ($id -notmatch 'learned\(s(\d+),') { continue }
    $slot = [int]$Matches[1]
    if (-not $slotInfo.ContainsKey($slot)) { continue }   # not a Pass-2 cohort agent
    $info = $slotInfo[$slot]
    $d = $draws[$info.Block]
    if ($null -eq $d) { continue }
    $rows += [PSCustomObject]@{
        Elo     = [int][double]$c[$eloI]
        SE      = [int][double]$c[$pmI]
        Games   = [int][double]$c[$gI]
        Draw    = $info.Block
        Seed    = $info.Seed
        Rung    = $info.Rung
        Lambda  = $d.Lambda
        Lr      = if ($d.LrSched -eq 'on') { "$($d.Lr)->dec" } else { $d.Lr }
        L2      = $d.L2
        Explore = if ($d.ExploreSched -eq 'on') { "$($d.Explore)->dec" } else { $d.Explore }
        Init    = $d.Init
        Depth   = $d.Depth
        Open    = $d.OpenPlies
        Arch    = $d.Arch
        FeatVer = $d.FeatVer
        Slot    = $slot
    }
}

if ($rows.Count -eq 0) { Write-Error "No Pass-2 cohort agents found in $Ratings."; exit 1 }

$sorted = $rows | Sort-Object -Property Elo -Descending
Write-Host ""
Write-Host "TD-Leaf Pass 2 -- pinned screening fit ($($rows.Count) rated cohort agents), sorted by Elo"
Write-Host "SCREENING ONLY: roster pinned, cannot dethrone. 1 seed/draw => inside the 50-150 Elo seed band."
Write-Host ""
$sorted | Format-Table -AutoSize Elo, SE, Games, Draw, Seed, Rung, Lambda, Lr, L2, Explore, Init, Depth, Open, Arch, FeatVer, Slot

# ---- reference point: how does the REF draw (previous best-known recipe) place? ----
$ref = $sorted | Where-Object { $_.Draw -eq 'REF' } | Select-Object -First 1
if ($ref) {
    $rank = ([array]::IndexOf($sorted.Draw, 'REF')) + 1
    Write-Host ("REF (previous best-known recipe) placed #{0} of {1} at Elo {2} +/- {3}." -f $rank, $sorted.Count, $ref.Elo, $ref.SE)
    Write-Host "  Draws above REF beat the prior recipe at this screening depth; those below did not."
}

# ---- shortlist + an honest look at whether the cut is meaningful ----
$n = [math]::Min($Top, $sorted.Count)
Write-Host ""
Write-Host "Shortlist (top $n by screening Elo):"
$sorted | Select-Object -First $n | ForEach-Object { Write-Host ("  {0,5}  {1,-4}  {2}" -f $_.Elo, $_.Draw, "lambda=$($_.Lambda) lr=$($_.Lr) init=$($_.Init) $($_.Arch) $($_.Depth)") }

if ($sorted.Count -gt $n) {
    $cutIn  = $sorted[$n - 1]
    $cutOut = $sorted[$n]
    $gap = $cutIn.Elo - $cutOut.Elo
    $combSE = [math]::Sqrt(($cutIn.SE * $cutIn.SE) + ($cutOut.SE * $cutOut.SE))
    Write-Host ""
    Write-Host ("Cut boundary: #{0} ({1}, {2}) vs #{3} ({4}, {5}) -- gap {6}, combined SE {7:N1}" -f `
        $n, $cutIn.Draw, $cutIn.Elo, ($n+1), $cutOut.Draw, $cutOut.Elo, $gap, $combSE)
    if ($gap -lt $combSE) {
        Write-Host "  WARNING: the top-$n boundary is INSIDE one combined SE -- the cut is arbitrary at this depth."
        Write-Host "  Widen the shortlist rather than treating rank $n vs $($n+1) as a real difference."
    }
}
