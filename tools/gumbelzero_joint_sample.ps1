# gumbelzero_joint_sample.ps1 - Generate the joint training x search-shape
# random-search draw list for the Gumbel-Zero self-play trainer. Deterministic
# given -SweepSeed, so the draw is reproducible and reviewable before any
# compute is spent.
#
# Methodology: random search over the joint hyperparameter space (Bergstra &
# Bengio 2012, Docs/works-cited.md), not one-axis-at-a-time -- every draw
# samples every axis independently and simultaneously. Mirrors
# gumbelzero_sample_pass2.ps1's approach for the training axes, extended with
# 3 more axes (cvisit/cscale/m) that apply ONLY at the certification head
# (gaz(sims=N,cvisit=C,cscale=S,m=M)@1) built in the roster phase, not passed
# to train.exe -- Gumbel-Zero's self-play generator does not read them (see
# gumbelzero_joint_study.ps1's header). One joint draw therefore fixes BOTH
# how a checkpoint is trained and how it is later played/rated.
#
# Architecture/init are NOT an axis here: train.exe's gumbelzero subcommand
# is linear-only, from-scratch-only (no --model-type/--mlp-hidden/--init
# flag exists), confirmed 2026-08-17 before this study launched. Adding MLP
# and a second init scheme is a follow-up round once trainer support exists,
# same two-stage pattern as Round A -> Round B.
#
# `sims` is sampled ONCE per draw and used for BOTH the self-play generator
# and the certification head the resulting checkpoints are rated under, per
# Docs/model-training-playbook.md's "Generator/search depth" guidance.
#
# Get-GumbelZeroJointDraws is the single source of truth for the sampling:
# dot-source this file (`. .\tools\gumbelzero_joint_sample.ps1`) and call the
# function -- as tools/gumbelzero_joint_study.ps1 does -- rather than
# re-implementing the draw logic.
#
# Run directly (not dot-sourced), this script ONLY samples and prints/exports
# -- it does not train, play, or rate anything:
#   .\tools\gumbelzero_joint_sample.ps1 -N 100 -SweepSeed 8817

param(
    [int]$N = 100,
    [int]$SweepSeed = 8817,
    [string]$Out = "models/sweep/gumbelzero_joint_draws.csv"
)

function Get-GumbelZeroJointDraws([int]$N = 100, [int]$SweepSeed = 8817) {
    $rng = New-Object System.Random($SweepSeed)
    function Pick($set) { $set[$rng.Next(0, $set.Count)] }

    $SimsSet   = 300,400,500
    $LrSet     = 0.003,0.01,0.03
    $L2Set     = 0.0,0.0003,0.001
    $ReplaySet = @(
        [PSCustomObject]@{ Cap=500;  Warm=16  },
        [PSCustomObject]@{ Cap=2000; Warm=32  },
        [PSCustomObject]@{ Cap=8000; Warm=128 }
    )
    $BatchSet  = 8,32,128
    $OpenSet   = 0,4,8        # openless / 4-random / 8-random (ranking/CHAMPION.md)
    $CvisitSet = 400,600,800,1000
    $CscaleSet = 40,70,100,130   # TENTHS -- cscale=70 means actual 7.0
    $MSet      = 8,12,16,20

    $rows = @()

    # Reference: the closest in-range anchor to two prior findings -- the
    # Gumbel-Zero Pass-1 sanity training recipe (sims/lr/l2/replay/batch/open,
    # snapped into THIS study's eligible sets) and the Slice-2 search-knob
    # study's practical recommendation gaz(sims=200,cvisit=500,cscale=50)@1
    # (plans/gumbel-mcts-results-4-copper-lantern.md), similarly snapped.
    $rows += [PSCustomObject]@{
        Draw="REF"; Sims=300; Lr=0.01; L2=0.0; ReplayCap=2000; ReplayWarm=32
        Batch=32; Open=4; Cvisit=600; Cscale=70; M=16; Seeds=1
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
            Cvisit = Pick $CvisitSet
            Cscale = Pick $CscaleSet
            M = Pick $MSet
            Seeds = 1
        }
    }
    return $rows
}

# ---- CLI-only body: skipped when this file is dot-sourced for the function ----
if ($MyInvocation.InvocationName -ne '.') {
    $rows = Get-GumbelZeroJointDraws -N $N -SweepSeed $SweepSeed
    $rows | Export-Csv -Path $Out -NoTypeInformation -Encoding Ascii
    $rows | Format-Table -AutoSize

    $SharedRungs = @(100,400,1500,4000)
    $totalRuns = ($rows | ForEach-Object { $_.Seeds } | Measure-Object -Sum).Sum
    $totalAgents = $totalRuns * $SharedRungs.Count
    Write-Host ""
    Write-Host "Draws: $($rows.Count) (1 reference + $N random)   Training runs: $totalRuns   Cohort agents (x$($SharedRungs.Count) rungs): $totalAgents"
    Write-Host "Rungs (games): $($SharedRungs -join ',')"
    Write-Host ""
    Write-Host "Sims split (also the certification head's sims for each draw's checkpoints):"
    $rows | Group-Object Sims | Sort-Object Name | ForEach-Object { Write-Host "  sims=$($_.Name): $($_.Count)" }
    Write-Host "Open-plies split:"
    $rows | Group-Object Open | Sort-Object Name | ForEach-Object { Write-Host "  open=$($_.Name): $($_.Count)" }
    Write-Host "Cvisit split:"
    $rows | Group-Object Cvisit | Sort-Object Name | ForEach-Object { Write-Host "  cvisit=$($_.Name): $($_.Count)" }
    Write-Host "Cscale split (tenths):"
    $rows | Group-Object Cscale | Sort-Object Name | ForEach-Object { Write-Host "  cscale=$($_.Name): $($_.Count)" }
    Write-Host "M split:"
    $rows | Group-Object M | Sort-Object Name | ForEach-Object { Write-Host "  m=$($_.Name): $($_.Count)" }
    Write-Host ""
    Write-Host "-> $Out"
}
