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

Settled and definitional facts live in [axioms.md](axioms.md) (the rules,
this project's rule choices, proofs from them, and stable empirical truths).
This file is for claims still in motion. A theory that settles into general
background knowledge graduates into `axioms.md`'s empirical tier, and comes
back here if new evidence reopens it.

## How this document is organized

- **[Breakthrough Theories](#breakthrough-theories)** holds theories about
  the game and its AI: strategy, eval design, training recipes, search
  engineering. These are numbered `1, 2, 3, ...` in one continuous sequence
  across the whole document, so a cross-reference like "see theory 6"
  unambiguously means the same entry no matter which subsection it lives in.
  They're grouped into topical subsections purely for scanability -- moving
  an entry between subsections, or adding a new subsection, never changes its
  number.
- **[Other](#other)** holds theories that aren't about Breakthrough's
  gameplay or AI substance -- the development process, tooling, community
  design, or anything else this project touches. Each topic inside `Other`
  gets its own subsection with its own short letter-prefixed numbering (for
  example `L1, L2, ...` for LLM-development theories), so topics never
  collide with each other or with the Breakthrough numbering above.

**When a new theory doesn't fit an existing subsection:** if it's about
Breakthrough's gameplay or AI, add a new subsection under Breakthrough
Theories (or file it under the closest existing one if it's a one-off that
doesn't obviously deserve its own group yet). If it's not about Breakthrough
at all, add a new lettered subsection under `Other` -- that section exists
specifically so off-topic theories have an immediate home instead of forcing
a premature top-level section. If a subsection under `Other` grows into a
real recurring research area (multiple entries, its own vocabulary, its own
open questions), promote it to a full top-level section next to
`Breakthrough Theories`, the same way this file promoted LLM-development
theories out of a single stray entry into their own subsection.

## Status legend

| Status | Meaning |
|---|---|
| Confirmed | Tested and the claim held up. |
| Refuted | Tested and the claim did not hold up. |
| Partially confirmed | Tested, with a mixed or caveated result. |
| Promising / unproven | Early signal in favor, but not yet statistically or experimentally settled. |
| Open / untested | Stated but not yet tested. |

## Index

| # | Theory | Status | Section | Origin | Tested in |
|---|---|---|---|---|---|
| 1 | Diverse-pool vulnerability | Refuted (not fully settled) | Gameplay Performance & Dethroning the Champion | [vs-champion-training-plan-1](../plans/vs-champion-training-plan-1-cozy-forest.md) | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) |
| 2 | Champion-dilution ceiling | Reopened (opener artifact, see theory 6) | Gameplay Performance & Dethroning the Champion | [vs-champion-training-plan-1](../plans/vs-champion-training-plan-1-cozy-forest.md) | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md), [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |
| 3 | More data fixes champloss-only miscalibration | Disproven | Training Data & Recipes | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) |
| 4 | Nonlinear model (MLP/NNUE) fixes champloss miscalibration | Open / untested | Model & Evaluator Design | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | -- |
| 5 | Color-specific evaluator weights compensate for Black's disadvantage | Open / untested | Model & Evaluator Design | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | -- |
| 6 | Symmetric random openers inflate vs-champion results | Partially confirmed | Gameplay Performance & Dethroning the Champion | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |
| 7 | Curriculum bootstrap succeeds where one-shot bootstrap failed | Open / untested | Gameplay Performance & Dethroning the Champion | [vs-champion-training-results-1](../plans/vs-champion-training-results-1-cozy-forest.md) | -- |
| 8 | Training-seed noise dominates hyperparameter effects | Confirmed | Training Data & Recipes | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 9 | Teacher search depth doesn't matter for linear-PST label quality | Confirmed | Training Data & Recipes | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 10 | Linear PST representation is the binding capacity ceiling | Confirmed | Model & Evaluator Design | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 11 | Dilution decay beats flat dilution | Promising / unproven | Training Data & Recipes | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 12 | Replay-extraction beats bespoke single-teacher self-play | Confirmed | Training Data & Recipes | [incremental-ml-eval-plan-1](../plans/incremental-ml-eval-plan-1-luminous-snail.md) | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md) |
| 13 | Incremental wall/column delta must replicate `evalPosFull`'s edge convention exactly | Confirmed | Search & Evaluation Engineering | [incremental-wall-column-eval-plan-1](../plans/incremental-wall-column-eval-plan-1-golden-forest.md) | [incremental-wall-column-eval-results-1](../plans/incremental-wall-column-eval-results-1-golden-forest.md) |
| 14 | An offline refutation book could dethrone the champion with less live compute | Open / untested | Gameplay Performance & Dethroning the Champion | [todo.md](../todo.md) | -- |
| 15 | Champdil recovers from an identical bad/random position better than the champion, independent of color | Promising / unproven (n=20) | Gameplay Performance & Dethroning the Champion | this session's conversation | [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |
| 16 | Per-heuristic incremental evaluation gives identical results at lower cpu/node, and generalizes | Confirmed | Search & Evaluation Engineering | [`3af970d`](https://github.com/Pehz63/Breakthrough/commit/3af970dca38c749d14f0b44d183b8c87f7b4f4a7) (chip count), [incremental-wall-column-eval-plan-1](../plans/incremental-wall-column-eval-plan-1-golden-forest.md) | [incremental-wall-column-eval-results-1](../plans/incremental-wall-column-eval-results-1-golden-forest.md), [incremental-ml-eval-results-1](../plans/incremental-ml-eval-results-1-luminous-snail.md) |
| 17 | Capturing a piece one ply from winning is always optimal, except when it is the last piece | Open / untested | Game-Theoretic Structure & Optimal Play | `todo.md`, this session's conversation | -- |
| 18 | Per-side capacity/distance difference is a meaningful predictor or evaluator signal | Partially resolved (analytic: redundant as a linear eval feature; predictor half open) | Game-Theoretic Structure & Optimal Play | `todo.md` | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) |
| 19 | Same-policy agents with different IDs score differently in gauntlets (identity artifact) | Confirmed (mechanisms not yet separated) | Gameplay Performance & Dethroning the Champion | this session's sanity check | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) |
| 20 | Seeded random eval noise is a cheap tie-breaker / diversity knob | Split by form: PST refuted at both scales; bounded tie-only jitter works at ~0-80 Elo cost | Model & Evaluator Design | `todo.md` noise idea | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) (corrected), [bounded-jitter-results-1](../plans/bounded-jitter-results-1-buzzing-floyd.md) |
| 21 | Exact decided-race detection (D14) adds playing strength at fixed depth | Refuted at d6 (shallow depths untested) | Model & Evaluator Design | 2026-07-11 session (axioms D9/D14) | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) |
| 22 | Deterministic first-found tie-breaking outperforms random tie-breaking | Weakened toward refuted (2 of 6 noise seeds beat baseline) | Model & Evaluator Design | bounded-jitter retest | [bounded-jitter-results-1](../plans/bounded-jitter-results-1-buzzing-floyd.md) |
| 23 | Deterministic tie-breaking creates a systematic directional (left-file) bias exploitable as a fingerprint | Confirmed (mechanism + empirical) | Game-Theoretic Structure & Optimal Play | developer question 2026-07-12 | [bounded-jitter-results-1](../plans/bounded-jitter-results-1-buzzing-floyd.md) |
| 24 | A residual/skip-connection chip-count term lets a learned value head spend its capacity on tie-breaking rather than re-deriving material counting | Refuted at 6 seeds (skip's calibration effect within seed noise at all capacities; the 2-seed "linear yes" was noise; capacity is the real lever) | Model & Evaluator Design | this session's conversation | [residual-mlp-results-2](../plans/residual-mlp-results-2-tingly-chipmunk.md) |
| 25 | Breakthrough has distinct game phases best served by separate phase-specialized models (mixture-of-experts) | Open / untested | Model & Evaluator Design | this session's conversation | -- |
| 26 | Low-Elo games are low-quality value-training data | Open / untested | Training Data & Recipes | developer hypothesis 2026-07-14 | -- |
| 27 | Lower value-model outcome-loss does not imply higher agent Elo (offline calibration and in-search strength diverge) | Promising / observed (1 recipe: MLP beat linear on loss ~0.17 but lost ~60-110 Elo at d4) | Model & Evaluator Design | residual/MLP Elo follow-up 2026-07-14 | [residual-mlp-results-2](../plans/residual-mlp-results-2-tingly-chipmunk.md) |
| L1 | Grounding an LLM in Breakthrough fundamentals/patterns (in-context or fine-tuned) improves theory generation and code quality | Open / untested | Other > LLM-Assisted Development | this session's conversation | -- |

## Breakthrough Theories

### Gameplay Performance & Dethroning the Champion

Theories about win rate, strength, and the standing goal of beating the
reigning champion agent -- including measurement artifacts (like opener
bias) that affect how trustworthy a "beats the champion" result is.

#### 1. Diverse-pool vulnerability

**Claim:** A model trained only on champion data performs fine in aggregate,
but loses disproportionately to structurally diverse opponents outside the
champion's style.

**Status:** Refuted in the current pool, but not fully settled.

**Origin:** [vs-champion-training-plan-1-cozy-forest.md](../plans/vs-champion-training-plan-1-cozy-forest.md) -- "Theory 1."

**Tested in:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- bucket-residual analysis (champion / classic-like / diverse) showed no diverse-bucket weakness.

**Notes:** Flagged as a standing longitudinal re-check (`todo.md`, `[Now]`): re-run `tools/train_vs_champion.ps1 -AnalysisOnly` after each future batch of diverse agents joins the pool, since today's "diverse" bucket may not stay diverse as the roster grows.

#### 2. Champion-dilution ceiling

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

#### 6. Symmetric random openers inflate vs-champion results

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

#### 7. Curriculum bootstrap succeeds where one-shot bootstrap failed

**Claim:** An iterative-depth ("curriculum") bootstrap could succeed at
self-improvement where a one-shot bootstrap failed.

**Status:** Open / untested.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- "Ideas This Inspired."

**Notes:** Motivated by the one-shot bootstrap arm's failure (630 Elo vs. its parent's 825).

#### 14. An offline refutation book could dethrone the champion with less live compute

**Claim:** Because the champion agent is deterministic, its preferred lines
can be mined from `games.tsv`; running deep budgeted searches (d8-d10,
`nb2m`) on those lines offline and storing best replies keyed by
`positionKey` would let a book + d6 search agent beat the champion using less
live computation than search alone.

**Status:** Open / untested -- no plan doc written yet.

**Origin:** `todo.md`, "The most promising follow-up for the standing dethrone goal" (`[Next]`).

**Tested in:** --

#### 19. Same-policy agents with different IDs score differently in gauntlets (identity artifact)

**Claim:** Two agents with provably identical policies but different canonical
ID strings can score meaningfully differently (up to ~100-200 Elo on small
pools) in `rank.exe gauntlet`, because (a) per-game srand seeds are derived
from the ID strings, so stochastic opponents play different games against
each, and (b) search side-state (TT, killer/history tables) persists across
games within a gauntlet process, so even deterministic opponents diverge
between replays and depend on the preceding game sequence.

**Status:** Confirmed as an artifact (the two mechanisms are not yet
separated or sized individually).

**Origin:** this session's sanity check that a champion-equivalent Advanced
agent "ties the champion."

**Tested in:** [heuristic-eval-overhaul-results-1-buzzing-floyd.md](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) --
`pairgen` produced byte-identical games for the adv-equivalent vs the
champion (policy identity proven), yet main-pool gauntlets at two seeds gave
the champion ID 1162/1193 vs the adv-equivalent 1064/1102, and a THIRD
equivalent (`exp(...,f0)@2`, pre-existing code) scored 1078, agreeing with
the adv-equivalent. Per-opponent records against deterministic learned agents
differed between gauntlets, implicating cross-game state. On the small climb
pool the same-policy gap reached ~200 (t1,c4 baseline 1362 vs t20,c80
baseline 1158 at n=108 games).

**Notes:** Practical rules: use pairgen (byte-level) or full pool refits to
compare near-identical agents, never single gauntlets; treat hill-climb
fitness as noisy at the +/-50-100 level beyond its printed SE; and when
comparing two configs, keep every OTHER ID segment identical so the artifact
is shared. Follow-up in the results doc's Future Work: a `--reset-state`
flag to clear search state between games would separate mechanism (b) from
(a). Standard mitigation practice (adopted 2026-07-12, recorded in
`Docs/benchmarking.md`'s "Measuring strength" section): strength comparisons
run on the full main roster with at least two `--seed` replicates per config,
and differences are read against the replicate spread rather than the printed
single-gauntlet SE.

#### 15. Champdil recovers from an identical bad/random position better than the champion, independent of color

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

### Training Data & Recipes

Theories about what data a training run should use and how, independent of
any specific opponent -- data sourcing, dilution schedules, and how much
seed-to-seed noise a comparison needs to survive before it means anything.

#### 3. More data fixes champloss-only's miscalibration

**Claim:** The champion-loss-only dataset produces a weak model because it
doesn't have enough data; more data would fix it.

**Status:** Disproven.

**Origin / Tested in:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- addendum.

**Notes:** 4x the data only moved the d4 screening Elo from 501 to 547, and real head-to-head play against the champion stayed 0-200. Reframed as a systematic label-distribution problem (one-sided data teaches a degenerate value function), not underfitting.

#### 8. Training-seed noise dominates hyperparameter effects

**Claim:** Random training-seed variance is large enough to dominate most
hyperparameter comparisons in the sweep.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 1.

**Notes:** Seed replicas showed a 50-150 Elo spread, an order of magnitude above rating error -- any sweep conclusion needs multiple seeds to be trustworthy.

#### 9. Teacher search depth doesn't matter for linear-PST label quality

**Claim:** The self-play teacher's search depth (d2 vs d4 vs d6) doesn't
meaningfully affect the quality of labels used to train a linear
piece-square-table value model.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 3.

**Notes:** d2, d4, and d6 teachers all landed around 510-525 Elo -- a cheap d2 teacher is not leaving quality on the table for this model class.

#### 11. Dilution decay beats flat dilution

**Claim:** Decaying the training-generator's random-move probability over
the course of a game produces better training data than a flat dilution
probability.

**Status:** Promising / unproven.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 4.

**Notes:** Directionally favored decay as the default, but the effect is within the seed-noise band established by theory 8.

#### 12. Replay-extraction beats bespoke single-teacher self-play

**Claim:** Extracting labeled training data from games already played by the
rated agent pool (`rank.exe extract`) produces better or equal training data
than generating a fresh bespoke self-play run with a single teacher, at zero
extra generation cost.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 5.

**Notes:** This result is what motivated `tools/train_scaling.ps1`'s replay-data arm, which went on to produce the d6 Elo 920 model promoted to `models/pst_value.txt`.

#### 26. Low-Elo games are low-quality value-training data

**Claim:** Training a value model on positions drawn from low-Elo (weak) agents'
games produces a weaker model than training on high-Elo games only. Weak play
mislabels positions: a position is labeled with its game's eventual outcome, but in
a low-Elo game that outcome often hinges on a later blunder, so the label is noisy
and does not reflect the position's true value. Mixed high-vs-low games may be
similarly poor (the result reflects the weaker side's error, not position value). A
reward signal weighted toward higher-Elo games may improve data quality.

**Status:** Open / untested.

**Origin:** developer hypothesis, 2026-07-14, raised while planning the
residual/MLP Elo follow-up.

**Tested in:** --

**Notes:** Test by retraining the existing value-model recipes on replay data
filtered by the participating agents' Elo: (a) excluding low-Elo agents' games,
(b) excluding mixed high-vs-low games, (c) excluding high-Elo games (the control),
and comparing the resulting models' Elo; plus an Elo-weighted-label variant
(stronger signal from higher-Elo games). The mechanism is the `todo.md`
"Extraction quality controls in rank.exe extract" item (`--min-elo` floor /
Elo-confidence weighting). Relates to theory 12 (replay-extraction beats
single-teacher self-play) -- this refines it by asking WHICH replayed games help --
and to theory 8 (training-seed noise dominates hyperparameter effects), which any
such comparison must clear with seed replicas before its deltas mean anything.

### Model & Evaluator Design

Theories about what an evaluator or value model should represent or weight
-- representation capacity, color asymmetry, and what a model class can or
can't learn.

#### 4. Nonlinear model fixes champloss miscalibration

**Claim:** A higher-capacity nonlinear model (MLP/NNUE) could succeed on the
champloss-only dataset where the linear model failed.

**Status:** Open / untested.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- Future Work.

**Notes:** Blocked on an MLP/NNUE value head existing (`src/ml_model.cpp`'s `g_modelTypes[]` currently only implements `linear`).

#### 5. Color-specific evaluator weights compensate for Black's disadvantage

**Claim:** Giving White and Black separate evaluator weights could compensate
for Black's structural disadvantage in Breakthrough.

**Status:** Open / untested.

**Origin:** [vs-champion-training-results-1-cozy-forest.md](../plans/vs-champion-training-results-1-cozy-forest.md) -- "Ideas This Inspired."

**Notes:** Motivated by matching a White/Black asymmetry seen in both the champion's historical record and the champdil model's results.

#### 20. Seeded random eval noise is a cheap tie-breaker / diversity knob

**Claim:** A tiny seeded random eval term ("dominated by the real evaluation
so tactics still win, but breaks ties and re-sorts move ordering within
near-equal branches") produces useful behavioral diversity at negligible
strength cost.

**Status:** Split by form after a corrected re-test. The per-piece (random
PST) form is refuted at both tested scales. The bounded per-position jitter
form (tie-only by construction) delivers the diversity at ~zero average
strength cost: a six-noise-seed sweep means 1113 vs baseline 1118, with 2 of 6
seeds beating baseline (the initial "~0-80 cost" was a single low-seed
artifact, see theory 22).

**Origin:** `todo.md`'s Heuristic Evaluator Feature Ideas (`[Now]` noise
idea). Implemented twice on the Advanced evaluator's Noise param, selected by
sign: n > 0 = random PST (per-(color,square) values in [-n, +n]); n < 0 =
bounded jitter (leaf returns realEval * 256 + (rawHashSum mod 2n+1) - n, the
developer's tie-only-by-construction design).

**Tested in:** first (climb-pool) pass in
[heuristic-eval-overhaul-results-1-buzzing-floyd.md](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md)
(corrected 2026-07-12: its "refuted even when dominated" framing over-reached
-- the chip=4 numbers were not a dominated config at all, and the mechanism
sentence was a guess); proper re-test in
[bounded-jitter-results-1-buzzing-floyd.md](../plans/bounded-jitter-results-1-buzzing-floyd.md)
on the main-roster instrument, 5 configs x 2 seed replicates at the champion
head, plus the in-code dominance (order-preservation) walk and an
effective-depth probe.

**Notes:** Evidence, split cleanly. (1) PST at chip=4 (n1 -522, n3 -800,
climb pool) refutes noise at MATERIAL scale; the dominance walk pins in code
that this config reverses real material preferences, so it never tested
tie-breaking. (2) PST at chip=80 on the main roster: 887/869 (n1) and 898/913
(n3) vs baseline 1118/1148 -- ~-240 in all four runs. The dominance walk
proves sibling material flips are impossible at this scale, and the
effective-depth probe caps the budget-consumption pathway at ~0.09 ply
(~10-15 Elo), so the damage flows through its tie DECISIONS; the supported
hypothesis (labeled as such) is the persistent per-square bias, which repeats
the same arbitrary preference at every tie all game. (3) Bounded jitter:
provably tie-only (dominance walk asserts zero reversals; any future reversal
is an implementation bug), deterministic-per-seed diversity confirmed by
byte-level pairgen checks, and main-roster means of -27 (n-1) / -79 (n-3)
against replicate spreads of 30-138 with only ~10 Elo of measured depth loss
-- whether the remainder is real tie-choice quality or replicate noise is
theory 22. Search-shape caveat: breaking ties cost +64% nodes/move at a bare
d4 head but washed out at the d6/ord/nb200k head; such numbers do not
transfer across heads.

#### 21. Exact decided-race detection (D14) adds playing strength at fixed depth

**Claim:** A leaf detector that returns a win sentinel for provably decided
races (axioms.md D9/D14: passed runner + distance margins + piece-count
margin) effectively sees race outcomes many plies beyond the nominal depth,
and should add Elo at a fixed search depth.

**Status:** Refuted at d6 against the current pool (no measurable effect);
shallow depths and budgeted searches untested.

**Origin:** this session, derived directly from `Docs/axioms.md` D9/D14
while implementing the Advanced evaluator (the RaceWin `g` param, 0/1,
`raceWinCheck` in `src/ai_eval.cpp` -- proven sound, with a one-tempo
tightened margin for the non-mover side, unit-tested with witness and
near-miss positions).

**Tested in:** [heuristic-eval-overhaul-results-1-buzzing-floyd.md](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) --
climb-pool gauntlet at the champion head: g1 = 1292 +/- 72 vs g0 = 1362 +/-
85 (a -70 +/- 111 difference, within noise), while costing ~8% us/move.

**Notes:** The likely reason: by d6 the search already resolves most races
that D14's conservative margins certify, so the detector rarely changes a
leaf value that mattered. The interesting untested regime is shallow (d2-d4)
or budget-cut searches, where the detector genuinely adds horizon. Note the
soundness is unconditional (rules-proven), so enabling it can change play
only toward truth; any Elo loss beyond timing noise would indicate an
implementation bug, and none was seen.

#### 22. Deterministic first-found tie-breaking outperforms random tie-breaking

**Claim:** At exact evaluation ties, the plain search's deterministic choice
(the first maximal move under capture-first ordering) is genuinely better
than a uniformly random choice among the tied moves -- i.e. tie-breaking
policy carries real Elo, and "first-found" encodes useful bias (captures and
stable piece order) rather than being arbitrary.

**Status:** Weakened toward refuted. A six-seed sweep showed random
tie-breaking is NOT robustly worse than deterministic first-found: 2 of 6
jitter seeds beat the no-jitter baseline outright and the mean was within
noise.

**Origin:** the bounded-jitter retest: at noise seed s=1 a provably tie-only
jitter showed mean drops of -27 (n-1) / -79 (n-3) vs baseline, which looked
like a tie-choice cost until the seed was varied.

**Tested in:** [bounded-jitter-results-1-buzzing-floyd.md](../plans/bounded-jitter-results-1-buzzing-floyd.md) --
noise-seed sweep (n-1 at seeds 1-6, main roster): baseline 1118 vs jitter
1037/1076/1148/1123/1181/1113 (mean 1113). Seeds s5 (1181) and s3 (1148) beat
baseline, s4 (1123) ties it.

**Notes:** The original single-seed signal was an artifact -- s=1 (1037) is
the worst of the six. First-found tie-breaking is the zero-variance default,
not a strength edge: some random tie-breaks are as good or better. The residual
open question is only whether the LOW seeds (s1/s2) are genuinely worse or just
the low tail of a neutral distribution; the clean test remains a variant that
picks the plain agent's move at ties while jittering elsewhere. Contrast with
the PST form's ~-240: PERSISTENT random bias at ties (same wrong preference all
game) is far worse than MEMORYLESS random tie choice, which is consistent with
tie decisions mattering only when they are correlated across a game.

#### 10. Linear PST representation is the binding capacity ceiling

**Claim:** The linear piece-square-table representation itself, not the
training recipe, is what caps model strength.

**Status:** Confirmed.

**Origin:** [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md).

**Tested in:** [training-sweep-results-1-luminous-snail.md](../plans/training-sweep-results-1-luminous-snail.md) -- Finding 8.

**Notes:** Was read as implying the next real strength lever is model capacity
(MLP/NNUE). Theory 27 challenges that reading: a hand-written MLP DID break the
linear loss ceiling (much lower outcome loss) but rated LOWER in Elo at depth 4, so
"more capacity" lowered loss without raising strength for that recipe. The linear
class caps LOSS-based fit, but raising capacity is not automatically a strength win
-- see theory 27 and theory 4.

#### 24. A residual/skip-connection chip-count term lets a learned value head spend its capacity on tie-breaking rather than re-deriving material counting

**Claim:** Because the strongest agents in the current pool are plain chip
counters or evaluators statistically tied with one (see `todo.md`'s Agent
Track goal: "the best agents so far are just chip counts"), a learned value
head that has to discover material counting from scratch may be spending
capacity re-deriving something already known. Architecturally fixing (or
strongly regularizing toward) a chip-count term as an additive skip
connection into the head's output -- so the head computes `chipCount +
learned(board)` instead of `learned(board)` alone -- would free the learned
part to specialize on the residual: distinguishing and tie-breaking among
positions with equal or near-equal material, which a pure material count
cannot do at all.

**Status:** Refuted as a calibration effect once properly seeded. The HARD (frozen)
skip was built (`ResidualModel` = `skipW*matDiff` + a linear or MLP inner) and
measured by a stratified logistic loss over `|matDiff|` buckets. A first 2-seed run
looked like the skip helped a LINEAR inner, but at 6 training seeds the effect falls
inside the seed-noise band at every capacity.

**Origin:** developer's hypothesis in conversation, motivated directly by the
Agent Track's current standing and posed as a way to make a future capacity
jump (MLP/NNUE, theory 4) target the right thing rather than just adding raw
capacity.

**Tested in:** [residual-mlp-results-2-tingly-chipmunk.md](../plans/residual-mlp-results-2-tingly-chipmunk.md)
(6-seed, the settled result; supersedes the 2-seed
[residual-mlp-results-1](../plans/residual-mlp-results-1-tingly-chipmunk.md)):
`sweep_pst_v2.ps1` groups F (linear) and G (MLP) across 6 training seeds on an
8000-game replay extract, reading each recipe's equal-material (`==0`) loss, plus a
`--val-split` held-out re-check.

**Notes:** The 2-seed pass (results-1) reported the linear skip lowering `==0` loss
~0.10 (0.742 vs 0.644) and "confirmed theory 24 at the linear level." At 6 seeds
that gap collapses to 0.019 (0.698 plain vs 0.679 residual) inside a ~0.18-0.20 seed
spread, and the held-out re-check gives 0.6267 vs 0.6268 -- the skip does essentially
nothing for equal-material calibration, in-sample or held-out. The MLP skip deltas
are also within noise and slightly POSITIVE (worse): +0.010 at hidden 16, +0.006 at
hidden 32. So the skip's calibration benefit does not survive proper seeding at any
capacity. What DOES move calibration is capacity itself: linear ~0.69 -> mlp(16)
~0.55 -> mlp(32) ~0.52, with the MLP seed spreads ~10x tighter -- the theory-10
story (break the linear PST ceiling with capacity, not a material scaffold). The
motivating premise ("a linear head wastes capacity re-deriving material") is not
supported: a linear v2 model already spans material, so fixing it changes only the
optimization path, and at 6 seeds that averages out. This is a direct win for the
~6-seed rule (theory 8): the 2-seed comparison invented an effect that is not there.
The linear group's full-roster Elo (depth 4, 6 seeds) agrees: residual-linear mean
747 vs plain-linear 799, within the seed spread and if anything slightly negative,
so the skip does not help strength either. The MLP capacity group's Elo -- whether
its far better calibration becomes strength (the theory-10 question) -- is the
pending measurement (full-scan MLP at depth 6 is slow). Decisions this session: HARD frozen skip and the
literal chip differential; the soft/regularized skip and a broader baseline remain
open (they could still matter for a model class that cannot already express
material, unlike the linear v2 here). Related to theory 4 (nonlinear capacity jump),
theory 10 (linear PST ceiling), theory 8 (training-seed noise), and theory 22.

#### 27. Lower value-model outcome-loss does not imply higher agent Elo

**Claim:** For a value model used as an alpha-beta leaf evaluator, a lower
outcome-prediction loss (better offline calibration) does not translate to higher
agent Elo. The two can diverge outright: a higher-capacity model that fits the
outcome labels better can play worse.

**Status:** Promising / observed -- one recipe, depth 4.

**Origin:** the residual/MLP Elo follow-up, 2026-07-14, when the MLP value model was
finally rated (the whole point of that follow-up).

**Tested in:** [residual-mlp-results-2-tingly-chipmunk.md](../plans/residual-mlp-results-2-tingly-chipmunk.md) --
6-seed full-roster run at depth 4. The MLP (129 -> 16 or 32 -> 1, ReLU) beat the
linear v2 model on equal-material loss by ~0.17 (0.52 vs 0.69) and on overall loss,
yet rated ~60-110 Elo LOWER in the same depth-4 search (mlp(32) ~652, mlp(16) ~675,
linear ~756); more hidden width lowered loss further and Elo further.

**Notes:** Candidate mechanisms: (1) overfitting the noisy outcome labels -- early
stopping showed even the linear model's validation loss bottoms at epoch 1 on 320k
positions, and these MLPs trained 6 epochs; (2) a loss-optimal evaluator is not a
good move-RANKER for alpha-beta (cf. the PST pruning ~3x worse than Classic in
[training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md)); (3)
miscalibration at the decision-relevant margins that a mean loss washes out.
Directly motivates training objectives beyond raw outcome log-loss (eval-blended
labels, ranking losses -- see `todo.md` Training Regimes), early stopping /
regularization for the MLP, and measuring move-ordering quality, not just loss. It
refutes the naive reading of theory 10 that "the next strength lever is capacity":
capacity lowered loss but not Elo here. And it is the concrete case the
always-measure-Elo standing rule exists to catch -- on calibration alone the MLP
looked like a large win.

#### 25. Breakthrough has distinct game phases best served by separate phase-specialized models (mixture-of-experts)

**Claim:** Breakthrough passes through qualitatively distinct phases
(opening/full-material, midgame, endgame/low-material) where different
strategies are optimal, rather than one continuum that varies smoothly. A
mixture-of-experts architecture -- a router conditioned on game phase
(startable as a simple total-material/piece-count classifier: high piece
count routes to an opener expert, low piece count to an endgame expert, with
a graded band in between) dispatching to separate phase-specialized learned
models -- would outperform one model trained to cover the whole game.

**Status:** Open / untested.

**Origin:** developer's hypothesis in conversation, framed directly as a
mixture-of-experts design with a material-count router.

**Tested in:** --

**Notes:** Relates to the Tapered/phase-split PST idea (`todo.md`, Training
Regimes), which addresses the same phase intuition via smooth weight
interpolation inside ONE linear model rather than hard routing between
separate models -- the two are complementary architectural bets on the same
underlying claim (phases matter) and could be compared directly (interpolated
single model vs. routed multi-model) once both exist. Also related to the
existing "separate opener model" idea (`todo.md`, Agent Track opener
section) and the "Ensemble / blended evaluator" idea (`todo.md`,
Board-State Evaluators), of which this is a phase-conditioned specialization:
routing instead of blending uniformly. Open design questions: (1) where the
phase boundaries actually are empirically -- is there a sharp regime change
or a smooth gradient? This is itself the crux of the phases-are-distinct
claim and could be tested independently of building the router, e.g. by
checking whether an evaluator's optimal weights genuinely diverge across
piece-count bins rather than sliding continuously; (2) whether the router
should be hard or soft/blended near boundaries; (3) how each expert should
be trained -- the same recipe with phase-filtered data, or a different
recipe per phase; (4) whether phase should be measured by piece count alone
or a richer signal (e.g. total remaining capacity, `Docs/axioms.md` Lemma B).

### Search & Evaluation Engineering

Theories about how the search and evaluator are *implemented* -- correctness
and performance of the engine itself, as opposed to what it should compute.
A change here should leave game outcomes and Elo unchanged; the theories are
about cpu/node, not strength.

#### 13. Incremental wall/column delta must replicate `evalPosFull`'s edge convention exactly

**Claim:** An incremental delta for the wall/column structure eval has to
reproduce `evalPosFull`'s exact edge-exclusion convention (top-row-wall and
rightmost-column pairs), or it will silently diverge from a full rescan.

**Status:** Confirmed (as a correctness lesson).

**Origin:** [incremental-wall-column-eval-plan-1-golden-forest.md](../plans/incremental-wall-column-eval-plan-1-golden-forest.md).

**Tested in:** [incremental-wall-column-eval-results-1-golden-forest.md](../plans/incremental-wall-column-eval-results-1-golden-forest.md) -- caught by the `test_eval.cpp` equivalence test on the first implementation attempt, before the fix.

**Notes:** This is the reason `evalPosLocal`'s `neighborStruct` explicitly documents `structOwner`'s single-ownership convention -- see `src/ai_eval.cpp` in `CLAUDE.md`'s file table.

#### 16. Per-heuristic incremental evaluation gives identical results at lower cpu/node, and generalizes

**Claim:** A board evaluator's terms can be computed incrementally -- updating
only the contribution of the (at most 2) squares a move actually changes,
instead of rescanning the whole board from scratch -- while still producing
results byte-identical to a full recompute. This composes with alpha-beta
search to lower per-node cpu cost with no change in search outcome (move
choice) or measured Elo, since make/unmake already visits exactly the two
changed squares as part of applying/reversing the move. The pattern is not
specific to one heuristic: it has been applied to chip count, and to the
Classic/Experimental wall and column structure terms, and generalizes to
other evaluator heuristics.

**Status:** Confirmed, with a scope qualifier from the chip-count study:
incremental is faster only while the cached term is nonzero. When every weight
an accumulator maintains is zero, the per-node maintenance calls are pure
overhead and the incremental path is measurably SLOWER (+18 to +35% us/move at
champion weights w0,l0) than the full-scan fallback, whose scan early-outs.

**Origin:** [`3af970d`](https://github.com/Pehz63/Breakthrough/commit/3af970dca38c749d14f0b44d183b8c87f7b4f4a7) "Optimize minimax search with incremental counters and capture-first ordering" (2026-06-04) -- the first instance of this pattern, replacing a full-board `chipDiff()` rescan with the `g_chipDiff`/`g_whiteCount`/`g_blackCount` counters maintained incrementally inside the move-apply/unapply code; predates this project's dedicated "incremental eval" plans and its scientific-methodology conventions (no companion plan/results doc, just a terse commit message from when this was still an unstructured hobby project). [incremental-wall-column-eval-plan-1-golden-forest.md](../plans/incremental-wall-column-eval-plan-1-golden-forest.md) -- second heuristic (wall/column structure) migrated to the same pattern, this time with a formal plan/results doc; generality across heuristics further confirmed by [incremental-ml-eval-plan-1-luminous-snail.md](../plans/incremental-ml-eval-plan-1-luminous-snail.md), which applies the same pattern to a different evaluator (the learned piece-square value model).

**Tested in:** [incremental-wall-column-eval-results-1-golden-forest.md](../plans/incremental-wall-column-eval-results-1-golden-forest.md) -- `test_eval.cpp`'s equivalence test walks the move tree asserting the incremental accumulator (`g_evalPos`) always equals a full `evalPosFull` recompute, and measured a **33-39% cpu/node reduction** (`ab(d4).classic(t2,c10,w3,l2)` -39.3%, two Experimental presets -37.9%/-33.4%) with byte-identical eval values, so game outcomes and Elo were unchanged by construction. [incremental-ml-eval-results-1-luminous-snail.md](../plans/incremental-ml-eval-results-1-luminous-snail.md) -- same incremental-accumulator pattern (`g_mlAcc`/`mlLeafScore`) applied to the learned value model, the pattern's second extension beyond chip count. The original chip-count commit (`3af970d`) predates this project's equivalence-test/results-doc discipline, so its correctness rests on the counters' logic (increment/decrement mirrored exactly on capture, in both `simulateMove` and `unsimulateMove`) rather than an automated equivalence check -- no regression has surfaced since, but it wasn't verified the same rigorous way as the two later heuristics. [chip-count-speedup-results-1-iterative-raven.md](../plans/chip-count-speedup-results-1-iterative-raven.md) (2026-07-10) retroactively fills that gap: `train.exe speed`'s eval-level ladder (`g_evalLevel` 1/2/3) reconstructed the pre-`3af970d` leaf and measured, with an in-harness equivalence check (same end board + node count across levels, PASS) standing in for the missing test. Measured chip-count speedup (v1->v2): **-45 to -62% us/move at the champion's zero-structure weights** (w0,l0, the historically relevant configuration, roughly a 2x speedup) and **-14 to -16%** when a full structure scan shares the leaf (w2,l2, depth >= 3). The same run re-measured the structure step (v2->v3, full per-leaf scan -> cached `g_evalPos`) at **-62 to -66% us/move** -- larger than the 33-39% above because the baselines differ: the 33-39% measured only the pt.2 refinement (bounding-box delta -> neighbor-local delta) against an already-incremental baseline, while the ladder's v2 baseline is the older full-per-leaf scan; the numbers nest consistently (~-40% pt.1 x ~-39% pt.2 = ~-63% total). It also found the zero-weight overhead in the Status qualifier above (the incremental machinery costs +18 to +35% when it maintains an always-zero accumulator), suggesting a weight-gated `g_evalIncremental` as a follow-up engine win.

**Notes:** The Forward weight rides along with wall/column in the same `g_evalPos` accumulator (see `ai_eval.cpp`), so Classic/Experimental have no remaining non-incremental term. `evalBeginSearch`/`evalEndSearch` seed and tear down the `g_evalPos` accumulator per search, which is why the wall/column and ML results stay exact rather than becoming an approximation; chip count's `g_chipDiff` is simpler still, just a running delta with no begin/end seeding needed. The next candidate for the same treatment is a future nonlinear (MLP/NNUE) value head (see theory 4) -- incrementality is harder there because hidden-layer activations don't decompose per-square the way a linear dot product does, so this pattern's applicability to that case is not yet established. **Second scope qualifier (Advanced-evaluator overhaul, [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md)):** the zero-weight lesson generalizes to nonzero weights. A term whose full-board scan is cheap (hole: 16 column checks; open: 64 reads) or whose local delta touches a wide affected set (mobility: a bounding-box sum paid at every make AND unmake, interior nodes included) measured SLOWER incrementally than as a per-leaf full scan when it is the only enabled term (+17 to +68% us/move on the ladder), while the all-terms-on mix still wins big incrementally (-41 to -52%). Incremental is not a per-term free win; it pays off when several terms share the per-move delta. The zero-weight half is now fixed in code (`posWeightsActive` gating, ladder-verified v2->v3 ~0% at w0,l0); per-term routing for sparse mixes is filed in `todo.md`.

### Game-Theoretic Structure & Optimal Play

Theories about Breakthrough's own strategic structure and optimal play --
proven-but-untested (or hard to prove) claims about the game itself, in the
same spirit as `Docs/axioms.md`'s derived and empirical tiers but not yet
settled enough to belong there. Distinct from Gameplay Performance above,
which is about a specific agent's win rate, not the game's structure. The
open-ended questions filed under `todo.md`'s "Interpret board analysis" (is
attacking the center or the edge better, is advancing through the center or
edge better, is keeping the hind pieces in place better, and so on) are
natural future entries here once any of them gets formalized into a
testable claim.

#### 23. Deterministic tie-breaking creates a systematic directional (left-file) bias

**Claim:** Because the evaluator and standard start are left-right mirror
symmetric, every left/right move choice is an exact eval tie, and the engine's
fixed first-found tie-break (root enumeration `x = 0..7`, left-diagonal
before right) resolves every one of them toward the left -- producing a
systematic queenside pile-up that is a pure artifact of the enumeration order,
not of position value. Any consistent tie-break creates such a directional
bias; the direction is arbitrary (reverse the enumeration and it flips to the
right), but the bias is inherent and could serve as an agent fingerprint or
exploitation target.

**Status:** Confirmed -- both the mechanism (from the code) and the effect
(empirically).

**Origin:** the developer's question of whether the no-jitter agent's tie
behavior is "an artifact of good sorting, or an artifact that any consistent
sort creates a directed search."

**Tested in:** [bounded-jitter-results-1-buzzing-floyd.md](../plans/bounded-jitter-results-1-buzzing-floyd.md) --
tie-default confirmed first-found in `src/ai_minimax.cpp` (strict `>`); a
console depth-5 mirror match played first move a2-b3 and 17 of 17 White moves
into files a-d, zero to the right.

**Notes:** Distinct from "good sorting": capture-first ordering helps alpha-
beta pruning and does NOT cause the bias; the left bias comes purely from the
arbitrary `x = 0..7` / left-diagonal-first order. The bias is not obviously a
losing flaw (the agent still wins the mirror and rates ~1133 vs the pool), but
it is a predictability concern -- a counter-agent could exploit "this build
always develops queenside" (see the adversarial counter-agent idea in
`todo.md`). The bounded jitter is one mitigation (it breaks the symmetry ties),
but its tie-breaking strength is search-config-dependent (theory 20 notes).
Related to `Docs/axioms.md` O4/D5 (the start's mirror symmetry) and E1 (the
White tempo advantage that still decides the mirror game).

#### 17. Capturing a piece one ply from winning is always optimal, except when it is the last piece

**Claim:** When an opponent has a piece that would win next ply if left
alone, capturing it is always at least as good as any other reply, with a
stated exception "except when it's the last piece."

**Status:** Open / untested.

**Origin:** `todo.md`, while scoping a search tool to compute/bound
"distance-to-win" as a rigorous companion to `Docs/axioms.md` Lemma B's
capacity measure.

**Tested in:** --

**Notes:** The exception clause is ambiguous as stated and should be pinned
down before testing. Reading A: "it" is the threatened piece, and the
exception is that piece being the opponent's LAST one -- in which case
capturing it doesn't just defuse a threat, it wins outright via A9/D6, a
stronger and different claim than mere optimality. Reading B: "it" is the
capturing piece, and the exception is that piece being the defender's OWN
last piece, where diverting it to capture might be suboptimal for other
reasons (e.g. needed to defend elsewhere). Testable once the distance-to-win
search tool above exists, or via targeted hand-constructed counterexample
positions checked against a deep search. Related to `Docs/axioms.md` D9
(passed runners) and D10 (back-rank outposts), which already formalize when
a piece IS one ply from winning.

#### 18. Per-side capacity/distance difference is a meaningful predictor or evaluator signal

**Claim:** The difference between the two sides' total remaining "capacity"
(`Docs/axioms.md` Lemma B: each side's sum of its own pieces' row-distance to
its own goal) is a meaningful predictor of who is winning, and/or a useful
evaluator feature.

**Status:** Partially resolved. The evaluator-feature half is settled
analytically: redundant for any linear evaluator that already has chip and
forward terms. The predictor-correlation half stays open.

**Origin:** `todo.md`'s Heuristic Evaluator Feature Ideas (extending the
existing Race-distance differential idea), the developer's own stated
hypothesis: "I suspect no, because running out of pieces is a loss but can
reduce your capacity. But I wouldn't be surprised either way. Maybe stronger
bots inherently chase it down fast by trying to capture and gain material
advantage."

**Tested in:** [heuristic-eval-overhaul-results-1-buzzing-floyd.md](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) --
the identity `capacityBlack - capacityWhite == forwardSum - 7*chipDiff` is
exact (derivable in two lines from Lemma B's definitions) and code-verified
by a unit test in `tests/test_eval.cpp` using the new
`capacityWhite/Black()` helpers (`src/board_analysis.cpp`).

**Notes:** The identity gives the developer's suspicion a precise form: the
capacity difference IS advancement minus 7x material, so it conflates the
two exactly as suspected, and adding it as a linear eval weight spans
nothing new (a capacity weight k is identical to forward += k, chip -= 7k).
Two consequences. (1) The pure-capacity play direction requires a NEGATIVE
chip weight, reachable only by the hill climber's `-AllowNegative` mode --
and the first signed climb ran that test: negative-chip candidates were
proposed 15 times (pure anti-material `c-80` six times) and every one scored
300-750 Elo below the chip-positive band, none accepted. Capacity-direction
play is decisively weaker at d4, matching the developer's suspicion. (2) The
open predictor half (does the raw difference correlate with game outcomes
across `ranking/matches.jsonl`) now has its helpers ready. Related to theory
5 (color-specific weights) and `Docs/axioms.md` E1, since any per-side
asymmetry here could interact with the known White/Black imbalance.

## Other

Theories that aren't about Breakthrough's gameplay or AI substance. Grouped
into subsections by topic, each with its own letter-prefixed numbering so
topics never collide.

### LLM-Assisted Development

Theories about the LLM-assisted development process used to build this
project: whether and how giving an LLM project-specific context changes the
quality of the theories, code, or plans it produces. Numbered `L#`.

#### L1. Context/fine-tuning grounding improves LLM theory generation and code quality

**Claim:** An LLM given data about Breakthrough fundamentals and this
project's recurring patterns (agent composition, eval structure, the
dilution/opener/ranking vocabulary in [terminology.md](terminology.md), the
shape of past theories above) -- whether supplied in-context or via
fine-tuning -- will be more directed, better-aligned, and generate stronger
theories and code than one working from a generic prompt alone.

**Status:** Open / untested.

**Origin:** raised in conversation, prompted by building out this theory log
and the terminology glossary -- the developer noted this is a theory about
LLM behavior, not about Breakthrough, and asked for it to be tracked
separately.

**Tested in:** --

**Notes:** Distinct from the Breakthrough theories above in kind, not just
topic: the "system under test" is the LLM's behavior across sessions, not a
game agent's win rate. A test would need its own methodology -- e.g. holding
the underlying model fixed and comparing theory/code quality with vs.
without this project's accumulated context (this file, `terminology.md`,
`CLAUDE.md`, past `plans/` docs) in-context, or comparing a base model
against one fine-tuned on this project's plans/results/code. "Better" would
need an operational definition (fewer refuted theories per session, less
rework, fewer correctness gotchas caught late, faster convergence to a
working plan) before this is testable rather than just plausible.

## References

Citation format: cite inline as a bracketed key, e.g. `[Sutton&Barto2018]`,
in a theory's Notes; resolve the key to a full citation here. No entries yet
-- add one here the first time a theory actually draws on external research
(for example, if a future TD-learning or self-play change is grounded in
Sutton & Barto's *Reinforcement Learning: An Introduction*, that's when its
entry belongs here, not before).
