---
name: never-report-unverified-numbers
description: Never state a number without its measurement; a stop-rule artifact or a round guess quoted as data is the recurring failure
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 292a8b1b-8d5f-4201-91e5-512e51507f7b
  modified: 2026-07-29T19:56:09.421Z
---

Never state a quantity unless you can name the measurement that produced it.
Two distinct failure modes, both caught in one session (2026-07-29):

1. **Invention.** Asserted "about 10,000 games/seed" as a training budget. It had
   no basis at all; it was a round number that made a cost projection look
   concrete.
2. **Laundering a weak artifact through a citation.** Claimed "single-teacher
   self-play converged at 500 games", citing `models/sweep/scaling.csv`. That
   file's entire self-play arm is 4 rows (250 and 500 games, 2 seeds each). The
   ladder stopped because a +16 mean gain fell under `train_scaling.ps1`'s
   20-Elo stop rule, while the seed spread WITHIN a size was 94 and 72 Elo, and
   no size above 500 was ever tested. It stopped; it did not converge. This one
   is more dangerous than plain invention because the citation makes it look
   checked.

**Why:** the developer's response was "You're being much too loose with this...
don't report knowledge when you don't have it." A wrong number with a decimal
point propagates into plans, docs and the theory log, and the project's whole
value is that its claims are tethered to measurements.

**How to apply:** before any figure, state how it was measured and its sample
size. Prefer bracketing two independent instruments over one precise-looking
number (e.g. the d6 game cost was confirmed by both a timed pairgen run and the
store's own per-agent cpu/move). When a prior study's conclusion is load-bearing,
open the raw artifact and check its n and its noise band before quoting it -- do
not trust the summary line. If a quantity is genuinely unknown, say it is
unknown and design the experiment to measure it, which is what the checkpoint
ladder in [[tdleaf-online-selfplay]] exists to do.

Related: [[evidence-tethered-claims]], [[no-unsolicited-time-estimates]],
[[elo-comparison-hygiene]].
