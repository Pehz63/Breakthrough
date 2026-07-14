# Residual/MLP follow-up: record Elo (primary), validate, document the agent clearly

## Context

The prior task (committed `e0d15b5`, `plans/residual-mlp-results-1-tingly-chipmunk.md`)
shipped a frozen chip-count skip (`ResidualModel`) and a hand-written MLP
(Multi-Layer Perceptron) value model, and measured theory 24 with an OFFLINE
stratified loss over `|matDiff|` buckets. Developer feedback reframes the follow-up:

- **Elo is the point and was not recorded.** The sweep ran `-NoRate`, so no new
  model has a pooled Elo. This project exists to raise Elo and do research; an
  unrated model achieves neither. **Recording Elo via a full run against the whole
  active roster is the primary deliverable of this follow-up.**
- **The MLP agent is under-documented.** No single place assembles what the model is
  (inputs, layers, width, activation, output), how the AGENT wraps it (a value model
  inside an alpha-beta search, NOT a board->move policy), how it was trained
  (supervised value regression on outcomes from the pool's replayed games, not
  self-play or imitation), the sweep's hyperparameter ranges, and the best model's
  Elo. Assemble this and leave a note making full documentation + Elo the standard
  for future agents.
- **Abbreviations:** name sweep groups descriptively -- the "linear residual-skip
  group" (F) and the "MLP capacity group" (G) -- not bare "F/G". Establish any
  abbreviation on first use.

Two other directions are already recorded in `todo.md` (done up front): the residual
skip design space, and the NNUE-style incremental MLP.

## Canonical description to make discoverable (the answer to "what is this agent?")

- **Model = `MLPModel`, a VALUE model: board -> one scalar.** Inputs = feature
  version 2, 129 binary inputs (64 White + 64 Black piece-presence, one per
  (color, square), + 1 side-to-move = +1/-1). Fully-connected. The CODE supports an
  arbitrary number of hidden layers (the `--mlp-hidden` widths list; designed for
  1-2), but EVERY model trained this session used exactly ONE hidden layer, of width
  16 or 32 (no 2-layer model exists yet -- that is part of the deferred wider/deeper
  MLP sweep). ReLU on hidden, LINEAR output. Output scalar (logit) -> `tanh * 900`,
  clamped inside the win sentinels = a white-centric board evaluation. Optionally wrapped by `ResidualModel` = a frozen
  chip-count skip (`skipW * material_diff` added to the logit, auto-calibrated to
  ~1.438 on this data) + the MLP learning the residual.
- **Agent = alpha-beta search + this value model.** e.g.
  `ab(d6,tt,ord,nb200k)@1.learned(<hash>)@1` = alpha-beta minimax at depth 6 with a
  200000-node budget (the champion's standard head; nb200k is the common budget),
  transposition table + move ordering, scoring leaves with the MLP. Part A rates the
  models at BOTH depth 4 and depth 6. It searches and evaluates; it is NOT a direct
  board->move policy.
- **Training = supervised value regression on outcomes.** `--from-data` on a
  `rank.exe extract` replay of the pool's stored match history
  (`ranking/matches.jsonl`): real games the rated pool already played, each position
  labeled by its game result (logistic loss). 8000-game sample, 6 epochs, lr 0.05,
  trained across several training-seed replicas (theory 8). Not self-play by this
  model, not imitation of a teacher's moves.

## Part A -- Record Elo (PRIMARY): full run against the whole roster

Rate the residual/MLP models against the whole active roster (63 agents) at the
standard heads, with about 6 training-seed replicas per recipe so the deltas clear
the training-seed-noise band (50-150 Elo, theory 8). The initial sweep trained only
2 seeds, so the extra seeds are trained first.

- `tools/sweep_pst_v2.ps1`: (i) add a `-Seeds` list (default about 6) so the linear
  residual-skip group (F) and MLP capacity group (G) train that many seed replicas
  per recipe instead of the hardcoded pair; (ii) add `-RateOnly` (complement of
  `-NoRate`): gate the training `foreach` on `-not $RateOnly` so the existing
  hash -> roster-append -> `rank.exe run` path rates already-trained slots. Together
  these let train and rate be separate phases (train once, rate at each head).
- Train the replicas: `sweep -Groups "F,G" -Seeds "<6 seeds>" -NoRate` (reuses the
  existing seeds' slots, trains the rest; cheap, no rating).
- Rate at BOTH standard heads (a depth ladder comparable to the pool and the
  champion), each at the common nb200k budget, by running `-RateOnly` twice with
  `-Wrapper` and `-MlpWrapper` set to the SAME head each run so the linear and MLP
  groups share it (theory 19: hold every other ID segment constant): first
  `ab(d4,tt,ord,nb200k)@1`, then `ab(d6,tt,ord,nb200k)@1`. No ad-hoc cheaper budget
  -- Elo comparability is the point. Each run appends the group IDs `on`, plays
  missing pairings across K shards, and refits Elo over the whole pool.
- The full-scan MLP at d6/nb200k over ~6 seeds is slow, so this is a long run:
  background it with `-Workers`. That per-second cost (not the per-node eval quality)
  is exactly what the deferred NNUE follow-up would remove.
- Read `ranking/ratings.tsv` and record, prominently: the **best model's Elo** and
  each recipe's mean Elo +/- the spread over its ~6 seeds; (1) residual-linear vs
  plain-linear (does the calibration + seed-stability win move Elo? theory 22
  predicts little -- the contrast is the finding), (2) the MLP capacity group vs the
  linear cells and vs the champion (does breaking the linear ceiling, theory 10,
  show strength?). The ~6 seeds per recipe give the spread to read deltas against.
- Caveat to record: the MLP is full-scan, so its Elo at a fixed node budget is a
  LOWER bound on an incremental (NNUE) version at equal wall-clock -- the motivation
  for the deferred NNUE direction.
- After: trim the sweep lines out of `ranking/roster.txt`, keeping `on` only any
  genuine keeper (e.g. a champion-tier model), per the roster curation policy. Why:
  `roster.txt` is the ACTIVE tournament set -- every `on`/`anchor` agent is scheduled
  against every other on each future `rank.exe run`, so leaving dozens of throwaway
  seed replicas `on` makes every later run pay O(N) extra games forever. Trimming
  stops only FUTURE scheduling and deletes nothing: every game played is already
  appended permanently to `ranking/matches.jsonl` (keyed by canonical agent ID), so
  the measured Elo is preserved and re-adding a line later replays no games.

## Part B -- Validation split + early stopping (trainer)

Resolves the in-sample-loss limitation and ships the todo item "Validation split +
early stopping." Default off, existing runs byte-identical.

- `src/ml_train.cpp` / `.h`: `valSplit` (0 = off) + `earlyStop` on
  `trainSupervisedValue`. Partition indices train/val by a deterministic
  `shuffleVec` + `seed`. SGD over TRAIN only; each epoch compute mean logistic loss
  over VAL and print `epoch e  loss=<train>  val=<val>`; `earlyStop` keeps the
  min-val-loss epoch. The final stratified-loss printout runs on the VAL subset when
  `valSplit > 0` (the held-out theory-24 measure).
- `tools/train_main.cpp`: `--val-split <f>` + `--early-stop` + usage lines.
- `tests/test_ml.cpp`: one light test (holdout shrinks the train set, a finite val
  loss is reported, early-stop run loads).

## Part C -- Held-out calibration re-check

Confirm the linear residual-skip finding survives on held-out data. Direct
`train.exe --val-split 0.2` runs (NOT the Part A slots) for the linear group
(plain vs `--residual-skip -1`) and one MLP width (plain vs residual), across the
same ~6 training seeds, on `data/replay_v2_residual.jsonl`; read the mean held-out
`==0` loss per recipe. Cheap (offline, no rating); models throwaway.

## Part D -- Document clearly + standing note

- `plans/residual-mlp-results-2-tingly-chipmunk.md` (companion to this plan, which is
  copied to `plans/residual-mlp-plan-2-tingly-chipmunk.md`): the canonical
  description above + the Elo table (headline) + held-out calibration + how measured.
- `ML.md`: add a short "worked example: the MLP value agent" connecting the pieces
  already in the model-types table, feature list, and agent overview into one
  inputs/layers/output/training/search/Elo picture; fix any bare group letters.
- `Docs/benchmarking.md` "Measuring strength": align it with the new CLAUDE.md
  standing rule -- a new model's strength is a FULL roster run (BT refit) at the
  standard heads (d4 and d6, nb200k) with about 6 training-seed replicas to clear the
  training-seed-noise band (theory 8), never a lone gauntlet; Elo is the primary
  agent metric; and every new agent is documented with its architecture + inputs +
  training + search wrapper + Elo. Mirror a feedback
  memory (+ `Docs/Memories/`).
- `todo.md`: mark "Validation split + early stopping" done.
- `Docs/theories.md`: theory 24 (+ Elo result + held-out calibration), theory 10 (the
  MLP Elo is the first direct strength test of breaking the linear ceiling).

## Verification

1. `.\tools\run_tests.ps1 -Build` -- all pass incl. the new val-split test.
2. Part C runs print `val=` per epoch and a held-out stratified loss; the
   residual-linear held-out `==0` loss < plain-linear's.
3. Part A: `ranking/ratings.tsv` gains the 12 model IDs with pooled Elo + SE; the
   best model's Elo is recorded in the results doc; report head-to-head + pareto
   tables cover them.
4. The docs answer "what is this agent, how trained, its Elo" in one place.
5. Commit (tests green); do NOT push.

## Out of scope (recorded in the docs, not built here)
- Residual skip design space (soft/regularized skip, broader baseline, wider/deeper
  MLP sweep) -- todo.md.
- NNUE-style incremental MLP (first-layer accumulator) for fixed-compute strength --
  todo.md.
- Low-Elo-games-as-low-quality-data study (theory 26, recorded now in
  `Docs/theories.md` + `todo.md`): retrain recipes on Elo-filtered replay data
  (exclude low-Elo / mixed / high-Elo games; Elo-weighted labels) and compare. A
  separate data-quality study, not this Elo-measurement task.
