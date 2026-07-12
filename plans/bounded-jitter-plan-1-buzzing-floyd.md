# Bounded per-position jitter (the surviving form of the noise idea)

## Context

The heuristic-eval overhaul (results:
`plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`) implemented todo.md's
noise idea as a seeded random PIECE-SQUARE table (Advanced params Noise `n` +
NoiseSeed `s`). The developer asked for the random jitter proper -- and auditing
the noise evidence while planning it exposed an over-claim in the existing docs
that this plan also corrects. The honest evidence state:

- **Established**: at chip=4 the PST noise typically exceeds a chip
  (~sqrt(pieces)*n), so it overrides material and costs -522 (n1) / -800 (n3)
  Elo. This refutes noise at material scale only; it is NOT a tie-breaker test,
  and describing those numbers as a "dominated ratios" result (as the results
  doc and theory 20 partially do) was wrong.
- **Weak / unresolved**: the chip=80 ablation config had ONLY chip and turn
  terms active; at fixed depth the turn term cancels between same-ply leaves,
  so equal-material leaves were EXACT real-eval ties and the noise (max +/-32 <
  80) could never flip a material preference -- i.e. that config already
  approximated pure tie-breaking, and it measured -271/-309. But that was a
  single climb-pool sample, inside reach of the ~200-Elo theory-19 identity
  artifact, and the "re-sorts near-equal branches" mechanism in the docs was a
  guess written as a finding. Whether random tie-breaking costs anything at all
  is OPEN.

This plan therefore does three things: implements the **bounded per-position
jitter** with the developer's tie-only-by-construction design (below), re-tests
BOTH noise forms against baseline on the proper instrument (the full main
roster), and corrects theory 20 and the results doc so claims match evidence
and hypotheses are labeled as such.

## Design

**Definition.** For a position, let `raw = sum over occupied squares of
H(seed, color, x, y)`, where `H` is a non-negative 32-bit hash (the existing
noise mixing core with a different salt so the two noise streams decorrelate).
The jitter value is `v = (int)(raw % (2*mag + 1)) - mag`, bounded in
[-mag, +mag] exactly, deterministic per (seed, position), and pseudo-randomly
re-rolled by every move (any make changes 2-3 terms of `raw`).

**Tie-only by construction (developer's design).** The jitter is NOT added at
the eval's native scale. The leaf returns

```
score = realEval * JITTER_SCALE + v        (JITTER_SCALE = 256)
```

Since `|v| <= 99` and `|v1 - v2| <= 198 < 256`, two positions whose real evals
differ by even 1 unit differ by >= 256 after scaling, so the jitter can NEVER
reverse a strict preference -- it reorders exact ties only. This replaces the
earlier relative-weights approach, which the developer correctly rejected: a
jitter "small vs chip" is still the same order of magnitude as the low-value
terms (wall 2, control 1, ...) under the climber's sum-80 normalization, and
would drown exactly the subtle signals it should only tie-break between.
Consequences:

- Scaling applies ONLY when jitter is enabled (n < 0), so no existing agent's
  behavior or identity changes.
- Win sentinels (WhiteWin/BlackWin and their decayed values) bypass the
  scaling as they already bypass everything else (early return). Overflow is
  safe: worst-case Advanced real eval is ~+/-60k; x256 is ~+/-15M, far below
  INT_MAX and the sentinel band.
- Multiplying by a positive constant preserves the real ordering exactly, so a
  jittered agent plays identically to its plain twin except at true eval ties.
  Elo neutrality on the main pool is therefore the EXPECTED result, and any
  significant drop indicates a bug.
- `n` (when negative) is no longer a strength weight, only a tie-granularity
  knob; the climber needs no change this pass (signed mode will find it
  ~neutral), and pinning it out of the climbed simplex is noted as follow-up.
- Caveat to document: aspiration windows are denominated in eval units; a
  future jittered agent with `aspN` would need a scale-aware window (no such
  agent exists in the pool).
- Cosmetic: displayed evals for a jittered agent are ~256x native scale (the
  UIs show whatever the evaluator returns); note in README.

**Exposure: the sign of the existing Noise param selects the form.**
`n > 0` = the shipped PST noise (unchanged); `n < 0` = bounded jitter with
`mag = -n`; `n = 0` = off. Rationale:

- Adding a 17th param would change the `adv` ID format and re-identify the two
  climb winners promoted yesterday (984 fresh games in `matches.jsonl`).
  Sign-as-mode keeps paramCount, letters, and every existing ID and behavior
  identical, so no codec version bump re-identifies anything.
- Precedent: `dil(rP,dN)` already uses a threshold-as-mode convention
  (`dilDepth <= 0` = random move).
- The registry range for Noise becomes -99..99 (was 0..99); the climber's
  `-AllowNegative` mode then explores BOTH noise forms natively (a sign flip on
  `n` switches form), and non-negative mode explores only PST noise.
- Honest caveat, flagged for approval: an ID like `adv(...,n-5,...)` is
  parseable TODAY (`lenientInt` ignores registry ranges) but computes garbage
  (`h % (unsigned)(-9)`) and has never been used or stored. Redefining n<0 is
  therefore behavior-changing only for never-used, semantically broken IDs, so
  the plan keeps `adv@1` rather than bumping to `@2`.

**Incremental path.** `raw` is a plain sum, so it gets its own accumulator (the
`g_mlAcc` pattern), NOT `g_evalPos` (the leaf applies a mod, so it cannot ride
the shared positional sum):

- New globals: `unsigned long long g_noiseAcc`, `bool g_noiseIncremental`,
  `int g_noiseSeed` (latched like `g_mlWeights`).
- `evalBeginSearch` (src/ai_eval.cpp): when the evaluator is Advanced, n < 0,
  and `g_evalLevel >= 3`, latch the seed, seed `g_noiseAcc` by scan, set the
  flag; `evalEndSearch` clears it.
- `simulateMove*/unsimulateMove*` (src/moves.cpp): under the flag, 2-3
  add/subtracts of `H(seed, color, x, y)` per move, mirroring the `g_mlAcc`
  hook shape exactly.
- `evalLeaf`: after nearWin/RaceWin, if n < 0 add
  `jitterValue(g_noiseAcc or scan, -n)`. The full path (`evalAdvanced`) and the
  benchmark level-1 branch compute `raw` by scan via a shared
  `noiseRawScan(seed)` helper so the paths cannot diverge.
- Gating fixes: the three `p[ADV_NOISE] != 0` guards (evalPosFull,
  evalPosLocal, posWeightsActive) become `> 0`, since jitter is not a
  `g_evalPos` term. `raw >= 0` always, so C++ `%` is safe.

## Files

- `src/globals.h` / `src/globals.cpp`: the three new globals.
- `src/ai_eval.cpp`: `noiseHashRaw` + `noiseRawScan` + `jitterValue` helpers,
  begin/end wiring, evalLeaf + evalAdvanced + level-1 branch, `> 0` guard
  changes, Noise registry `lo` -99.
- `src/moves.cpp`: 4 accumulator hooks.
- `tests/test_eval.cpp`: jitter unit tests (bounded for several seeds/boards,
  deterministic per seed, inert at n=0, differs from PST form at |n| equal);
  a second Advanced incremental walk config with n = -2 asserting
  `g_noiseAcc` == fresh scan and `evalLeaf` == `evaluateBoard` at every node;
  and the **dominance (order-preservation) walk** (developer's test design):
  walk a bounded game tree, and at every node evaluate ALL sibling children --
  the exact comparisons alpha-beta makes -- under the plain config and the
  noisy config, counting REVERSALS (plain says A > B strictly, noisy says
  A < B). Assertions:
  - jitter form (n < 0, any magnitude/seed): reversals == 0. Provable by the
    x256 construction, so any reversal is an implementation bug. This is the
    mechanical verification that the jitter is tie-only.
  - PST form at chip=4, n1: reversals > 0. Pins the old form's non-dominance
    in code as the recorded evidence behind the corrected theory-20 claim.
  - PST form at chip=80, n1: report the count (expected ~0 among material
    pairs by the x80 granularity argument), documenting why that ablation
    config was accidentally tie-like.
  Also: equal-real-eval sibling pairs must receive differing jittered scores
  for at least some seeds (the tie actually breaks, i.e. the term does
  something).
- `tools/hill_climb.ps1`: no change needed (signed mode already mutates n's
  sign); update only the header comment to mention the two noise forms.

## Runs / measurement (after tests pass)

Measurement instrument (developer-set convention, recorded this session): agent
strength comparisons run on the FULL main roster -- `rank.exe gauntlet` with the
default `ranking/roster.txt` (61 active rated agents, anchored scale, pool held
frozen) -- not the small climb pool. The climb pool is only the hill climber's
cheap fitness function. The main pool's diversity is the point: variants never
play each other, but they are compared through the same diverse frozen
opponents. Yesterday's climb-pool noise numbers stand as recorded but are
superseded by this instrument for conclusions.

1. `.\tools\run_tests.ps1 -Build` green; rebuild `rank.exe`, `train.exe`,
   `breakthrough.exe`.
2. Theory-20 retest on the main roster, 4 games/opponent (~244 games, SE ~+/-30),
   at the champion head `ab(d6,ord,nb200k)`, scale t20,c80, five configs at
   BOTH seeds 7 and 8 (two replicates to average over the theory-19 identity
   artifact; only `n`/`s` differ across configs so every other ID segment is
   shared):
   - baseline `n0` (deterministic first-found tie-breaking)
   - `n-1,s1` and `n-3,s1` (bounded jitter: memoryless, provably tie-only)
   - `n1,s1` and `n3,s1` (PST noise at c80: persistent per-square bias, also
     unable to flip material at this scale -- re-measuring yesterday's weak
     climb-pool result on the proper instrument, with dose-response)
   This is a three-way experiment: deterministic tie-break vs persistent-bias
   noise vs memoryless jitter, over the same diverse frozen opponents. Verdict
   rules: the jitter configs are EXPECTED Elo-neutral within the replicate
   spread (neutrality confirms the tie-breaker idea in this form; a significant
   drop indicates an implementation bug, since tie-only is proven by
   construction). The PST configs settle whether random tie-breaking/persistent
   bias genuinely costs Elo (the currently unresolved half of theory 20).
3. Diversity check (the idea's actual purpose): `rank.exe pairgen` the jittered
   champion-equivalent vs the plain one, and vs itself at two seeds: confirm
   games differ across seeds but are byte-identical per seed (deterministic
   diversity).
4. Optional if time allows: one speedBench ms-budget row (adv chip + n-2) --
   expected cost is 2-3 adds per make/unmake plus one mod per leaf, near zero.

## Docs + workflow

- README noise bullet: describe both forms (n>0 PST, n<0 bounded jitter).
- `src/CLAUDE.md`: ai_eval + moves entries, globals table rows.
- `Docs/benchmarking.md`: add a short "Measuring strength" section recording
  the instrument convention above (main-roster gauntlet as the default for
  strength comparisons; climb pool = climber fitness only; two-seed replicates
  against the theory-19 identity artifact; full refit for permanent ratings).
- Auto-memory (feedback type) + `Docs/Memories/` mirror: the developer's
  benchmarking preference, so future sessions default to the full rank.
- **Correct the over-claims in the existing docs** (the documentation failure
  this audit found): `Docs/theories.md` theory 20 and the noise section of
  `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md` currently present
  the chip=80 result as a refutation "even when dominated" with a guessed
  mechanism stated as a finding. Rewrite both to the evidence split in this
  plan's Context (material-scale refutation = established; tie-break-scale =
  was weak/climb-pool/artifact-ranged, now superseded by the main-roster
  retest), label the old mechanism sentence as the hypothesis it was, and
  record the corrected status. Theory 19 gains the two-seed-replicate
  mitigation as standard practice.
- Auto-memory (feedback type) + `Docs/Memories/` mirror: strong claims in
  results docs and the theory log must be tethered to the evidence that
  actually tested them, with interpretation/mechanism guesses explicitly
  labeled as hypotheses -- this correction is the worked example.
- `todo.md`: update the noise item's trailing note (bounded variant shipped +
  outcome).
- Archive this plan as `plans/bounded-jitter-plan-1-buzzing-floyd.md` with a
  companion `bounded-jitter-results-1-buzzing-floyd.md`.
- Commit (tests green first; no push).

## Verification

- Full suite green including the new n=-2 walk (accumulator == scan at every
  make/unmake) and boundedness tests.
- Console: enter a negative Noise weight for Advanced (range now -99..99) and
  confirm the prompt accepts it (the EVAL_PARAM_UNSET sentinel fix from
  yesterday already covers negative-lo prompting).
- Gauntlet numbers recorded in the results doc with the exact commands/seeds.
