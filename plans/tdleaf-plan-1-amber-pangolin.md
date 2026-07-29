# TD-Leaf(lambda) self-play bootstrap - plan 1

Origin: `todo.md` line 555, "TD-Leaf(lambda) self-play bootstrap (value) `[Later]`",
promoted to active work by developer instruction 2026-07-29.

## What this is

Every value model in this project today is trained by a **supervised, offline**
signal: a position gets a fixed label (final game outcome, a teacher's eval, or
the position-oracle's fitted Elo gap), and the model is fit to those labels in a
batch afterwards. TD-Leaf(lambda) is a different regime. Its target for a
position is the model's OWN evaluation of a later position, backed up through
the search. It is online and bootstrapped rather than offline and supervised.

The "Leaf" part is what makes it fit a search engine. A position's usable value
is not its static eval, it is the minimax value the search returns, and that
value IS the static eval of the leaf at the end of the principal variation. So
the gradient that should be applied for position `s_t` is the gradient of the
evaluator at `leaf(s_t)`, not at `s_t`. That is the Baxter/Tridgell/Weaver
result (KnightCap).

## The math actually implemented

For one self-play game with non-terminal positions `s_0 .. s_{N-1}` and final
white-centric outcome `z` in {0, 0.5, 1}:

1. Search `s_t`, walk to its principal-variation leaf `l_t`.
2. `r_t` = the model's raw logit at `l_t` (white-centric), `p_t = sigmoid(r_t)`.
3. TD errors `d_j = p_{j+1} - p_j`, with `p_N := z`.
4. Eligibility-weighted sum by backward recursion: `e_t = d_t + lambda * e_{t+1}`, `e_N = 0`.
5. Output gradient for the cross-entropy loss against the lambda-return:
   `gOut_t = -e_t`, applied at `l_t`'s features via the existing
   `Model::gradStep(x, n, gOut, lr, l2)`.

Using cross-entropy rather than squared error means `dL/dlogit = p - target`
exactly, so no `p(1-p)` factor has to be carried by hand and the step is better
conditioned near saturation.

**Two closed-form checks this yields, both unit-tested:**

- `lambda = 1` collapses the eligibility sum to `e_t = z - p_t` (telescoping),
  so `gOut_t = p_t - z`. TD-Leaf at lambda=1 is EXACTLY outcome-supervised
  training on PV-leaf positions. This is the bridge to the existing regime.
- `lambda = 0` gives `gOut_t = p_t - p_{t+1}`, pure one-step TD.

## Design decisions and why

| Decision | Choice | Rationale |
|---|---|---|
| PV extraction | Probe the transposition table along the played line | The obvious alternative, re-searching at decreasing depth, costs up to `d` x the node budget per move (~6x at `d6/nb200k`), not the ~40% first assumed. TT probes are ~free. Cost: the PV can be short when entries are overwritten, so **mean PV depth is reported as a diagnostic**. |
| Engine changes | **None** to `ai_minimax.cpp` | A `g_collectPV` branch in the hot recursion would change us/node for every rated agent and invalidate the ranking instrument's cost figures. Board snapshot + TT probe keeps the shared search untouched. |
| Model architecture | Linear v2 PST (129 -> 1, **130 params**) primary; one MLP arm | All 5 current category champions are 130-param linear models. Theory 37 refuted the wide head on speed, theory 39 has MLPs generalising worse across openings, theory 40 measured a 142-Elo MLP seed spread. |
| Initialisation | Both champion-bootstrapped and from-scratch arms | Developer decision 2026-07-29. Bootstrapped is likeliest to produce a dethroning agent; scratch is the cleaner claim. |
| Update schedule | Online primary, batched behind a flag | Developer decision 2026-07-29. Online is what the todo item names; the flag lets the study measure whether serialising actually buys anything. |
| Generator search depth | **d6/nb200k**, the head the cohort is certified at; d4 kept only as a deliberately swept axis | An earlier draft defaulted to d4 purely because it is 17x cheaper. That was never agreed, and it violates the standing "compute is cheap, do not pre-shrink" rule. It also mismatches distributions: TD-Leaf's target IS the search's backed-up value, so a d4 generator produces d4-quality targets for a model certified at d6 (CHAMPION.md rule 5). Rating dominates training cost by ~100x, so training depth buys nothing worth having. The repo's "teacher depth is irrelevant" finding was measured for SUPERVISED training and does not transfer for free. |
| Game count | **Not an input.** Checkpoint ladder per run, every checkpoint rated | See below. |
| Rating instrument | Cohort joins the roster (~20% of pool) for a full anchored refit | Developer decision 2026-07-29. Gauntlets screen only (CHAMPION.md rule 1), and this project's record has pairwise/gauntlet order being anti-predictive of pooled Elo. Putting the cohort in the pool also makes the self-play agents play EACH OTHER, which a gauntlet never does. |

## Game count is UNKNOWN and must be measured

Two claims were made during planning and both are withdrawn. Recorded here
because the reasoning that produced them is the failure mode to avoid.

**Withdrawn claim 1: "about 10,000 games/seed."** Fabricated. No basis in the
repo or the literature.

**Withdrawn claim 2: "single-teacher self-play converged at 500 games."** This
cited `models/sweep/scaling.csv`, but that file cannot support it. Worse, the
project had **already** recorded the correct reading:
`plans/training-sweep-results-1-luminous-snail.md` item 3 (2026-07-24) says the
stop "triggered on noise, not on convergence" and warns in as many words "do not
trust 'self-play plateaus at 500'". That warning was never read before the claim
was made, because none of the places the result is normally quoted from carried
it. Registered as `SELF-PLAY CONVERGENCE UNSUPPORTED` in `Docs/corrections.md`. The entire self-play arm is four rows:

| games | seed 1001 | seed 2002 | mean |
|---|---|---|---|
| 250 | 536 | 442 | 489 |
| 500 | 541 | 469 | 505 |

`train_scaling.ps1` stopped because the +16 mean gain fell under its `-ConvergeElo 20`
rule. But the seed spread WITHIN a size is 94 Elo (250) and 72 Elo (500), so the
+16 it stopped on is 4-6x smaller than its own noise. Each point is a 4-games/pair
gauntlet at +/- 29-31, understated ~1.5x by defect 3. And no size above 500 was
ever tested. **That ladder stopped; it did not converge.** Quoting a stop-rule
artifact as a measurement is the same error as inventing a number, only harder
to spot.

The honest position: nothing in this repo or in the literature fixes the game
count for ONLINE self-play learning, which has never been run here. KnightCap
reached its gain in ~300 games, but online against varied human opponents from
a hand-tuned start, and the same work reports self-play TD-Leaf as much weaker.

So the count is not an input. Each run writes checkpoints on a game-count
ladder (`--ckpt-at`), every checkpoint is rated as its own agent in the pool,
and the learning curve is an OUTPUT of the study.

## Prior on self-play in this project

Every self-play variant tried here so far has LOST to replay data mined from
the ranked pool, by roughly 250 Elo (`train_scaling.ps1` phase 1 vs phase 2,
and `sweep_pst_v2.ps1` group C's 3-generation bootstrap chains). Those were all
offline and supervised, which is exactly what TD-Leaf changes, but the prior is
unfavourable and the study should not be designed as though it were not.

## Measured per-game cost

Measured facts only. An earlier draft of this section carried wall-clock
projections for the whole study; those were removed as noise (`CLAUDE.md`, "Do
not narrate estimates") and because the first of them was superseded by direct
measurement of the real thing.

| What | Result | How |
|---|---|---|
| TD-Leaf self-play, d6/nb200k, **PV walk included** | **0.636 s/game** | `train.exe tdleaf`, 20 games, one process |
| TD-Leaf self-play, d6/nb200k, 500-game run | **0.041 s/game** | same, 500 games (startup amortised) |
| Under 12-way parallel contention | **~1.27 s/game** | checkpoint timestamps during the cohort study |
| learned-vs-learned, d6/nb200k, no PV walk | 1.02 s/game | `rank.exe pairgen`, 6 games; cross-checked against `standings.tsv`'s 16-19 ms cpu/move x 67 plies |
| learned-vs-learned, d4/tt/ord, no PV walk | 0.061 s/game | same |

The load-bearing one is the first: **the PV walk is cheap**, which is the whole
justification for TT-probe extraction over re-searching. The earlier assumption
that it would add ~40% was itself unverified; it was not measured until the
implementation existed.

Two structural notes that do matter for planning, as opposed to estimates:

- **Rating dominates training by roughly two orders of magnitude.** Training a
  500-game cell takes 20 s; one 4-games/pair gauntlet screen of that single
  model took 7 min 58 s. Design decisions should be made about rating cost, not
  training cost.
- **The online update serialises a seed's games.** A single run cannot be
  sharded; parallelism comes from running seeds and arms concurrently.

## Success criterion

Per the standing instruction, a new agent is not done until its Elo is measured
on a full-roster anchored refit at the standard heads. The criterion is a
TD-Leaf model that beats its own initialisation at the `ab(d6,tt,ord,nb200k)@1`
head in one shared fit, with >= 6 seed replicas so the comparison clears the
50-150 Elo training-seed-noise band (theory 8). Beating the openless category
champion is the stretch goal, not the bar.

## Files

- `src/ml_tdleaf.h` / `src/ml_tdleaf.cpp` - the regime (new)
- `tools/train_main.cpp` - `tdleaf` subcommand
- `tests/test_ml.cpp` - lambda=0 / lambda=1 closed forms, PV-walk validity
- `build_train.bat`, `build_tests.bat` - link the new file
