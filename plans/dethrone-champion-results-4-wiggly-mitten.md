# Dethrone the Champion, Phase 3 (Symmetrization + Seed Ensembling) -- Results

Companion to `dethrone-champion-plan-1-wiggly-mitten.md` (phase 3, first
increment). Sequel to results-1 (phase 0), results-2 (quiescence), results-3
(refutation book). Session date 2026-07-17. Tests the todo's "weight
symmetrization + seed-ensembling" `[Now]` item against theory 8's seed-noise
band, on the champion's own recipe.

## Headline

**Both interventions HURT, decisively, and s98 remains champion.** Mirror-
symmetrizing the champion's own weights cost 135 Elo (1079 -> 944), cleanly
isolated. A 6-seed mirror-symmetrized weight ensemble cost 144 Elo relative to
the seed mean (1068 -> 924). Neither is within noise: the gaps are ~11x the
per-agent SE and they widened, not shrank, when the sample grew from 8 to 32
games/pair. The todo's premise -- "weight symmetrization + seed-ensembling is a
free variance cut against the 50-150 Elo seed noise" -- is refuted for PLAYING
STRENGTH. The likely reason ties three existing theories together: the left-
right symmetry is exact for the value FUNCTION, but a symmetric model's extra
evaluation ties get broken by the engine's directional first-found rule (theory
23), and the asymmetric component the champion learned is doing useful
tie-breaking work against a pool of deterministic, directionally-biased
opponents (theory 19) rather than being pure noise.

## What was built

- **`train.exe ensemble`** (`trainEnsemble`, `src/ml_train.cpp`; registered in
  `g_regimes[]`, dispatched in `tools/train_main.cpp`): loads K trained linear
  feature-v2 value models, optionally projects each onto its left-right mirror
  symmetry (`w'[sq] = (w[sq] + w[mirror sq]) / 2` per color plane via
  `mlv2MirrorIndex`, side-to-move and bias untouched), then averages the K
  weight vectors and biases into ONE linear model saved with an
  `ensemble(k=K,mirror=M):<files>` provenance string. Rationale as designed:
  the rules and standard start are left-right mirror symmetric (axioms O4/D5
  give the color swap; the plain x -> SIZE-1-x reflection is a separate exact
  symmetry of both), so the true value function is mirror symmetric and a
  trained model's anti-symmetric component is sampling noise; and for a LINEAR
  model the ensemble of K seed replicas IS their weight average, free at
  inference and still incremental in search.
- **Test** (772 assertions / 82 cases pass): a hand-computed two-model
  mirror+average round trip through save/ensemble/load.

## The experiment

The champion's exact recipe was re-run at 6 training seeds: `selfplay-
supervised --feature-version 2 --from-data data/pg_oracle_champ.jsonl
--epochs 6 --lr 0.05 --seed {1001..6006}` into `models/sweep/slot3..8.txt`.
The original dataset (98999 positions from 2000 oracle-vs-old-champion pairgen
games, symmetric 6-ply random openers, seed 104) was still on disk with its
`.meta.json` sidecar. **Determinism check passed:** slot5 (seed 3003, the seed
the vs-champion study promoted) reproduced the champion's model byte-identically
(hash `5801570e`), so the six replicas are five new seeds plus the champion's
own weights.

Challengers built from them:

- `slot9` = mirror-symmetrized average of all six seeds (hash `e5b3b014`).
- `slot10` = mirror-symmetrized champion weights ALONE (hash `fead67b7`) --
  isolates the symmetrization effect from the ensembling effect, because it is
  the champion's exact weights with only the mirror projection applied.

Rostered at both standard heads (`ab(d4,tt,ord,nb200k)`,
`ab(d6,tt,ord,nb200k)`): the five new seeds (slot5 is not rostered -- its policy
IS the champion row), slot9, slot10 -- 14 new IDs, 8 games/pair against the full
roster, then slot9/slot10's d6 contender pairs boosted to 32 games, then the
full-roster anchored refit.

## Measurement (d6 head, full-roster anchored refit, 2026-07-17)

| Elo | +/- | agent | note |
|---|---|---|---|
| 1107 | 19 | seed 4004 (slot6) | best single seed |
| 1097 | 19 | seed 1001 (slot3) | |
| **1079** | 12 | **seed 3003 = s98 (champion)** | retained |
| 1062 | 18 | seed 5005 (slot7) | |
| 1047 | 18 | seed 6006 (slot8) | |
| 1017 | 17 | seed 2002 (slot4) | worst single seed |
| **944** | 12 | **mirror-symmetrized champion (slot10)** | -135 vs champion |
| **924** | 12 | **6-seed mirror ensemble (slot9)** | -144 vs seed mean |

Six-seed spread 1017-1107 (90 Elo, squarely inside theory 8's 50-150 band; the
champion's promoted seed 3003 is a middling 1079, not the best). Seed mean
~1068.

The d4 head (8 games/pair, not boosted, so preliminary) agrees directionally:
seeds 686-764, ensemble 680, mirror-champion 702 -- both interventions at or
below the seed range.

Decomposition, from slot10 vs slot9:
- **Mirror symmetrization alone: -135 Elo** (champion 1079 -> mirror-champion
  slot10 944). This is the clean isolation: identical weights, only the mirror
  projection differs.
- **Adding the 6-seed weight average: a further ~-20** (slot10 944 -> slot9
  924). Averaging sharp per-seed models produced a still-weaker blurry model.

## Interpretation

The result is surprising against the design rationale, and that gap is the
finding. The value FUNCTION is exactly left-right symmetric, so projecting a
model onto the symmetric subspace provably cannot change how it ranks a position
against its own mirror. Yet playing strength dropped 135 Elo. Reconciling those:

1. **A symmetric model has more exact evaluation ties, and ties are broken
   directionally (theory 23).** Two mirror-image candidate moves get identical
   scores from a symmetric model, so the choice falls to the engine's fixed
   first-found order (left-diagonal, x = 0..7), the queenside pile-up theory 23
   documents. The asymmetric champion instead has a definite, learned preference
   at each such fork. Against a POOL OF DETERMINISTIC opponents (theory 19),
   that learned preference is not noise -- it is a fixed policy that happens to
   score better than the enumeration-order default against these specific
   opponents.
2. **Averaging linear PST weights is not a strength ensemble.** The six seeds
   spread their weight mass differently (90 Elo apart); averaging washes out
   each one's decisive, sharp features into a flatter table that plays weaker
   than any single seed. "The ensemble is the average for a linear model" is
   true arithmetically but false as a strength claim -- the average of good
   players is not a good player here.

LABELED HYPOTHESIS (untested, a theory-27 parallel worth checking): mirror
symmetrization may still LOWER the outcome-prediction loss (it is a variance cut
on the weights) while LOWERING Elo -- the same calibration/strength divergence
theory 27 found for the MLP. The ensemble/mirror models were not trained via
selfplay-supervised so no stratified loss printed; recomputing their held-out
loss would test this and is filed below.

## Deviations from the plan

- Phase 3 as planned bundles four training fixes (eval-blended labels,
  extraction filters, symmetrization/ensembling, early stopping by default).
  This increment ships and measures only symmetrization + ensembling -- the one
  fix that produces a challenger from the champion's own recipe with no new data
  generation. The other three remain queued in `todo.md` and are the live
  remaining dethrone path.
- Slot5 (the champion-weight replica) is deliberately not rostered to avoid a
  duplicate-policy identity (theory 19).
- A pure-average (mirror=0) ensemble was NOT built, so "averaging alone, no
  mirror" is not isolated. slot10 already isolates mirroring (the dominant
  harm); the pure-average control is filed as Future Work.

## Future Work

- **Pure-average (mirror=0) ensemble** (tethered to the decomposition): build
  `train.exe ensemble --mirror 0` over the six seeds and rate it, to isolate
  whether weight averaging ALONE hurts or only hurts on top of mirroring. One
  cheap train + fill + boost cycle.
- **Calibration-vs-strength for symmetrization** (tethered to the labeled
  hypothesis): recompute slot9/slot10 held-out stratified loss and compare to
  the champion's. If symmetrization lowers loss while lowering Elo, it is
  another theory-27 instance and a caution against loss-guided model selection.
- **Confirm the tie-breaking mechanism** (tethered to interpretation 1): rate a
  symmetric model whose ties are broken by a fixed non-directional rule, or
  measure the symmetric model's left-file development bias (theory 23's console
  probe) vs the champion's -- if the symmetric model is MORE left-biased and
  more predictable, that supports the mechanism.
- **Softmax/temperature sampling** (todo `[Now]`, related): if directional
  tie-breaking is the harm, a stochastic tie policy is the natural counter and
  its own dethrone-adjacent experiment.

## Ideas This Inspired

- "Free variance cut" intuitions from linear-model theory (symmetry projection,
  seed averaging) do not transfer to search strength, because the search's
  tie-breaking turns weight structure into policy. Any weight-space
  regularization should be validated on Elo, not just weight norm or loss --
  the same lesson as theory 27, now for a second class of intervention.
- The champion's ASYMMETRY is apparently load-bearing. That reframes theory 23's
  left-file bias from "a predictability flaw" toward "possibly a feature the top
  agents rely on against a biased pool" -- worth a dedicated look, since it also
  bears on how a community pool of diverse agents would shift these results.
- Seed selection beats seed averaging here: the best single seed (1107) is +28
  over the champion and +183 over the ensemble. A cheap win may be to just train
  more seeds of the proven recipe and PICK the best by full-roster Elo, rather
  than combine them -- though that best-of-K is an over-fit-to-the-pool risk to
  measure against held-out opponents.

## Commit

One commit: the ensemble regime (src/, tools/) + its test, ML.md autodoc
regen, the six seed models + slot9/slot10 (models/sweep/), roster + roster_top
additions, new match data + refit artifacts (ranking/), this results doc,
theory 8 update + new theory 30 (Docs/theories.md), CHAMPION.md defended-
challenges rows, todo strike-out, and the CLAUDE.md reference updates.
