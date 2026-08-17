# Gumbel AlphaZero bot -- Slice 2, Pass 2: broad hyperparameter sweep

## Context

Slice 2 Pass 1 (shipped 2026-08-16/17, `plans/gumbel-mcts-plan-2-hidden-greeting-mist.md`
/ `...-results-2-...md`) built the Gumbel-Zero self-play trainer and verified
its mechanisms end to end, but ran only one configuration (sims=50, lr=0.01,
default replay/batch/open-plies) and measured no Elo. Per
`Docs/model-training-playbook.md`'s mandatory three-pass process, a new
regime cannot be judged, promoted, or described as strong until it clears
Pass 2 (a broad hyperparameter sweep, presented as a grid and reviewed before
running) and Pass 3 (optimize). This plan covers Pass 2.

This plan was designed interactively over the course of the session rather
than produced up front through a single planning pass: the grid below went
through a design step, a clarifying-questions step, and a developer
correction before being run, matching the playbook's "design the grid, then
stop and show it" requirement.

## Clarifying questions answered before designing the grid

The developer asked five questions about the trainer's mechanics and cost
before any grid was finalized:

1. **What is replay?** The fixed-capacity ring buffer (`GumbelZeroReplayBuffer`)
   that mixes recent self-play plies into each training step, rather than
   training only on the ply just generated. Capacity and warmup (how many
   records must accumulate before training starts) are the two knobs.
2. **What is open-plies? Does it match the 3 divisions?** The number of
   uniform-random opening plies per side the self-play generator plays before
   handing off to the trained policy, a diversity knob. It maps directly onto
   `ranking/CHAMPION.md`'s category split: 0 = openless, 4 = the 4-random
   category, 8 = the 8-random category (the two book categories don't apply,
   since Gumbel-Zero's generator uses a uniform-random opener, not a mined
   book). Grounded on this rather than arbitrary values, per the developer's
   explicit correction (see below).
3. **Do high sims/replay/batch values make a run take a very long time, and
   which axes affect per-game vs per-convergence cost?** Sims and batch size
   scale per-game wall time roughly directly (more search work or more
   backward passes per step). Replay capacity/warmup affects memory and how
   soon training starts, not per-step cost. None of these axes are free to
   raise arbitrarily.
4. **Empirical cost measurement.** Measured directly rather than estimated:
   at the Pass-1 recipe's settings, self-play + training costs single-digit
   milliseconds per game-step at low sims, rising with sims (see the Round A
   grid's cost awareness below and the later compute-matched follow-up's
   direct measurements: 11.22/18.64/40.45/78.88/150.08 ms/game at
   sims=50/100/200/400/800 under one fixed config).
5. **What parallelization exists?** This project parallelizes training
   sweeps by running independent `train.exe` processes in parallel
   (`Start-Process` fan-out, matching every other sweep driver in
   `tools/`), not by threading a single run or using a GPU. No GPU path
   exists in this codebase; the self-play loop is single-threaded CPU work
   per process.

## Grid design

### Round A: 21-draw random search

Following the established `tools/tdleaf_sample_pass2.ps1` precedent (random
search over the joint hyperparameter space, Bergstra & Bengio 2012, not
one-axis-at-a-time), `tools/gumbelzero_sample_pass2.ps1` draws 21 rows (REF +
20 random draws) over:

| Axis | Values | Grounding |
|---|---|---|
| `sims` | 25, 50, 100, 200 | Cost-aware range around the Pass-1 default (50) |
| `lr` | 0.003, 0.01, 0.03 | Log-spaced around the Pass-1 default |
| `l2` | 0.0, 0.0003, 0.001 | Untested axis in Pass 1 |
| `(replay-capacity, replay-warmup)` | (500,16), (2000,32), (8000,128) | Paired presets so warmup stays a sane fraction of capacity, not drawn independently |
| `batch-size` | 8, 32, 64 | Untested axis in Pass 1 |
| `open-plies` | 0, 4, 8 | **Revised from an initial arbitrary {2,4,8} after developer correction** ("These numbers are arbitrary right? ... look at the 5 ranking divisions. 3 of them are relevant") -- now exactly the openless/4-random/8-random categories |

REF is hardcoded to the Pass-1 sanity recipe (sims=50, lr=0.01, l2=0.0,
replay=2000/32, batch=32, open=4) as a control every draw is compared
against. `sims` is sampled once per draw and used for BOTH the self-play
generator and the certification head (`gaz(sims=N)@1`), per the playbook's
generator/search-depth-matching guidance -- since sims is itself the swept
axis, a mismatched certification budget would confound "more search budget
helped" with "the checkpoint was rated at the wrong budget."

Rated at 3 shared rungs (100/400/1500 games) via a pinned screening fit
(`rate --pin ranking/standings.tsv`) over a dedicated screening store
(`ranking/matches_screen_gz.jsonl`), never the canonical ladder, mirroring
TD-Leaf Pass 2's precedent and the incident it exists to prevent (2026-07-29,
a screening cohort flooding the permanent store).

Approved by the developer as designed (multiple-choice confirmation) before
running.

### Round B: seed replication + extended ladder

Round A's result (below, see the results doc) showed most draws still rising
at rung 1500, the exact situation the playbook warns a ladder stopping short
of the peak will understate. Reviewed with the developer; the chosen fix was
both seed-replicating the top 8 Round-A draws (by their own rung-1500 Elo)
AND extending the ladder to a 4th rung (4000), applied to every seed of every
promoted draw so the "consistent rungs" rule still holds within the round.
Each promoted draw keeps its Round-A seed (reproduces byte-identically, plus
gets a new rung-4000 checkpoint) and adds 2 new seeds (original+10000,
original+20000). 8 draws x 3 seeds x 4 rungs = 96 checkpoints, slots
723..818 as designed (actual usage ran to 882 after a mid-run bug produced
some redundant, harmlessly-duplicated training runs -- see the results doc).

Approved by the developer as designed (multiple-choice confirmation) before
running.

### Follow-up: compute-matched sims comparison

Not part of the original grid. Raised by a developer question mid-session
("Can you train on fewer sims and run it at higher sims? Does it perform
just as well?") and then corrected twice by the developer during the
investigation: first that an initial cross-serving comparison was missing a
quarter of its own 2x3 design, then that comparing checkpoints at matched
RUNG (game count) rather than matched COMPUTE is not a fair comparison,
since higher sims costs more compute per game. The corrected design: one
fixed configuration (REF's hyperparameters: lr=0.01, l2=0.0,
replay-capacity=2000, replay-warmup=32, batch=32, open-plies=4), sims as the
only varying axis (50/100/200/400/800, dropping 25 after Round A/B already
showed it unproductive), and game counts chosen per arm so that
`games x measured_ms_per_game` is held equal (~60,700 ms-equivalent) across
all 5 arms. Full results: the results doc and `Docs/theories.md` theory 48.

## Verification

Per the playbook, Pass 2 is screening only, not a strength claim:

1. Round A and Round B both rated via `rate --pin ranking/standings.tsv`
   (a pinned fit cannot dethrone anything, and existing pool ratings are
   held fixed as anchors) over the dedicated screening store, never the
   canonical ladder.
2. Report back to the developer after each round (interactivity checkpoints
   per the playbook), including unflattering findings as found: most Round-A
   draws not yet plateaued, Round-B's leader still rising in 2 of 3 seeds at
   the extended ladder's final rung, and a direct match loss to the
   simplest non-learned baseline in the codebase.
3. No promotion, no unpinned refit, and no strength claim leaves this pass.
