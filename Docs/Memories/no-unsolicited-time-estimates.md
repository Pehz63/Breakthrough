---
name: no-unsolicited-time-estimates
description: Do not narrate wall-clock/cost estimates; just run the work and only escalate if a step projects past a day
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 292a8b1b-8d5f-4201-91e5-512e51507f7b
  modified: 2026-07-29T19:55:01.260Z
---

Do not produce wall-clock or compute-cost estimates unless asked. Set the work
up and run it. Only if a step passes ~2 hours, measure its actual progress rate
and project; only if that projection exceeds **one day** raise it to the
developer to decide on reducing scope. Anything under a day is not worth
mentioning at all.

**Why:** the developer said plainly, 2026-07-29, "You talk so so much about wall
clock and experiment time estimates. I don't care... if it's anything less than
a day, then it's not worth fussing over at all. Don't waste your tokens making
estimates all the time." This is a **cross-session pattern**, not a one-off, and
they explicitly asked for it to be fixed durably. Estimate tables also crowd out
the substance of a reply and invite exactly the kind of unmeasured number that
[[never-report-unverified-numbers]] warns about.

**How to apply:** no cost tables, no "this will take ~N hours" asides, no
pre-shrinking an experiment on cost grounds (that also violates the project's
standing "compute is cheap" rule). Launch it, report when it lands. If a number
is genuinely load-bearing for a decision the developer must make, give the one
number and the measurement behind it, not a projection matrix.

Related: [[never-report-unverified-numbers]], [[strength-benchmarking-instrument]].
