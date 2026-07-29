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
| Game count | **Convergence-stopped doubling ladder**, not a fixed budget | See below. |

## Game count is measured, not assumed

An earlier draft of this plan asserted 10,000 games/seed. That number had no
basis and is withdrawn. The relevant measured anchors:

- `models/sweep/scaling.csv` (this repo): single-teacher self-play under the
  SUPERVISED regime converged at **500 games** (mean screening Elo 489 at 250
  -> 505 at 500, a +16 gain below the study's own +20 stop rule). The replay
  arm got *worse* from 4000 (760) to 8000 (693).
- KnightCap's original TD-Leaf result reached its gain in ~300 games, but
  online against varied human opponents from a hand-tuned start, and the same
  work reports self-play TD-Leaf as substantially weaker.

Neither transfers cleanly: the repo anchor saturates precisely because a FIXED
teacher generates a FIXED distribution, which is the assumption TD-Leaf breaks.
So the study reuses `tools/train_scaling.ps1`'s method instead of a guess -
double the game count until the mean screening Elo gain over seeds falls below
a threshold.

## Measured cost basis

`rank.exe pairgen`, learned-vs-learned, 6 games, single process, wall clock
including process startup (so d4 is if anything overstated):

| Head | s/game | Cross-check |
|---|---|---|
| `ab(d6,tt,ord,nb200k)@1` | 1.02 | `standings.tsv` gives 16-19 ms cpu/move; 67 plies x 17 ms = 1.1 s/game. Independent agreement. |
| `ab(d4,tt,ord)@1` | 0.061 | 17x cheaper |

Worst case if the ladder runs to a 4000-game cap (cumulative 7,750 games/seed):
**~11 min/seed at d4, ~3.0 h/seed at d6/nb200k**. TD-Leaf's online update
serialises a seed's games, so wall clock is set by games-per-seed; parallelism
comes from running seeds and arms concurrently.

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
