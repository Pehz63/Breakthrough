---
name: position-oracle-campaign
description: "The position-oracle project (measure per-position Elo advantage by designed playouts, train a dist model to beat the d8 oracle at prediction) and its standing rules"
metadata: 
  node_type: memory
  type: project
  originSessionId: 5cf2b95b-5d37-4c3e-be5d-5f6a58390e26
  modified: 2026-07-19T13:57:23.745Z
---

The developer's position-oracle project (started 2026-07-18): measure any
position's Elo advantage as a distribution (mean = the Elo handicap making the
game a coin flip, SD = volatility of conversion) by playing DESIGNED fresh
games from the position between rated agents at controlled Elo gaps, then
train a two-headed dist model to amortize the measurement. Success criterion,
locked in dist-eval's VERDICT line: beat the calibrated d8/nb2m oracle's root
search score on held-out outcome NLL AND mu MAE. Playing Elo of the models is
a separate secondary measurement (theory 27 caution). Theories 34 and 35 track
the claims. Plan: plans/position-oracle-plan-1-lazy-popping-simon.md.

**Standing rules for this project:**
- The developer explicitly chose designed new data over the historical corpus
  (found data has pseudo-replication and no gap design) and said to spare no
  expense on compute, expecting retrains as the pool improves.
- `data/labels/ratings_snapshot.tsv` is the campaign's frozen Elo basis; never
  overwrite it mid-study. Raw label stores are keyed by rung index with the id
  list frozen in the store's .meta.json, so a future ratings refit re-labels
  via labelfit + retrain WITHOUT replaying any games.
- Tie-only jitter (Advanced Noise < 0) is deterministic per seed and never
  draws from rand(), so jitter agents CANNOT vary playouts from a fixed
  position. Strong stochastic ladder rungs must be depth-diluted (dil(rP,dN))
  or random-diluted agents. [[evidence-tethered-claims]]
- Replays and from-position playouts must ttClear() per game: cross-game TT
  pollution otherwise makes outcomes depend on process history (found when
  posgen reruns diverged 51 vs 113 games).
- Per-position sigma below ~150 Elo is not identifiable from ~60 games; the
  dense eval-tier design (~800+ games/position) exists for that.
