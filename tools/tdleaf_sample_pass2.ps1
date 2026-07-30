# tdleaf_sample_pass2.ps1 - Generate the Pass-2 random-search draw list for the
# TD-Leaf study. Deterministic given -SweepSeed, so the draw is reproducible and
# reviewable before any compute is spent.
#
# Methodology: random search over the joint hyperparameter space (Bergstra & Bengio
# 2012, Docs/works-cited.md), not one-axis-at-a-time -- every draw samples every axis
# independently and simultaneously. See Docs/model-training-playbook.md and
# Docs/hyperparameter-log.md.
#
# Get-TDLeafPass2Draws is the single source of truth for the sampling: dot-source
# this file (`. .\tools\tdleaf_sample_pass2.ps1`) and call the function -- as
# tdleaf_study.ps1 does -- rather than re-implementing the draw logic, so the study
# that actually trains/rates can never drift from what got reviewed here.
#
# Run directly (not dot-sourced), this script ONLY samples and prints/exports -- it
# does not train, play, or rate anything:
#   .\tools\tdleaf_sample_pass2.ps1 -N 24 -SweepSeed 42

param(
    [int]$N = 24,
    [int]$SweepSeed = 42,
    [string]$Out = "models/sweep/tdleaf_pass2_draws.csv"
)

function Get-TDLeafPass2Draws([int]$N = 24, [int]$SweepSeed = 42) {
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
        # Both Arch and FeatVer are properties of the MODEL FILE, so they can only be
        # chosen when starting from scratch. With --init set, train.exe loads the init
        # model's own architecture and feature version regardless of --model-type /
        # --feature-version (the same constraint, checked in trainTDLeaf and confirmed
        # by a real sanity run 2026-07-30 where a champion-init + --model-type mlp draw
        # silently trained linear). Sampling Arch unconditionally would waste budget on
        # 16 of 24 draws where it can have no effect, so both are forced to the init
        # model's actual shape (linear, v2) on champion-init draws, matching each other.
        $init = Pick $InitSet
        $arch    = if ($init -eq "scratch") { Pick $ArchSet }    else { "linear" }
        $featver = if ($init -eq "scratch") { Pick $FeatVerSet } else { 2 }
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
            Arch = $arch
            FeatVer = $featver
            Seeds = 3
        }
    }
    return $rows
}

# ---- CLI-only body: skipped when this file is dot-sourced for the function ----
if ($MyInvocation.InvocationName -ne '.') {
    $rows = Get-TDLeafPass2Draws -N $N -SweepSeed $SweepSeed
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
}
