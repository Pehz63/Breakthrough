# Gumbel AlphaZero bot -- Slice 2: search-shape knobs (cvisit/cscale/m) -- Results

Companion to `plans/gumbel-mcts-plan-4-copper-lantern.md`.

## Summary

1. `ai_gumbel.cpp`'s three search-shape constants (`kGumbelCVisit`,
   `kGumbelCScale`, `kGumbelM`) are now per-agent roster knobs:
   `gaz(sims=N[,cvisit=C][,cscale=S][,m=M])@1`.
2. A 5-round screening sweep on one already-trained checkpoint found the
   paper's own defaults (50, 1.0, 16) are not the strength optimum for this
   weak, undertrained network: `cvisit` in [500,1000], `cscale` in [5.0,10.0],
   `m` in [8,16] beats the defaults by roughly 85-100 Elo, with the top of
   that range forming a broad, statistically flat plateau rather than one
   sharp point (theory 49, `Docs/theories.md`). Recommended default:
   `gaz(sims=200,cvisit=500,cscale=50)@1`.

## Part 1: implementation

### Design chosen

New global state, mirroring `AlphaBeta`'s own per-search toggle convention
exactly:

- `g_gumbelCVisit` (double, default 50.0), `g_gumbelCScale` (double, default
  1.0), `g_gumbelRootM` (int, default 16) -- declared in `globals.h`,
  defined in `globals.cpp`, right after `g_aspirationWindow`.
- `ai_gumbel.cpp`'s four internal read sites (`gumbelSimulate`'s non-root
  selection, `gumbelSearch`'s round-end Sequential-Halving cut, its root
  breadth clamp, and `gumbelImprovedPolicy`) now read these globals instead
  of the former `static const` file-scope constants. `kGumbelMaxSimPlies`
  (the per-simulation ply safety guard) was deliberately left hardcoded --
  it is a headroom guard, not a search-shape lever worth sweeping.
- New `AgentSpec` fields: `gumbelCVisit` (int), `gumbelCScaleTenths` (int,
  tenths of `c_scale`, matching `learned()`'s existing `risk=<tenths>`
  convention rather than introducing float parsing into the ID grammar),
  `gumbelRootM` (int). Defaulted in `seedAgentDefaults` to 50/10/16.
- `agentChooseMove` (`agents.cpp`) saves/sets/restores the three globals
  from these fields in the SAME unconditional block that already handles
  `g_useTT`/`g_useMoveOrder`/etc, so one tournament can freely mix agents
  with different search shapes.
- `ranking.cpp`: new single-spelling label tables (`LBL_CVISIT`,
  `LBL_CSCALE`, `LBL_ROOTM`, no legacy aliases), `gaz`'s parsing branch
  relaxed from a fixed 1-argument arity to a loop over `args[1..]` mirroring
  `ab(deep=K,...)`'s exact structure (duplicate-flag guards, an
  unknown-flag catch-all), and `rankAgentId`'s `gaz` branch appends
  `cvisit=`/`cscale=`/`m=` only when non-default.

### Why no signature changes anywhere

Both `gumbelSearch` and `gumbelExplore` (the registered `ExplorerDef`
wrapper, whose signature is shared with `Greedy`/`AlphaBeta` and could not
grow new parameters without touching all three) keep their exact original
signatures. `gumbelSimulate`'s non-root read happens deep inside recursion;
threading two new parameters through every stack frame would have touched
far more code than reading a global. This also means every EXISTING direct
caller of `gumbelSearch` -- `ml_gumbelzero.cpp`'s self-play trainer,
`tests/test_gumbel.cpp`'s positional-argument test -- needed zero changes
and behaves byte-identically (the globals default to exactly what was
previously hardcoded).

### Scope boundary

Serving-time only. `ml_gumbelzero.cpp` calls `gumbelSearch` directly,
bypassing `agentChooseMove`, so self-play training still runs at the paper
defaults regardless of what any roster agent's `cvisit=`/`cscale=`/`m=`
says. Exposing these as TRAINING hyperparameters (reopening Pass 2's grid
with 2-3 more axes) was explicitly left out of scope, a separate decision
if the developer wants it later.

### No version bump

`gaz`'s row in `g_rkExplorers` stayed at version 1. An id with none of the
new flags present is unaffected (defaults reproduce prior behavior exactly),
mirroring how `ab()`'s own optional flags (`tt`, `ord`, `qs`, `margin=`,
`nodes=`, ...) accumulated over the project's history without ever bumping
`ab`'s version.

### Tests added

- `tests/test_gumbel.cpp`: a knob-validation test proving `g_gumbelRootM`
  actually narrows the root candidate set (`m=1` visits <=1 candidate vs
  `m=16` visiting several more, same board, same sim budget) -- the
  "instrument actually does something" check this project's Standing
  Instructions require before trusting a lever. A second test proves
  `agentChooseMove` correctly threads a gaz agent's knobs into the globals
  during the call and restores the prior values afterward (sentinel-value
  check).
- `tests/test_ranking.cpp`: an ID round-trip battery for the grown `gaz(...)`
  grammar -- defaults elided from the canonical form, each flag
  independently non-default, flags in any parse order (with the direct
  `rankAgentFromId` check, not `parseOk`'s canonical-string comparison,
  since a reordered input parses to the SAME agent without itself being
  canonical), duplicate-flag rejection, unknown-flag rejection, and an
  out-of-range `m=0` rejection.

### A defect this exposed and fixed before it shipped

The first version of the `agentChooseMove` restore-hygiene test set the
three globals to sentinel values (999/999/999), asserted they survived the
call, and returned WITHOUT resetting them to the real defaults. Since these
are process-wide globals and Catch2 runs every `TEST_CASE` in one process,
this silently corrupted `tests/test_gumbelzero.cpp`'s
`gumbelImprovedPolicy` reference test (which assumes the paper defaults
apply) whenever it happened to run afterward -- caught by the full suite
run (`got[i] == Approx(0.0) `, expected `~0.0005810841`), fixed by
resetting the globals to 50.0/1.0/16 explicitly before the new test
returns, with a comment explaining why (leave shared global state as you
found it, not as a sentinel).

## Part 2: identifying good values

### Round 1 -- random search over all 3 axes (free)

8 random draws (curated value sets, not continuous ranges, matching Pass
2's own precedent) + REF (paper defaults), against a `rand@1` anchor, 24
games/pair, self-contained round robin (`ranking/matches_gaz_knobs.jsonl`,
never the canonical ladder). Fixed: checkpoint slot746 (Pass 2 Round B's
R17, seed 2117, rung 4000 -- the strongest Gumbel-Zero checkpoint on disk),
`sims=200` (Pass 2's found productive range).

| cvisit | cscale | m | Elo | +/- |
|---|---|---|---|---|
| 200 | 3.0 | 8 | 1057 | 30 |
| 50 (REF) | 1.0 | 16 | 990 | 28 |
| 150 | 0.5 | 32 | 990 | 28 |
| 25 | 2.0 | 32 | 963 | 28 |
| 200 | 0.5 | 4 | 925 | 27 |
| 25 | 0.5 | 4 | 850 | 27 |
| 100 | 1.0 | 2 | 677 | 30 |
| 10 | 0.5 | 2 | 673 | 30 |
| 10 | 3.0 | 2 | 626 | 32 |
| rand (anchor) | -- | -- | 0 | -- |

**Finding: `m` dominates.** Every `m<=2` config (677, 673, 626) sits well
below every `m>=4` config (850-1057), regardless of how different their
`cvisit`/`cscale` values are (10-100 for `cvisit`, 0.5-3.0 for `cscale`, all
tried at `m=2`). This rules out `cvisit`/`cscale` as the explanation for
`m=2`'s weakness -- it is `m` itself. The top and 2nd/3rd rows (`m` in
{8,16,32}) cannot be cleanly separated from `cvisit`/`cscale` here, since
each `m` value in that band was tried at only one or two `cvisit`/`cscale`
combinations -- the exact single-seed-random-search confound this project
already learned to check for with lr/sims in Pass 2.

### Round 2 -- isolating cvisit/cscale, m held fixed at 16

A controlled 3x3 factorial (`cvisit` in {10, 50, 200} x `cscale` in {0.5,
1.0, 3.0}), `m=16` fixed throughout so nothing here can be confounded with
`m`. 24 games/pair, `ranking/matches_gaz_knobs_m16.jsonl`.

| cvisit \\ cscale | 0.5 | 1.0 | 3.0 |
|---|---|---|---|
| 10 | 687 | 768 | 794 |
| 50 (REF) | 810 | 800 | 800 |
| 200 | 813 | 862 | 896 |

(+/- 25-27 on every cell; `rand` anchor at 0, omitted from the grid.)

**Finding: both axes rise, cleanly, past the paper defaults.** `cvisit=200`
beats `cvisit=10` at every matched `cscale` (813 vs 687, 862 vs 768, 896 vs
794). `cscale=3.0` beats `cscale=0.5` at every matched `cvisit` except the
REF row, where all three cscale values landed within noise of each other
(810/800/800). The top-right corner (200, 3.0 -> 896) beats REF (800) by
~96 Elo, with no confound possible since `m` never varied in this round.

### Round 3 -- pushing further along the winning diagonal

Both axes still favored their tested maximum, so the range was extended:
`(cvisit, cscale)` in {(50,1.0)=REF, (200,3.0), (500,5.0), (1000,10.0),
(2000,15.0)}, `m=16` fixed, 32 games/pair,
`ranking/matches_gaz_knobs_push.jsonl`.

| cvisit | cscale | Elo | +/- |
|---|---|---|---|
| 500 | 5.0 | 903 | 31 |
| 2000 | 15.0 | 851 | 31 |
| 1000 | 10.0 | 847 | 31 |
| 200 | 3.0 | 830 | 31 |
| 50 (REF) | 1.0 | 804 | 31 |

**Finding: an interior peak, near `cvisit=500, cscale=5.0`.** Elo rises
monotonically from REF through 500/5.0 (804 -> 830 -> 903), then is flat to
slightly down at 1000/10.0 and 2000/15.0 (847, 851) -- both still comfortably
inside 903's error band on the low side, so the exact peak location is not
pinned down tighter than "somewhere near 10x the paper defaults," but it is
clearly not still rising at 20-40x.

### Round 4 -- re-isolating m at the new best corner

Round 1 only isolated `m` cleanly at REF's own `cvisit`/`cscale` (all
`m<=2` samples happened to also vary `cvisit`/`cscale`, and the `m>=4`
comparison was confounded as noted above). This round holds `cvisit=500,
cscale=5.0` fixed (Round 3's peak) and varies only `m` in {4, 8, 16, 32},
32 games/pair, `ranking/matches_gaz_knobs_m_at_best.jsonl`.

| m | Elo | +/- |
|---|---|---|
| 16 | 892 | 36 |
| 8 | 881 | 36 |
| 32 | 822 | 35 |
| 4 | 800 | 36 |

**Finding: `m=16` (the paper default) and `m=8` are statistically tied as
best even at this new corner** (892 vs 881, well inside each other's error
bars). `m=32` shows no further benefit (822, borderline behind). `m=4` is
measurably worse (800, close to 2 error bars below 892) -- consistent with
Round 1's "narrow m hurts" finding, just less extreme than `m=2`'s 300+ Elo
cost.

### Round 5 -- final head-to-head of the leading candidates, one shared fit

Rounds 1-4 each fit their own pool, so their Elo numbers are not directly
comparable to each other (only order and error bands transfer). To answer
"what's the single best config" directly, 4 reasoned picks plus REF plus
`rand@1` were rated together in ONE round robin: the Round 3 peak at both
tied-best `m` values, Round 3's runner-up re-checked fresh, and Round 2's
cheaper/less-extreme strong point. Same checkpoint (slot746), `sims=200`, 32
games/pair, `ranking/matches_gaz_knobs_final4.jsonl`.

| rank | cvisit | cscale | m | Elo | +/- |
|---|---|---|---|---|---|
| 1 | 1000 | 10.0 | 16 | 893 | 31 |
| 2 | 500 | 5.0 | 16 | 879 | 31 |
| 3 | 500 | 5.0 | 8 | 875 | 31 |
| 4 | 200 | 3.0 | 16 | 806 | 31 |
| 5 (REF) | 50 | 1.0 | 16 | 793 | 31 |

**Finding: the top of the landscape is a broad plateau, not a single point.**
The top 3 (893, 879, 875) all sit within one error band of each other --
statistically indistinguishable in this fit. `cvisit=200,cscale=3.0` beat REF
by only +13 here (806 vs 793), inside the error bars and weaker than the
+26 to +96 Round 2/3 found for the same point in their own separate fits --
read as fit-to-fit noise (different opponent pool each round), not a
contradiction, since this project's own hygiene rule is to never compare
absolute Elo across fits.

**Practical recommendation: `gaz(sims=200,cvisit=500,cscale=50)@1`** (`m=16`,
the default) -- tied for best in this head-to-head and needs no `m=` flag.
`cvisit=1000,cscale=100` numerically edged it out (893 vs 879) but the 14-Elo
gap is well inside both points' +/-31 error bars, so this is not a basis to
prefer it over the simpler config.

### Combined picture

- **`m`**: needs to be at least ~8. `m<=2` costs 300+ Elo outright,
  regardless of `cvisit`/`cscale` (Round 1, non-confounded). `m=4` still
  costs ~90-100 Elo even at the best `cvisit`/`cscale` corner (Round 4).
  `m=8` through `m=16` are the sweet spot; `m=32` shows no further gain.
- **`cvisit`/`cscale`**: both independently rise well past the paper
  defaults (Round 2, `m` held fixed, non-confounded), peaking near
  `cvisit=500, cscale=5.0` -- about 10x the paper's own values -- with no
  further gain out to 20-40x (Round 3).
- **Best config found**: `gaz(sims=200,cvisit=500,cscale=50)@1` (`m=16`, or
  `m=8`, or `cvisit=1000,cscale=100` -- all four statistically tied per
  Round 5's shared-fit head-to-head below) on slot746, roughly +85 to +100
  Elo over `gaz(sims=200)@1` (the paper-default id) in a controlled,
  self-contained comparison.

Total: 3,440 games across the 5 rounds, all played in well under a minute
of wall clock per round (linear-model checkpoint, `sims=200` search),
against `rand@1` as the sole external anchor -- these Elo numbers are
internal to their own cohort/fit and not comparable to `ranking/standings.tsv`,
Pass 2's own gauntlet numbers, or each other's round (Round 5 exists
specifically to give the leading candidates one shared fit).

## Correctness notes

- All four rosters/stores are scratch (`ranking/roster_gaz_knobs*.txt`,
  `ranking/matches_gaz_knobs*.jsonl`), gitignored following the established
  one-off-store pattern, never touching the canonical roster or ladder.
- `rank.exe check` confirmed all 4 rosters (10, 9, 5, and 4 active agents
  respectively) validate cleanly before playing: every id's model hash
  matched slot746's actual file content, and every canonical round trip
  held.
- Extreme `cvisit`/`cscale` values (up to 2000 / 15.0) do not cause any
  numerical issue: `gumbelSigma`'s output saturates the softmax so the
  search effectively becomes greedy on its own backed-up value once any
  visits exist, a legitimate (if extreme) point in the design space, not an
  artifact.

## Future Work

- **Generality across checkpoints.** Everything above used ONE checkpoint
  (slot746). Whether `cvisit~500, cscale~5.0` is a property of this
  specific undertrained network (e.g. compensating for a weak, noisy
  policy prior by trusting backed-up values more) or a general Gumbel-Zero
  finding is untested. Would directly settle by re-running Round 2-4's
  design against a different Pass-2 checkpoint (e.g. REF's own rung-4000
  checkpoint, or a `model_games`/`teacher_games` regime checkpoint if one
  is ever trained with a joint model).
- **Interaction with `sims`.** Everything above held `sims=200` fixed. It
  is untested whether the `cvisit`/`cscale` optimum shifts at a different
  sims budget (e.g. does a wider sim budget need LESS aggressive
  `cvisit`/`cscale`, since more simulations already give completedQ more
  chances to be accurate?).
- **The lr/sims confound from Pass 2** is still open (a separate,
  training-time question, not touched by this serving-time investigation).
- Whether this ~100 Elo gain, even if it generalizes, is enough to change
  the 32-0 loss to the plain chip counter (`ab(deep=6)@1.classic(chip=100)@2`)
  from Pass 2 is untested -- a 32-game direct match at the new best config
  would settle it directly and is cheap.

## Ideas This Inspired

- If `cvisit`/`cscale` genuinely compensate for prior weakness, the optimal
  value might DECREASE over training as the policy head improves -- an
  annealing schedule (start high, anneal toward the paper defaults as
  training progresses) rather than a fixed per-agent constant, worth a note
  if this regime is ever taken further.
- The same "hold everything but one axis fixed, walk the boundary" method
  used in Rounds 2-4 here is a cheap, general way to characterize marginal
  effects without waiting for a full random-search grid -- worth reusing
  the next time a small number of axes need characterizing (as opposed to
  training hyperparameters, where retraining cost makes a full grid the
  more efficient design).
