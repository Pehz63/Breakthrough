# tdleaf_sample_pass2.ps1 - Generate (not run) the Pass-2 random-search draw list for
# the TD-Leaf study. Deterministic given -SweepSeed, so the draw is reproducible and
# reviewable before any compute is spent. Prints a table and writes
# models/sweep/tdleaf_pass2_draws.csv.
#
# Methodology: random search over the joint hyperparameter space (Bergstra & Bengio
# 2012, Docs/works-cited.md), not one-axis-at-a-time -- every draw samples every axis
# independently and simultaneously. See Docs/model-training-playbook.md and
# Docs/hyperparameter-log.md.
#
# This script ONLY samples and prints. It does not train, play, or rate anything.

param(
    [int]$N = 24,
    [int]$SweepSeed = 42,
    [string]$Out = "models/sweep/tdleaf_pass2_draws.csv"
)

$rng = New-Object System.Random($SweepSeed)
function Pick($set) { $set[$rng.Next(0, $set.Count)] }
function PickBool($p) { $rng.NextDouble() -lt $p }

$LambdaSet  = 0.0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0
$LrSet      = 0.001,0.003,0.01,0.03,0.1
$L2Set      = 0.0,0.0003,0.001,0.003
$ExploreSet = 0.0,0.01,0.02,0.05
$InitSet    = "champion","scratch"
$DepthSet   = "d4","d6"
$OpenSet    = 2,4,8
$ArchSet    = "linear","mlp16","mlp32"
$FeatVerSet = 1,2

$rows = @()

# Reference: the previous best-known recipe, re-run on the new consistent 6-rung
# ladder (not reused from the old inconsistent-ladder data), 6 seeds since every
# other draw is compared against it.
$rows += [PSCustomObject]@{
    Draw="REF"; Lambda=0.7; Lr=0.01; LrSched="off"; L2=0.0; Explore=0.0; ExploreSched="off"
    Init="champion"; Depth="d6"; OpenPlies=4; Arch="linear"; FeatVer=2; Seeds=6
}

for ($i = 1; $i -le $N; $i++) {
    $init = Pick $InitSet
    $rows += [PSCustomObject]@{
        Draw = "R$i"
        Lambda = Pick $LambdaSet
        Lr = Pick $LrSet
        LrSched = if (PickBool 0.5) { "on" } else { "off" }
        L2 = Pick $L2Set
        Explore = Pick $ExploreSet
        ExploreSched = if (PickBool 0.5) { "on" } else { "off" }
        Init = $init
        Depth = Pick $DepthSet
        OpenPlies = Pick $OpenSet
        Arch = Pick $ArchSet
        FeatVer = if ($init -eq "scratch") { Pick $FeatVerSet } else { 2 }   # forced when init=champion
        Seeds = 3
    }
}

$rows | Export-Csv -Path $Out -NoTypeInformation -Encoding Ascii
$rows | Format-Table -AutoSize

$totalRuns = ($rows | ForEach-Object { $_.Seeds } | Measure-Object -Sum).Sum
$totalAgents = $totalRuns * 6   # 6 rungs, shared across every draw
Write-Host ""
Write-Host "Draws: $($rows.Count) (1 reference + $N random)   Training runs: $totalRuns   Cohort agents (x6 rungs): $totalAgents"
Write-Host ""
Write-Host "Architecture split:"
$rows | Group-Object Arch | ForEach-Object { Write-Host "  $($_.Name): $($_.Count)" }
Write-Host "Feature-version split (scratch-init draws only, others forced to v2):"
$rows | Where-Object { $_.Init -eq "scratch" } | Group-Object FeatVer | ForEach-Object { Write-Host "  v$($_.Name): $($_.Count)" }
Write-Host "Init split:"
$rows | Group-Object Init | ForEach-Object { Write-Host "  $($_.Name): $($_.Count)" }
Write-Host ""
Write-Host "-> $Out"
