---
name: ml-training-process-discipline
description: "Never one-shot a model training study; 4-pass process (sanity/calibration/broad sweep/optimize), min 3 seeds at the optimize pass only, consistent Pass-3 rungs sized from a Pass-2 pilot, present the config grid, be interactive about findings"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 292a8b1b-8d5f-4201-91e5-512e51507f7b
  modified: 2026-08-25T22:16:21.040Z
---

Full procedure lives in the project doc `Docs/model-training-playbook.md`
(read in full before any ML training work in this repo) -- this memory is the
durable cross-session reminder that it exists and why.

**The rule.** Never treat a single working configuration as a finished study.
Four passes, always:

1. **Sanity** -- one config, verify the plumbing and the instrument actually
   measure what they claim.
2. **Calibration** -- a small pilot sweep (8-16 draws, 1 seed each, generous
   rung ladder) whose only job is to size Pass 3: where does a decent config's
   performance actually plateau (sets Pass 3's shared rung list), do the best
   pilot draws hug the edge of any axis's range (signal to widen it), and a
   checklist review of `Docs/hyperparameter-log.md` plus the current code
   surface for any axis that belongs in the sweep but isn't in the list yet.
   This is also where "present the grid before running it" now happens, since
   the grid it's presenting is Pass 3's, calibrated by this pass's evidence.
3. **Broad sweep** -- vary hyperparameters (and model size/architecture)
   widely at random, **1 seed per draw**, the SAME checkpoint/rung list across
   every draw, no exceptions. Then narrow with a decision tree / regression
   over the results (`analysis/predict_peak_elo.py`) to find robust regions
   rather than trusting any single draw's point value -- this is what makes 1
   seed safe here. If this project's parallel node-budget/wall-clock-budget
   tracks (`ranking/CHAMPION.md`) both apply, measure every draw against both;
   a size/architecture choice that fits one track can silently blow the other.
4. **Optimize / validate** -- for the shortlist only: extend rungs until they
   visibly plateau, THEN train >= 3 seeds each (**the 3-seed floor lives
   here, not at Pass 3**) before treating a result as real. Then either build
   a specific best config from the validated shortlist or hand off to an
   automated search (`tools/hill_climb.ps1`) that runs until it stops
   improving.

Two more standing requirements layered on top: present the Pass-3 configuration
grid to the developer as its OWN clearly labeled thing before running it (not
buried in a plans document), and be interactive at every pass boundary and at
final results -- surface findings, invite pushback, don't just hand over a
written doc and call it done.

**Why the 3-seed floor sits at Pass 4, not Pass 3 (revised 2026-08-25):**
Pass 3's 1-seed draws are safe specifically because a decision tree pools many
of them to find a region, not a single point -- unlike the TD-Leaf incident
below, where isolated 1-2 seed pairwise comparisons were quoted directly as
findings and had to be retracted. Effect sizes clearly outside the 50-150 Elo
seed-noise band (theory 8) are trustworthy even at 1 seed (concrete example
from this project: l2>0 rating ~800 vs l2=0 rating ~900 in a GAZ sweep);
anything smaller needs Pass 4's seed replication before it's treated as real.

**Why Calibration is its own pass:** two real GAZ incidents the original
3-pass version of this rule never accounted for. (a) Round 5's joint sweep
dropped model architecture as an axis entirely because the trainer had no
`--model-type` flag yet -- a second full sweep (round 6) was needed once it
landed, i.e. the broad sweep got silently repeated because it wasn't actually
broad enough. (b) Round 5 fixed a 4-rung ladder (100/400/1500/4000 games)
before any training happened, and 31 of 101 arms peaked exactly at the final
rung -- no evidence they wouldn't have kept improving. A calibration pilot
sizes the rung ladder from evidence and forces an axis-completeness review
before the expensive sweep locks in, though it can't by itself discover an
axis the code doesn't support yet -- that's still a manual checklist item.

**Why the process exists at all:** developer correction, 2026-07-30, after a
TD-Leaf self-play cohort study ran with 1-2 seeds on most arms and an
inconsistent rung ladder (one arm got 5 checkpoints, others got 2, so the arms
that skipped the game count that turned out to matter most could never be
fairly compared). Direct quotes: "you try to 1-shot the task by guessing a
configuration and running it and saying 'ok done'... I don't want passing
work, I want a configuration that will make it shine and do its best," and "Be
more interactive with me... I tend to notice many things you don't."

**How to apply:** any time a training run, hyperparameter sweep, or new agent
config is in scope in this repo, read the playbook first, not just this
summary. [[tdleaf-online-selfplay]] is the worked-example incident the process
came from; the GAZ round-5/round-6 studies
(`plans/gumbel-mcts-results-5-violet-harbor.md`,
`plans/gumbel-mcts-arch-plan-6-silver-thistle.md`) are the worked example for
why Calibration became its own pass.
