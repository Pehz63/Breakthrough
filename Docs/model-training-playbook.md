# Model training playbook

**Read this in full before designing, training, or evaluating any ML model or
training regime.** Not a summary to skim: the process rules here exist because
skipping them has already produced wasted work in this project (see "Where
these rules came from" at the bottom). `CLAUDE.md` keeps only a one-line gate
pointing here; this file is the complete procedure.

This is a process document, not a systems reference. For "what code exists and
how it fits together," go to `ML.md` (architecture + extension registries) and
`src/CLAUDE.md`'s `ml_train.cpp`/`ml_tdleaf.cpp`/`ranking.cpp` entries. For
"how to add an agent to the roster and read a ranking result," go to
`Docs/ranking-workflow.md`. This file is "how to run the experiment that
produces a model worth rating in the first place," and it sits upstream of
both.

---

## The pipeline: four passes, never one

The recurring failure this document exists to prevent: build something, run
one configuration, see it produce a number, and report it as finished. That is
a smoke test, not an experiment. A model is not ready for judgment until it has
been through four distinct passes, and **the developer must hear from you at
the boundary of each one** (see "Interactivity requirements" below), not just
at the very end.

### Pass 1: Sanity

One configuration, run small. The question is "does the plumbing work," not
"is this good." Concretely:

- Does it run to completion without error.
- Does every new mechanism do what it claims. If a knob is supposed to change
  behavior, run it at two settings and confirm the output actually differs
  (the general instrument-validation rule in `CLAUDE.md` applies in full here).
  A closed-form check is worth writing when one exists: TD-Leaf's lambda=1
  reduction to outcome-supervised training was unit-tested before any real run,
  and it caught nothing wrong, but it was the reason the lambda=1 arm's later
  result (see below) was trustworthy rather than "maybe the code is just
  broken at that setting."
- Is the resulting model even loadable by the search path it will eventually be
  rated through (`mlSetModel` + a real move, not just `save()` succeeding).

Pass 1 output is a single working artifact and a short "it runs, here's what I
checked" note. It is never a strength claim, and it is never where the work
stops.

### Pass 2: Calibration

A small pilot sweep whose only job is to size Pass 3 correctly before real
compute is spent on it — not itself a source of findings. Concretely:

- **Rung-ladder sizing.** Run 8-16 random draws (fewer than Pass 3's full
  population) at 1 seed each, with a deliberately generous or open-ended
  checkpoint ladder, and watch where a reasonably good draw's performance
  actually flattens. Use that evidence to set the shared rung list Pass 3 will
  use, rather than guessing a ceiling. A guessed ceiling can bind a real
  fraction of the population without anyone noticing: GAZ's round-5 joint
  sweep fixed a 4-rung ladder (100/400/1500/4000 games) before any training
  happened, and theory 50's analysis of that data found 31 of 101 arms peaked
  exactly at the final rung — meaning for nearly a third of the population,
  there is no evidence they wouldn't have kept improving past the ladder's
  ceiling (`plans/gumbel-mcts-results-5-violet-harbor.md`).
- **Range sizing.** Check whether the pilot's best draws sit at the edge of
  any axis's eligible-value range. A best-performer hugging a boundary is a
  concrete signal to widen that axis before locking Pass 3's grid, not
  something to notice only after 100 draws are already spent. (A true optimum
  legitimately sitting at a natural boundary, like "no regularization," is
  possible too — this is a signal to look closer, not an automatic instruction
  to always widen further.)
- **Axis-completeness review.** This is a checklist step, not something the
  pilot's own data can surface: before locking Pass 3's axis list, check
  `Docs/hyperparameter-log.md` for this regime, and separately confirm no
  capability has landed since the last sweep in this regime that belongs in
  the list. GAZ's round-5 sweep dropped model architecture as an axis
  entirely because `train.exe gumbelzero` had no `--model-type` flag yet at
  the time; once it did, round 6 became a second full calibration-through-
  optimize cycle just to cover the axis round 5 couldn't
  (`plans/gumbel-mcts-arch-plan-6-silver-thistle.md`). A pilot sweep cannot
  discover an axis the code doesn't support yet — only a deliberate review
  can, which is why this step is a checklist item here rather than an output
  of the pilot's data.

Pass 2's two data-driven outputs (the rung list and any widened ranges) feed
directly into "Design the grid, then stop and show it" below, which now sits
at the Pass 2 -> Pass 3 boundary: present Pass 3's grid to the developer with
the calibration evidence behind it, not intuition alone.

### Pass 3: Broad sweep

Vary the hyperparameters and configuration choices widely enough to actually
learn the shape of the problem, not just to confirm your first guess, using
the rung list and ranges Pass 2 calibrated. Draw at random across the full
space (axes now include model size/architecture, not just training
hyperparameters — see "Model architecture guidance" below), **1 seed per
draw** by default: Pass 3's job is to learn the shape of the space, and that
comes from analyzing many draws together (see below), not from trusting any
single draw's point value. If this project's parallel node-budget and
wall-clock-budget tracks (`ranking/CHAMPION.md`) both apply to what's being
swept, measure each draw against **both** — a size/architecture choice that
fits comfortably under one track can silently blow the other (see
"Computation guidance" below).

Pass 3 is exploratory by design: expect some arms to fail, underperform, or
surprise you. That is the pass earning its keep. Then narrow: fit a decision
tree or regression model over the sweep's results
(`analysis/predict_peak_elo.py` already does this) to find hyperparameter/size
regions that are robustly good, rather than reading the sweep as a leaderboard
of single points. This is what makes 1 seed per draw safe here in a way it was
not safe in the TD-Leaf incident below — that incident's under-seeded arms
were single pairwise comparisons quoted directly as findings ("d4 vs d6 came
out -13... indistinguishable"), not pooled through a model that discounts any
one draw's noise. An effect size clearly outside this project's measured
50-150 Elo seed-noise band (theory 8) is trustworthy signal even from 1-seed
data; anything smaller needs the multi-seed validation in Pass 4 before it's
treated as real. Report the shape of the result (what moved the outcome, what
didn't, what was surprising, which regions got dropped and why), not just a
shortlist.

### Pass 4: Optimize / validate

Two things happen here, in order, for the region(s) Pass 3 shortlisted:

1. **Confirm the plateau.** Extend the rung ladder for the shortlisted
   survivors specifically — as a superset of Pass 3's shared rungs, not a
   different ladder — until their curves visibly flatten. Do this only for the
   shortlist, not the whole Pass-3 population; extending everyone's rungs
   would spend real compute re-confirming configs already known to be
   mediocre.
2. **Validate across seeds.** Train >= 3 seeds of each shortlisted
   configuration at its confirmed-good rung. This is where this document's
   seed-count minimum applies (see "Configuration design rules" below) — not
   at Pass 3's wide sweep, where the decision-tree analysis already discounts
   individual draws' noise.

Then either (a) construct a specific configuration you have a real reason to
believe is close to the best available from what's now a properly validated
shortlist, or (b) hand the search off to an automated optimizer that iterates
until it stops finding improvements. This project already has a working
example of (b): `tools/hill_climb.ps1` — stochastic hill-climbing over
evaluator weights, fitness = `rank.exe gauntlet` Elo, greedy-from-best with
simplex-step mutation, a dedupe cache, and a `-Promote` flag that appends
winners to the real roster and refits. If Pass 3 exposes a continuous or
high-dimensional space (weight mixes, schedule shapes), adapting that pattern
is very likely cheaper than hand-picking. If Pass 3 exposes a small discrete
space (a handful of architecture or regime choices), hand-selecting the
best-supported one is enough and a separate search harness is overkill — use
judgment, and say which you picked and why.

Only after Pass 4 does "done" become the right word, and even then "done"
means ready for certification (full-roster Elo measurement,
`Docs/ranking-workflow.md` Workflow B), not that the strength claim exists yet.

---

## Configuration design rules

### 1. Minimum 3 seeds per shortlisted configuration at Pass 4, trim only after seeing results

Do not certify, or treat as a finding, any configuration measured at 1 or 2
seeds. Seed-to-seed spread in this project's training runs has been measured
as large as 50-150 Elo between replicas of one recipe (the seed-noise band,
`CLAUDE.md`'s vocabulary section, theory 8) — a 1-seed or 2-seed result sits
inside noise that size and cannot support a comparison. This floor applies at
**Pass 4**, to the configurations Pass 3's shortlist actually carries forward
— Pass 3's own wide sweep runs 1 seed per draw by design (see "Pass 3: Broad
sweep" above) and draws its conclusions from a model fit across many draws,
not from any single draw's point value. If Pass 4's results come back tight
and unambiguous across a first batch of shortlisted configs, it is fine to run
fewer seeds on less central configs of a *later* study — but that reduction
happens after you have evidence the axis is low-variance, never as the
starting assumption.

Direct developer correction, 2026-07-30, after the TD-Leaf cohort study ran
several axes (lambda, learning rate, generator depth) at 1-2 seeds and quoted
their pairwise comparisons directly as findings — several had to be retracted
in the results doc's own Future Work section as "not safe to quote."
Originally this floor was written as applying to the whole broad-sweep pass;
relocated to Pass 4 on 2026-08-25 once the pipeline gained a decision-tree
narrowing step (Pass 3) whose whole purpose is to discount single-draw noise
across a wide 1-seed sweep, which the TD-Leaf study did not have.

### 2. Rungs (checkpoints) must be the same across every configuration within Pass 3

If one Pass-3 arm gets checkpoints at 100/250/500/1000/2000 games, every other
arm in that same sweep gets the same five checkpoints, not a coarser subset.
The concrete failure this prevents: in the TD-Leaf cohort study, the base
recipe was checkpointed at all five of those points and turned out to peak at
1000 games. Every *other* arm (different lambda, different init, different
learning rate, different generator depth) was only checkpointed at 500 and
2000 — so none of them were ever measured at the point that mattered. It is
impossible to say from that data whether the base recipe's advantage over the
other arms is real or is an artifact of being the only arm sampled at its own
optimum.

Mechanically: decide the rung list once per sweep, from Pass 2's calibration
evidence (see "Pass 2: Calibration" above), and pass the same list to every
training run in Pass 3 (see `train.exe tdleaf --ckpt-at` as the existing
mechanism — pass an identical `--ckpt-at` value across every arm's
invocation). Pass 4 may extend the ladder further for the shortlist alone
(see "Pass 4: Optimize / validate" above) — that is a superset applied after
Pass 3 closes, not a violation of this rule.

**Before setting a range for any axis, check `Docs/hyperparameter-log.md`** for
what's already been tried in this or another regime. A value that's already
known to be untested (never varied, always left at a default) is informative
too — it's a candidate axis for the new sweep, not a gap to silently repeat.
This check is also part of Pass 2's axis-completeness review.

### Design the grid, then stop and show it — before Pass 3 runs anything

Once Pass 2's calibration gives you a proposed grid (axes, values per axis,
rung list, resulting agent count — seed count is fixed at 1 for Pass 3 itself,
see above), **present it as its own clearly labeled section of your response,
in the chat, not only inside a plans/ document** — a table is usually the
right format: block name, what varies, rung list, resulting agent count, total
games it implies. State it plainly if the scale crosses the 2-hour/1-day
threshold (`CLAUDE.md`'s compute rule) so the developer can weigh in on scope
with real numbers rather than being surprised by a large `git status` later.

This is not a formality. The developer has direct, useful opinions about
configuration design that are easy to miss by design alone — ask directly
whether the axis list is missing something, whether the seed/rung numbers look
right, and whether the scale is what they intended before spending the compute.
Do not proceed to training until this has been shown and no objection raised
(silence after a clear presentation is enough to proceed; skipping the
presentation is not).

---

## Interactivity requirements

Also a direct developer correction (2026-07-30): results and plans that only
exist inside a written document, discovered by the developer reading it later,
are not sufficient. The developer notices things in the data that get missed
by habit or by not looking for them, and that only happens if they are shown
the data, not just handed a link to it.

Concretely, at each of the following points, say something to the developer
directly in the conversation — not just "see the doc" — and where there's a
real judgment call, use the question flow, not a rhetorical aside:

- **After Pass 1.** What you verified, what (if anything) looked off, whether
  you're confident the instrument is sound enough to spend Pass-2 compute on.
- **After Pass 2 (calibration).** What the pilot draws showed about rung
  timing and any range boundaries, and the rung list / ranges you're
  proposing for Pass 3 as a result — this is also where "Design the grid,
  then stop and show it" happens.
- **After Pass 3 (broad sweep).** The shape of the result: what moved the
  outcome and by how much, what didn't move it, anything that surprised you
  or contradicts a standing assumption in this project's theory log, and
  which region(s) the decision-tree narrowing shortlisted for Pass 4. Point
  these out even if they don't change your recommended next step — the
  developer may draw a different conclusion from the same numbers than you
  did.
- **Before Pass 4 (or before certification).** The configuration(s) you're
  about to commit more compute to, and why, stated plainly enough for the
  developer to disagree with the choice before it's spent.
- **At final results.** Do not just finish a results document and say "done."
  Walk through what you found, ask the developer what they make of the
  surprising parts, and treat the conversation as part of the deliverable, not
  optional narration around it.

## Questions to clarify with the developer

These are decision points repeatedly discovered, this session, to need
explicit sign-off rather than a default guess. Ask before assuming, using the
AskUserQuestion flow when there are genuinely distinct options:

- **Initialization**: from scratch, from an existing model, or both as
  parallel arms?
- **Update schedule** (for any online/iterative regime): strictly online, or a
  batched variant, or both?
- **Generator/self-play search depth**: match the certification head exactly,
  or explore a cheaper depth as its own axis? (Default: match the
  certification head — see "Generator depth" below — treat a mismatch as a
  deliberate, flagged choice, not a cost-saving default.)
- **New-mechanism scope for this round** (schedules, opponent pools, replay
  mixing, or anything else requiring new code): build everything before the
  next data run, or phase it — cheap/well-understood mechanisms now, bigger
  untested ones deferred until simpler additions are shown insufficient?
- **Model architecture**: linear (the established default here, see below) or
  a higher-capacity architecture, and if the latter, why this study needs it.
- **Scope when a step crosses the 1-day compute threshold**: this is the one
  case `CLAUDE.md`'s compute rule already requires you to surface — a
  projection and a choice, not a decision made for the developer.

---

## Extension points: adding a new training regime

Reference table, not a tutorial — read the named file's header comment before
writing code, since the real detail (exact signatures, gotchas) lives there
and duplicating it here would drift out of sync.

| To add | Touch |
|---|---|
| A new training regime (a new way to produce a model) | New `.cpp`/`.h` pair under `src/` (see `src/ml_tdleaf.h`/`.cpp` as the template: a pure, unit-testable core function plus a `TrainXConfig` struct plus one `trainX(cfg)` entry point), a `g_regimes[]` row in `src/ml_train.cpp`, a CLI subcommand in `tools/train_main.cpp`, and both build batches (`build_train.bat`, `build_tests.bat`) |
| A new model architecture | `src/ml_model.h`/`.cpp`: subclass `Model`, add a `g_modelTypes[]` row, wire the factory in `makeModel`/`loadModel` |
| A new value-feature layout | `src/ml_features.h`/`.cpp`: extend `mlExtractValueFeatures*`, bump `MLV_FEATURES`/`MLV2_FEATURES` |
| Tests for the new regime | `tests/test_ml.cpp` — at minimum: any closed-form/degenerate-case check the math admits (see the lambda=0/lambda=1 pattern), an end-to-end "does it run and actually move the weights" test, and a "the saved model is loadable by the real search path" test |

Model slots (`ML_SLOTS` in `src/ml_eval.h`, currently 256): a slot number is
part of an agent's canonical identity, so claim a contiguous unclaimed range
for a new study and record it in `src/CLAUDE.md`'s `slotFile()` allocation
note. Current allocation: 0-2 fixed conventions, 3-80 general sweep (76-79
reserved for position-oracle dist models), 81-99 vs-champion study, 100-125
scaling study, 126-127 trainer scratch, 128-165 TD-Leaf study. Check
`src/CLAUDE.md` for the current tail before claiming a range, since this list
will have grown.

---

## Running an experiment: the commands

This sequence runs three times per study, at different scale each time: a
small, generously-ranged pass for Pass 2's calibration; the full grid at 1
seed/draw for Pass 3; and the shortlist at >= 3 seeds/config plus extended
rungs for Pass 4. The commands are identical each time, only the draw count,
seed count, and rung list change.

1. **Train**, once per configuration in the grid. `train.exe <regime> --out
   <path> --ckpt-at "<same rung list for every arm>" --seed <arm's seed> ...`
   For an online/checkpoint-ladder regime, the ladder runs to the highest rung
   regardless of the nominal `--games`, so `--ckpt-at` alone is enough to
   specify a run.
2. **Publish** each trained checkpoint into its own model slot
   (`models/sweep/slot<N>.txt`).
3. **Get content hashes**: `rank.exe check` prints `models/sweep/slot<N>.txt =
   <hash8> (slot N)` for every slot — needed to build canonical agent IDs.
4. **Build a study roster + cohort list**: copy the real `ranking/roster.txt`,
   append `on <id>` lines for the new agents (one canonical ID per line,
   `<head>.learned(s<slot>,<hash8>,<arch>)@1`), and write the same IDs to a
   separate plain-text cohort file. **Also add the initialization model itself
   as an explicit control agent if it isn't already active in the real
   roster** — the TD-Leaf study's broad sweep (under this document's original
   3-pass numbering, "Pass 2"; the same activity is Pass 3 under the current
   4-pass numbering) nearly reported "beats its own init" without a measured
   init, because that model's roster entry was commented `off`.
   Check this before playing anything, not after.
5. **Validate**: `rank.exe check --roster <study roster>`. Non-canonical IDs
   and stale versions fail here with the fix printed.
6. **Play, without letting the driver auto-rate**:
   `tools\run_rank.ps1 -Workers <N> -NoRate play --roster <study roster>
   --cohort <cohort file> --games <per-pair count>`. **The `-NoRate` flag is
   required.** Without it, `run_rank.ps1` finishes with an unpinned `rate` that
   silently overwrites the canonical `ranking/ratings.tsv` and `standings.tsv`
   with a fit that includes the study cohort — this happened once (2026-07-29)
   and had to be repaired from git. `--cohort` is likewise required for any
   nontrivial roster; without it, a fresh `--games N` pass tops up every
   *existing* roster pair too, which on this project's store size is tens of
   thousands of games unrelated to the study.
7. **Screen**: `rank.exe rate --roster <study roster> --pin
   ranking/standings.tsv`. Full mechanism, guarantees, and the reusable-system
   design discussion: `Docs/ranking-workflow.md`. One line worth repeating
   here because it bounds what Pass 2/3/4 can conclude: **a pinned fit can
   never dethrone anything** — it's a screening instrument, and its output
   lives in `ranking/*_pinned.*`, never the canonical files. When a
   single-candidate gauntlet (`rank.exe gauntlet`) is the right instrument
   instead of a pinned group fit, use `ranking/roster_screening_pool.txt` as
   the default opponent pool — a fixed, regime-diverse, Elo-spread set of 26
   agents (+ `rand@1`), built so any new agent's screening Elo lands on an
   interpretable point of the scale regardless of its strength. It predates
   `ranking/CHAMPION.md`'s 2026-08-24 6-category restructure and still
   describes the earlier 5-opener-category taxonomy in its own header
   comment; check that header before relying on its category coverage.
8. **Certify** (after Pass 4, and only for the configuration(s) chosen to
   keep): edit the real `ranking/roster.txt`, fill contenders to >= 32
   games/pair, run a plain unpinned `rank.exe run`. This is the only step that
   can move the real standings or a champion title.

## Model architecture guidance

The established baseline in this project, as of the last full-roster fit, is a
**130-parameter linear v2 sparse piece-square model** (129 features + bias) —
every current category champion is this shape. Higher-capacity architectures
have been tried and have not beaten it: an MLP dist head was refuted on speed
without a strength gain (theory 37), nonlinear evaluators were found to
generalize worse across opening distributions than linear ones in the
diversified pool (theory 39), and MLP dist models showed a 142-Elo seed spread
in one fit, at the top of the seed-noise band (theory 40). **Default to the
linear architecture** for a new regime's first pass; treat reaching for an MLP
or larger architecture as a deliberate choice with its own stated reason, not
a default step up in "sophistication."

Model size/architecture is a **Pass-3 sweep axis** like any other hyperparameter
once a regime is past Pass 1, not a one-time upfront decision — Pass 2's
axis-completeness review exists specifically so a relevant architecture choice
doesn't get silently left out of a sweep the way GAZ round 5 left out
architecture entirely (see "Pass 2: Calibration" above). When architecture is
in the sweep, "big enough but not too big" is answered by Pass 3's
decision-tree narrowing over that axis together with the dual-compute-track
check below, not decided by eye beforehand.

## Generator/search depth

For any regime whose training signal comes from the model's own search (not
just its static eval), the generator depth should match the head the model
will be certified at, not a cheaper depth chosen to save training time.
Training cost is negligible next to rating cost in every case measured so far
in this project (roughly two orders of magnitude, TD-Leaf's pass 1: 20s to
train 500 games at d6/nb200k vs 8 minutes to gauntlet-screen the result), so a
cheaper generator buys savings on the cheap side of the pipeline while risking
a distribution mismatch with the expensive side. If a generator-depth
comparison is itself the question being studied, it is a proper Pass-3 axis
with its own seeds and rungs like any other, not a cost-saving default for
everything else.

## Computation guidance

The general rules live in `CLAUDE.md` ("Compute is cheap; do not pre-shrink an
experiment," the 2-hour/1-day rule, "do not narrate estimates") and apply here
without modification — read them there, this section only states their
consequences specific to training studies:

- Prefer extending a checkpoint ladder over truncating one. If a study's
  results are still moving at the highest rung tested, that is a reason to add
  a higher rung before drawing a conclusion, not a reason to stop at the data
  you have (see theory 46: online self-play showed a peak followed by decline
  that a short ladder would have entirely missed).
- A convergence/early-stop rule is only valid if its stop threshold is
  demonstrably larger than the seed-to-seed noise at that sample size. If that
  hasn't been checked, prefer a fixed, fully-rated rung ladder over a stopping
  rule (`Docs/corrections.md`, `SELF-PLAY CONVERGENCE UNSUPPORTED`, is the
  standing example of a stop rule that fired on noise and was quoted as a
  finding anyway).
- Do not narrate the cost of the study you're about to run. Design it, state
  the resulting scale plainly if it's large (agent count, game count implied),
  and if the 2-hour/1-day threshold is in play, follow that rule exactly — one
  projection, one decision point, nothing else.
- **When both of `ranking/CHAMPION.md`'s compute tracks apply to what's being
  swept, measure a Pass-3 draw's realized `cpu_ms_move` and node throughput
  directly, not just whichever track it happens to certify under.** A
  size/architecture choice that fits comfortably under one track can silently
  blow the other: two wide-MLP cores in `ranking/CHAMPION.md`'s 2026-08-24
  round 3 measured 2-3.3x over a 150ms wall-clock budget while running fine
  under the parallel 200k-node budget, because node and wall-clock cost don't
  scale together once per-node evaluator cost varies. Also: AB's `time=`
  budget enforcement itself has a known granularity defect for very expensive
  evaluators (`todo.md`, `src/ai_minimax.cpp`'s `budgetTripped()`), so a sweep
  cannot currently trust that flag to actually cap an expensive candidate's
  cost — measure the realized number, don't assume the budget enforces it.

## Certification gate (unconditional)

A model is not done, and must not be promoted, described as ready, or shipped,
until:

1. Its Elo has been measured by a full-roster Bradley-Terry refit at the
   project's standard heads, with enough seed replicas to clear the seed-noise
   band. Offline proxies (training loss, calibration, win-rate vs a weak
   fixed opponent) are diagnostic, never a substitute.
2. It is documented as one complete picture, in the model's own results doc
   and in `ML.md`'s registry tables: feature set + count, architecture,
   the search wrapper that turns it into an agent, how it was trained (regime
   + data source + labels), the hyperparameters actually run vs. what the code
   supports, and its certified Elo. A reader must never have to reconstruct
   what an agent is from scattered pieces.

This is unconditional and applies regardless of how promising Pass 2/3
screening numbers look — screening numbers are an input to deciding what to
certify, never a substitute for certifying it.

---

## Known pitfalls

Concrete, each one cost real time or produced a wrong number when missed.

- **`run_rank.ps1` silently rates unless told not to.** It absorbs a leading
  `play` token and always finishes with an unpinned `rate`. Always pass
  `-NoRate` when playing a study cohort. (This class of bug — a convenience
  driver with an unconditional side effect at the end — has not been ruled out
  in the *other* `tools/*.ps1` drivers; treat a new driver script's tail with
  suspicion until checked.)
- **`mlSetModel` takes ownership.** Pairing it with `delete` on the same
  pointer is a double free.
- **The test suite clobbers tracked `models/manifest.{json,md}`** (via
  `trainDistValue`'s manifest write in `test_ml.cpp`). Expect this after
  running the suite and restore with `git checkout` rather than committing it.
- **An init/reference model can be inactive in the real roster** (commented
  `off`), silently removing your control condition. Check the control is
  actually rated *before* running the rest of a study, not after.
- **Windows path/heredoc mangling.** Writing PowerShell or C++ string literals
  containing backslash-escapes (`\t`, `\r`, Windows paths) through a Python
  heredoc or a non-raw string can corrupt them into real control characters.
  Verify generated script/doc files render correctly after any such edit.
- **A distinct-games check matters even inside a screening fit.** The pinned
  fit's error bars are only honest if the underlying games are actually
  independent — check the distinct-trajectory ratio (`CLAUDE.md`'s ranking
  hygiene rule, defect 3) on any new cohort's games before trusting its `+/-`.
- **A sweep's axis list can silently omit a real axis if the code doesn't
  support it yet.** GAZ's round-5 joint sweep dropped model architecture as an
  axis entirely because `train.exe gumbelzero` had no `--model-type` flag at
  the time; a second full calibration-through-optimize cycle (round 6) was
  needed once it landed. Check the current capability surface, not just
  `Docs/hyperparameter-log.md`'s history of what's been tried, at Pass 2's
  axis-completeness review.
- **A rung ladder fixed before any data exists can bind a real fraction of the
  population.** GAZ's round-5 4-rung ladder (100/400/1500/4000 games) left 31
  of 101 arms peaking exactly at the final rung, with no evidence they
  wouldn't have kept improving further. Size the ladder from Pass 2's
  calibration evidence, not a guess.

---

## Advice the developer has given about computation (verbatim, for reference)

- "Compute is cheap; do not pre-shrink an experiment to save it... If cost
  genuinely forces a choice, say so and let the developer decide rather than
  deciding alone." (2026-07-26)
- "You talk so so much about wall clock and experiment time estimates. I don't
  care... if it's anything less than a day, then it's not worth fussing over
  at all. Don't waste your tokens making estimates all the time." (2026-07-29)
- "There has to be a more established existing convention" — on process, not
  computation specifically, but the same spirit applies: reuse this project's
  own existing patterns (`hill_climb.ps1`'s search, `--ckpt-at`'s ladder,
  `rate --pin`'s screening) rather than inventing a parallel mechanism.
  (2026-07-29)
- "Should have at least 3 seeds per configuration... Configurations shouldn't
  vary so much, rungs should be consistent... don't 1-shot the task... be more
  interactive... properly thoroughly plan the configurations and verify with
  me." (2026-07-30, the direct source of this document.)

## Where these rules came from

Every rule above traces to a specific, named incident in this project, mostly
from the TD-Leaf self-play study (`plans/tdleaf-plan-1-amber-pangolin.md`,
`plans/tdleaf-results-1-amber-pangolin.md`). Read those if you want the full
worked example: a real sanity/broad-sweep cycle (numbered Pass 1/Pass 2 in
that study, since it predates this document's 2026-08-25 four-pass revision —
its "Pass 2" is the activity now called Pass 3), a real pinned-fit screening
result, a real case of an interior optimum a short ladder would have missed,
and the `run_rank.ps1` overwrite incident this playbook's pitfalls section
warns about.

**The four-pass structure (2026-08-25).** The original three-pass version of
this document (sanity / broad sweep / optimize) put the 3-seed minimum on the
broad sweep itself. A developer-led redesign split calibration out into its
own pass and relocated the seed floor to Pass 4, directly informed by two GAZ
incidents neither the TD-Leaf study nor the original three-pass document had
surfaced: round 5's joint sweep dropped model architecture as an axis because
the trainer didn't support it yet, requiring a second full sweep (round 6)
once it did; and round 5's fixed 4-rung ladder left 31 of 101 arms peaking at
its own final rung with no plateau confirmation. Read
`plans/gumbel-mcts-results-5-violet-harbor.md` and
`plans/gumbel-mcts-arch-plan-6-silver-thistle.md` for the worked example.
