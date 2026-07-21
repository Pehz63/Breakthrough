---
name: position-oracle-campaign
description: "The position-oracle project (measure per-position Elo advantage by designed playouts, train a dist model) -- COMPLETE, all 4 models beat the d8 oracle. Standing rules for any retrain/follow-up."
metadata: 
  node_type: memory
  type: project
  originSessionId: 5cf2b95b-5d37-4c3e-be5d-5f6a58390e26
  modified: 2026-07-21T05:56:19.545Z
---

The developer's position-oracle project (started 2026-07-18, campaign
completed 2026-07-21): measure any position's Elo advantage as a
distribution (mean = the Elo handicap making the game a coin flip, SD =
volatility of conversion) by playing DESIGNED fresh games from the position
between rated agents at controlled Elo gaps, then train a two-headed dist
model to amortize the measurement.

**Outcome: theory 34 confirmed.** All four production models
(`models/dist_lin.txt`, `dist_mlp_s1001.txt`, `dist_mlp_s2002.txt`,
`dist_mlp_wide.txt`) beat the calibrated depth-8/2M-node oracle on both
outcome NLL and mu MAE. Best: `dist_mlp_wide` (74,690 params), MAE 146.2 vs
oracle 191.3, NLL 0.408 vs 0.450. Theory 35 (sigma/volatility validity) only
weakly supported (0.02-0.29 correlation with measured sd) -- do not
conflate the two, the mu result is strong and the sigma result is not.
Playing-strength Elo diverges from prediction-quality ranking (theory 27,
reconfirmed a third time): `dist_lin` beats two of three MLP configs in
actual play despite losing to them on prediction. Full numbers:
`plans/position-oracle-results-1-lazy-popping-simon.md`.

**Standing rules for any retrain or follow-up campaign:**
- `data/labels/ratings_snapshot.tsv` is the campaign's frozen Elo basis;
  never overwrite it mid-study. Raw label stores are keyed by rung index
  with the id list frozen in the store's `.meta.json`, so a future ratings
  refit re-labels via labelfit + retrain WITHOUT replaying any games.
- Tie-only jitter (Advanced Noise < 0) is deterministic per seed and never
  draws from rand(), so jitter agents CANNOT vary playouts from a fixed
  position -- confirmed the hard way (a full ladder design built around
  jitter rungs had to be scrapped before any games were wasted on it).
  Strong stochastic ladder rungs must be depth-diluted (`dil(rP,dN)`) or
  random-diluted agents. [[evidence-tethered-claims]]
- Replays and from-position playouts must `ttClear()` per game: cross-game
  TT pollution otherwise makes outcomes depend on process history.
- When testing a scaling/convergence question (position count, games per
  position, hyperparameters), use a log-triplet checkpoint schedule (tight
  clusters at widening gaps: 1,2,3 / 5,6,7 / 25,26,27 / 105,106,107 / ...)
  rather than a doubling sequence -- the developer pushed back hard on
  doubling because it can't distinguish real convergence from run-to-run
  noise, and the triplets caught a real training-run outlier this session
  that would have been misread as a regression under a coarser schedule.
- MLP-class dist models need meaningfully more train positions than linear
  ones to show their advantage (crossover was ~1700-6800 positions here,
  still climbing at the full 22,788) -- don't judge an MLP's architecture
  from a small-data proxy without checking its own convergence curve.
- `tools/label_study.ps1`'s training-phase completion check now verifies
  the output file + log marker, not just process `.ExitCode` (which proved
  unreliable for redirected-output processes on this machine).
- Untested input-feature ideas (material as explicit input, mover's-
  perspective color canonicalization) are logged in `todo.md`; the latter
  is flagged as the strongest candidate given the MLP's demonstrated data
  appetite.
