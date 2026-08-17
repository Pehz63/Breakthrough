# gumbelzero_sample_pass2.ps1 - Generate the Pass-2 random-search draw list for
# the Gumbel-Zero self-play trainer. Deterministic given -SweepSeed, so the
# draw is reproducible and reviewable before any compute is spent.
#
# Methodology: random search over the joint hyperparameter space (Bergstra &
# Bengio 2012, Docs/works-cited.md), not one-axis-at-a-time -- every draw
# samples every axis independently and simultaneously, mirroring
# tools/tdleaf_sample_pass2.ps1's approach for the TD-Leaf study. See
# Docs/model-training-playbook.md and Docs/hyperparameter-log.md.
#
# open-plies is grounded on ranking/CHAMPION.md's category split rather than
# arbitrary values: 0/4/8 are exactly the openless / 4-random / 8-random
# categories (the two book categories don't apply -- Gumbel-Zero's self-play
# generator uses uniform-random opening plies, not a mined book).
#
# `sims` is sampled ONCE per draw and used for BOTH the self-play generator
# and the certification head the resulting checkpoints are rated under
# (`gaz(sims=N)@1`), per Docs/model-training-playbook.md's "Generator/search
# depth" guidance: since sims is itself the swept axis here, matching
# generator and certification avoids confounding "more search budget helped"
# with "the checkpoint was rated at a mismatched budget."
#
# Get-GumbelZeroPass2Draws is the single source of truth for the sampling:
# dot-source this file (`. .\tools\gumbelzero_sample_pass2.ps1`) and call the
# function -- as tools/gumbelzero_study.ps1 does -- rather than
# re-implementing the draw logic, so the study that actually trains/rates can
# never drift from what got reviewed here.
#
# Run directly (not dot-sourced), this script ONLY samples and prints/exports
# -- it does not train, play, or rate anything:
#   .\tools\gumbelzero_sample_pass2.ps1 -N 20 -SweepSeed 77

param(
    [int]$N = 20,
    [int]$SweepSeed = 77,
    [string]$Out = "models/sweep/gumbelzero_pass2_draws.csv"
)

function Get-GumbelZeroPass2Draws([int]$N = 20, [int]$SweepSeed = 77) {
    $rng = New-Object System.Random($SweepSeed)
    function Pick($set) { $set[$rng.Next(0, $set.Count)] }

    $SimsSet  = 25,50,100,200
    $LrSet    = 0.003,0.01,0.03
    $L2Set    = 0.0,0.0003,0.001
    # (capacity, warmup) presets, paired so warmup always stays a sane
    # fraction of capacity rather than drawing the two independently.
    $ReplaySet = @(
        [PSCustomObject]@{ Cap=500;  Warm=16  },
        [PSCustomObject]@{ Cap=2000; Warm=32  },
        [PSCustomObject]@{ Cap=8000; Warm=128 }
    )
    $BatchSet = 8,32,64
    $OpenSet  = 0,4,8   # openless / 4-random / 8-random (ranking/CHAMPION.md)

    $rows = @()

    # Reference: the Pass-1 sanity recipe (models/sweep/slot650.txt), so every
    # random draw is compared against the configuration already known to run
    # end to end.
    $rows += [PSCustomObject]@{
        Draw="REF"; Sims=50; Lr=0.01; L2=0.0; ReplayCap=2000; ReplayWarm=32
        Batch=32; Open=4; Seeds=1
    }

    for ($i = 1; $i -le $N; $i++) {
        $replay = Pick $ReplaySet
        $rows += [PSCustomObject]@{
            Draw = "R$i"
            Sims = Pick $SimsSet
            Lr = Pick $LrSet
            L2 = Pick $L2Set
            ReplayCap = $replay.Cap
            ReplayWarm = $replay.Warm
            Batch = Pick $BatchSet
            Open = Pick $OpenSet
            Seeds = 1
        }
    }
    return $rows
}

# ---- CLI-only body: skipped when this file is dot-sourced for the function ----
if ($MyInvocation.InvocationName -ne '.') {
    $rows = Get-GumbelZeroPass2Draws -N $N -SweepSeed $SweepSeed
    $rows | Export-Csv -Path $Out -NoTypeInformation -Encoding Ascii
    $rows | Format-Table -AutoSize

    $SharedRungs = @(100,400,1500)
    $totalRuns = ($rows | ForEach-Object { $_.Seeds } | Measure-Object -Sum).Sum
    $totalAgents = $totalRuns * $SharedRungs.Count
    Write-Host ""
    Write-Host "Draws: $($rows.Count) (1 reference + $N random)   Round-A training runs: $totalRuns   Round-A cohort agents (x$($SharedRungs.Count) rungs): $totalAgents"
    Write-Host "Rungs (games): $($SharedRungs -join ',')"
    Write-Host ""
    Write-Host "Sims split (also the certification head's sims for each draw's checkpoints):"
    $rows | Group-Object Sims | Sort-Object Name | ForEach-Object { Write-Host "  sims=$($_.Name): $($_.Count)" }
    Write-Host "Open-plies split:"
    $rows | Group-Object Open | Sort-Object Name | ForEach-Object { Write-Host "  open=$($_.Name): $($_.Count)" }
    Write-Host ""
    Write-Host "-> $Out"
}
