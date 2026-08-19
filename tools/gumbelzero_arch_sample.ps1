# gumbelzero_arch_sample.ps1 - Generate the mlp/conv architecture x training x
# search-shape random-search draw list for the Gumbel-Zero self-play trainer.
# Deterministic given -SweepSeed, so the draw is reproducible and reviewable
# before any compute is spent.
#
# Follow-up round to the joint sweep (gumbelzero_joint_sample.ps1 /
# plans/gumbel-mcts-results-5-violet-harbor.md), which was linear-only,
# from-scratch-only because train.exe's gumbelzero subcommand had no
# --model-type/--mlp-hidden/--conv-channels flag when that study launched.
# Those flags now exist (src/ml_gumbelzero.cpp, added 2026-08-18), so this
# script covers the deferred architecture axis: mlp (both heads) and conv
# (value head only, policy head stays linear -- see ml_model.h's ConvModel
# doc comment). Linear itself is NOT redrawn here -- results-5 already
# covers it exhaustively (404 checkpoints).
#
# Methodology: random search over the joint hyperparameter space (Bergstra &
# Bengio 2012, Docs/works-cited.md), not one-axis-at-a-time -- every draw
# samples every axis independently and simultaneously. The 9 shared axes
# (sims/lr/l2/replay/batch/open/cvisit/cscale/m) reuse results-5's exact
# eligible sets for direct comparability. Stratified by architecture rather
# than a coin-flip per draw: N mlp draws and N conv draws are sampled as two
# separate blocks (from one continuing RNG stream, so still fully
# deterministic given -SweepSeed) so the study gets an exact, not merely
# expected, 50/50 split -- a cleaner basis for an architecture comparison
# than binomial noise around 50/50 would give.
#
# Conv's FC head (--mlp-hidden, reused by the trainer for that purpose) is
# fixed to empty (direct linear read-out over the flattened conv trunk) this
# round, not swept -- a second axis on top of --conv-channels would widen an
# already-11-axis draw further; sweeping the FC head jointly with channel
# depth is a natural follow-up, not this round's scope.
#
# `sims` is sampled ONCE per draw and used for BOTH the self-play generator
# and the certification head, per Docs/model-training-playbook.md's
# "Generator/search depth" guidance -- same convention as the joint sweep.
#
# Get-GumbelZeroArchDraws is the single source of truth for the sampling:
# dot-source this file (`. .\tools\gumbelzero_arch_sample.ps1`) and call the
# function -- as tools/gumbelzero_arch_study.ps1 does -- rather than
# re-implementing the draw logic.
#
# Run directly (not dot-sourced), this script ONLY samples and prints/exports
# -- it does not train, play, or rate anything:
#   .\tools\gumbelzero_arch_sample.ps1 -N 45 -SweepSeed 8917

param(
    [int]$N = 45,
    [int]$SweepSeed = 8917,
    [string]$Out = "models/sweep/gumbelzero_arch_draws.csv"
)

function Get-GumbelZeroArchDraws([int]$N = 45, [int]$SweepSeed = 8917) {
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
    $BatchSet   = 8,32,128
    $OpenSet    = 0,4,8        # openless / 4-random / 8-random (ranking/CHAMPION.md)
    $CvisitSet  = 400,600,800,1000
    $CscaleSet  = 40,70,100,130   # TENTHS -- cscale=70 means actual 7.0
    $MSet       = 8,12,16,20

    $MlpHiddenSet     = "16","32","64","64,32"
    $ConvChannelsSet  = "8,8","16,16","16,16,16","32,32"

    function NewSharedAxes() {
        $replay = Pick $ReplaySet
        [PSCustomObject]@{
            Sims = Pick $SimsSet; Lr = Pick $LrSet; L2 = Pick $L2Set
            ReplayCap = $replay.Cap; ReplayWarm = $replay.Warm
            Batch = Pick $BatchSet; Open = Pick $OpenSet
            Cvisit = Pick $CvisitSet; Cscale = Pick $CscaleSet; M = Pick $MSet
        }
    }

    $rows = @()

    # References: the Round-5 REF anchor's shared-axis values (sims=300,
    # lr=0.01, l2=0.0, replay=(2000,32), batch=32, open=4, cvisit=600,
    # cscale=70, m=16), one per architecture at that architecture's own
    # built-in trainer default (mlp-hidden=32; conv-channels=16,16).
    $rows += [PSCustomObject]@{
        Draw="REF_MLP"; ModelType="mlp"; Arch="32"
        Sims=300; Lr=0.01; L2=0.0; ReplayCap=2000; ReplayWarm=32
        Batch=32; Open=4; Cvisit=600; Cscale=70; M=16; Seeds=1
    }
    $rows += [PSCustomObject]@{
        Draw="REF_CONV"; ModelType="conv"; Arch="16,16"
        Sims=300; Lr=0.01; L2=0.0; ReplayCap=2000; ReplayWarm=32
        Batch=32; Open=4; Cvisit=600; Cscale=70; M=16; Seeds=1
    }

    for ($i = 1; $i -le $N; $i++) {
        $axes = NewSharedAxes
        $rows += [PSCustomObject]@{
            Draw = "M$i"; ModelType = "mlp"; Arch = (Pick $MlpHiddenSet)
            Sims = $axes.Sims; Lr = $axes.Lr; L2 = $axes.L2
            ReplayCap = $axes.ReplayCap; ReplayWarm = $axes.ReplayWarm
            Batch = $axes.Batch; Open = $axes.Open
            Cvisit = $axes.Cvisit; Cscale = $axes.Cscale; M = $axes.M
            Seeds = 1
        }
    }
    for ($i = 1; $i -le $N; $i++) {
        $axes = NewSharedAxes
        $rows += [PSCustomObject]@{
            Draw = "C$i"; ModelType = "conv"; Arch = (Pick $ConvChannelsSet)
            Sims = $axes.Sims; Lr = $axes.Lr; L2 = $axes.L2
            ReplayCap = $axes.ReplayCap; ReplayWarm = $axes.ReplayWarm
            Batch = $axes.Batch; Open = $axes.Open
            Cvisit = $axes.Cvisit; Cscale = $axes.Cscale; M = $axes.M
            Seeds = 1
        }
    }
    return $rows
}

# ---- CLI-only body: skipped when this file is dot-sourced for the function ----
if ($MyInvocation.InvocationName -ne '.') {
    $rows = Get-GumbelZeroArchDraws -N $N -SweepSeed $SweepSeed
    $rows | Export-Csv -Path $Out -NoTypeInformation -Encoding Ascii
    $rows | Format-Table -AutoSize

    $SharedRungs = @(100,400,1500,4000)
    $totalRuns = ($rows | ForEach-Object { $_.Seeds } | Measure-Object -Sum).Sum
    $totalAgents = $totalRuns * $SharedRungs.Count
    Write-Host ""
    Write-Host "Draws: $($rows.Count) (2 reference + $(2*$N) random, $N mlp + $N conv)   Training runs: $totalRuns   Cohort agents (x$($SharedRungs.Count) rungs): $totalAgents"
    Write-Host "Rungs (games): $($SharedRungs -join ',')"
    Write-Host ""
    Write-Host "Model type split:"
    $rows | Group-Object ModelType | Sort-Object Name | ForEach-Object { Write-Host "  $($_.Name): $($_.Count)" }
    Write-Host "Arch split:"
    $rows | Group-Object ModelType,Arch | Sort-Object Name | ForEach-Object { Write-Host "  $($_.Name): $($_.Count)" }
    Write-Host "Sims split (also the certification head's sims for each draw's checkpoints):"
    $rows | Group-Object Sims | Sort-Object Name | ForEach-Object { Write-Host "  sims=$($_.Name): $($_.Count)" }
    Write-Host ""
    Write-Host "-> $Out"
}
