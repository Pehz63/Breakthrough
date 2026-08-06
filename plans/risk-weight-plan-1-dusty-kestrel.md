# Risk-adjusted LearnedValue scoring (mu + k*sigma)

## Context

`todo.md` (mu/sigma-agent-types entry, tagged `[Now]`) flagged that the
position-oracle dist models (`DistModel`, two heads: mu = mean White
advantage, sigma = volatility) are trained to predict both a mean and a
deviation, but every existing search path reads mu only --
`mlValueScore`/`mlLeafScore` never consult sigma, and `mlValueScoreDist`
(which does read both) is an analysis/GUI accessor, never wired into move
choice. The developer asked to build the missing "use both" scoring path and
screen its Elo.

The two variants the todo item names are (a) sampling a value from
N(mu,sigma) at each leaf, and (b) mean +/- k*sigma leaf scoring (an
"optimistic" agent at +k, a "cautious" one at -k). This work covers (b) only:
it is a deterministic function of the existing mu/sigma outputs, fits the
existing `LearnedValue` evaluator's parameter-array pattern directly, and is
the more direct reading of "rate a position based on both its mean and
deviation." (a) introduces a new stochasticity source (a `rand()` draw per
leaf) with its own determinism/pairing implications and stays an open
`todo.md` item.

## Design

**Scoring.** `mlValueScoreRisk(turnColor, slot, riskTenths)` in
`src/ml_eval.cpp`: if `riskTenths == 0` or the slot holds no `DistModel`,
delegates to the existing `mlValueScore` (byte-identical, mu-only). Otherwise
it near-win-shortcuts, extracts features per the model's `featureVersion()`,
calls `DistModel::forwardDist` for raw `(mu, sigma)` in the model's native
output units (the same units `forward()`/`mlValueScore` use pre-tanh), forms
`mu + (riskTenths/10.0)*sigma`, and squashes it through the existing
`mlSquashToEval` helper -- identical tanh/scale/clamp behavior to every other
learned score.

**Evaluator wiring.** `LearnedValue` gains a second param, `Risk` (default 0),
and its `evalLearnedValue` fn now always calls `mlValueScoreRisk(turnColor,
p[0], p[1])` (a superset of the old `mlValueScore(turnColor, p[0])` call,
identical when `p[1]==0`). Search-path cost: the existing incremental
accumulator (`g_mlAcc`/`g_mlAccVec`) only ever tracks the mean, so a nonzero
Risk forces the full-scan leaf path -- `evalBeginSearch` skips
`mlIncrementalBegin` (calling `mlIncrementalEnd` explicitly instead) whenever
`params[1] != 0`, and `evalLeaf` falls through to `evaluateBoard` ->
`evalLearnedValue` -> `mlValueScoreRisk` on every leaf, at the same per-leaf
cost a v1 (non-incremental) model already pays.

**Units.** Risk is an integer count of TENTHS of a sigma multiple (`risk=5`
-> k=0.5), range -50..50 (k in [-5.0, 5.0]). This is a pivot from an initial
whole-integer design (range -5..5, so the smallest nonzero step was k=1.0);
the developer asked why the sweep couldn't probe values smaller than the
already-tested k=2.0, and since nothing built this session was yet committed
or rostered, the parameter was rescaled to tenths before any screening ran.

**ID grammar.** `learned(...)`'s special arg form (model slot + content hash
+ optional architecture descriptor) gains an optional trailing `risk=<tenths>`
field, always last, omitted at its default of 0 so an unrisked agent's
canonical id is unchanged. Unlike the architecture fields (recipe, mu/sigma
shape, connectivity), which `archDescForSlot` re-derives from the slot file
and are therefore tolerated as "superseded spelling" when absent from a
hand-written short id, Risk is a real functional parameter with no file to
re-derive it from, so `rankAgentId` always re-emits it verbatim from
`evalParams[1]`.

## Files

- `src/ai_eval.cpp`: `LearnedValue`'s `EvalDef` (paramCount 1 -> 2, `Risk`
  param), `evalLearnedValue`, `evalBeginSearch`'s incremental-begin gate.
- `src/ml_eval.h` / `src/ml_eval.cpp`: `mlValueScoreRisk`.
- `src/ranking.cpp`: `LBL_RISK`/`LBLN_RISK`, the emit-side `risk=` append, the
  parse-side strip-before-existing-positional-parsing block, range validation
  (-50..50), and the `learned()` grammar doc comment.
- `tests/test_ml.cpp`: `mlValueScoreRisk` direct tests (k=0 identical to
  mu-only, knob-changes-output in both directions, non-dist fallback, decided
  positions) and a `LearnedValue`+`evalBeginSearch`/`evalLeaf` integration
  test (accumulator forced off, `evalLeaf == evaluateBoard`).
- `tests/test_ranking.cpp`: `learned(...,risk=<tenths>)` round-trip (positive,
  negative, explicit zero, out-of-range rejection, `linpol()` non-application).
- `src/CLAUDE.md`, `ML.md`, `Docs/terminology.md`: mechanism description +
  units, updated in place of the "search never reads sigma" claim that this
  work makes conditionally false.

## Screening grid (confirmed with the developer before running)

Design settled over three rounds: model scope (all 10 rostered `position_elo`
cores considered, developer chose s76-only for this first pass), k
granularity (whole integers considered, developer asked for finer resolution
given k=+/-2.0 already looked broken in the Pass-1 sanity check, which
prompted the tenths pivot above), and the specific k values (an initial
"0.25/0.5/0.75/1.0" proposal was inconsistent with tenths resolution --
0.25 needs hundredths -- caught by the developer and corrected).

| Axis | Value |
|---|---|
| Core | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1` only |
| k (tenths) | -1.0, -0.5, -0.2, -0.1, 0.1, 0.2, 0.5, 1.0 (8 new agents; k=0 is the already-rostered baseline, not replayed) |
| Games/pair | 8 |
| Mechanism | `rank.exe play --cohort` into its own screening store (`ranking/matches_risk_study.jsonl`, per the project's "screening cohorts get their own store" convention), then `rank.exe rate --pin ranking/standings.tsv` |

## Verification plan

1. Pass 1 sanity before the grid: gauntlet the s76 core at risk omitted,
   `risk=2`, `risk=-2` (pre-rescale, whole-integer units) against the full
   rated pool -- confirms the mechanism runs end to end through the real
   search/game-playing path (not just unit tests) and that the knob visibly
   changes play (different game lengths against the same opponent).
2. Full test suite green before and after the tenths rescale.
3. `rank.exe check` on both the real roster and the study roster, confirming
   no existing canonical id changed and the 8 new ids are canonical.
4. Pinned screening fit per the grid above; read `standings_risk_study_pinned.tsv`.
5. Archive this plan + a companion results doc, add a theory-log entry, update
   `todo.md`, commit.
