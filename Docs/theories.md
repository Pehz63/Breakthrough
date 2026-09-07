# Theory Log

> **[ELO HYGIENE UNVERIFIED - flagged 2026-07-25]** The Elo comparisons in this
> document predate the ranking-claim hygiene rules and may be mistaken. Two
> defects were found in this project's Elo reporting: numbers were read from
> `ranking/ratings.tsv`, which mixes RETIRED agents (`active = gone`, superseded
> `@N` identities frozen at old game counts) in with live ones; and agents were
> sometimes compared across different SEARCH HEADS, which are different agents
> whose Elos are not interchangeable. Any finding here that was accepted or
> refuted on an Elo comparison, including any comparison against the champion,
> may therefore be wrong and needs re-evaluation against `ranking/standings.tsv`
> (active only, grouped by head) within a single fit. Full explanation:
> `Docs/benchmarking.md`, "Elo comparison hygiene". Tracked in `todo.md`; remove
> this banner once this document's numbers have been re-verified.

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
| 14 | An offline refutation book could dethrone the champion with less live compute | Refuted in the naive (foreign-agent-mined) form, CONFIRMED in the self-mined form (theory 33). **Premise 1 (the target is not reproducibly deterministic) is measured as repaired, 2026-08-27** (238/238 subject-colours reproducible, against 11.4% of pairs pre-fix), unblocking the entry's other named repair: a book carrying a full winning continuation so the live brain never plays ([refutation-oracle-results-1](../plans/refutation-oracle-results-1-quiet-lodestone.md)) -- a 2026-07-18 book mined from the book-wearer's OWN wins dethroned the champion, 1145 vs 1074 | Gameplay Performance & Dethroning the Champion | [todo.md](../todo.md) | [dethrone-champion-results-3](../plans/dethrone-champion-results-3-wiggly-mitten.md), [dethrone-champion-results-5](../plans/dethrone-champion-results-5-wiggly-mitten.md) |
| 15 | Champdil recovers from an identical bad/random position better than the champion, independent of color | Promising / unproven (n=20) | Gameplay Performance & Dethroning the Champion | this session's conversation | [opener-bias-results-1](../plans/opener-bias-results-1-synchronous-stearns.md) |
| 16 | Per-heuristic incremental evaluation gives identical results at lower cpu/node, and generalizes | Confirmed | Search & Evaluation Engineering | [`3af970d`](https://github.com/Pehz63/Breakthrough/commit/3af970dca38c749d14f0b44d183b8c87f7b4f4a7) (chip count), [incremental-wall-column-eval-plan-1](../plans/incremental-wall-column-eval-plan-1-golden-forest.md) | [incremental-wall-column-eval-results-1](../plans/incremental-wall-column-eval-results-1-golden-forest.md), [incremental-ml-eval-results-1](../plans/incremental-ml-eval-results-1-luminous-snail.md) |
| 17 | Capturing a piece one ply from winning is always optimal, except when it is the last piece | Open / untested | Game-Theoretic Structure & Optimal Play | `todo.md`, this session's conversation | -- |
| 18 | Per-side capacity/distance difference is a meaningful predictor or evaluator signal | Partially resolved (analytic: redundant as a linear eval feature; predictor half open) | Game-Theoretic Structure & Optimal Play | `todo.md` | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) |
| 19 | Same-policy agents with different IDs score differently in gauntlets (identity artifact) | Confirmed. **Mechanism (b) is now separated and closed, 2026-08-27**: cross-game search state no longer makes deterministic agents diverge, measured directly at 238/238 subject-colours reproducible within and across processes by `rank.exe determinism` (210/210 for node-budgeted agents in all 4 passes), after `playOneGame` gained a per-game `ttClear()` on 2026-08-03. Mechanism (a), ID-derived per-game seeds against stochastic opponents, is untouched and remains the live half | Gameplay Performance & Dethroning the Champion | this session's sanity check | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) |
| 20 | Seeded random eval noise is a cheap tie-breaker / diversity knob | Split by form: PST refuted at both scales; bounded tie-only jitter works at ~0-80 Elo cost | Model & Evaluator Design | `todo.md` noise idea | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) (corrected), [bounded-jitter-results-1](../plans/bounded-jitter-results-1-buzzing-floyd.md) |
| 21 | Exact decided-race detection (D14) adds playing strength at fixed depth | Refuted at d6 (shallow depths untested) | Model & Evaluator Design | 2026-07-11 session (axioms D9/D14) | [heuristic-eval-overhaul-results-1](../plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md) |
| 22 | Deterministic first-found tie-breaking outperforms random tie-breaking | Weakened toward refuted (2 of 6 noise seeds beat baseline) | Model & Evaluator Design | bounded-jitter retest | [bounded-jitter-results-1](../plans/bounded-jitter-results-1-buzzing-floyd.md) |
| 23 | Deterministic tie-breaking creates a systematic directional (left-file) bias exploitable as a fingerprint | Confirmed (mechanism + empirical) | Game-Theoretic Structure & Optimal Play | developer question 2026-07-12 | [bounded-jitter-results-1](../plans/bounded-jitter-results-1-buzzing-floyd.md) |
| 24 | A residual/skip-connection chip-count term lets a learned value head spend its capacity on tie-breaking rather than re-deriving material counting | Refuted at 6 seeds (skip's calibration effect within seed noise at all capacities; the 2-seed "linear yes" was noise; capacity is the real lever) | Model & Evaluator Design | this session's conversation | [residual-mlp-results-2](../plans/residual-mlp-results-2-tingly-chipmunk.md) |
| 25 | Breakthrough has distinct game phases best served by separate phase-specialized models (mixture-of-experts) | Open / untested | Model & Evaluator Design | this session's conversation | -- |
| 26 | Low-Elo games are low-quality value-training data | Open / untested | Training Data & Recipes | developer hypothesis 2026-07-14 | -- |
| 27 | Lower value-model outcome-loss does not imply higher agent Elo (offline calibration and in-search strength diverge) | Promising / observed (MLP beat linear on loss ~0.17 yet lost ~95-130 Elo, at BOTH depth 4 and depth 6) | Model & Evaluator Design | residual/MLP Elo follow-up 2026-07-14 | [residual-mlp-results-2](../plans/residual-mlp-results-2-tingly-chipmunk.md) |
| 28 | Learned piece-square evaluators counter the chip counter head-to-head far above their pooled Elo | Confirmed as a pattern; tactical explanation refuted (qs probe), positional hypothesis remains | Gameplay Performance & Dethroning the Champion | [dethrone-champion-plan-1](../plans/dethrone-champion-plan-1-wiggly-mitten.md) | [dethrone-champion-results-1](../plans/dethrone-champion-results-1-wiggly-mitten.md), [dethrone-champion-results-2](../plans/dethrone-champion-results-2-wiggly-mitten.md) |
| 29 | Quiescence (captures-only stand-pat leaf extension) adds strength at the d6/nb200k head | Refuted for the learned-eval champion (pooled tie, loses h2h 9-23); weak positive inside noise for the chip counter (+19 at ~1 SE) | Model & Evaluator Design | [dethrone-champion-plan-1](../plans/dethrone-champion-plan-1-wiggly-mitten.md) phase 1 | [dethrone-champion-results-2](../plans/dethrone-champion-results-2-wiggly-mitten.md) |
| 30 | Weight mirror-symmetrization + seed-ensembling of a linear value model is a free variance cut that raises Elo | Refuted for playing strength: mirroring cost 135 Elo alone, the 6-seed mirror ensemble 144 (mechanism hypothesized: symmetric ties + directional tie-break) | Model & Evaluator Design | [dethrone-champion-plan-1](../plans/dethrone-champion-plan-1-wiggly-mitten.md) phase 3 | [dethrone-champion-results-4](../plans/dethrone-champion-results-4-wiggly-mitten.md) |
| 31 | Quiescence induces a "posturing" style (deferring an even trade until it lands exactly at the search horizon, since only pending-capture leaves get a deeper look) | Open / untested -- mechanism plausible from code, but a same-pair avg-plies/repetition check was inconclusive | Model & Evaluator Design | this session's conversation, 2026-07-17 | -- |
| 32 | This pool's shared left-file tie-break bias (theory 23) makes an asymmetric value model an adaptation, not noise -- mirror-symmetrizing discards real fitted signal about how THIS pool plays | Open / untested -- proposed 5-way design (unflipped / flipped / averaged / left-onto-both / right-onto-both) not yet built | Model & Evaluator Design | this session's conversation, 2026-07-17 | -- |
| 33 | Mining a book from the WEAK/book-wearing agent's OWN wins (not a stronger agent's wins over it) fixes the naive refutation book's brain-portability failure (theory 14) | Confirmed as a major result: dethroned the champion (1145 vs 1074, 25-7 head-to-head); open question whether "opponent must be stronger" is load-bearing or just "own win, any opponent" suffices | Gameplay Performance & Dethroning the Champion | developer hypothesis, 2026-07-18 | [dethrone-champion-results-5](../plans/dethrone-champion-results-5-wiggly-mitten.md) |
| 34 | Per-position Elo advantage measured by designed fresh-game playouts at controlled Elo gaps trains a distributional model that out-predicts the calibrated d8 oracle at position strength (beat it on held-out outcome NLL AND mu MAE) | Confirmed: all 4 configs beat the oracle (best MAE 146.2 vs 191.3, NLL 0.408 vs 0.450); theory 27 also reconfirmed (prediction-quality ranking diverges from playing-Elo ranking) | Model & Evaluator Design | developer idea 2026-07-18 | [position-oracle-results-1](../plans/position-oracle-results-1-lazy-popping-simon.md) |
| 35 | Position volatility (sigma) is identified by the flatness of the win-prob vs Elo-gap curve and the learned sigma head predicts conversion reliability on held-out positions | Weakly supported: sigma-vs-measured-sd correlation only 0.02-0.29 across configs; real but far weaker than theory 34's mu result | Model & Evaluator Design | developer framing 2026-07-18 (advantage is a range, not a point) | [position-oracle-results-1](../plans/position-oracle-results-1-lazy-popping-simon.md) |
| 36 | The dist MLP heads' first-hidden ReLU activations are sparse enough that a sparse leaf-tail forward (skip dead units) or a second-accumulated-layer delta yields a large additional speedup on top of the first-layer accumulator | Confirmed: static dead-ReLU ~90% and per-move activation churn ~10-12% across all 3 heads (predicted ceiling ~8-9x); the bit-identical sparse leaf-tail forward realized 7.1x on top of the first-layer accumulator (2.84 us/node, 12.7x vs full-scan for the wide head), meeting the predicted ceiling. Refuted the plan's prior assumption of dense (~50%-active) heads | Efficiency & Speed | this session's mlp-sparsity measurement, 2026-07-22 | [nnue-incremental-mlp-results-1](../plans/nnue-incremental-mlp-results-1-crystalline-taco.md) |
| 37 | An NNUE-shaped dist mu head (wide first layer, tiny rest, `129 -> 512 -> 8 -> 1`) is both cheaper per leaf and at least as strong as a balanced MLP, because the accumulated first layer is free and the tiny tail collapses | Refuted on efficiency, wash on strength: prediction is neutral (held-out MAE 143.5 / NLL 0.408, == the wide head) and Elo is in the MLP band (6 seeds 908-1037, mean ~973, top seeds reach dist_lin 1038), but at 2.47 us/node it is ~2x SLOWER than the standard 128,64 head (1.24). Once the accumulator frees the first-layer UPDATE, the leaf still READs + ReLUs all H first-hidden pre-activations (O(H) scalar), which the widest H maximizes; the ~17x-cheaper tail was never the bottleneck. The "gated on a vectorized read" escape was TESTED and refuted: an AVX2-intrinsic leaf read (vectorized double accumulator read + dense-FMA tail) gives a real ~1.2-1.3x speedup for EVERY shape (std 1.24 -> 1.00, NNUE 2.47 -> 1.89, wide 2.86 -> 2.43 us/node) but as a constant factor it does not change the ranking, so NNUE-wide stays ~1.9x slower than the narrow standard head. Real NNUE affords wide first layers only because its domain is strength-unsaturated (width buys accuracy); here it does not | Efficiency & Speed | this session's design discussion + speed A/B + AVX2 implementation, 2026-07-25 | [nnue-shaped-head-results-1](../plans/nnue-shaped-head-results-1-brisk-walrus.md) |
| 38 | An opening book's measured Elo lift is not transferable strength but an artifact of always starting from the same position: it holds only while the opponent reproduces the replies it made when the book was mined, so it should collapse once the opening is diversified | Confirmed on the mechanism, and the pooled fit adds three results the pairwise tests could not. The `book` opener is keyed on `positionKey(sideToMove)` alone with NO response tree, so it is opponent-blind at lookup but opponent-dependent in value. Pairwise: four cores given self-mined books gained +31pp (s98), +38pp (s111 dist), +12pp (s3) and 0pp (no-TT `adv`) at the fixed start, and under `pairgen --open-plies 8` every gain vanished (-41, -50, -33pp), leaving nulls. **Full-roster refit 2026-07-26** (13 book agents added, contenders boosted to 32 games/pair, 116-agent anchored fit): (a) a pairwise sweep can be ANTI-predictive -- `adv(chip=77,...)@1.opener(book,book=2)@1` went 32-0 against the champion pairwise yet rates 1033 +/- 9 against its own bare self's 1018 +/- 9, a 1.2 SE nothing, while that core's OWN book costs it **107 Elo** (911 vs 1018); (b) "books are core-specific" is REFUTED -- own beats borrowed for `classic` (+110 vs +70), ties for `s98` (+55 vs +52, 0.2 SE), and loses for `s3` (-33 vs +1) and `adv` (-107 vs +15); (c) book DEPTH, varied for the first time (6/16/30/60 ply), has no consistent direction: `classic` reads +54/+45/+71/+110 but `s98` reads +70/+82/+9/+55, with the s98 30-ply rung 73 Elo below its 16-ply neighbour at +/- 10 each and no mechanism tested. Entry count is unrelated to Elo (the 553-entry oracle book is the worst loadout on `classic`, the 24-entry `adv` self-book the worst anywhere, the best is 134 entries). A self-mined book is also a NO-OP by construction for a fully deterministic agent, since in-book it replays what its own brain would have chosen anyway. Theory 53 tests whether fuzzy nearest-cluster matching, rather than the exact hash this theory's book is keyed on, survives the exact collapse this entry describes | Gameplay Performance & Dethroning the Champion | developer question, 2026-07-26 | [book-opener-audit-results-1-vivid-lantern](../plans/book-opener-audit-results-1-vivid-lantern.md) |
| 39 | Nonlinear (hidden-layer) evaluators generalise WORSE across opening distributions than linear ones in this domain, so evaluator capacity that pays at a fixed start is a liability once the opening varies | **Untested hypothesis, filed not chased** (developer scoping decision 2026-07-26: the session was after general opener effects, not rigour on the architecture x opener intersection). Observed, not established. In the diversified-opening pool at 32 games/pair (`ranking/standings_open.tsv`, 6080 games) the dist family orders by capacity, worst-last: linear mu 1215, `129,128,64,1` 1088 (2 seeds), `129,512,8,1` 1034 (4 seeds), `129,256,128,1` 1025, all +/- 16. The plain chip counter is 1166 and the replay-trained LINEAR learned models are 1136-1178. So every hidden-layer dist model rates BELOW a material counter while both linear families rate at or above it. Win rate against that chip counter at the same head (fit-independent, n=32 each): linear dist 41%, `128,64` 31-34%, `256,128` 25%, `512,8` 19-31%, against replay-trained linear models at 59-69%. TWO CAVEATS THAT KEEP THIS UNTESTED: (a) the fixed-start baselines for the dist MLPs are 8-game fills, which this project's own hygiene rule 2 says never to conclude from, so the SWING column is unreliable even though the diversified column is not; (b) the mechanism is unprobed -- 'distribution-level overfit' is a guess, and notably theory 37 measured this architecture's held-out prediction as NEUTRAL (MAE 143.5 / NLL 0.40795), but on positions posgen sampled from standard-start games, i.e. the same distribution the hypothesis says it is overfit to. A same-model comparison of mu discrimination on standard vs diversified position pools would test it directly and was not run. Also note the hypothesis is NOT about the 8-unit bottleneck specifically: `129,256,128,1` has no bottleneck and rates 1025, statistically level with `129,512,8,1`'s 1034, so it is having hidden layers at all rather than their shape | Model & Evaluator Design | developer question 2026-07-26 ("is it maybe overtrained and overfitted, or too wide or thin in one of the layers?") | [diversified-opening-pool-results-1-vivid-lantern](../plans/diversified-opening-pool-results-1-vivid-lantern.md) |
| 40 | Single-seed conclusions about MLP dist models are worthless because their seed-to-seed spread is at the TOP of theory 8's band | Supported by direct observation, 2026-07-26. The four rated seeds of `129,512,8,1` span 986 to 1128 in one fit at +/- 16 each, a **142-Elo range**, against theory 8's stated 50-150 training-seed-noise band. Consequence for this session: `learned(model=111,78ef6974)` was singled out as the dist model that "does poorly" under diversified openings, but at 1128 it is the BEST of its four seeds and its architecture's mean is 1034. The premise of the question was an artifact of having rated only the luckiest seed | Model & Evaluator Design | fell out of the theory 39 ladder | [diversified-opening-pool-results-1-vivid-lantern](../plans/diversified-opening-pool-results-1-vivid-lantern.md) |
| 41 | A random opener's Elo cost on a fixed-depth search is highly nonlinear in ply count: a short window (4 plies) is roughly free while a longer one (8 plies) is sharply costly | Confirmed, 2026-07-28, same-fit two-point measurement (the champion-category-split session's merged roster). On the `classic` chip-counter control (`ab(deep=6,tt,ord,nodes=200k)@1`), `.opener(rand,moves=4)@1` cost 931->946 (+15, not significant, combined SE 14.4), but `.opener(rand,moves=8)@1` cost 931->759 (**-172, ~12 combined SE, highly significant**). The openless category's leading core (`learned(model=76,ef183148,dist,lin,...)`) showed the same shape: -60 at 4-ply, -240 at 8-ply -- a 4x jump for doubling the random window, not a linear one. Directionally consistent with the single existing `.opener(rand,moves=6)@1` measurement on record (~-217 Elo, `Docs/terminology.md`, "Opener (identity-level)"), which now sits between these two points on a real curve for the first time. Mechanism untested: whether the cliff sits at a specific ply threshold or just scales with distance from 4 was not probed, since only 4 and 8 were rostered | Gameplay Performance & Dethroning the Champion | champion-category-split session, 2026-07-28 | [champion-split-results-1-kind-beaming-cerf](../plans/champion-split-results-1-kind-beaming-cerf.md) |
| 42 | An ONLINE bootstrapped target (TD-Leaf) beats the offline supervised targets this project has used so far, at the same architecture and search head | **Supported at screening level, 2026-07-29.** In a pinned screening fit at 8 games/pair, the base recipe peaked at **1051 vs its initialisation's 791, +260 Elo**, at the same 130-parameter linear architecture and the same `ab(deep=6,tt,ord,nodes=200k)@1` head. The unfavourable prior (every offline self-play variant here lost to ranked-pool replay by ~250) did NOT hold for the online version. NOT certified: a pinned fit cannot dethrone, 8 games/pair is half the standard, and the peak needs an unpinned 32-games/pair refit | Model & Evaluator Design | `todo.md` line 555, promoted by developer instruction 2026-07-29 | [tdleaf-results-1](../plans/tdleaf-results-1-amber-pangolin.md) |
| 43 | The eligibility decay lambda matters, and lambda=1 (provably == outcome-supervised on PV leaves) isolates how much of any gain is the bootstrap rather than the leaf-targeting | **Confirmed, and the strongest result of the study, 2026-07-29.** lambda=1 scored **713 at 500 games -- BELOW the 791 initialisation, i.e. worse than not training** -- and 254-287 Elo behind lambda=0.7. So the +260 gain is specifically the temporal-difference BOOTSTRAP, not "train on PV leaves instead of played positions". Separately, lambda=0 (one-step TD) is close to lambda=0.7 (-56 at g500, +28 at g2000), so trace LENGTH matters far less than having a bootstrap at all. n=2 seeds per cell, but the lambda=1 gap is an order of magnitude outside any noise band | Model & Evaluator Design | falls out of the closed forms derived while implementing the gradient core, 2026-07-29 | [tdleaf-results-1](../plans/tdleaf-results-1-amber-pangolin.md) |
| 44 | For a BOOTSTRAPPED target, generator search depth matters, unlike the supervised case where this project measured teacher depth as irrelevant | **Not supported, and NOT settled (n=1).** A d4 generator scored -13 at g500 and +2 at g2000 against the d6/nb200k base -- indistinguishable, consistent with the existing supervised-era finding rather than with this theory. One seed per rung cannot separate "depth is irrelevant" from "this seed was lucky", so this is suggestive only and the theory stays open pending a 4+ seed re-run. Note the plan chose d6 anyway on distribution-match grounds, so nothing in the study depended on this being true | Model & Evaluator Design | raised while auditing an unjustified d4 default, 2026-07-29 | [tdleaf-results-1](../plans/tdleaf-results-1-amber-pangolin.md) |
| 45 | A convergence-stopped ladder whose stop threshold is smaller than its own seed noise will stop at the first rung and report a false ceiling | **Confirmed twice, and the SECOND confirmation is the more useful one.** First recorded 2026-07-24 in `training-sweep-results-1` item 3, which correctly called the stop "triggered on noise, not on convergence" and warned "do not trust 'self-play plateaus at 500'". Re-confirmed by re-analysis 2026-07-29 (no new games): `train_scaling.ps1`'s self-play arm is 4 rows (250 -> 536/442, mean 489; 500 -> 541/469, mean 505), stopped on a +16 gain under a 20-Elo rule against within-size seed spreads of 94 and 72 Elo, with no size above 500 ever run. **The failure the second confirmation actually exposes is documentary, not statistical:** the correct caveat existed in a results doc and was still contradicted while planning the TD-Leaf study, because nothing at the point of citation (`tools/CLAUDE.md`'s script row, the script header, `todo.md`'s summary) carried it. Fix applied: hindsight-tag the claim wherever it can be quoted, not only where it was analysed. Consequence for method: prefer a fixed ladder rated end to end (`train.exe --ckpt-at`) over an early-stop rule whose threshold is not known to exceed the seed band. Separately, none of this transfers to ONLINE regimes, which is why the TD-Leaf study measures game count rather than assuming it | Benchmarking & Measurement | `training-sweep-results-1` item 3 (2026-07-24); re-surfaced when the developer challenged a citation of it, 2026-07-29 | [training-sweep-results-1](../plans/training-sweep-results-1-luminous-snail.md), [tdleaf-plan-1](../plans/tdleaf-plan-1-amber-pangolin.md) |
| 46 | Online self-play value learning has an INTERIOR optimum in game count -- past some point more self-play games make the agent worse, not merely no better | **Supported, 2026-07-29.** The base recipe rose 869 -> 944 -> 1000 -> **1051 (1000 games)** then FELL to 1016 at 2000. Paired per seed the 1000->2000 step is -23/-33/-61/-26, mean -35.8, all four seeds the same sign. Mechanism untested (overfitting to the self-play distribution? needs learning-rate decay?), and the ladder's 500/1000/2000 spacing locates the optimum only within a factor of 2. Directly motivates rating a checkpoint ladder rather than assuming a count -- the withdrawn "500 games" figure would have stopped 51 Elo short of the peak | Model & Evaluator Design | emerged from the checkpoint ladder, 2026-07-29 | [tdleaf-results-1](../plans/tdleaf-results-1-amber-pangolin.md) |
| 47 | Blending a dist model's sigma (volatility) head into search-time leaf scoring via mu + k*sigma, instead of mu alone, improves playing strength (the "optimistic"/"cautious" agent idea) | **Refuted for the s76 linear core, cleanly.** Pinned screening fit, 8 games/pair, 720 games/agent: EVERY nonzero k tested (both signs, |k| in {0.1,0.2,0.5,1.0}) rated below the k=0 baseline (1007), monotonically worse with |k|: k=-0.1 941, k=-0.2 928, k=-0.5 871, k=-1.0 789 (SE 14-15) and k=0.1 910, k=0.2 865, k=0.5 818, k=1.0 779 (SE 14-15) -- a 66-228 Elo cost, with even the smallest step (|k|=0.1) already costing 66-97 Elo. Negative k (cautious, mu-k*sigma) beat positive k (optimistic) at every matched magnitude, though the gap narrows toward |k|=1.0. Single core, single seed (no training involved, so seed-noise doesn't apply the usual way, but this is one architecture only) | Model & Evaluator Design | `todo.md`'s mu/sigma-agent-types entry, developer request 2026-08-06 | [risk-weight-results-1-dusty-kestrel](../plans/risk-weight-results-1-dusty-kestrel.md) |
| 48 | A bootstrapped self-play trainer's simulation count per move has an INTERIOR optimum under a FIXED TOTAL TRAINING COMPUTE budget, generalizing theory 46's game-count optimum to a second, tradeable axis | **Supported, 2026-08-17.** One fixed Gumbel-Zero config (lr=0.01, l2=0.0, replay-capacity=2000, replay-warmup=32, batch=32, open-plies=4, seed=6001), one compute budget held equal across 5 arms (~60,700 ms-equivalent, game count derived from each arm's own measured ms/game so sims and game count trade off at constant total compute): Elo rose 585 (sims=50, 5400 games) -> 646 (sims=100, 3250) -> 673 (sims=200, 1500) -> peaked at **683 (sims=400, 770 games)** then fell to 617 (sims=800, 400 games), all +/- 21-22 SE via `rank.exe gauntlet --games 2`. The 400->800 fall is outside the 400 point's error band. Separately rules out sims=25 (485 +/- 24 at its own native rung) as below the productive range. A same-game-count (rung-matched) comparison across sims values is NOT this result and is confounded, since higher sims costs measurably more compute per game, so it conflates "more search per move helped" with "this arm got more total compute" | Model & Evaluator Design | developer question 2026-08-17 ("Can you train on fewer sims and run it at higher sims? Does it perform just as well?") | [gumbel-mcts-results-3-amber-thicket](../plans/gumbel-mcts-results-3-amber-thicket.md) |
| 49 | Gumbel MCTS's search-shape constants (root Gumbel-top-k breadth `m`, `c_visit`, `c_scale`) are real, non-confounded search-strength levers independent of `sims`, and the paper's defaults are not the strength optimum for a weak, undertrained checkpoint | **Supported, 2026-08-17, screening level (one fixed checkpoint, no retraining).** Four-round investigation, self-contained round robins (own scratch store, `rand@1` anchor, never the canonical ladder), all at `sims=200` on Pass 2 Round B's R17 checkpoint (slot746): (1) an 8-draw random search over all 3 axes found `m<=2` costs 300+ Elo regardless of `cvisit`/`cscale` (626-677 vs 850-1057 for `m>=4`), not confounded since a wide `cvisit`/`cscale` range was tried at `m=2` and ALL scored low; (2) holding `m=16` fixed, a controlled 3x3 `cvisit`x`cscale` grid found both axes independently rising within `cvisit` in [10,200] and `cscale` in [0.5,3.0] (paper defaults 50/1.0 underperform the top corner 200/3.0 by ~96 Elo, 800 vs 896); (3) pushing further along that diagonal (still `m=16` fixed) located an **interior peak near `cvisit=500, cscale=5.0` (903 Elo)**, roughly flat to slightly down at `cvisit=1000-2000` (847-851); (4) re-isolating `m` at that new corner confirmed `m=16` (892) and `m=8` (881) are statistically tied as best, `m=32` no better (822), `m=4` measurably worse (800) even here. 2,960 games total across the 4 rounds, all near-instant (linear-model checkpoint). Not yet checked against a different checkpoint or `sims` value, so whether this optimum is checkpoint-specific or general is untested. **Round 5 follow-up, same day:** a single shared fit (one round robin, not stitched across rounds) head-to-head of the leading candidates against REF found the top of the landscape is a broad plateau, not one point -- `cvisit=1000,cscale=10.0` (893), `cvisit=500,cscale=5.0` at `m=16` (879) and `m=8` (875) all sit within one error band of each other (+/-31 each); `cvisit=200,cscale=3.0` (806) beat REF (793) by only +13 in this fit, inside the error bars, weaker than the +26-96 Round 2/3 showed for the same point in their own separate fits -- read as fit-to-fit noise, not a contradiction, since absolute Elo does not carry across fits. Practical recommendation: `gaz(sims=200,cvisit=500,cscale=50)@1` (`m=16`, the default), tied for best and requires no `m=` flag | Model & Evaluator Design | developer instruction 2026-08-17 ("update the gumbel agents to a new version that supports more configurations. Then try to identify best values for these in the same way you explored values for the training") | [gumbel-mcts-results-4-copper-lantern](../plans/gumbel-mcts-results-4-copper-lantern.md) |
| 50 | Theory 46's interior optimum in self-play game count (online value learning) generalizes ACROSS hyperparameter configurations for Gumbel-Zero, not just for one fixed recipe: the majority of configurations peak and decline within a shared checkpoint ladder, not just occasionally | **Supported, 2026-08-18, at much larger scale than theory 46's original 4-seed measurement.** 101 random hyperparameter draws (joint training x search-shape sweep), each checkpointed at the same 4 rungs (100/400/1500/4000 games): only 8/101 arms (7 rising, 1 falling) are monotonic across the whole ladder, **93/101 (92%) are non-monotonic**, and among those the peak rung is roughly evenly spread across the ladder (11/30/29/31 at rungs 100/400/1500/4000 respectively) rather than concentrated late. The single best checkpoint found in the whole sweep (Elo 816 +/- 19, gauntlet-screened against `ranking/roster_screening_pool.txt`) peaks at rung 1500 and falls to 604 by rung 4000 -- a result a final-rung-only screen would have missed entirely, reporting 604 (rank ~30th of 404) as its ceiling instead of 816 (1st). Mechanism untested (same open question as theory 46): replay buffer aging, distributional overfitting on a still-small self-play sample, or something else. 1 seed per configuration, so no single arm's exact peak location is a reliable measurement on its own -- the 92%-non-monotonic RATE across 101 independent draws is the supported claim, not any one arm's specific curve | Model & Evaluator Design | fell out of screening 404 checkpoints across 101 configurations, 2026-08-18 | [gumbel-mcts-results-5-violet-harbor](../plans/gumbel-mcts-results-5-violet-harbor.md) |
| 51 | Within the Gumbel-Zero self-play + MCTS training regime, MLP capacity substantially improves playing strength over both linear and conv, in contrast to theory 27's finding (higher-capacity MLP LOST Elo despite lower loss) under the `tdleaf_self` supervised-regression + alpha-beta regime | **Supported for mlp-vs-conv at the population level, suggestive for mlp-vs-linear; the single-seed HEADLINE NUMBER does not hold up under replication, 2026-08-22.** 92 draws (45 mlp + 45 conv + 2 REF), 4 shared rungs, pinned screening fit against the same 27-agent pool: mlp's best-of-46 checkpoint (hidden=32, rung=4000, block M34) reached 1023 +/- 20 Elo (rank #2 of 395 pool agents, beaten only by the pool's single best `ab(deep=6)` linear agent at 1039); conv's best-of-46 reached only 806. Same-day seed-replication of the top 5 mlp blocks (2 additional seeds each, search-shape held fixed) found the ORIGINAL seed was the highest of its own 3-seed group in **5 of 5** blocks -- the textbook regression-to-the-mean signature of a value selected for being the best of 45 draws. M34's own two replicas landed at 913 and 979 (3-seed mean 971.7), so 1023 specifically is a favorable single-seed draw, not the recipe's expected strength. **What DID survive replication**: all 15 new seed x block data points (rung=4000) are >= 775, all but one clear conv's best-of-46 (806), and all 15 clear-or-approach Round 5's best pure-linear checkpoint (816) -- the mlp-vs-conv and mlp-vs-linear POPULATION-LEVEL gaps (46 independent draws per architecture, not one seed pair) hold up; only the single extreme number does not. l2=0 dominates the top of the table again (18/20 of top-20 checkpoints), replicating theory 50's underlying study's l2 pattern. Mechanism for the regime-dependence (why capacity helps in self-play+MCTS but hurt in supervised-regression+alpha-beta) is untested | Model & Evaluator Design | follow-up round to theory 50's study, once `train.exe gumbelzero` gained `--model-type`/`--mlp-hidden`/`--conv-channels` support, developer instruction 2026-08-19 ("Run what's currently supported. Randomly sample for mlp and convolution."), seed-replication follow-up same-day per developer instruction ("And a few other top ones") | [gumbel-mcts-arch-results-6-silver-thistle](../plans/gumbel-mcts-arch-results-6-silver-thistle.md) |
| 52 | Theory 51's mlp-vs-conv Elo gap is substantially explained by a capacity asymmetry the round-6 sweep never varied (mlp's policy AND value heads both got nonlinear hidden layers, conv's value head had a direct linear readout and its policy head was unconditionally linear), not by convolution being an inherently weaker architecture for this board representation | **Supported, 2026-08-23.** Giving conv's value head a real FC head (`--mlp-hidden 32`) and its policy head an MLPModel (new `--policy-mlp` flag, decoupling policy-head architecture from value-head architecture) on top of round 6's single best conv recipe (block C15, 806 Elo at rung=4000) raised conv's 3-seed mean at rung=4000 from 748.7 (baseline, exactly replicating C15's recipe) to **953.0** (+204.3), inside the seed-noise band of theory 51's own seed-corrected mlp top-4 means (937.0-971.7). The two fixes are super-additive: FC head alone gave +72.0, policy-MLP alone gave +65.3, both together gave +204.3, well above the +137.3 additive sum. Speed is essentially unaffected (73-79 ms/move across all 4 conditions at rung=4000), so this is close to a pure capacity win, not a speed/strength tradeoff, though conv remains roughly 10x slower per move than mlp regardless (the conv trunk itself dominates its cost, not the small FC/policy additions), so mlp likely keeps a real efficiency edge even with the strength gap closed. Caveats: one recipe tested, not a re-sweep of conv's other hyperparameters, and 3 seeds per condition (reliable per-cell, but the FC/policy interaction's exact shape is not fully distinguishable from noise at this sample size) | Model & Evaluator Design | developer questions 2026-08-23 ("I would expect it to be strongest. What was tried? What could be improved? ... could it maybe have been weak because it hadn't enough time to properly learn?") | [gumbel-mcts-arch-results-6-silver-thistle](../plans/gumbel-mcts-arch-results-6-silver-thistle.md) |
| 53 | Matching an opening position to its nearest MINED CLUSTER (SMARTSTART, Steinmetz & Gini, IJCAI 2015), rather than requiring an exact position-hash match, lets an opening book keep giving guidance well past the point where an exact-hash book goes silent under theory 38's collapse | **Supported at the mechanism level, Pass 1a plumbing sanity, 2026-08-26.** One core (`ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2`, the same core behind `book13`/`book14`), one cluster book mined from its own 1520 kept-replay wins (`models/cbook1.txt`, 4 clusters/bucket, `keep=6`, mirror=canon). Against an opponent forced random for its own first 8 moves (`.opener(rand,moves=8)@1`, 60 games, both colours), the exact-hash `book14`'s hit rate fell from 100% at half-move 0 (trivially, the shared start position) to 13% at half-move 1 and **0% from half-move 3 onward**, while `cbook1`'s fuzzy match kept firing (non-empty legality intersection) at 53-100% through half-move 15, with a mean intersection size of 1-6 moves against 22+ legal moves -- a real narrowing, not a degenerate always-everything match. This tests the mechanism (does a match keep firing), not playing strength: the intersection contains SOME legal moves in the matched cluster's history, which says nothing yet about whether those moves are good ones, that is Pass 2/3's job. An offline k-means seed-stability check on the same dump found moderate seed sensitivity (pairwise co-clustering Rand index 0.78-0.85 across half-moves 8/16/24), which is why the study design keeps >= 3 seeds per configuration rather than treating a single k-means fit as settled | Gameplay Performance & Dethroning the Champion | developer request to apply the SMARTSTART paper to this project, 2026-08 | [cluster-book-results-1-noble-swimming-scott](../plans/cluster-book-results-1-noble-swimming-scott.md) |
| 54 | The transposition table hands one agent another agent's search results, because it is ONE process-wide table keyed by the position hash alone, cleared once per GAME rather than per move, and both players of a game search through it | **Confirmed and FIXED, 2026-08-27.** Two distinct wrong reads, both reachable in ordinary ranked play: a cross-evaluator read (White stores a score its evaluator produced, Black probes the same position, gets a hit, and returns White's number as its own) and a cross-strength read (`ttProbe` accepts any entry whose stored depth is at least the prober's remaining depth, so a shallower agent reads a deeper agent's entries). Found while mining refutation lines, where a book plays back without searching and so exposes any dependence on the miner having searched: **132 of 238 mined lines stopped reproducing, all 132 against a `,tt,` opponent, 0 of 77 against non-`tt` opponents**. Two-setting control, same 8 targets and an oracle identical but for the flag: tt-free left the book 0 of 8, `tt` left it 8 of 8, and BOTH still reported a winning record because the wearer's brain covered for the silent book. Fixed by mixing a searcher context (evaluator index, full evalParams array, `g_useQuiescence`, and the ROOT side to move) into the TT key, which gives each player a disjoint region of the one table. Regression test in `tests/test_ai_integration.cpp`, validated to FAIL with the fix reverted (Black's node count 157 clean vs 135 contaminated, 30 key matches, 3 cutoffs). **169 of 217 active roster agents carry `tt`, so every stored game between two of them predates the fix and is not reproducible by the current binary. The Elo consequence is NOT measured.** | Gameplay Performance & Dethroning the Champion | fell out of building `rank.exe refute`, 2026-08-27 | [refutation-oracle-results-2-quiet-lodestone](../plans/refutation-oracle-results-2-quiet-lodestone.md) |
| 55 | The mined lines that leave the refutation book do so because stage 2 overwrote a position an earlier winning line depended on (the ownership hole: the check gated which position a repair may BRANCH at but committed the winning line's whole path unconditionally) | **Refuted, 2026-08-29.** The hole is real and was fixed, but it does not explain out-of-book lines. A clean A/B with the 14 wall-clock-budgeted agents excluded (210 targets, same frozen snapshot and oracle), fixed binary vs a pre-fix binary built from `src/ranking.cpp` at `3d945c2`, produced byte-identical books and identical reports on all 210 rows: 202/210 audited with **12 lines out of book and 0 collisions**, and the check never fired in either run. An earlier comparison (book21 vs book22) appeared to show the fix making things worse, and was void: stage 1, which is identical code in both binaries, had already diverged (3532 book entries vs 3530), so the runs were not comparable. Of the 12, seven are conceded lines with no coverage to lose, and the other five are theory 56 | Gameplay Performance & Dethroning the Champion | stage 3's collision counter firing on 1 position of 3657, 2026-08-29 | [refutation-oracle-results-2-quiet-lodestone](../plans/refutation-oracle-results-2-quiet-lodestone.md) |
| 56 | WITHDRAWN, not a theory. This entry held a root-cause GUESS for the non-reproducing-opponent defect (that state our side's search writes is read by the opponent's search). The number is kept so citations of "theory 56" resolve | Withdrawn 2026-08-31. A cause hypothesis for a bug does not belong in this log. It is not a claim about how the domain works, it has a lifetime of one debugging session, and the defect it would explain is only INDICATED, so if the direct test kills the deduction this entry does not get refuted, it evaporates. Being listed here also invited testing the MECHANISM when the right next action is confirming the DEFECT. Theory 54 is not a precedent for it: 54 earned its entry by being confirmed and by having durable consequences other docs cite. The defect, the deduction behind it, the mechanism guess and the confirmation test all live in `todo.md` | Gameplay Performance & Dethroning the Champion | withdrawn from the theory log, 2026-08-31 | [refutation-oracle-results-2-quiet-lodestone](../plans/refutation-oracle-results-2-quiet-lodestone.md) |
| 57 | The refutation miner's run-to-run irreproducibility comes from the 14 wall-clock-budgeted (`time=`) agents alone | Supported, not isolated. With them excluded, two separate processes running two different binaries produce byte-identical books and identical 210-row reports. With them included, two runs of the same workload differ by two book entries at the end of stage 1, and stage 2's path dependence amplifies that into nine extra concessions. An independent replay of book21 moved exactly one row of 238, a `time=150ms` core, which also shifted its own out-of-book ply from 13 to 16. Consistent with wall-clock budgets alone, but not separated from any other property those 14 agents share. **Mechanism identified 2026-09-01**: the property is the budget itself, not something else those agents share. `budgetTripped()` sampled the clock once per 4096 nodes and kept no memory of the answer, and the iterative-deepening loop never checked before starting a new depth, so `time=` bounded nothing. Realized cost ran 3.9x to 7.1x the flag (193.78 / 900.30 / 3217.37 ms/move at 50 / 150 / 450 ms), which means how far a `time=` agent searched was governed by machine load rather than by its own ID. Fixed the same day. See `TIME BUDGET NOT ENFORCED`, `Docs/corrections.md`, and theory 59 for whether enforcement is enough to make these agents reproducible | Efficiency & Speed | book21 vs book22 failing to reproduce, 2026-08-29 | [refutation-oracle-results-2-quiet-lodestone](../plans/refutation-oracle-results-2-quiet-lodestone.md) |
| 58 | A shared transposition table degrades play through its ORDERING channel alone, separately from its value channel, and the damage concentrates on evaluators with large tie sets (chip counters) because ordering decides which of several equal-scoring moves is played | Open, untested, and the instrument does not exist yet. Developer proposal, 2026-08-31: rather than only fixing the shared table (theory 54), deliberately re-share it and study what the sharing does. Two code facts sharpen it. (1) `src/transposition.cpp`'s `ttProbe` fills `fromSq`/`toSq` BEFORE the depth test and returns false after it, commented "usable for ordering regardless of depth", so the ordering channel fires on entries too shallow to give a cutoff and therefore had strictly MORE contact surface than the value channel under the pre-fix table. The `s_ttCtx` fix closed both at once, so the ordering channel has never been measured alone. (2) `src/ai_minimax.cpp`'s `if (eval > best)` is strictly greater, so among tied moves alpha-beta keeps the FIRST found, which is exactly what ordering controls. Full-width alpha-beta returns the same VALUE under any ordering, so tie-breaking is the mechanism by which ordering can change the MOVE at fixed depth. A second and separate mechanism applies under a node budget, where worse ordering completes fewer iterative-deepening plies. Isolate them by running unbudgeted at fixed depth (tie-breaking only) and budgeted (combined). Framing correction: natural sharing is not adversarial, since both agents at a node with side-to-move S compute the best move FOR S, so a foreign entry is optimal for a different evaluator and depth rather than opposed. The adversarial case needs deliberate poisoning (store the WORST move as the hint), which also bounds the effect: if a maximally poisoned ordering channel costs little, the theory dies cheaply | Gameplay Performance & Dethroning the Champion | developer proposal, 2026-08-31, arising from theory 54's fix | not yet tested |
| 59 | Enforcing the wall-clock budget does NOT make a `time=` agent reproducible, so `rankAgentIsDeterministic` misclassifies every one of them. It derives determinism from whether an agent draws `rand()`, and a `time=` agent draws none, so `pairGameTarget` pins any two of them at exactly 2 games as both floor and ceiling, on the reasoning that further games would only store replays. But the stopping point of a wall-clock search is set by machine load, not by the ID, so two `time=` agents playing twice are 2 SAMPLES of a noisy process rather than 2 replays of one game. If so, the time track's error bars are understated for the opposite reason to `Docs/benchmarking.md`'s defect 3: not too few distinct games per row, but too few rows scheduled for a pair that is not actually deterministic | **Confirmed 2026-09-01, with the size of the effect depending on whether the budget actually binds.** `rank.exe determinism --replicas 3 --only "time=150ms"` on the FIXED binary exits 2: 31 of 34 subject-colours reproduce, 3 do not. Which 3 is the informative part. Both colours of `ab(deep=12,tt,ord,time=150ms)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` diverge (at half-moves 24 and 13), as does White of `ab(deep=6,tt,ord,time=150ms)@2.learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` (half-move 34). Every reproducing agent is a `deep=6` core fast enough to finish depth 6 inside 150 ms, where the depth cap stops the search and the clock is never consulted at the decision point. So non-reproducibility appears exactly where the budget BINDS, and **P1's whole purpose is to make the budget bind on every core**: raising the cap to `deep=12` turns the time track from mostly-deterministic into genuinely stochastic. **Classifier fixed 2026-09-02**: `rankAgentIsDeterministic` now checks `timeBudgetMs > 0` first and returns non-deterministic for any live wall-clock agent, closing the gap this theory identified (`src/ranking.cpp`, `src/CLAUDE.md`'s Determinism paragraph). Consequence still open: fixing the classifier raises the time track's game targets, so `pairGameTarget` will schedule more than 2 games for `deep=12,time=` pairs going forward, but the 3 time-track category certifications in `ranking/CHAMPION.md` were decided under the OLD 2-game pinning and have not been re-run against the new targets (tracked separately in `todo.md`, "Re-certify the 3 `time=150ms` categories") | Efficiency & Speed | noticed while fixing theory 57's mechanism, 2026-09-01 | [budget-parity-results-1-steady-meridian](../plans/budget-parity-results-1-steady-meridian.md) |
| 60 | A node budget and a wall-clock budget are not two spellings of one normalization: a node budget equalizes SEARCH DEPTH across cores and charges an expensive evaluator nothing for being expensive, while a wall-clock budget equalizes TIME and makes the expensive evaluator pay for itself in plies. If so, the two compute tracks are asking two genuinely different questions ("whose evaluator is better at equal search" versus "who plays best per second") and neither is redundant | **Confirmed 2026-09-02.** All 15 time-track cores run at `nodes=200k` over 240 games land within **0.84 plies** of each other in MEAN COMPLETED depth (5.95 to 6.79) while their cpu cost spans **18.4x** (28.7 to 527.4 ms/move). The same 15 cores calibrated to 250 ms/move land at 1.19x in wall clock but split into two depth groups: 7.23-7.62 plies for the 13 classic and linear cores, 5.36-5.39 for the two MLP cores. Equalizing one quantity is exactly what makes the other unequal. Practical consequence: `nodes=200k` is not arbitrary in effect even though the number was chosen that way, since it pins the roster at ~6.1 plies, and raising it toward `nodes=1450k` would put the node track at the same depth AND the same wall clock as the time track, making the two nearly redundant for 13 of the 15 cores. CORRECTED 2026-09-02: this row first reported a 0.43-ply spread, which was the spread of MEAN EFFECTIVE depth. That statistic is contaminated by roughly 9% short-circuit plies (median 234 nodes against a 200,000-node cap) and its fractional part is a DISCARDED iteration, so it is not what the agent plays. The corrected 0.84 plies is the spread of mean COMPLETED depth over budget-limited plies. The direction of the theory is unchanged, the tightness was overstated about twofold | Efficiency & Speed | [budget-parity-plan-1](../plans/budget-parity-plan-1-steady-meridian.md) | [budget-parity-results-1](../plans/budget-parity-results-1-steady-meridian.md) |
| 61 | Cost per node is not constant across search depth within one core: odd depths measure cheaper per node than the even depths on either side. If the mechanism is the ratio of evaluated leaves to interior nodes changing with parity, then cost per LEAF is the stable quantity and a `leaves=` budget would calibrate to a wall clock in one pass where a node budget needs a rescale round | **Observed on all 15 cores, mechanism NOT established.** `classic(chip=100)@2` measures 0.1408 (d5), 0.2207 (d6), 0.1432 (d7), 0.2237 (d8), 0.1481 (d9) us/node. `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` measures 0.2145 (d4), 0.1841 (d5), 0.2037 (d6), 0.1655 (d7), 0.2177 (d8). Not an opponent-mix artifact: splitting `model=10`'s ladder by opponent gives d6 0.2039-0.2457, d7 0.1642-0.1880, d8 0.2310-0.2475, so the depth term dominates the opponent term. The effect is load-bearing rather than a curiosity: node budgets derived from d8 (even) us/node undershot a 250 ms target by 19% on average, which a rescale pass then corrected to 1.19x spread. The test that would settle it is recording `g_lastLeafs` alongside `g_lastNodes` in the match store and recomputing cost per LEAF across the same ladder | Efficiency & Speed | noticed while calibrating per-core node budgets, 2026-09-02 | [budget-parity-results-1](../plans/budget-parity-results-1-steady-meridian.md) |
| 62 | How much a change of compute-budget RULE changes the move played is set by the evaluator's score resolution, not by the rule. A coarse evaluator whose quiet positions tie exactly will pick the same move under any budget because move-generation order breaks the tie, while a fine-grained learned evaluator flips its argmax whenever the search depth shifts | **Supported on 3 cores, the resolution mechanism itself untested.** `rank.exe agree` polls two agents from identical positions with the transposition table wiped before both searches. At matched ~250 ms/move, fixed depth versus a calibrated node budget agrees 96.9% [95.2-98.0, n=581] on `classic(chip=100)@2`, 70.9% [67.7-74.0, n=805] on `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` and 61.7% [58.1-65.1, n=749] on `learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1`. Splitting by whether the polled agent reached the SAME effective depth: when it did, every core agrees 88-100%, so the disagreement tracks depth reached and not the rule. `classic(chip=100)@2` resolves to chip 100 with wall and column both 0, so `evalPosFull` returns 0 and its score for any non-terminal position is exactly `g_chipDiff * 100`, a pure material count. That the evaluator is coarse is read off `g_evaluators[0]`, but that coarseness is WHY it is depth-insensitive remains a hypothesis. The test that would separate resolution from hand-written-versus-learned is running `agree` on `classic` with non-zero `wall` and `column` weights, or on `advanced(...)` | Model & Evaluator Design | [budget-parity-plan-1](../plans/budget-parity-plan-1-steady-meridian.md) | [budget-parity-results-1](../plans/budget-parity-results-1-steady-meridian.md) |
| 63 | A budgeted iterative-deepening search without `part` throws away the iteration the budget cut, so effective depth is the wrong statistic for what an agent plays and a large fraction of any node or time budget buys nothing. If so, an agent reporting effective depth 6.3 is EXACTLY a depth-6 agent, the turn weight cannot go live through mixed leaf parity no matter how the budget lands, and the budget is paying a multiple of the cost of the depth it actually plays | **Confirmed 2026-09-02, and the waste is larger than expected.** `ai_minimax.cpp` adopts a cut iteration only under `g_keepPartial`, and no roster agent carries `part`. Measured with `rank.exe agree`: on `ab(deep=12,tt,ord,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`, every one of the **233 of 233** budget-limited plies that completed depth 6 played the identical move to `ab(deep=6,tt,ord)@2` on the same core, despite having searched part of a depth-7 iteration first. Control against the iterative-deepening-versus-direct-search confound: `ab(deep=6,tt,ord,nodes=999999k)@2` (budgeted branch, cap never binds, no partial iteration) agrees with a direct `ab(deep=6,tt,ord)@2` on **99.6%** (782/785), so the 100% is not propped up by a compensating error. Cost side: charging each ply the core's measured nodes/move at its completed fixed depth, a mean of **54% of the node budget goes into the discarded iteration** (range 30% to 68% across 15 cores at `nodes=200k`), and raising the budget to 250k leaves the mean at 54% because the waste is structural rather than a tuning error. A `nodes=200k` agent is a depth-6 agent paying about 2.2x the price of a depth-6 search. Turn-weight corollary confirmed twice over: without `part` the mixed-parity leaves never reach the root (the 233/233 above), and even WITH `part`, `classic(turn=0,chip=100)@2` versus `classic(turn=1,chip=100)@2` at `nodes=200k,part` agreed on **100%** of 426 polls, since a turn weight of 1 against a chip weight of 100 cannot flip the `a > alphaPrev` adoption test. NOT measured: whether `part` makes an agent stronger. It changes the move 25% of the time (75.0% agreement, 708 polls) but does not move it toward the next ply, agreeing with `deep=7` on 52.6% against the discarding agent's 56.4%, intervals overlapping  SCOPE CORRECTION 2026-09-03 (see `BUDGET HEAD NOT ROSTERED`, `Docs/corrections.md`): every number in this row was measured on `ab(deep=12,tt,ord,nodes=200k)@2`, which no roster agent uses. On the roster head `ab(deep=6,tt,ord,nodes=200k)@2` the node cap binds on a mean of only **3.8%** of plies (range 0.0% to 14.8% across 15 cores, 12 games each), so the node track is in practice a fixed-depth-6 track and the 54% waste figure does not describe it. The mechanism is unaffected and holds wherever the budget binds, which at `deep=7` is 37% to 79% of plies | Efficiency & Speed | noticed while answering whether effective depth 6.3 is "about a 7", 2026-09-02 | [budget-parity-results-1](../plans/budget-parity-results-1-steady-meridian.md), [budget-parity-results-2](../plans/budget-parity-results-2-steady-meridian.md) |
| 64 | `part` (`g_keepPartial`) does not adopt a depth-d judgement. `searchRootWhite`/`Black` keep walking the remaining root moves after the budget trips, and `budgetTripped` short-circuits each one to a static `evalLeaf` one ply in with no reply searched, so those scores are biased high for the side to move and can win the `a > alphaPrev` adoption test on merit they do not have. If so, `part` is not a cheap approximation of the next ply but a distinct and partly arbitrary player | **Confirmed 2026-09-03.** New telemetry `g_lastPartAdopt` compares the root-scan index of the move that set alpha against `rootDeep`. On `ab(deep=12,tt,ord,part,nodes=200k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`, 279 budget-limited plies: 62.0% no adoption, 19.4% adopted a genuinely searched root move, **18.6% adopted a post-cut static one**, so half of all adoptions (52 of 106) rest on a score no search produced. Corollary confirmed separately: the root is NOT ordered (`orderMoves` runs only at interior nodes), so a partial iteration covers an arbitrary geometric slice of the root moves. Behaviour side: turning `part` on moves the agent away from `deep=6` (92.2% -> 70.1% agreement) without moving it toward `deep=7` (56.4% -> 52.6%, intervals overlapping). **Elo added 2026-09-04**, pinned fit over 1,228 games per cell, core held fixed: `part` costs **-388** (m169), **-319** (m349), **-53** (`classic(chip=100)@2`), **-513** (m113 MLP) and **-140** (m97) against `ab(deep=6,tt,ord,nodes=200k)@2` on the same core, the largest effect anywhere in that grid and negative on every core. NOT tested: whether stopping the root scan at the trip, so `part` compares only searched values, makes `part` good. That is a smaller change than gating and nobody has tried it | Efficiency & Speed | [budget-parity-results-1](../plans/budget-parity-results-1-steady-meridian.md) | [budget-parity-results-2](../plans/budget-parity-results-2-steady-meridian.md) |
| 65 | Iterative deepening spends most of a budget on its last iteration, so declining an iteration that cannot finish should cost nothing and free most of the budget. If so, a node-budget agent can be given a much larger cap without paying for the doomed iterations the larger cap would otherwise start, and should reach a deeper completed depth for the same or fewer nodes | **Confirmed 2026-09-03 on depth and nodes, and 2026-09-04 on Elo for 4 of 5 cores.** Measured with the new `rank.exe nodeprofile`, the last completed iteration is **64% to 83%** of everything spent to reach it on all 15 cores, alternating with ply parity (classic growth 5.85x, 2.78x, 4.55x, 2.80x, 3.94x from d5 to d9). Depth 7 costs 1.39x to 2.79x the whole 200k budget on every one of the 15 cores, so it can never finish there. The new `rem=N` head flag declines an iteration unless N% of the node budget is unspent. It is never deeper than the ungated agent on any ply (guaranteed by construction, verified), and at the `rem=70` knee saves 35-39% of nodes while 2-3% of plies lose a completed ply. Spending the saving: `ab(deep=12,tt,ord,rem=70,nodes=300k)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` **dominates plain `nodes=200k` on both axes**, 163,580 nodes/move against 178,033 while searching deeper on 17.9% of plies and shallower on 0.9%. The predicted knee of `rem=77`, derived from the median growth ratio, was too aggressive because the growth distribution has a left tail. **Elo measured 2026-09-04**, pinned fit, 1,228 games per cell, delta against `ab(deep=6,tt,ord,nodes=200k)@2` on the same core. At the same 200k budget the gate is a wash for 29-38% less CPU (-4 m169, -13 m349, -61 classic, +34 m113, -17 m97). Spending the saving is where the gain is: `rem=70,nodes=300k` and `nodes=550k` give +35/+42 (m169), +51/+41 (m349), +122/+163 (m113 MLP) and +0/+10 (m97). **`classic(chip=100)@2` is the counterexample** at -66/-49, the only core the gate hurts, and it is also the core with the most budget left after depth 6 (31% of a 200k cap against 60-62% elsewhere). Untested hypothesis for that exception: its cheap doomed depth-7 iteration seeds the TT for the next move, so the gate discards a real benefit. The test is the same pair with `noTT` | Efficiency & Speed | noticed while measuring per-iteration node cost, 2026-09-03 | [budget-parity-results-2](../plans/budget-parity-results-2-steady-meridian.md) |
| 66 | The odd/even alternation in alpha-beta iteration cost is set by the EVALUATOR's granularity, not by move ordering. A coarse evaluator whose value changes only when material changes gives most even-depth leaves (the opponent has replied and recaptured) the identical score, and equal scores satisfy the `>= beta` cutoff test, so even iterations are cheap. A fine-grained evaluator has to search them | **Confirmed 2026-09-05, and the move-ordering half of the original hypothesis is REFUTED.** Alternation index (median growth ratio into an odd depth / into an even depth, over the d5..d9 ladder, `rank.exe nodeprofile`, 12 games, `ab(deep=9,tt,ord,nodes=999999k)@2`): `classic(chip=100)@2` reads **1.52** against 1.10-1.18 for 13 of the 14 learned cores. Isolating test: `classic`'s positional term is bounded by 49 with one of `wall`/`column` at 1 and by 98 with both (`structOwner`, `src/ai_eval.cpp`), all below the chip weight of 100, so those variants cannot reorder any material-differing position and change ONLY how ties break. They read **1.30** (`column=1`), **1.32** (`wall=1`) and **1.20** (both), with the depth-6 share rising 64.0% -> 70.0/68.9/70.7%, inside the pack's 70.4-73.8%. Quiescence, which resolves the capture asymmetry at the leaf, removes about half of it on the chip counter (1.52 -> 1.26) and nothing on a learned core (1.12 -> 1.10). Removing `ord` makes the chip counter's asymmetry LARGER (1.52 -> 1.70) and does so entirely on the odd side (even 2.79x -> 2.81x, unmoved), so capture-first ordering is not what makes even iterations cheap. Alternation and median game length correlate at r = -0.797 (t = -4.76, df = 13) across the 15 cores, and the `column=1` run separates cause from correlation: granularity moved alternation two thirds of the way to the pack while game length moved 7 plies | Efficiency & Speed | developer question about the low even-ply shares, 2026-09-04 | [budget-parity-results-2](../plans/budget-parity-results-2-steady-meridian.md) |
| 67 | Carrying a move's unspent node budget into the same side's next move (`retain`) makes the CHOICE of node count matter less, because a core that frequently leaves a large remainder can bank it and buy depth later. If so, the Elo-vs-node-count curve should be flatter under `retain` than without it | **Mechanism built and verified 2026-09-05, Elo IN FLIGHT.** New binary `ab()` flag. The purse is `g_nodeCarry[2]` (per side), reset per game beside every `ttClear`. It converges rather than exploding, because a bigger cap raises the `rem=` gate's absolute threshold until a full iteration fits and drains the purse. Measured at `rem=70,nodes=200k`: the chip counter goes 108,032 -> 165,058 mean nodes/move with a 429,035 peak and plies at depth >= 7 rising 19.7% -> 36.8%, and the tdleaf_self linear `model=169` goes 116,712 -> 174,098 with a 501,604 peak and 16.2% -> 23.7%. Mean spend stays under the nominal cap, so total per-game compute is conserved while individual moves spend 2.1-2.5 caps. **Elo settled 2026-09-05: REFUTED as stated, but a narrower version holds.** `retain` does NOT make the choice of node count matter less in general. It removes a starvation that exists only at the 100k rung. 40-cell pinned cohort, 5 cores x 4 budgets (100k/200k/300k/400k) x {`rem=70`, `rem=70,retain`}, 1,328-1,358 games per cell, one fit (`--pin ranking/q5/pin_no_d12.tsv`), read as the slope of Elo on log2(budget). Slope change under `retain`, Elo per doubling: tdleaf_self lin `model=169` +103.3 -> +36.7 (-66.6+-12.2, z=-5.45), tdleaf_self lin `model=349` +81.3 -> +24.0 (-57.3+-11.9, z=-4.83), position_elo mlp `model=113` +40.0 -> +29.5 (z=-1.01), chip counter +3.3 -> +1.7 (z=-0.15), pool_games lin `model=97` +15.2 -> +18.9 (z=+0.36). Those slopes are misleading on their own, and this is the load-bearing caveat: the plain curve is concave and the whole difference is the FIRST step. Plain rung-to-rung on `model=169` is +144, +39, +12 while `retain` is +13, +39, +23, and refitting on 200k/300k/400k only kills the effect (+10.2+-27.2, z=0.37 on `model=169`, -9.7+-26.9, z=-0.36 on `model=349`, every core under z=1.4). What survives: `retain` is worth +142+-17 and +120+-17 at 100k on the two tdleaf_self cores, which makes `retain` at 100k indistinguishable from plain at 200k (-2+-18 and +2+-18), one full doubling of budget. From 200k up it is worth roughly nothing. That matches the `rem=70` gate, which declines a deepening iteration unless 30% of the cap is unspent, so the smaller the cap relative to one iteration's cost the more there is to bank. **Re-anchored to the DEFAULT 2026-09-06** with a second 40-cell cohort at `rem=0` (the engine default, never previously played) plus `rem=0,retain` as a control. Three findings. (a) The control passes: `rem=0,retain` minus `rem=0` has max |z| = 1.75 over 20 cells, so with no gate there is no remainder and `retain` is inert, as the mechanism predicts. (b) The gate alone is FREE: `rem=70` minus `rem=0` is within noise in all 20 cells while saving 29-48% of the wall clock, so the iteration it declines really does contribute nothing. (c) `retain` is CPU-NEUTRAL against the default, at 0.85x-1.01x its ms/move, not the 1.2x-1.9x it appears to cost against `rem=70`. Against the default the 100k gain is +137+-17 (`model=169`) and +104+-16 (`model=349`) at 1.00x the CPU. The mechanism needs both halves: the gate finds the wasted budget, `retain` recycles it, and neither alone does anything for Elo. It cannot help a core with no depth slope at all: the chip counter's search changes measurably under `retain` (108,032 -> 165,058 mean nodes/move) and its Elo does not, which is separate evidence that it is depth-saturated at these budgets | Efficiency & Speed | developer proposal, 2026-09-04 | [budget-parity-results-2](../plans/budget-parity-results-2-steady-meridian.md) |
| 68 | The wall-clock track leaves budget unspent for the same reason the node track does, so `retain` should help there too. `nextIterationFits` declines an iteration it predicts will not fit, which is why a time-budgeted search finishes at roughly 40% of its allowance (measured 40.4 / 39.0 / 40.2% at 50 / 150 / 450 ms on a chip counter). Banking the unspent remainder into the same side's next move should buy depth exactly as the node purse does | **Built and unit-tested 2026-09-06, Elo UNMEASURED.** `g_timeCarry[2]` in `globals.cpp`, fed from the new `g_lastSearchMs`, read and written by the same `agentChooseMove` block as the node purse and independent of it. Tests assert the purse fills (at 40 ms on a chip counter at least one move finishes with time to spare), never goes negative, holds only the unspent part, is per side, is cleared per game, and that a node-only `retain` agent leaves it at zero. The node result it is extrapolating from is narrow, so the prediction is narrow too: `retain` helped only where the budget was starving the core (+142 and +120 Elo at 100k nodes, nothing from 200k up), so expect a gain at the short rungs of a time ladder and nothing at the long ones. The test is a 40-cell cohort, 4 cores x {25, 50, 100, 200, 400ms} x {plain, `retain`}, read as the knee of Elo against realized ms **Elo settled 2026-09-06. Mechanism CONFIRMED, strength claim REFUTED at matched CPU.** 40 cells, 2,136 games each, head `ab(deep=12,tt,ord[,retain],time=Xms)@3`. The mechanism works and works better than on the node side: plain realizes 0.34-0.56 of its flag, `retain` realizes 0.82-0.90, so it very nearly closes the underspend. Raw per-rung gains are large at short budgets (+125, +90 on `position_elo mlp model=113` at 25 and 50ms; +76, +70, +105 on `tdleaf_self lin model=169`). But it roughly DOUBLES the spend, so the fair test is `retain` at flag X against plain at flag 2X, which land within a few ms of each other: 11 of 16 such cells are within 1.5 SE of zero with alternating signs. At matched wall clock time-`retain` neither helps nor hurts, unlike the node purse which was CPU-neutral (0.85x-1.01x) and bought +137 Elo. The value here is that `time=X` starts meaning X and the cross-core realized-ms spread tightens to 1.1x, which is an instrument property rather than strength | Efficiency & Speed | developer instruction, 2026-09-06 | [time-ladder-results-1](../plans/time-ladder-results-1-copper-vireo.md) |
| 69 | The node track and the wall-clock track are both described as compute-normalization tracks in `ranking/CHAMPION.md`, so a fixed budget on either should cost every core about the same per move | **Half REFUTED, half CONFIRMED, 2026-09-06.** Cross-core spread in realized ms/move at one budget: the wall-clock track is 1.1x-1.6x (1.1x at every rung with `retain`), the node track is 18.0x at `ab(deep=12,tt,ord,rem=70,nodes=300k)@3` and 36.4x at `ab(deep=6,tt,ord,nodes=200k)@3`, where `position_elo mlp model=113` costs 400ms/move against the chip counter's 11ms. A node budget normalizes NODES, which isolates evaluator quality from evaluator speed and is a legitimate question, but it is not a compute comparison and must not be called one. `CHAMPION.md` needs rewording, not a new measurement. **Follow-up measurement, same date:** the live wall-clock track is not currently a wall-clock instrument either, for a DIFFERENT reason. Its 45 rostered `time=150ms` agents (320 games each) realize a mean of 20.5 ms/move on the 43 `deep=6` heads (range 7.5-75.9) and 61.5 ms on the 2 `deep=12` heads. At depth 6 the DEPTH CAP binds long before the clock, so the flag is close to decorative. That is not what `retain` fixes: `retain` recycles budget the clock gate declined to spend, and nothing recycles budget a depth cap never asked for. Re-specifying the track needs both changes, and both mint new identities | Benchmarking & Measurement | this session's CPU tables, 2026-09-06 | [time-ladder-results-1](../plans/time-ladder-results-1-copper-vireo.md) |
| 70 | `retain` should be the default on every budgeted head, node or wall-clock. Two claims: (a) it makes an agent's realized spend approach the budget it was given, and (b) it delivers much higher Elo at low budgets and slightly higher Elo at moderate budgets | **CONFIRMED on (a), CONFIRMED with a stated scope on (b), 2026-09-06.** Pooled inverse-variance across cores, contrasts taken inside a single fit. **(a) Budget realization.** Fraction of a `time=` flag actually spent, 20 cells (4 cores x 5 rungs): plain 0.432 (sd 0.053), `retain` 0.854 (sd 0.028). It nearly doubles the realization AND halves its spread, so the budget becomes both larger and more predictable. Cross-core spread in realized ms at a fixed flag tightens from 1.1x-1.6x to 1.1x at every rung. On the node side the same mechanism raises the chip counter from 108,032 to 165,058 mean nodes/move against a 200k cap. **(b) Elo, node track**, `rem=70,retain` minus the `rem=0` default, 5 cores: +52.5+-7.2 at 100k (z=7.25), +8.4+-4.4 pooled over 200k/300k/400k (z=1.92), and the low-minus-moderate difference is +44.1+-8.4 (z=5.22). So 'much higher at low, slightly higher at moderate' is the correct shape, with the moderate term positive but not individually significant. **(b) Elo, time track**, `retain` minus plain at the same flag, 4 cores: +67.3+-6.5 (25ms), +44.6+-6.6 (50ms), +33.2+-6.6 (100ms), +41.4+-6.9 (200ms), +17.6+-6.8 (400ms). Same declining shape, every rung significant. **SCOPE, and it differs by track.** On the NODE track the gain is free: `rem=70,retain` costs 0.85x-1.01x the default's ms/move, so +52.5 pooled at 100k comes at no extra compute. On the TIME track it is NOT free: `retain` roughly doubles the spend, and matched-CPU (`retain`@X vs plain@2X, landing within a few ms) pools to +2.8+-3.3 over all 16 cells (z=0.84), a wash. There the value is that `time=X` starts meaning X, an instrument property, and the practical consequence is that replacing a plain flag with a `retain` one at equal compute means roughly HALVING the flag. **Heterogeneity is large and the pooled means must not be quoted alone**: I^2 is 93% at 100k on the node track, where the per-core range is +137 (`tdleaf_self lin model=169`) to -12 (`pool_games lin model=97`). `retain` cannot help a core with no depth slope, and the chip counter is the proof: its search changes measurably under `retain` and its Elo does not move across a 4x budget range. The pooled SE is also a lower bound, since cells of one fit share a pinned pool and are not fully independent | Efficiency & Speed | developer instruction, 2026-09-06 | [budget-parity-results-2](../plans/budget-parity-results-2-steady-meridian.md), [time-ladder-results-1](../plans/time-ladder-results-1-copper-vireo.md) |
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

**Status:** Refuted in the naive form (a book of a stronger agent's mined
moves handed to a weaker live brain). Two repaired variants remain open:
reset-state reproducibility + a book that stays in book to the win.

**Origin:** `todo.md`, "The most promising follow-up for the standing dethrone goal" (`[Next]`).

**Tested in:** [dethrone-champion-results-3-wiggly-mitten.md](../plans/dethrone-champion-results-3-wiggly-mitten.md) --
`rank.exe bookgen` mined 553 positions from 29 d8/nb2m-oracle wins over the
s98 champion; the `book` opener replayed them from two brains, measured at 32
games/pair on the full-roster instrument. s98+book rated 1059 +/- 14 vs plain
s98's 1075 +/- 13 and went 14-18 in the direct pair; chip-counter+book rated
967 +/- 13 vs 983 +/- 12 bookless and went 7-25 against s98. Both book agents
also went 3-29 / 20-12 vs the oracle (the 20-12 is the oracle facing its own
replayed positions, a curiosity, not a dethrone).

**Correction 2026-07-26.** This entry originally read the foreign book as making
agents WORSE than bookless. That direction is not supported and the refutation
should be read as a null, not a harm. Under the hygiene rules
(`Docs/benchmarking.md`): 7-25 nominal is 4-10 (29%) on 14 distinct games against
bare classic's 9-23 (28%) on a genuine 32, so the two are indistinguishable. The
Elo deltas are likewise inside their error bars, and in the current fit one of them
has flipped sign: `learned(model=98,...).opener(book,book=1)` now reads 1066 +/- 12 against
bare s98's 1043 +/- 11 (+23, about 1.4 combined SE), where the original fit read
-16. Neither reading is a separation. The theory is still refuted, because it
predicted a book would let a d6 agent BEAT the champion and no version of the
numbers shows a gain, but "the foreign book gave no measurable lift" is the claim
the evidence supports.

**Notes:** Two premises failed, and both are the durable lesson. (1) The
"deterministic" target is not reproducibly deterministic ACROSS RUNS:
cross-game TT/killer/history state (theory 19 mechanism b) made s98 deviate
from the mined lines within a few plies -- 3 of the 32 source games did not
even reproduce their own stored result at bookgen time -- so the booked agent
falls out of book holding an oracle-shaped middlegame with a weaker brain.
(2) Mined moves are not brain-portable: the oracle's moves win because the
oracle's deep search stands behind them at every subsequent ply. Repair path,
now the concrete open version of this theory: a `--reset-state` mode so
deterministic pairs actually replay (also unblocks theory 19's mechanism
separation), plus a book that carries a full winning continuation or a
response tree over the target's deviations (pairgen's `--branch-tries`
machinery is the natural miner). Related: theories 19 (the blocking
artifact), 28 (why the chip counter loses these middlegames), 23
(deterministic tie-bias as the book's implicit key).

**Update 2026-08-27 (premise 1 measured as repaired):** the reset-state
prerequisite is no longer outstanding. `playOneGame` gained a per-game
`ttClear()` on 2026-08-03, and `rank.exe determinism` now measures the property
directly rather than assuming it: all 119 active deterministic agents replay
exactly against a fixed deterministic probe, both colours, within a process and
across separate processes (238/238 subject-colours, 210/210 for the node-budgeted
subset in all four passes). The only agents that ever varied are `model=111` and
`model=113` on the `time=150ms` head, the two lines already carrying `# cost flag`
in `ranking/roster.txt`, and `model=113` failed just once in four passes.

For contrast, the pre-fix store measures the failure this theory tripped over:
across 625,501 rows, only **2,545 of 22,322** strictly deterministic ordered pairs
(11.4%) replayed identically. That is what the naive book was mined against.

Premise (2) remains the reason the naive form failed, and theory 33's self-mined
book is one repair. The OTHER repair this entry names, a book that carries a full
winning continuation so the live brain never plays at all, is now unblocked and
is the subject of `plans/refutation-oracle-plan-1-quiet-lodestone.md`. Phase 0
(this gate) is done; the miner is not built. See
`plans/refutation-oracle-results-1-quiet-lodestone.md`.

**Update 2026-07-18 (theory 33 confirms a repaired form):** premise (2), not
(1), turned out to be the fixable one. Mining the BOOK-WEARER'S OWN wins
(instead of a foreign stronger agent's wins) removes the brain-portability
failure by construction, since the line-owner and the wearer are the same
brain. This dethroned the champion outright (1145 vs 1074). See theory 33 for
the full result and its own open scrutiny question (genuine strength vs a
pool-specific/memorization effect). The reset-state prerequisite from premise
(1) remains unaddressed and would still improve measurement quality here.

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

#### 28. Learned piece-square evaluators counter the chip counter head-to-head far above their pooled Elo

**Claim:** Learned piece-square value models at the d6 head beat the classic
chip counter in direct play at rates their pooled Elo says should be nearly
impossible -- the chip counter has a style-level weakness class, and the
learned models exploit it regardless of training recipe.

**Status:** Confirmed as an empirical pattern. The mechanism is hypothesized
(labeled below), not tested.

**Origin:** phase 0 of the dethrone plan
([dethrone-champion-plan-1-wiggly-mitten.md](../plans/dethrone-champion-plan-1-wiggly-mitten.md)),
while diagnosing why the 2026-07-14 refit compressed the top of the table.

**Tested in:** [dethrone-champion-results-1-wiggly-mitten.md](../plans/dethrone-champion-results-1-wiggly-mitten.md) --
the chip counter `ab(deep=6,tt,ord,nodes=200k).classic(chip=100)` scored 41/72
(57%) against the 36 d6-head residual/MLP-study models rated ~300 Elo below
it (expected score at that gap: ~85-95%), and was swept 0-2 by eleven of
them spanning every recipe class in the study (linear plain s3/s4/s6, linear
residual s10/s12/s13, mlp16 s16/s22, mlp32 s28/s33/s35). Against the d4-head
cohort of the SAME models it scored 94%. Top learned/adv agents held 88-92%
against the same new opponents. Boosted 32-game head-to-heads agree: s98
beats the chip counter 23-9, s96 beats it 18-14. The consequence is the
2026-07-17 dethrone: s98 certified at 1064 +/- 14 vs the chip counter's 976
+/- 13 on the identical search head.

**Notes:** Labeled hypothesis for the mechanism: equal-material positional
tie-breaking. The chip counter's eval is blind among equal-material lines
(exact ties everywhere material does not change), so its play there is
enumeration-order default (theory 23's left-file bias), while ANY piece-square
table always has a positional preference. A learned opponent can steer the
game through material-equal channels where the chip counter drifts. Tests
that would settle it: (a) replay chip-counter-vs-s98 games logging the chip
counter's root eval and count decisive plies where its top moves were exact
ties; (b) ablate s98 toward material-only and watch the head-to-head
collapse; (c) check whether quiescence (a tactics fix, dethrone plan phase 1)
moves the 57% number at all -- if it does not, the weakness is positional,
not a leaf-tactics horizon artifact. Related: theory 1 (this is what its
standing longitudinal re-check exists to catch -- a NEW style class joining
the pool and exposing out-of-distribution weakness, here in the old champion
itself), theory 19 (det-vs-det replicate noise qualifies the per-pair
records), theory 27 (pooled metrics hiding head-to-head structure), theory
23 (the tie-default mechanism the hypothesis builds on).

**Update 2026-07-17 (test (c) ran, phase 1):** quiescence did not close the
gap -- it widened it. The chip counter WITH quiescence lost its learned-model
head-to-heads worse than without (5-27 vs s98 and 4-28 vs s96, from 9-23 and
14-18 plain), while gaining against its own classic family. The tactical
(leaf-horizon) explanation is refuted; the positional tie-breaking hypothesis
stands, with tests (a) and (b) still open. See
[dethrone-champion-results-2](../plans/dethrone-champion-results-2-wiggly-mitten.md).

#### 33. Mining a book from the book-wearer's OWN wins fixes the naive refutation book's brain-portability failure

**Claim:** Theory 14's book failed because it mined the STRONG agent's
(oracle's) winning moves and handed them to a WEAKER brain that couldn't
reproduce the oracle's follow-up once out of book. Mining the WEAK/book-
wearing agent's OWN wins instead -- even if those wins came against a
stronger opponent -- fixes this by construction: the line-owner and the
wearer are the same brain, so there is no handoff mismatch. In-book, it plays
exactly what it would have chosen anyway; out-of-book, it reverts to its own
normal search, never a foreign, badly-fitting position.

**Status:** Unresolved (downgraded 2026-07-26 from "Confirmed as a major result").
It did dethrone the reigning champion on the instrument as it stood, but see the
correction at the end of this entry: the headline records do not reproduce, and
one of them was read off the wrong search head.

**Origin:** developer's hypothesis in conversation, 2026-07-18, directly
targeting theory 14's identified brain-portability failure mode.

**Tested in:** [dethrone-champion-results-5-wiggly-mitten.md](../plans/dethrone-champion-results-5-wiggly-mitten.md) --
`rank.exe bookgen` re-run with the two agent roles swapped from theory 14's
book (`--a` = classic, the book-wearer itself, `--b` = s98, the target),
mining classic's own 7 reproducible wins (of 9 historically recorded,
32 stored games) into a 134-entry book (`models/book2.txt`), zero new code.
Boosted to 32 games/pair, full-roster refit: classic+selfbook rated 1145 +/-
13, ABOVE s98's 1074 +/- 12 (non-overlapping error bands), and went 25-7
(78%) against s98 directly, up from bookless classic's 9-23 (28%) and the
theory-14 book's WORSE 7-25 (22%). It also went 27-5 (84%) against the
ORACLE -- an opponent it was never mined against -- and 32-0 against its own
bookless self.

**Notes:** The oracle result reframes the mechanism. The book fires from very
early-game positions (the standard start is identical across every game
classic plays, regardless of opponent), so its effect is not really "a
countermeasure tuned to s98's specific weaknesses" -- it is closer to "the
best opening/early-game line classic has ever been observed to find, locked
in and replayed on demand," which then generalizes to any opponent. Candidate
explanation for why classic's own LIVE search doesn't reliably find this line
on its own: cross-game search-state carryover (theory 19 mechanism b) makes
even a fully deterministic agent's play depend on incidental prior-game
state, so it sometimes finds its own best line and sometimes doesn't; the
book removes that variance. This softens the original hypothesis's emphasis
on "the mined opponent must be stronger" -- the load-bearing ingredient may
simply be "the book-wearer's own win, against anyone," untested (see the
results doc's Future Work). Open scrutiny question, NOT resolved by this
theory: whether this represents genuine transferable strength or a
pool-specific effect (the community-competition-vision memory's "one script"
degenerate-strategy concern) -- both readings are consistent with the data
so far, since the book has only been tested inside this project's own
deterministic, shared-tie-break-convention pool. Related: theories 14 (the
premise this repairs), 19 (the blocking artifact for premise 1, still open),
23 (the shared tie-break convention the pool-specific reading would invoke).

**Correction 2026-07-26 (status downgraded to Unresolved).** Re-audited under the
hygiene rules in `Docs/benchmarking.md`. Three of the four numbers above do not
survive, and the pool-specific reading of the open scrutiny question is now the
better-supported one. See theory 38 for the mechanism and
[book-opener-audit-results-1-vivid-lantern.md](../plans/book-opener-audit-results-1-vivid-lantern.md).

- "**32-0 against its own bookless self**" is wrong: that record belongs to
  `ab(deep=6,ord,nodes=200k)@1.classic(chip=100)@2`, a DIFFERENT head (no TT), which is
  defect 2. Against its actual bookless self at the shared `ab(deep=6,tt,ord,nodes=200k)`
  head the store reads 29-3, and that decomposes into run `20260718T175325Z` at
  5-3 of 8 and run `20260718T175433Z` at 24-0 of 24.
- The pair is deterministic, so those 32 rows are not 32 games (defect 3). Fresh
  reproduction: eight separate 8-game processes each returned exactly 5-3, and one
  64-game process returned 34-30 (53%). The 91% that the certification rests on is
  the top of a 53%-100% range produced by cross-game TT state alone.
- "25-7 (78%) against s98" is 8-4 (67%) on 12 distinct games; "27-5 (84%) against
  the oracle" is 12-2 (86%) on 14 distinct games.
- The theory-14 book was NOT "WORSE" than bookless: 7-25 nominal is 4-10 (29%) on
  14 distinct games, against bare classic's 9-23 (28%) on a genuine 32.

What does survive is the contrast that motivated the theory: bare classic scores
28% against s98 over 32 genuinely distinct games, and the self-booked version
scores 67% over 12. The self-mined book does something real at the fixed start.
What is refuted is its size, its reproducibility, and the "generalizes to any
opponent" reading.

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

**Notes:** Seed replicas showed a 50-150 Elo spread, an order of magnitude above rating error -- any sweep conclusion needs multiple seeds to be trustworthy. Re-confirmed 2026-07-17 on the champion's own recipe: six seeds of `pg_oracle_champ` at the d6 head spanned 1017-1107 (90 Elo), and the seed the vs-champion study promoted (3003, the reigning champion) is a middling 1079, NOT the best -- the best seed (4004) is +28. But averaging the seeds' WEIGHTS does not bank that spread as strength: it is decisively worse (theory 30). Seed SELECTION by full-roster Elo, not seed averaging, is the way to spend replicas, subject to the held-out-overfit caveat.

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

#### 29. Quiescence adds strength at the d6/nb200k head

**Claim:** A captures-only stand-pat extension at depth leaves (the `qs` head
flag) converts the d6/nb200k head's idle node budget into tactical horizon
and therefore Elo.

**Status:** Refuted for the learned-eval champion on a CLOSED-BOOK (no-opener)
det-vs-det comparison; weak positive inside noise for the chip counter.
REOPENED by a 2026-07-17 opener probe: under a 6-ply random opener the same
pair reverses to a clear win for qs (see Notes). Shallow heads and tight
budgets untested (same open regime as theory 21).

**Origin:** dethrone plan phase 1
([dethrone-champion-plan-1-wiggly-mitten.md](../plans/dethrone-champion-plan-1-wiggly-mitten.md)),
motivated by the champion head using only ~15-20k of its 200k node budget.

**Tested in:** [dethrone-champion-results-2-wiggly-mitten.md](../plans/dethrone-champion-results-2-wiggly-mitten.md) --
full-roster refit with all contender pairs at 32 games. `s98+qs` 1073 +/- 15
vs plain s98 1074 +/- 14 (pooled tie) while LOSING the direct pair 9-23;
`classic+qs` 1002 +/- 14 vs plain 983 +/- 12 (+19, ~1 SE). At the classic
head qs cost ~40% nodes/move and ~33% cpu/move, still far under budget.

**Notes:** Reads as the theory-21 story repeating for a second leaf
extension: at d6 with tt+ord the main search already resolves the exchanges
that matter, so capture-only leaf knowledge rarely changes a decision.
LABELED HYPOTHESIS (untested): stand-pat is systematically optimistic in
Breakthrough because no quiet moves exist (Lemma B, every move advances
irreversibly) and the decisive threats are runner ADVANCES, which a
captures-only extension never explores -- a runner-threat-aware quiescence
(D9 detection via the existing row counts) is the natural follow-up. The
probe also produced theory 28's test-(c) answer: quiescence made the chip
counter's learned-model head-to-heads WORSE (5-27 vs s98, 4-28 vs s96), so
that weakness is positional, not tactical. Methodology: the 8-games/pair
preliminary read (+80, level with the oracle) fully inverted at 32
games/pair -- never quote preliminary fills (see `Docs/benchmarking.md`).

**Update 2026-07-17 (opener probe, developer hypothesis):** the closed-book
9-23 loss was suspected of being a narrow, correlated sample rather than a
representative one, since s98 and s98+qs share the same evaluator and, without
forced diversity, walk nearly the same path every game (theory 19's only
source of variation between them is incidental cross-game search-state
carryover, not a deliberately varied sample). `rank.exe pairgen` between the
same two IDs with a 6-ply (3-move-per-side) random opener, 150 games: **91-59
(60.7%) in favor of s98+qs** -- a full reversal. This does not overturn the
strength-vs-tactics conclusion above (theory 28's test-(c) answer stands), but
it does overturn "quiescence doesn't help s98": the closed-book comparison was
measuring a narrow correlated sample, not qs's average behavior. Caveat: theory
6 found symmetric random openers can inflate a challenger's apparent win rate
by handicapping a deterministic base agent into off-policy moves; that
mechanism is milder here (both agents share nearly the same evaluator, so both
absorb the same handicap) but is not ruled out, so 91-59 should not be read as
the final number either. The clean permanent fix is the `--reset-state` mode
already filed under theory 14/19 -- it would let a closed-book comparison
generate many genuinely distinct games without needing artificial opener
randomization at all. Until that exists, qs's true effect on s98 is unsettled
between "roughly neutral" (closed-book) and "a real win, partly opener-
inflated" (opener probe); re-run the certification-standard 32-game boost with
this opener before trusting either number as the standing figure.

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
The full-roster Elo (6 seeds, depth 4 AND depth 6) agrees: the skip has no reliable
strength effect at any capacity or depth (linear d4 759 vs 753, d6 959 vs 947; the
MLP skip deltas bounce +14..+45, all inside ~100+ seed spreads). Separately, the MLP
capacity group answered the theory-10 question and the answer is negative: its far
better calibration made it WEAKER, not stronger (theory 27). Decisions this session: HARD frozen skip and the
literal chip differential; the soft/regularized skip and a broader baseline remain
open (they could still matter for a model class that cannot already express
material, unlike the linear v2 here). Related to theory 4 (nonlinear capacity jump),
theory 10 (linear PST ceiling), theory 8 (training-seed noise), and theory 22.

#### 27. Lower value-model outcome-loss does not imply higher agent Elo

**Claim:** For a value model used as an alpha-beta leaf evaluator, a lower
outcome-prediction loss (better offline calibration) does not translate to higher
agent Elo. The two can diverge outright: a higher-capacity model that fits the
outcome labels better can play worse.

**Status:** Promising / observed, now seen repeatedly in a second, unrelated
model family: the position-oracle dist model (theory 34) shows the same
divergence at every search depth measured. At d4, two of three MLP configs
lost to linear in play despite beating it on prediction; once all three MLP
configs were also rated at d6/nb200k, ALL THREE lost to linear in play
(Elo 974/967/931 vs linear's 1031) while still beating it on prediction
(MAE 147.9-150.0 vs 161.3) -- the developer's own expectation that a d6 MLP
would become the new champion did not hold. See
[position-oracle-results-1](../plans/position-oracle-results-1-lazy-popping-simon.md).
Mechanism there not yet investigated; flagged as future work.

**Origin:** the residual/MLP Elo follow-up, 2026-07-14, when the MLP value model was
finally rated (the whole point of that follow-up).

**Tested in:** [residual-mlp-results-2-tingly-chipmunk.md](../plans/residual-mlp-results-2-tingly-chipmunk.md) --
6-seed full-roster runs at depth 4 AND depth 6. The MLP (129 -> 16 or 32 -> 1, ReLU)
beat the linear v2 model on equal-material loss by ~0.17 (0.52 vs 0.69) and on
overall loss, yet rated ~95-130 Elo LOWER in the same search at BOTH depths (d6:
linear ~953, mlp(16) ~858, mlp(32) ~823; d4: linear ~756, mlp(16) ~675, mlp(32)
~652); more hidden width lowered loss further and Elo further, at both depths.

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

**Related:** theory 51 finds the opposite direction (MLP capacity clearly
HELPING Elo) under a different regime -- Gumbel-Zero self-play training +
Gumbel MCTS search, rather than this theory's supervised outcome-regression +
alpha-beta search. Read together, the two suggest the divergence this theory
describes is regime-dependent rather than a fixed property of MLP capacity
itself; which regime factor (search algorithm, training objective, or their
interaction) drives the difference is untested.

#### 30. Weight mirror-symmetrization + seed-ensembling is a free variance cut that raises Elo

**Claim:** For a linear v2 value model, (a) projecting the weights onto their
left-right mirror symmetry (the value function is exactly mirror symmetric, so
the anti-symmetric component is sampling noise) and (b) averaging K seed
replicas' weights (for a linear model the ensemble IS the weight average) both
cut training-seed variance (theory 8) at zero inference cost and should raise
Elo.

**Status:** Refuted for playing strength. Both interventions HURT.

**Origin:** `todo.md` Training Regimes, "Weight symmetrization + seed-ensembling
for linear models" (`[Now]`), framed as a direct attack on the theory-8 seed
noise.

**Tested in:** [dethrone-champion-results-4-wiggly-mitten.md](../plans/dethrone-champion-results-4-wiggly-mitten.md) --
`train.exe ensemble` (new regime) built a mirror-symmetrized champion (slot10)
and a 6-seed mirror ensemble (slot9) from six seed replicas of the champion's
exact recipe; full-roster refit with the d6 contender pairs at 32 games.
Mirroring the champion's OWN weights cost 135 Elo (1079 -> 944, a clean
isolation: same weights, only the mirror projection differs); the 6-seed mirror
ensemble was 924, -144 vs the seed mean (~1068) and worse than every single
seed (1017-1107). The effect widened from the 8-game preliminary, so it is not
under-sampling.

**Notes:** The design rationale is sound for the value FUNCTION (the x ->
SIZE-1-x reflection is an exact symmetry of the rules + standard start, so
projecting onto the symmetric subspace cannot change how a model ranks a
position vs its mirror), which makes the 135 Elo drop the finding. LABELED
HYPOTHESIS for the mechanism: a symmetric model produces MORE exact evaluation
ties (mirror-image moves score identically), and the engine breaks ties by a
fixed directional first-found rule (theory 23's left-file bias); the asymmetric
component the champion learned is a definite learned preference at each such
fork that scores better than the enumeration default against a pool of
DETERMINISTIC, directionally-biased opponents (theory 19) -- i.e. the asymmetry
is not noise for PLAYING, only for the value function. Averaging linear PST
weights separately blurs each seed's sharp decisive features into a flatter,
weaker table ("the ensemble is the average" is arithmetic truth, not a strength
claim). Second labeled hypothesis (a theory-27 parallel, untested): mirror
symmetrization may LOWER outcome-prediction loss while lowering Elo. Repair
paths / open controls: a pure-average (mirror=0) ensemble to isolate averaging
from mirroring; a non-directional tie-break or softmax tie policy (todo `[Now]`)
if the tie mechanism is confirmed; seed SELECTION by Elo instead of averaging
(the best seed is +28 over the champion). Related: theories 8 (the noise this
targeted), 23 (directional tie-break), 19 (deterministic biased pool), 27
(calibration/strength divergence), and axioms O4/D5/E1.

#### 31. Quiescence induces a "posturing" style via even-trade square-value bias

**Claim:** Quiescence only extends leaves where the mover has a pending
capture, and only cares whether a capture exists, not whether it is even or
favorable. For a per-square (not just per-material) value model, resolving
an EVEN trade can still shift the evaluation, because it changes which
specific squares end up occupied. This creates an incentive structure where
lines that happen to land a pending (even) trade exactly at the search
horizon get a more informed second look than lines that don't, so the agent
may defer an attack, massing pieces so that a trade is available right at the
horizon, rather than attacking as soon as it can -- a "posturing" style,
distinct from and not explained by the race-blindness mechanism in theory 28.

**Status:** Open / untested. Plausible from the code, not yet confirmed by a
controlled experiment.

**Origin:** developer's hypothesis in conversation, 2026-07-17, following up
on quiescence's agent-dependent strength effect (theory 29).

**Tested in:** -- A same-pair (s98 vs s98+qs) check of average game length and
final piece counts was run informally in conversation and was inconclusive:
pool-wide, s98+qs averages slightly SHORTER games than plain s98 (46.8 vs 49.4
plies), which cuts against a naive "always defers, so always longer" reading,
but the specific stored sample was too repetitive (theory 19b) to isolate a
clean signal either way.

**Notes:** The proposed test is a new Advanced-evaluator term, tentatively
"Cluster": the existing Wall/Column orthogonal-adjacency logic
(`structOwner`/`pairContrib`, `src/ai_eval.cpp`), restricted to the middle
rows only (excluding the 2 rows nearest each side's home row and the 2 nearest
the goal row, avoiding confounds with the existing Hole/Control/RaceWin terms
that already own that territory). Confirmed from the code: Wall/Column
currently scan the WHOLE board with no row restriction, so this is a new
variant, not already covered. The critical test: hill-climb the Advanced
weight mix TWICE, once with `qs` off and once with `qs` on, and see whether
Cluster's climbed weight differs sharply between the two runs (and whether
Race/RaceWin's weight rises when `qs` is on, the same blind-spot prediction
theory 28/29 already made). Not yet implemented. Filed to `todo.md`'s
Heuristic Evaluator Feature Ideas.

#### 32. This pool's shared left-file bias makes model asymmetry an adaptation, not noise

**Claim:** Theory 30 found that mirror-symmetrizing a linear value model's
weights cost significant Elo, which was surprising given the value function
is genuinely mirror-symmetric. This theory proposes why: most of the pool's
deterministic search agents share the SAME first-found left-file tie-break
bias (theory 23), including the very champion (the old classic incumbent)
whose games trained s98's weights. A model that has learned an asymmetric
preference matching or countering that shared pool bias is exploiting real,
useful information about how THIS SPECIFIC pool plays -- not fitting noise --
so removing the asymmetry throws away a genuine adaptation.

**Status:** Open / untested. A concrete 5-way experimental design has been
proposed but not built.

**Origin:** developer's hypothesis in conversation, 2026-07-17, refining
theory 30's own labeled mechanism hypothesis (directional tie-breaking against
a biased pool).

**Tested in:** -- Proposed design, reusing `trainEnsemble`'s existing
`mlv2MirrorIndex` machinery (`src/ml_train.cpp`) with additional modes beyond
the current average (`--mirror 1`): (a) **unflipped** -- the champion's
original weights (already measured, theory 30's 1079/1074 baseline); (b)
**flipped** -- a full reflection, `w'[i] = w[mirror(i)]` for every square, no
averaging (tests whether the SPECIFIC direction of the learned asymmetry
matters, or whether any consistent asymmetry would do); (c) **averaged** --
the existing mirror=1 mode (already measured at 944, theory 30); (d)
**left-onto-both** -- copy each mirror pair's LEFT-column value onto both
squares (tests whether the left half was better-fitted, e.g. because
opponents' left-biased play made left-side positions more common/decision-
relevant in training); (e) **right-onto-both** -- the same, mirrored. If (a)
>> (b), the specific learned direction matters (fitted to something real
about the pool, not just "break the tie somehow"). If (b) ~ (a), both >> (c),
asymmetry itself is what matters, not its direction. If (d) or (e) >> (a),
one half's fitted values are better than the natural mix. Not yet
implemented or measured. Related: theories 23, 28, 19, 30, and axioms O4/D5.

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

#### 34. A distributional model trained on designed fresh-game gap labels out-predicts the d8 oracle at position strength

**Claim:** Per-position Elo advantage can be MEASURED by playing designed
fresh games from the position between rated agents at controlled Elo gaps
(the win-probability curve's horizontal slide is the advantage, its flatness
the volatility), and a two-headed model (mu + log-sigma over the v2
piece-square features) trained on those raw outcomes predicts a held-out
position's measured advantage better than the d8/nb2m oracle's own root
search score does (after mapping the oracle's score to the Elo scale by a
1-parameter calibration fitted on training positions). Success criterion,
locked before the campaign: beat the calibrated d8 baseline on BOTH held-out
outcome likelihood and mean-advantage error (train.exe dist-eval's VERDICT
line).

**Status:** Confirmed. All four production configs (a linear model and three
MLP variants) beat the calibrated depth-8/2M-node oracle baseline on BOTH
outcome NLL and mu MAE, on the held-out 700-position eval tier, cleanly and
not marginally: best config (dist_mlp_wide) MAE 146.2 / NLL 0.4079 vs the
oracle's 191.3 / 0.4498.

**Origin:** developer's idea in conversation, 2026-07-18 (rank a position by
its Elo advantage as a mean + standard deviation), sharpened by the
developer's insistence on designed new data over found corpus data and on
oracle-grade quality.

**Tested in:** [position-oracle-plan-1-lazy-popping-simon.md](../plans/position-oracle-plan-1-lazy-popping-simon.md) --
the posgen/label/labelfit pipeline plus train.exe dist-value and dist-eval.
[position-oracle-results-1-lazy-popping-simon.md](../plans/position-oracle-results-1-lazy-popping-simon.md)
has the full numbers, including three exploratory sweeps run mid-session
(position-count scaling, hyperparameters, MLP-specific scaling) that
materially changed the final training recipe.

**Notes:** The theory-27 caution is carried explicitly and is now confirmed
a THIRD time by this same campaign: the roster Elo of the dist models
diverges from their prediction-quality ranking (dist_lin beats two of the
three MLP configs in actual play at matched depth despite losing to them on
prediction), so beating the oracle at prediction did not translate cleanly
to playing strength. The labels are relative to the labeling ladder's style
of play (mixed random-dilution and depth-dilution rungs), the same scoping
caveat agent Elo itself carries. Related: theories 26 (this design sidesteps
low-Elo label noise by conditioning on the players' measured strength), 27,
and the Elo-tie labeling idea in todo.md (the mean at zero gap realizes it).

#### 35. Position volatility (sigma) is identified by the flatness of the win-prob vs Elo-gap curve and predicts conversion reliability

**Claim:** Positions differ not only in who is favored but in how RELIABLY
the advantage converts. This is measurable as the flatness of the
win-probability curve against the players' Elo gap: from a volatile (sharp,
swingy) position, even a much stronger player wins unreliably, so the curve
is flat and the fitted latent-SD sigma is large; from a quiet position the
gap predicts the winner well and sigma is small. The dist model's sigma head
learns this signal well enough that its predicted sigma correlates with the
per-position measured sd on held-out positions.

**Status:** Weakly supported, not confirmed. The sigma head produces real but
weak signal: predicted sigma correlates with measured sd at only 0.12-0.29
(Pearson) and as low as 0.02 (Spearman, for two of the four configs) on the
held-out eval tier. Positive, better than nothing, well short of the mean
head's clean win on theory 34. The hyperparameter sweep's lrSigma/lr-ratio
axis came back flat, meaning this session never found a setting that
specifically improved sigma -- the mu-tuned recipe was simply carried over.

**Origin:** the developer's framing that a position's Elo advantage is a
range, not a point (2026-07-18), operationalized as the latent-Gaussian
probit model (probitPoint, pi/8 constant).

**Tested in:** same campaign as theory 34 (dist-eval's SD-validity line:
correlation of predicted sigma with measured sd, plus the per-epoch mean
sigma and material-stratified sigma printouts).
[position-oracle-results-1-lazy-popping-simon.md](../plans/position-oracle-results-1-lazy-popping-simon.md)
has the full numbers and flags a dedicated sigma-only sweep as future work.

**Notes:** Identification caveats recorded at design time: (1) dilution
rungs inject move randomness that inflates measured sigma at weak rungs, so
the gap design uses the strongest pair available at each gap level and the
labelfit per-pairing QC table watches for rung-dependent inflation; (2) a
global sigma floor absorbs pool-level noise, so positional volatility is the
deviation from that floor, not the absolute value; (3) small sigmas are not
identifiable from sparse per-position samples (the synthetic-recovery test
pins this), which is why the eval tier plays a dense design per position.

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

**Tested in:** [incremental-wall-column-eval-results-1-golden-forest.md](../plans/incremental-wall-column-eval-results-1-golden-forest.md) -- `test_eval.cpp`'s equivalence test walks the move tree asserting the incremental accumulator (`g_evalPos`) always equals a full `evalPosFull` recompute, and measured a **33-39% cpu/node reduction** (`ab(deep=4).classic(chip=10,wall=3,column=2)` -39.3%, two Experimental presets -37.9%/-33.4%) with byte-identical eval values, so game outcomes and Elo were unchanged by construction. [incremental-ml-eval-results-1-luminous-snail.md](../plans/incremental-ml-eval-results-1-luminous-snail.md) -- same incremental-accumulator pattern (`g_mlAcc`/`mlLeafScore`) applied to the learned value model, the pattern's second extension beyond chip count. The original chip-count commit (`3af970d`) predates this project's equivalence-test/results-doc discipline, so its correctness rests on the counters' logic (increment/decrement mirrored exactly on capture, in both `simulateMove` and `unsimulateMove`) rather than an automated equivalence check -- no regression has surfaced since, but it wasn't verified the same rigorous way as the two later heuristics. [chip-count-speedup-results-1-iterative-raven.md](../plans/chip-count-speedup-results-1-iterative-raven.md) (2026-07-10) retroactively fills that gap: `train.exe speed`'s eval-level ladder (`g_evalLevel` 1/2/3) reconstructed the pre-`3af970d` leaf and measured, with an in-harness equivalence check (same end board + node count across levels, PASS) standing in for the missing test. Measured chip-count speedup (v1->v2): **-45 to -62% us/move at the champion's zero-structure weights** (w0,l0, the historically relevant configuration, roughly a 2x speedup) and **-14 to -16%** when a full structure scan shares the leaf (w2,l2, depth >= 3). The same run re-measured the structure step (v2->v3, full per-leaf scan -> cached `g_evalPos`) at **-62 to -66% us/move** -- larger than the 33-39% above because the baselines differ: the 33-39% measured only the pt.2 refinement (bounding-box delta -> neighbor-local delta) against an already-incremental baseline, while the ladder's v2 baseline is the older full-per-leaf scan; the numbers nest consistently (~-40% pt.1 x ~-39% pt.2 = ~-63% total). It also found the zero-weight overhead in the Status qualifier above (the incremental machinery costs +18 to +35% when it maintains an always-zero accumulator), suggesting a weight-gated `g_evalIncremental` as a follow-up engine win.

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
