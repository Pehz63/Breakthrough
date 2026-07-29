---
name: tdleaf-online-selfplay
description: TD-Leaf shipped 2026-07-29 as the first online bootstrapped value regime; pinned-fit screening vs unpinned certification
metadata: 
  node_type: memory
  type: project
  originSessionId: 292a8b1b-8d5f-4201-91e5-512e51507f7b
  modified: 2026-07-29T21:35:59.822Z
---

Shipped 2026-07-29 (`src/ml_tdleaf.cpp`, `train.exe tdleaf`): TD-Leaf(lambda),
the project's first **online, bootstrapped** value regime. Every other regime is
supervised and offline (fixed label computed once, batch fit). Here a position's
target is the model's own eval of a later position backed up through the search,
applied at the principal-variation leaf, with weights moving during play.

Load-bearing facts:

- **lambda=1 provably reduces to outcome-supervised training on PV leaves**
  (`gOut_t = p_t - z`, telescoping), lambda=0 is one-step TD. Both unit-tested.
  So the lambda=1 arm is a free control isolating the bootstrap's contribution.
- **PV leaves come from TT probes along the played line**, never re-searching at
  decreasing depth (~6x the node budget per move at d6/nb200k). `ai_minimax.cpp`
  is deliberately untouched: a PV-collection branch in the hot recursion would
  shift us/node for every rated agent and invalidate the ranking instrument.
  Walk truncates 7% (d4) / 13% (d6); mean depth reached is reported per run.
- **Generator depth defaults to d6/nb200k, the certification head.** An earlier
  draft defaulted to d4 purely because it is 17x cheaper -- rejected: a TD-Leaf
  target IS the search's backed-up value, so a shallower generator mismatches
  the head being rated, and rating cost dominates training by ~100x anyway.
- **Game count is deliberately NOT an input.** `--ckpt-at` writes a rung ladder
  and every rung is rated as its own agent (see
  [[never-report-unverified-numbers]] for the withdrawn "converges at 500"
  claim this replaced).
- Unfavourable prior: every *offline* self-play variant here lost to ranked-pool
  replay data by ~250 Elo. Theories 42-45 track the open questions.

**Ranking infrastructure added alongside** (full write-up:
`Docs/ranking-workflow.md`, which is the doc to read, not the CLAUDE.md table
rows): `rank.exe rate --pin <tsv>` (`rankFitBTPinned`) holds listed agents at
their Elo and solves only the rest, so a cohort is measured on a stationary
scale; `rank.exe play --cohort <ids>` schedules only pairs touching a listed
agent (without it a 32/pair pass adds ~702,000 unrelated roster-vs-roster fill
games). A pinned fit **can never dethrone a champion** -- their ratings are its
inputs. Certification remains the unpinned refit. `ML_SLOTS` raised 128 -> 256;
TD-Leaf study owns slots 128..165.

Related: [[elo-comparison-hygiene]], [[strength-benchmarking-instrument]].
