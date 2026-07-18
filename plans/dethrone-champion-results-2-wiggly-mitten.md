# Dethrone the Champion, Phase 1 (Quiescence at the d6 head) -- Results

Companion to `dethrone-champion-plan-1-wiggly-mitten.md` (phase 1) and sequel to
`dethrone-champion-results-1-wiggly-mitten.md` (phase 0). Session date 2026-07-17.

## Headline

**Quiescence does not dethrone s98. The champion is unchanged.** At the
certification standard (32 games/pair among contenders, full-roster anchored
refit), the quiescence variant of the champion ties it in pooled Elo (1073 +/-
15 vs 1074 +/- 14) while losing the direct head-to-head 9-23, and the
quiescence variant of the chip counter gains only +19 (1002 +/- 14 vs 983 +/-
12, inside ~1 SE). The most valuable phase 1 result is negative knowledge:
the chip counter's weakness to learned evaluators is NOT a leaf-tactics
horizon artifact -- adding quiescence made its learned-model head-to-heads
WORSE (5-27 vs s98, 4-28 vs s96, from 9-23 and 14-18) -- which strengthens
theory 28's positional tie-breaking hypothesis (its discriminating test (c)
was exactly this probe). Filed as theory 29 (refuted at the champion head /
weak positive for the chip counter).

## What was built (the `qs` flag)

An opt-in captures-only stand-pat extension at true depth leaves:

- `g_useQuiescence` global + `AgentSpec::useQuiescence` (default off; saved/
  restored by `agentChooseMove` like the other feature toggles).
- `quiesceMax`/`quiesceMin` in `src/ai_minimax.cpp`, entered from the depth
  leaves of BOTH search paths (plain and ordered/TT). Stand-pat = static
  `evalLeaf`; only capture moves are extended; termination is inherent
  (captures strictly shrink material, A7) with a `QS_MAX_PLY = 32` cap;
  quiescence nodes count against the node budget (budget-cut inside qsearch
  sets `s_budgetHit` like any interior cut); qnode-entry `canWin*` checks and
  the win-decay keep the plain leaf's sentinel semantics. Budget-cut
  pseudo-leaves do NOT extend (no budget left to spend).
- ID codec: a `qs` flag on the `ab(...)` head (canonical position after
  `ord`), e.g. `ab(d6,tt,ord,qs,nb200k)@1.classic(t1,c4,w0,l0)@2`. No version
  bump: absent-flag agents are byte-identical.
- Tests (`754 assertions / 80 cases` pass): a horizon-fix value test (a
  defended-capture exchange reads +4 at plain depth 1 and 0 with qs -- the
  recapture gets resolved), a search-path invariance test (tt/ord preserve
  the exact search value with qs on), and codec round-trip + duplicate-flag
  rejection.

### How to test

- `.\tools\run_tests.ps1 -Build`
- Roster IDs `ab(d6,tt,ord,qs,nb200k)@1.classic(t1,c4,w0,l0)@2` and
  `ab(d6,tt,ord,qs,nb200k)@1.learned(s98,5801570e)@1` validate via
  `rank.exe check` and play normally (both are rostered `on`).

## Measurement (full-roster refit, 2026-07-17, all contender pairs at 32 games)

| Elo | +/- | cpu ms/mv | nodes/mv | agent |
|---|---|---|---|---|
| 1187 | 21 | 147.9 | 476818 | oracle d8/nb2m (reference) |
| **1074** | 14 | 12.7 | 43157 | **s98 (champion, retained)** |
| 1073 | 15 | 11.9 | 44133 | s98 + qs |
| 1061 | 14 | 12.5 | 42997 | s96 |
| 1023 | 13 | 20.4 | 96885 | adv(c75,h3,r2) |
| 1022 | 13 | 10.6 | 71315 | classic d6 ord |
| 1014 | 13 | 35.9 | 110833 | adv(c77,d-2,b1) |
| 1002 | 14 | 8.1 | 28008 | classic d6 tt,ord + qs |
| 983 | 12 | 6.1 | 20004 | classic d6 tt,ord (the phase-0 dethroned incumbent) |

Head-to-heads at 32 games each:

- s98 beats s98+qs **23-9** (identical eval, qs the only difference).
- s98+qs vs the learned cohort: loses to s96 10-22 and s99 12-20; even with
  ord-classic 16-16; beats classic+qs 20-12. Oracle beats it 29-3.
- classic+qs vs learned: **5-27 vs s98, 4-28 vs s96** (plain classic was 9-23
  and 14-18) -- quiescence made the chip counter MORE beatable by learned
  PSTs while helping it against its own family (23-9 vs tt-only classic and
  vs asp100).
- Oracle sweeps plain s98 **32-0**.

Costs: at the classic head qs added ~40% nodes/move (20004 -> 28008) and ~33%
cpu/move (6.1 -> 8.1 ms), well inside the nb200k budget. At the s98 head it
added only ~2% nodes -- the learned eval's games rarely reach leaf positions
with unresolved captures that survive stand-pat cutoffs.

## Interpretation

1. **Why no strength at the top:** same regime as theory 21's race detector
   (null at d6): the d6+tt+ord search already resolves most exchanges that
   matter, so a capture-only leaf extension rarely changes a decision that
   was not already correct. LABELED HYPOTHESIS (untested): quiescence's
   stand-pat is also systematically optimistic in Breakthrough because there
   are no quiet moves (every move advances irreversibly, Lemma B) and the
   decisive threats are RUNNER ADVANCES -- non-captures that quiescence never
   extends -- so the extension resolves the one threat class the search
   handles well anyway while staying blind to the class that kills.
2. **Theory 28 sharpened:** the probe its notes proposed as test (c) ran, and
   the tactical explanation is refuted. What remains is the positional
   hypothesis: learned PSTs beat the chip counter by steering through
   equal-material channels where the chip counter's eval is blind. Directly
   supported by the direction of the effect: fixing tactics made the
   head-to-head worse, consistent with qs burning budget/decision changes on
   the wrong axis.
3. **Under-sampling lesson, again:** at the 8-games/pair fill, s98+qs read
   1145 +/- 24 -- level with the oracle, +80 over s98. All of that inverted
   at 32 games/pair (1073, 9-23 against s98). The phase-0 rule (boost before
   reading the table) is now twice-validated. Preliminary 8-game reads of a
   new agent should never be quoted.

## Deviations from the plan

- The plan's phase 1 imagined the qs challenger dethroning the chip counter;
  phase 0 had already moved the target to s98, so phase 1 tested qs on BOTH
  the chip counter (theory-28 probe) and s98 (the actual dethrone attempt).
  Both outcomes are recorded above; neither took the throne.
- A reference row `ab(d8,tt,ord,nb200k)` (idle-budget headroom quantifier) was
  NOT rostered: phase 0's re-target made it moot for the throne question, and
  the oracle pairs boost already anchored the reference comparison.
- Two extra one-off boost rosters were used beyond the plan
  (`ranking/roster_top.txt` gained the qs agents; `ranking/roster_oracle_qs.txt`
  resolved the oracle pairs after the misleading 8-game read).

## Roster state

Both qs agents stay `on`: classic+qs (~1002) is a distinct style probe (the
only agent strong vs the classic family but crushed by learned PSTs), s98+qs
(~1073) has a head-to-head profile distinct from s98 despite the pooled tie.
Retire later per the curation policy if their profiles prove redundant.

## Addendum 2026-07-17 (opener probe reopens the s98 verdict)

The closed-book (no-opener) comparison above compares two agents that share
the same evaluator and, absent forced diversity, mostly walk the same path
(theory 19's only variance source between them is incidental cross-game
search-state carryover) -- so a small closed sample can be dominated by
whatever a few correlated lines happen to do, not by qs's real average
behavior. Testing this (developer hypothesis): `rank.exe pairgen` between
s98+qs and plain s98, 150 games, 6-ply (3-move-per-side) random opener ->
**91-59 (60.7%) in favor of s98+qs**, a full reversal of the 9-23 closed-book
loss above. "Quiescence doesn't help s98" does not survive this probe; see
theory 29's 2026-07-17 update for the full writeup, including the theory-6
symmetric-opener-inflation caveat and the recommended `--reset-state`
permanent fix. The strength-vs-tactics conclusion (quiescence made the chip
counter's LEARNED-model losses worse, supporting theory 28) is unaffected --
that used a different pair (classic+qs vs learned agents) and is not
implicated by this closed-book-sampling concern.

## Future Work

- **Runner-threat quiescence** (tethered to the labeled hypothesis in
  Interpretation 1): extend qs to also try moves that create or advance a
  passed runner (D9 detection exists via row counts), not just captures. If
  the hypothesis is right, that variant should outperform capture-only qs,
  especially for the chip counter.
- **Shallow-head qs** (tethered to the theory-21 parallel): qs at d2-d4 heads
  or under tight budgets is untested here, same as the race detector's open
  regime -- the mechanism says leaf extensions matter more when the main
  search is shallow.
- **Theory 28 positional test** (tethered to Interpretation 2): the remaining
  discriminators are the root-eval tie-count replay and the
  ablate-s98-to-material-only test from the theory's notes.
- **Why s98+qs loses to plain s98 head-to-head while tying pooled** (tethered
  to the 9-23 result): candidate mechanism is stand-pat mispricing exchanges
  that the outcome-trained PST already prices implicitly (double counting).
  A pairgen replay logging both agents' root evals at divergence plies would
  localize it.

## Ideas This Inspired

- A "leaf extension" registry axis (qs / race-win / runner-threat / none) so
  future leaf semantics are one-line ablatable agent flags like tt/ord.
- Pooled-Elo ties that hide 23-9 head-to-heads keep recurring (s96 vs s98 in
  phase 0, s98 vs s98+qs here) -- the top of this pool is genuinely cyclic,
  which is exactly the anti-gaming scenario the community-competition notes
  flagged; a head-to-head-aware tiebreak or a "defends the throne" match
  format may eventually be needed to declare champions.
- The nodes/move delta of qs (~2% on s98 vs ~40% on classic) is itself a cheap
  behavioral fingerprint of how "tactically hot" an evaluator's preferred
  positions are.

## Commit

One commit covering: the qs implementation (src/), tests, roster + boost
rosters, new match data + refit artifacts (ranking/), this results doc,
theory 28 note + theory 29 (Docs/theories.md), todo.md updates, and the
README/CLAUDE.md reference updates.
