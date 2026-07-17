# Dethrone the Champion, Phase 0 (Re-certify the Throne) -- Results

Companion to `dethrone-champion-plan-1-wiggly-mitten.md`. Session date 2026-07-17.
This doc records phase 0 only (the instrument re-certification). Later phases
(quiescence, refutation book, learned-eval fixes) get their own results docs.

## Headline

**The chip-counter champion was already dethroned, and phase 0 certified it.**
After boosting the top-contender pairs to 32 games each and refitting the full
roster, the learned value model `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1`
is the number 1 target-class agent at **Elo 1064 +/- 14**, while the incumbent
chip counter `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` sits at **976 +/-
13** (+88, non-overlapping error bands) and loses the direct head-to-head
**9-23** (28%) at 32 games. The two agents share the exact same search head
(alpha-beta depth 6, transposition table, move ordering, 200k node budget), so
this also closes the Agent Track's first goal tier: **a learned evaluator now
beats the classic chip counter at equal search depth**, on the project's own
instrument. The second tier (win at lower compute) stays open: s98 costs 13.1
cpu ms/move against the chip counter's 5.0 (2.6x).

The d8/nb2m oracle remains above both (1145 +/- 21) and remains reference-class,
not the target, per the developer's standing ruling.

## What phase 0 did

1. `rank.exe check`: roster valid, 63 active agents, 67558 stored games.
2. Diagnosed why the 2026-07-14 refit no longer matched the docs' picture
   (champion "~1140" vs table rows at 969-1029).
3. Boosted the top: a temporary roster `ranking/roster_top.txt` (anchor + 11
   contenders, the d8 oracle deliberately excluded as reference-class) played
   to 32 games/pair -- 1584 new games, 8 shards, a few minutes wall clock --
   then a full-roster refit (`rank.exe rate`) as the permanent record.
4. Declared the new target and updated `todo.md`, `Docs/theories.md` (theory
   28), and `Docs/benchmarking.md` (scale-drift note).

## Finding 1: the 07-14 fit compression was two effects, one of them real

Between the 07-12 fit (commit `688eaa7`) and the 07-14 fit (after the
residual/MLP study added 72 new rated IDs), every top agent dropped 80-112 Elo:
oracle 1254 -> 1142, ord-classic 1135 -> 1029, s98 1124 -> 1045, incumbent
1062 -> 969. Two causes:

- **Prior-mass compression (artifact).** The Bradley-Terry fit adds 0.5 virtual
  games at 50% score per PLAYED pair (`src/ranking.cpp`, `rankRate`). 72 new
  IDs x ~60 opponents added ~2100 pairs of prior mass, dragging every rating
  toward its opponents' mean. The scale compresses as the pool grows even if no
  one's real results change. Consequence recorded in `Docs/benchmarking.md`:
  never compare absolute Elo across fits; read order and error bands within one
  fit.
- **Real differential losses (signal).** Marginal records against the 72 study
  IDs (the only new games between the two fits, 2/pair): oracle 133-11 (92%),
  s98 130-14 (90%), s96 and adv(c75) 127-17 (88%), ord-classic 114-30 (79%),
  incumbent tt,ord 108-36 (75%). Against the 36 **d6-head** study models alone
  the incumbent scored 41/72 (57%) and was swept 0-2 by ELEVEN of them,
  spanning every recipe class in the study (linear plain s3/s4/s6, linear
  residual s10/s12/s13, mlp16 s16/s22, mlp32 s28/s33/s35), while beating the
  d4-head cohort of the same models 94%. The learned agents at the top kept
  88-92% against the same opponents. So the order flip at the top is real
  differentiation, not an artifact: **learned piece-square evaluators as a
  class beat the chip counter head-to-head far above their pooled-Elo
  prediction** (filed as theory 28).

## Finding 2: the resolved top (32 games/pair among contenders)

Full-roster anchored refit, 2026-07-17, active agents:

| Elo | +/- | agent |
|---|---|---|
| 1145 | 21 | ab(d8,tt,ord,nb2m).classic (oracle, reference-class) |
| **1064** | 14 | **ab(d6,tt,ord,nb200k).learned(s98) -- new champion** |
| 1042 | 14 | ab(d6,tt,ord,nb200k).learned(s96) |
| 1020 | 14 | ab(d6,ord,nb200k).adv(t20,c75,...,h3,r2,g1) |
| 1020 | 13 | ab(d6,ord,nb200k).classic |
| 1011 | 14 | ab(d6,ord,nb200k).adv(t20,c77,...,d-2,b1,g1) |
| 982 | 13 | ab(d6,tt,ord,asp100,nb200k).classic |
| 979 | 13 | ab(d6,tt,ord,asp50,nb200k).classic |
| 976 | 13 | ab(d6,tt,ord,nb200k).classic (the dethroned incumbent) |
| 972 | 13 | ab(d6,tt,nb200k).classic |
| 964 | 13 | ab(d6,tt,ord,asp25,nb200k).classic |
| 956 | 13 | ab(d6,tt,ord,nb200k).learned(s99) |

Boosted head-to-heads (32 games each):

- s98 vs incumbent: **23-9**. s98 vs tt-only classic 28-4, vs asp50 22-10, vs
  asp25 18-14, vs asp100 17-15, vs s99 29-3.
- Non-transitive edges (real, worth knowing): s96 beats s98 20-12, ord-classic
  beats s98 18-14, both advs edge s98 18-14 and 17-15. The incumbent loses to
  ord-classic 14-18 and goes 16-16 with both advs.
- The declared criterion is pooled Elo on the full-roster refit ("outrating it
  outright", todo.md standing loop), and there s98 leads s96 by 22 and
  everything else by 44+.

## The new champion, documented (standing rule)

`ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1`, Elo 1064 +/- 14 (this fit).

- **Inputs:** feature v2, 129 sparse binary inputs (64 White-occupancy + 64
  Black-occupancy piece-square bits + side-to-move).
- **Architecture:** LinearModel value head (a piece-square table with logistic
  training), tanh*900 output squash, incremental `g_mlAcc` leaf path.
- **Search wrapper:** alpha-beta explorer, depth 6, transposition table + move
  ordering, 200k node budget -- identical head to the dethroned incumbent.
- **Training:** supervised value regression on game outcomes from `rank.exe
  pairgen` games of the d8/nb2m oracle vs the then-champion (symmetric 6-ply
  random openers), the "oracle" family best of the vs-champion study, promoted
  to slot 98. Full recipe and provenance:
  `plans/vs-champion-training-results-1-cozy-forest.md`.
- Its head-to-head win over the chip counter is opener-insensitive (the
  opener-bias study measured 66.2% with the champion playing its own policy;
  the clean roster head-to-head is now 72%).

## Caveats

- Deterministic-vs-deterministic pairs diverge across replays only through
  cross-game TT/killer/history state inside a shard process (theory 19
  mechanism b) -- that is why 32-game det-det head-to-heads are not 32-0 or
  16-16. The boost averages over that state distribution; treat per-pair
  records as noisy at the +/- few-games level. Pooled Elo over 66 boosted
  pairs is the robust read.
- The marginal-record analysis (finding 1) was derived by differencing the
  07-12 and 07-14 fit snapshots; exact per-pair splits were spot-confirmed via
  `rank.exe history` for the incumbent only.
- Elo numbers in this doc are NOT comparable to numbers quoted in docs written
  before 2026-07-14 (see the scale-drift note in `Docs/benchmarking.md`).
  Within-fit order and gaps are the meaningful quantities.

## Deviations from the plan

- The plan's step "restore the full roster" was unnecessary: the boost ran on a
  separate `--roster ranking/roster_top.txt`, so the main roster was never
  modified. `roster_top.txt` is kept (committed) as a reusable top-resolution
  instrument.
- The plan anticipated possibly discovering "s98 already dethroned the
  champion" -- that is exactly what happened, so phase 0's deliverable changed
  from "declare the target" to "certify the dethrone + declare s98 the new
  target" for phases 1-3.
- Two `--seed` replicates were not run for the boost: every contender pair is
  deterministic-vs-deterministic, where `--seed` changes nothing (it varies
  stochastic opponents' games only). The 32-game averaging over cross-game
  search state is the operative replication here.

## Future Work

- **Theory 28 mechanism** (tethered to finding 1): WHY do learned PSTs beat the
  chip counter head-to-head at ~300 Elo pooled-rating deficits? Labeled
  hypothesis: positional tie-breaking among material-equal lines -- the chip
  counter's eval is blind between equal-material positions, and a PST always
  has a preference. Test: replay incumbent-vs-s98 games logging the incumbent's
  root eval; count plies where its eval saw a tie among top moves and the
  learned side's choice later won material; or ablate s98 to material-only and
  watch the head-to-head collapse.
- **Theory 1 longitudinal re-check** (standing todo item): the MLP/residual
  cohort is exactly "a future batch of diverse agents joining the pool" -- run
  `tools/train_vs_champion.ps1 -AnalysisOnly` against the grown store to
  re-test the diverse-bucket residuals, now that a style class exists that
  demonstrably exploits the old champion.
- **Cycle structure at the top** (tethered to finding 2): s96 > s98 > incumbent
  > ord-classic > s98 is a rock-paper-scissors loop at 32 games/pair. A
  dedicated 100+-game round-robin among the top 6 would map it and say how
  stable the pooled order is to roster composition changes.
- **Prior-mass drift** (tethered to finding 1): if cross-fit comparability ever
  matters more than undefeated-agent regularization, consider a fixed-total
  prior (independent of pair count) in `rankFitBT`. For now the benchmarking
  note suffices.

## Ideas This Inspired

- Per-opponent-class Elo (vs random family / vs classics / vs learned / vs
  diluted) as a cheap first cut of the todo's agent behavioral classifier --
  finding 1 shows one number per agent hides exploitable structure.
- Seed-replica ensembling (already a todo item) looks stronger now: the 11
  incumbent-sweeping study models were single seeds of recipes whose OTHER
  seeds lost; averaging replicas might average away exploitability too.
- A "top-resolution" maintenance habit: after any large cohort joins the pool,
  re-run the `roster_top.txt` boost before reading the table.
- Phase 1 (quiescence) gets a sharper question: does fixing the chip counter's
  leaf-tactics horizon close its 57% gap vs learned models (tactical
  explanation), or not (positional explanation, supporting the theory-28
  hypothesis)?

## Commit

Single commit covering: new games + refit artifacts (`ranking/`), the archived
plan + this results doc (`plans/`), the roster_top instrument, `todo.md` goal
rewrite, theory 28 (`Docs/theories.md`), and the benchmarking scale-drift note.
Docs + data only, no source changes (test-suite exception applies).
