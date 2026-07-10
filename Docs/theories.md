# Theory Log

A running list of testable theories that have come up while developing
Breakthrough's AI (eval design, training recipes, data sources, search
budgets, and so on). Each entry tracks where the idea came from, where it was
tested, and its current status, so the project doesn't lose track of what's
been settled versus what's still an open question.

Add or update an entry whenever a results doc confirms, refutes, or opens a
new testable theory (see the "Update the theory log" step in `CLAUDE.md`'s
after-every-functional-change workflow). Not every theory needs a `plans/`
doc to exist yet -- `todo.md` is a valid origin for an idea that hasn't been
formally planned out.

## Status legend

| Status | Meaning |
|---|---|
| Confirmed | Tested and the claim held up. |
| Refuted | Tested and the claim did not hold up. |
| Partially confirmed | Tested, with a mixed or caveated result. |
| Promising / unproven | Early signal in favor, but not yet statistically or experimentally settled. |
| Open / untested | Stated but not yet tested. |

## Index

| # | Theory | Status | Origin | Tested in |
|---|---|---|---|---|
| 1 | Diverse-pool vulnerability | Refuted (not fully settled) | [vs-champion-training-plan-1](../plans/vs-champion-training-plan-1-cozy-forest.md) | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) |
| 2 | Champion-dilution ceiling | Reopened (opener artifact, see theory 6) | [vs-champion-training-plan-1](../plans/vs-champion-training-plan-1-cozy-forest.md) | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md), [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |
| 3 | More data fixes champloss-only miscalibration | Disproven | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) |
| 4 | Nonlinear model (MLP/NNUE) fixes champloss miscalibration | Open / untested | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | -- |
| 5 | Color-specific evaluator weights compensate for Black's disadvantage | Open / untested | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | -- |
| 6 | Symmetric random openers inflate vs-champion results | Partially confirmed | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |
| 7 | Curriculum bootstrap succeeds where one-shot bootstrap failed | Open / untested | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | -- |
| 8 | Training-seed noise dominates hyperparameter effects | Confirmed | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 9 | Teacher search depth doesn't matter for linear-PST label quality | Confirmed | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 10 | Linear PST representation is the binding capacity ceiling | Confirmed | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 11 | Dilution decay beats flat dilution | Promising / unproven | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 12 | Replay-extraction beats bespoke single-teacher self-play | Confirmed | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 13 | Incremental wall/column delta must replicate `evalPosFull`'s edge convention exactly | Confirmed | [incremental-wall-column-eval-plan-1](../plans/incremental-wall-column-eval-plan-1-golden-forest.md) | [incremental-wall-column-eval-results-1](../plans/incremental-wall-column-eval-results-1-golden-forest.md) |
| 14 | An offline refutation book could dethrone the champion with less live compute | Open / untested | [todo.md](../todo.md) | -- |
| 15 | Champdil recovers from an identical bad/random position better than the champion, independent of color | Promising / unproven (n=20) | this session's conversation | [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |

## Theories

### 1. Diverse-pool vulnerability

**Claim:** A model trained only on champion data performs fine in aggregate,
but loses disproportionately to structurally diverse opponents outside the
champion's style.

**Status:** Refuted in the current pool, but not fully settled.

**Origin:** [vs-champion-training-plan-1-cozy-forest.md](../plans/vs-champion-training-plan-1-cozy-forest.md) -- "Theory 1."

**Tested in:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- bucket-residual analysis (champion / classic-like / diverse) showed no diverse-bucket weakness.

**Notes:** Flagged as a standing longitudinal re-check (`todo.md`, `[Now]`): re-run `tools/train_vs_champion.ps1 -AnalysisOnly` after each future batch of diverse agents joins the pool, since today's "diverse" bucket may not stay diverse as the roster grows.

### 2. Champion-dilution ceiling

**Claim:** Training on a randomly-diluted version of the champion itself
can't produce data strong enough to beat the champion -- oracle or
branch-mined data is required instead.

**Status:** Reopened. The head-to-head win that refuted it was an opener
artifact (theory 6).

**Origin:** [vs-champion-training-plan-1-cozy-forest.md](../plans/vs-champion-training-plan-1-cozy-forest.md) -- "Theory 2."

**Tested in:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- the diluted-champion (champdil) model went 50-30 (62.5%) against the champion at d6, a genuine head-to-head win. [opener-bias-results-1-synchronous-stearns.md](../plans/opener-bias-results-1-synchronous-stearns.md) -- that win required the symmetric random opener.

**Notes:** The theory-6 study reran this head-to-head with the champion playing
its own opening (config C) instead of forced-random moves: champdil dropped from
65% to 40% (80 games each), i.e. it does NOT beat a champion that plays its own
game. The original refutation stands only under symmetric random openers, so the
"you can't beat the champion with a random dilution of itself" claim is back to
unsettled. A clean re-test would generate champdil data with an asymmetric opener
and re-measure, or use no-opener paired evaluation with real opening diversity
from a different source.

### 3. More data fixes champloss-only's miscalibration

**Claim:** The champion-loss-only dataset produces a weak model because it
doesn't have enough data; more data would fix it.

**Status:** Disproven.

**Origin / Tested in:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- addendum.

**Notes:** 4x the data only moved the d4 screening Elo from 501 to 547, and real head-to-head play against the champion stayed 0-200. Reframed as a systematic label-distribution problem (one-sided data teaches a degenerate value function), not underfitting.

### 4. Nonlinear model fixes champloss miscalibration

**Claim:** A higher-capacity nonlinear model (MLP/NNUE) could succeed on the
champloss-only dataset where the linear model failed.

**Status:** Open / untested.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- Future Work.

**Notes:** Blocked on an MLP/NNUE value head existing (`src/ml_model.cpp`'s `g_modelTypes[]` currently only implements `linear`).

### 5. Color-specific evaluator weights compensate for Black's disadvantage

**Claim:** Giving White and Black separate evaluator weights could compensate
for Black's structural disadvantage in Breakthrough.

**Status:** Open / untested.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- "Ideas This Inspired."

**Notes:** Motivated by matching a White/Black asymmetry seen in both the champion's historical record and the champdil model's results.

### 6. Symmetric random openers inflate vs-champion results

**Claim:** Evaluating with symmetric random openers (`--open-plies 6` applied
to both sides) inflates every "beats the champion" result in the
vs-champion-training study.

**Status:** Partially confirmed -- it inflated the dilution result (Theory 2's
basis) but not the oracle headline result.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- Future Work #1.

**Tested in:** [opener-bias-results-1-synchronous-stearns.md](../plans/opener-bias-results-1-synchronous-stearns.md) -- all three layers (asymmetric-opener head-to-head sweep, mechanism tabulation, asymmetric-opener retrain).

**Notes:** Mechanism confirmed: a positionally-aware judge finds the random
opener leaves the champion objectively worse off on ~64% of its opener plies
(mean delta +54, 60/60 games hurt at n=60). Consequence is agent-dependent. The
champdil (dilution) model's symmetric 65% head-to-head win COLLAPSES to 40% once
the champion plays its own opening (config C, champion true policy), so Theory 2's
"dilution data beats the champion" was largely an opener artifact -- see the
caveat added to theory 2. The oracle model's win SURVIVES (58.8% symmetric ->
66.2% with the champion playing true policy) and is opener-insensitive (its
champion-random and challenger-random configs both sit near 65%), so the headline
tie is not an artifact. Read as: the symmetric opener does handicap the champion,
but only a marginal challenger's win depends on that handicap. Layer 3 (retraining
the oracle on asymmetric-opener data) showed a large d6 drop (1137 -> 832), but this
is confounded by training-label skew (the asymmetric recipe's win:loss ratio is
4.46:1 vs the symmetric recipe's 2.55:1, a known degradation mode for this
project's linear value models -- see the champloss addendum) and should not be read
as further confirmation; Layer 1's fixed-model evaluation remains the clean test and
it says the oracle's real strength holds up. Directly affects the confidence of
theories 1 and 2.

### 7. Curriculum bootstrap succeeds where one-shot bootstrap failed

**Claim:** An iterative-depth ("curriculum") bootstrap could succeed at
self-improvement where a one-shot bootstrap failed.

**Status:** Open / untested.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- "Ideas This Inspired."

**Notes:** Motivated by the one-shot bootstrap arm's failure (630 Elo vs. its parent's 825).

### 8. Training-seed noise dominates hyperparameter effects

**Claim:** Random training-seed variance is large enough to dominate most
hyperparameter comparisons in the sweep.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 1.

**Notes:** Seed replicas showed a 50-150 Elo spread, an order of magnitude above rating error -- any sweep conclusion needs multiple seeds to be trustworthy.

### 9. Teacher search depth doesn't matter for linear-PST label quality

**Claim:** The self-play teacher's search depth (d2 vs d4 vs d6) doesn't
meaningfully affect the quality of labels used to train a linear
piece-square-table value model.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 3.

**Notes:** d2, d4, and d6 teachers all landed around 510-525 Elo -- a cheap d2 teacher is not leaving quality on the table for this model class.

### 10. Linear PST representation is the binding capacity ceiling

**Claim:** The linear piece-square-table representation itself, not the
training recipe, is what caps model strength.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 8.

**Notes:** Implies the next real strength lever is model capacity (MLP/NNUE), which is itself untested -- see theory 4.

### 11. Dilution decay beats flat dilution

**Claim:** Decaying the training-generator's random-move probability over
the course of a game produces better training data than a flat dilution
probability.

**Status:** Promising / unproven.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 4.

**Notes:** Directionally favored decay as the default, but the effect is within the seed-noise band established by theory 8.

### 12. Replay-extraction beats bespoke single-teacher self-play

**Claim:** Extracting labeled training data from games already played by the
rated agent pool (`rank.exe extract`) produces better or equal training data
than generating a fresh bespoke self-play run with a single teacher, at zero
extra generation cost.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 5.

**Notes:** This result is what motivated `tools/train_scaling.ps1`'s replay-data arm, which went on to produce the d6 Elo 920 model promoted to `models/pst_value.txt`.

### 13. Incremental wall/column delta must replicate `evalPosFull`'s edge convention exactly

**Claim:** An incremental delta for the wall/column structure eval has to
reproduce `evalPosFull`'s exact edge-exclusion convention (top-row-wall and
rightmost-column pairs), or it will silently diverge from a full rescan.

**Status:** Confirmed (as a correctness lesson).

**Origin:** [incremental-wall-column-eval-plan-1-golden-forest.md](../plans/incremental-wall-column-eval-plan-1-golden-forest.md).

**Tested in:** [incremental-wall-column-eval-results-1-golden-forest.md](../plans/incremental-wall-column-eval-results-1-golden-forest.md) -- caught by the `test_eval.cpp` equivalence test on the first implementation attempt, before the fix.

**Notes:** This is the reason `evalPosLocal`'s `neighborStruct` explicitly documents `structOwner`'s single-ownership convention -- see `src/ai_eval.cpp` in `CLAUDE.md`'s file table.

### 14. An offline refutation book could dethrone the champion with less live compute

**Claim:** Because the champion agent is deterministic, its preferred lines
can be mined from `games.tsv`; running deep budgeted searches (d8-d10,
`nb2m`) on those lines offline and storing best replies keyed by
`positionKey` would let a book + d6 search agent beat the champion using less
live computation than search alone.

**Status:** Open / untested -- no plan doc written yet.

**Origin:** `todo.md`, "The most promising follow-up for the standing dethrone goal" (`[Next]`).

**Tested in:** --

### 15. Champdil recovers from an identical bad/random position better than the champion, independent of color

**Claim:** From the same random-opener starting position, champdil (the model
trained on champion-vs-diluted-champion self-play) wins more often than the
champion does, once the position's own color bias is factored out -- i.e. it
isn't just that champdil got lucky with color, it genuinely continues badly-
started games better.

**Status:** Promising / unproven -- n=20 snapshots, real signal, small sample.

**Origin:** raised in conversation while discussing why champdil's original
symmetric-opener head-to-head (Theory 6) looked strong; the developer proposed
a color-swap control: play the SAME random-opener snapshot to conclusion twice,
once with each color assignment, and classify by who wins both.

**Tested in:** `rank.exe opener-swap` (new subcommand, `rankOpenerSwap` in
`src/ranking.cpp`), champdil (s96) vs the champion, 20 snapshots, 6-ply opener,
seed 42. Of 20 snapshots: White won both continuations 11 times (55%, a color
effect -- consistent with Breakthrough's known White advantage, e.g. the
champion's own historical record is White 96.5% vs Black 87.9%), Black won both
2 times (10%, also a color effect), and in the remaining 7 (35%, the "agent
effect" bucket) **champdil won both continuations every time -- the champion
never won both, 0/20**.

**Notes:** This cleanly separates "does the position favor a color" (65% of
outcomes here) from "does one agent recover better regardless of color" (35%),
which no earlier measurement in this investigation (Layers 1/2/3, or the
general per-agent opener-Elo gap) could isolate. Within the isolated agent-effect
bucket the result is one-sided in every sample so far, which is suggestive, but
n=20 is still small -- see Future Work in the results doc for a larger-sample
follow-up before treating this as settled.

## References

Citation format: cite inline as a bracketed key, e.g. `[Sutton&Barto2018]`,
in a theory's Notes; resolve the key to a full citation here. No entries yet
-- add one here the first time a theory actually draws on external research
(for example, if a future TD-learning or self-play change is grounded in
Sutton & Barto's *Reinforcement Learning: An Introduction*, that's when its
entry belongs here, not before).
