# Dethrone the Champion: Phased Plan

Archive name when executed: `plans/dethrone-champion-plan-1-wiggly-mitten.md` (+ a
companion results doc per phase, `dethrone-champion-results-<N>-wiggly-mitten.md`).

## Context

The standing project loop (todo.md, Agent Track) makes "dethrone the current #1"
the recurring success criterion. The champion is the d6 chip counter
`ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`; the d8/nb2m oracle above it is
reference, not the target. Developer rulings for this plan (2026-07-16):

- **Depth stays d6.** A deeper cap at the same node budget (e.g.
  `ab(d8,tt,ord,nb200k)`) is reference-class like the oracle, not a legitimate
  dethrone. Quiescence at the d6 cap DOES count as a d6-class search improvement.
- **Full phased program**: re-certify the top, then quiescence, then the
  refutation book, then the learned-eval fixes. Each phase is session-sized with
  its own Elo measurement; reassess between phases.

What the record shows going in:

- **Closest failure:** `learned(s98)` (linear PST, oracle-vs-champion data)
  statistically tied the champion on the 07-12 fit (1137 vs 1140) and its
  head-to-head win survives the opener-bias correction (66.2% with the champion
  playing its own openings). champdil's win was an opener artifact (65% -> 40%).
  The MLP capacity jump calibrated better but rated 95-130 Elo WORSE at both
  depths (theory 27): overtrained (validation optimum is epoch 1, trained 6),
  raw outcome labels, full-scan leaf.
- **Fit drift (found this session):** the post-07-14 refit compressed the whole
  top ~100 Elo. `learned(s98)` (1045) now sits nominally ABOVE both classic d6
  rows (1029 ord-only, 969 tt+ord). Two near-identical classic rows differ by 60
  (theory-19 smell), and the latest results doc's "champion ~1140" matches the
  ORACLE's current rating, not the champion's. The top is under-sampled at 8
  games/pair and unresolved.
- **Idle budget (found this session):** the champion's nodes/move is ~15-20k
  against its 200k budget; the d6 cap, not the budget, stops the search. ~90% of
  its compute is idle. Per the ruling above this is not the dethrone path, but it
  is the headroom quiescence spends, and a reference row can quantify it.

## Phase 0 - Re-certify the throne (compute only, no new code)

The instrument must be trusted before any challenger is measured.

1. `.\tools\run_rank.ps1 -Build check` to validate the roster.
2. **Diagnose the compression.** Compare the 07-12 and 07-14 fits. Candidate
   cause: the BT prior (0.5 virtual games per played pair) scales with pool
   size, and the MLP study added ~36 IDs x ~60 opponents of pairs; read
   `rankRate` in `src/ranking.cpp` to confirm, and check the champion's record
   vs the new IDs with `rank.exe history --agent "ab(d6,tt,ord,nb200k)"`. If the
   prior is the cause, record it in `Docs/benchmarking.md` (scale compresses as
   the pool grows; read ORDER and pm bands, not absolute Elo).
3. **Resolve the top.** Temporarily set roster to anchor + the top ~12 active
   agents (oracle, s94-s99, both adv climbers, classic d6 rows, best dilution
   rung), run `.\tools\run_rank.ps1 -Workers 8 --games 32`, then restore the
   full roster and refit everything (`rank.exe rate`). Matches are keyed by ID,
   so nothing is lost. Two `--seed` replicates per benchmarking.md.
4. **Declare the target.** If s98 confirms above the classic d6 rows, the
   chip counter has already been dethroned by the project's own criterion and
   s98 becomes the new target for phases 1-3 (it is deterministic too, so the
   book path still applies). Either way, update todo.md's yardstick paragraph
   and write the phase-0 results doc.

## Phase 1 - Quiescence at the d6 head (the sanctioned search lever)

Todo `[Next]`: "Quiescence search (extend on captures / near-wins)". Converts the
champion's idle budget into tactical horizon while staying d6-class.

- **Implementation:** opt-in `qs` flag on the ab head, parallel to `tt`/`ord`/
  `asp`: at depth-0 leaves run a stand-pat alpha-beta extension over captures
  and one-step-win threats (reuse `canWinWhite/Black` from
  `src/board_analysis.cpp`). Quiescence nodes count against the node budget via
  the existing node counter, so nb200k stays honest.
  - Files: `src/ai_minimax.cpp/.h` (qsearch + toggle), `src/agents.cpp/.h`
    (AgentSpec flag), `src/ranking.cpp` (ID codec segment `qs` on the ab head;
    no version bump needed since absent-flag agents are byte-identical),
    `tests/` (equivalence test: qs-off byte-identical to current; tactical
    positions from `boards/puzzle*.txt` where qs fixes a horizon blunder).
- **Reference row (optional, clearly labeled):** roster `ab(d8,tt,ord,nb200k)`
  as reference-class alongside the oracle to quantify the idle-budget headroom.
  Not a dethrone claim.
- **Measurement:** challenger `ab(d6,tt,ord,qs,nb200k)@1.classic(t1,c4,w0,l0)@2`.
  Full main-roster gauntlet with >=2 `--seed` replicates, then roster + full
  refit. `pairgen` vs the champion for the byte-level head-to-head (det-vs-det =
  2 distinct games, so pooled Elo is the real verdict). Track cpu/move and
  nodes/move deltas (qs should eat idle budget, wall-clock roughly flat).
- **Success:** outranks the phase-0 target with non-overlapping pm bands.
- **Theory log:** new entry "quiescence adds Elo at fixed d6/nb200k"; cross-link
  theory 21 (its untested shallow-horizon regime is adjacent).

## Phase 2 - Offline refutation book (theory 14, todo's own top pick)

Exploit the target's determinism: mine its preferred lines, refute them offline
with deep search, play the refutations from a book at zero live cost.

- **Book builder** (`rank.exe bookgen`, new subcommand in `src/ranking.cpp` +
  `tools/rank_main.cpp`): walk the target's games in `ranking/matches.jsonl` /
  `games.tsv` (it is deterministic, so its lines recur), take positions on its
  lines up to ply ~12-16, run a deep budgeted search (the d8/nb2m oracle config,
  optionally d10) at each, and store best replies keyed by `positionKey` where
  the deep eval shows a decisive edge. Output: a text book file under `models/`
  or `data/` with a content hash.
- **Book-follower opener:** a `g_openers[]` registry entry (`src/ai_random.cpp/.h`)
  + `.opener(book,<tag>)@1` ID segment - the registry was built as exactly this
  extension point. Plays the book move while the position is in book, hands off
  to the brain when out of book.
- **Agent:** champion-build brain + book opener. Out of book it IS the base d6
  agent, so pool Elo should not regress - verify that explicitly.
- **Measurement:** `pairgen` head-to-heads vs the target (both colors), then
  full-roster refit. Success = lowers the target's Elo AND outranks it.
- **Honest labeling (community-vision memory):** det-vs-det pairs replay the
  same 2 games, so a 2-line book can sweep the head-to-head by construction.
  Report this as a counter-style ("gotcha") dethrone, distinct from broad
  strength: the two claims to substantiate are (a) oracle-verified winning lines
  exist against the target's true policy, (b) the book agent's pool-wide Elo
  does not drop. Update theory 14 with the outcome.

## Phase 3 - Learned eval at equal depth (the mission)

Attack theory 27's mechanisms on the proven data recipe (oracle-vs-champ +
champdil-vs-champ pairgen + high-Elo replay). This is the path that serves the
project's stated goal, not just the throne.

1. **Eval-blended labels** (todo `[Now]`): label = lambda*outcome +
   (1-lambda)*sigmoid(teacherEval/scale). The teacher's root score is currently
   thrown away; record it during pairgen/self-play generation and add the blend
   to `src/ml_train.cpp` (flag in `tools/train_main.cpp`).
2. **Extraction quality controls** (todo `[Next]`, theory 26): `--min-elo`
   floor and positionKey dedup/repeat-capping in `rank.exe extract`; run the
   theory-26 filter comparison (exclude-low / exclude-mixed / exclude-high
   control) as a rider on the same runs.
3. **Weight symmetrization + seed-ensembling** for linear models (todo `[Now]`):
   mirror-average each weight and average K seed replicas - a free variance cut
   against the 50-150 Elo seed band (theory 8).
4. **Early stopping on by default** for these runs (`--val-split --early-stop`,
   already shipped) - the MLPs were trained 6x past their validation optimum.
5. **MLP retry only after 1-4 land**, with the same fixes, then NNUE
   incrementalization (widen `g_mlAcc` to a vector, recompute only touched
   hidden units) so capacity competes at wall-clock parity.
- **Measurement:** ~6 training seeds per recipe, pooled Elo from full-roster
  refits at BOTH standard heads (d4 and d6), read against seed spread - the
  standing instruction. Update theories 4, 10, 26, 27.
- **Success:** a learned agent at or above the target at the d6 head.

## Measurement + hygiene rules (all phases)

- Full main-roster instrument, never the climb pool; >=2 `--seed` replicates for
  deterministic-agent comparisons; ~6 training seeds for trained models;
  `pairgen` byte-level comparison for near-identical policies; read deltas
  against replicate spread (theories 8/19, `Docs/benchmarking.md`).
- `.\tools\run_tests.ps1 -Build` green before every commit.
- After each phase: companion results doc in `plans/`, `Docs/theories.md`
  updates, `todo.md` strike-outs, README/ML.md as touched, commit at the
  checkpoint (no push).
- Trim temporary study roster lines after rating; their games persist in
  `matches.jsonl`.

## Verification (end-to-end)

- Phase 0: top-5 pm bands non-overlapping; declared target written into todo.md
  and the results doc; compression cause identified or explicitly left open.
- Phase 1: qs-off equivalence test passes (byte-identical games); at least one
  unit test where qs corrects a horizon blunder; full refit shows the qs agent's
  position vs the target with 2 seed replicates.
- Phase 2: bookgen round-trip test on a synthetic match store; opener-registry
  unit test; pairgen sweep + refit showing both success criteria.
- Phase 3: 6-seed sweeps rated at d4+d6; stratified loss and validation loss
  reported alongside Elo (never instead of it).
