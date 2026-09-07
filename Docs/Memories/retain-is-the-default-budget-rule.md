---
name: retain-is-the-default-budget-rule
description: retain replaces the plain budget rule on every budgeted head; the plain condition is retired from studies
metadata:
  type: project
---

As of 2026-09-06 the `retain` carry-over flag is the only budget rule carried in
Breakthrough budget studies. The plain (non-retain) condition is retired, not
merely deprioritised: `tools/make_time_ladder_roster.py` generates `retain` cells
only, and the 20 plain cells of the wall-clock ladder's first pass are benched
`off` in `ranking/q7/roster_timeladder.txt` rather than deleted.

**Why:** measured over 20 cells, a plain `time=` head realizes 0.432 of its flag
(sd 0.053) against `retain`'s 0.854 (sd 0.028), so the flag does not describe the
spend. Elo pools to +52.5+-7.2 at `nodes=100k` (free, 0.85x-1.01x CPU against
`rem=0`) and +67.3+-6.5 at `time=25ms`, declining as the budget grows. Theory 70.

**How to apply:** do not add a plain arm to a new budget study, and do not quote
a plain-vs-retain gap as free strength on the TIME track, where `retain` roughly
doubles the spend and the matched-CPU contrast pools to +2.8+-3.3, a wash. There
the value is that the flag becomes honest, and replacing a plain flag with a
`retain` one at equal compute means roughly halving the flag. Heterogeneity is
large (I^2 = 93% at 100k), so expect a new core's gain to track its own depth
slope rather than the pooled mean. See [[elo-comparison-hygiene]] and
[[never-report-unverified-numbers]].

Two things NOT settled by this. The standing 228-agent roster still runs plain
heads, and migrating it mints a new identity per agent, so it needs games from
scratch and a re-certification of every `ranking/CHAMPION.md` category. And the
live wall-clock track is broken for a second, unrelated reason: its 43 `deep=6`
`time=150ms` agents realize a mean 20.5 ms/move because the depth cap binds long
before the clock. `retain` does not fix that.
