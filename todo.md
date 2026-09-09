# Todo

Priority tags: `[Now]` = active focus, `[Next]` = queued up, `[Later]` = valuable but not soon, `[Dream]` = fun idea, not currently planning to do it. The two tracks below are prioritized independently, a `[Now]` in one track says nothing about the other.

- Keep optimizing {cpu: days, dev: high}

---

# 1v1 / GUI Track

- Display whose turn it is in the main board area `[Next]` {cpu: seconds, dev: high}
- Add a button or something that changes the white and black pieces to red and blue in the GUI `[Next]` {cpu: seconds, dev: high}
- Best moves list or recommendation arrow `[Later]` {cpu: seconds, dev: high}
- GUI: play against a named saved agent `[Next]` {cpu: seconds, dev: high}
- GUI: agent-vs-agent ladder / leaderboard view `[Later]` {cpu: seconds, dev: high}
## Pondering / 1v1 Performance
- Run the side-bar evaluator's minimax at iterative deepening with no node or depth limit
  while waiting for the human's turn. Only use this for the visual evaluation readout, not
  the move actually played `[Now]` {cpu: seconds, dev: medium}
- Pipeline eval: instantly show a precomputed eval, then precompute the response and
  resulting eval for every possible next move in the background while waiting for the
  opponent's turn (can be parallelized) `[Next]` {cpu: seconds, dev: medium}
- Parallelize more computations `[Later]` {cpu: seconds, dev: high}
- New "rush" mode: Player B is an agent that runs iterative deepening against all of Player
  A's possible moves while waiting for A to move. On B's turn it plays the best move it
  already found for A's actual move, so it responds instantly with hardly any computation.
  The longer A takes, the deeper B has precomputed `[Next]` {cpu: seconds, dev: medium}

---

# Agent Track (rank.exe / ML / Search)

A composable system of interchangeable parts. An **Agent** = a **Move Chooser** (its "brain"),
which is either a **Search** (a **Move-Tree Explorer** + a **Board-State Evaluator**) or a
**Policy** (a direct, no-lookahead move picker: a heuristic or a learned move-rater). Each axis
is a registry, so adding one is a single table entry + a function body, and everything
(UIs, tournaments, docs) picks it up automatically.

**Goal:** a learned evaluator that beats the classic chip counter at EQUAL search
depth, then at lower compute (deeper-for-cheaper). This is the project's core ML
research question, and it is what same-head, loadout-matched comparisons are
for. A second, broader goal sits alongside it and is what the category titles
track directly (revised 2026-08-23): the strongest agent overall, any search
technique included, excluding only reference-class agents that win purely by
spending more search compute on an already-established technique rather than a
different one (see `ranking/CHAMPION.md`'s reference-class rule). A different
search algorithm, for example Gumbel MCTS (`gaz(...)`), winning a category is
evidence for that approach, not a result excluded by head identity. Since
2026-07-28 the throne is split into parallel category champions rather than
one single champion. As of 2026-08-24 that is 6 categories: 3 opener/loadout
divisions (openless / `.opener(rand,moves=8)@1` / a 20% full-random dilution
division) crossed with 2 compute tracks (`nodes=200k` / `time=150ms`),
declared in `ranking/CHAMPION.md` (single source of truth; the numbers below
are tagged to their fit dates and predate the split, so they describe the
single-champion era's history, not a current category leader). The earlier
4-book/8-book/4-random categories are demoted to ladder/study data in
CHAMPION.md's "Deferred categories" appendix, not deleted, pending a future
book-mining redesign (self-maximizing per-division book mining, not yet
scoped).
**The first tier was achieved
and certified 2026-07-17** (`plans/dethrone-champion-results-1-wiggly-mitten.md`):
after boosting the top pairs to 32 games each and refitting, a learned PST
(`ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1`, trained on oracle-vs-champion
data) dethroned the classic chip counter, 1064 +/- 14 vs 976 +/- 13 (direct
head-to-head 23-9). The chip counter's weakness class is now known: d6-head
learned piece-square models beat it far above their pooled Elo (theory 28).
Fit-scale caveat: Elo is not comparable across fits as the pool grows (see
`Docs/benchmarking.md`). Three follow-up phases then did NOT dethrone s98 and
each refuted a theory: quiescence (phase 1, theory 29 -- s98+qs ties pooled,
loses the pair 9-23); the oracle-mined refutation book (phase 2, theory 14's
naive form -- both book agents rated below their bookless selves); weight
symmetrization/ensembling (phase 3, theory 30 -- mirroring the champion's own
weights alone cost 135 Elo). **A FOURTH attempt, 2026-07-18, DID dethrone**:
mining a book from the WEAK agent's OWN wins (not a stronger agent's wins over
it -- theory 33, the repaired form of theory 14) turned the classic chip
counter itself back into the reigning champion,
`ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2.opener(book,book=2)@1` at 1145 +/- 13,
25-7 against s98 directly and 27-5 against the d8/nb2m oracle it was never
mined against. **This has an open scrutiny flag** (`ranking/CHAMPION.md`,
results-5's "open scrutiny question"): it is a 134-entry hard-coded book on the
original evaluator, not a generalized strength improvement, and whether it is
genuine transferable strength or a pool-specific/memorization effect is
unresolved. The learned-eval fixes remain untried regardless of that question --
eval-blended labels, Elo-filtered extraction, seed SELECTION by Elo (the best
of 6 champion-recipe seeds is +28 over s98) -- alongside the now-open follow-ups:
apply the same self-mining fix to s98's own book, test whether "stronger
opponent" is load-bearing or any own-win suffices, runner-threat quiescence,
and the reset-state prerequisite. See the results docs (1-5) for full detail.

**Standing loop:** the recurring success criterion for any new agent is dethroning
the current #1 in `ranking/ratings.tsv`, either by outrating it outright or by
countering its specific build/weaknesses (see the adversarial counter-agent idea
below). rank.exe's pool + gauntlet already measures this on every run.

Legend: **(P1)** = built in the first pass (versatility proof). Everything else is future work
against the same seams.

- Add variety in openers and moves `[Now]` {cpu: hours, dev: medium}
  - Either an opening book, arbitrary rewards for certain opening positions, or a separate opener model for training
  - Random move chooser out of top candidates (especially ties); the board-state-evaluator noise variant of this idea moved to the Heuristic Evaluator Feature Ideas section below
  - Elo-rate the existing SCRIPTED openers (Offensive/Defensive) as ID modules: an optional `op(o|d)@1` ID segment (absent = Standard), roster the champion build under each opener, and let the BT fit price the openers directly. Generalize beyond the 3 built-in openers: rate arbitrary candidate opening move sequences the same way (Elo given/taken vs the pool), and learn to prefer strong ones and avoid weak ones. The general version is a bigger project (developer's own estimate: "too much work for now") `[Next]` (built-in openers) / `[Later]` (arbitrary sequences) {cpu: minutes-hours, dev: medium}. ~~A sibling idea for RANDOM (not scripted) openers shipped~~: a pluggable opener registry `g_openers[]` (`src/ai_random.h`) + `AgentSpec::openerKind`/`openerArg` + a `.opener(<kind>[,<arg>])@1` ID segment lets ANY agent be rostered/gauntletted both with and without an opener, so the Elo gap is a general, per-agent opener-sensitivity score (currently one kind, `rand`: e.g. champion 1140 clean vs 923 with `.opener(rand,moves=6)@1`, champdil 1153 vs 962). The registry is exactly the extension point for the SCRIPTED (`off`/`def`) and opening-book openers above -- adding one is a table row + fn, and the same ID slot names it. See `Docs/agents.md` and `Docs/terminology.md`'s "Opener (identity-level)" entry `[done]`
  - Mine `matches.jsonl` for an opening book: tabulate the first 8-10 plies of >= 900 Elo games by `positionKey` with win rates and visit counts, emit a book file, and add a book-follower opener that plays the book move while in book `[Next]` {cpu: minutes, dev: low}
  - ~~Color-swap recovery test: play the same random-opener snapshot to conclusion twice with colors swapped, to separate "the position favors a color" from "this agent recovers better."~~ Shipped as `rank.exe opener-swap`. Champdil vs the champion at n=20 (theory 15, `Docs/theories.md`): 65% of outcomes were a color effect (White won both 55%, Black won both 10% -- consistent with the champion's own White/Black split), but in the remaining 35% agent-effect bucket champdil won every time (7/7), the champion never (0/7) -- promising but small-sample signal that champdil recovers from bad positions better than the champion, independent of color. Follow-up (larger sample, try with oracle too) filed in `plans/opener-bias-results-1-synchronous-stearns.md`'s Future Work `[Next]` {cpu: hours, dev: medium}
  - ~~Random-first-K-plies opening diversity for data generation~~ (shipped as pairgen `--open-plies`); extend the same knob to tournament and self-play generation `[Next]` {cpu: seconds, dev: low}
  - Sweep `--open-plies` length for the oracle-vs-champion training regime (0/2/4/6/8/12 tried against the single untested value of 6 used in the first vs-champion study), gauntlet-screen each, to check whether 6 was actually a good choice or just a guess `[Next]` {cpu: hours, dev: medium}
  - Learned opener: a policy head trained only on plies < 10 of high-Elo replay games, used as an opener module that hands off to the main brain once out of phase. Specific angle worth testing: train the opener on WINNING-line data (see the refutation-book idea below) and hand off to a cheaper depth-5 search after the opener phase, on the theory that a strong precomputed opening plus a shallower live search could beat the d6 champion for less total compute -- directly on-target for the session's "beat d6 without searching deeper" goal `[Later]` {cpu: hours, dev: high}
  - Single-line refutation book (extended, more concrete version of the offline-refutation idea below): find ONE oracle-verified winning line against the champion for each color (2 lines total), play that fixed line against every other deterministic agent in the roster, and whenever an opponent deviates from the line (or the line stops applying), use the oracle to find a new winning continuation from that deviation point. Builds a small branching decision tree rooted at "beat the champion," tested for robustness against the whole pool, not just the champion. Consider separate White/Black book models plus a combined one `[Later]` {cpu: hours, dev: medium}
  - ~~Offline refutation book against the champion: run deep budgeted searches (d8-d10, nb2m) on the champion's preferred opening lines (it is deterministic, so its lines are minable from games.tsv), store best replies keyed by `positionKey`. A book + d6 search agent then attempts the dethrone with LESS live computation by construction.~~ Shipped 2026-07-17 as `rank.exe bookgen` (mines A's winning positions/moves from stored games) + the `book` opener (`.opener(book,<N>)@1` plays `models/book<N>.txt`). First verdict (mining the STRONG agent's wins over the weak target): REFUTED in this naive form (theory 14, `plans/dethrone-champion-results-3-wiggly-mitten.md`) -- both book agents rated ~16 Elo below their bookless selves. **Reversed 2026-07-18** (theory 33, `plans/dethrone-champion-results-5-wiggly-mitten.md`): mining the WEAK/book-wearing agent's OWN wins instead (zero new code, just swapped which agent's wins get kept) fixes the brain-portability failure by construction and DETHRONED s98 outright (classic+selfbook 1145 +/- 13 vs s98 1074 +/- 12, 25-7 head-to-head, 27-5 vs the oracle it was never mined against). Open scrutiny flag: genuine strength vs pool-specific effect, unresolved (`ranking/CHAMPION.md`). Remaining open repairs: a `--reset-state` mode for reproducible det-vs-det play (still needed, drift rate was 12/32 for this pairing); testing whether "opponent must be stronger" is load-bearing or any own-win suffices; applying the same self-mining fix to s98's own book; and the stay-in-book-to-the-win / response-tree variant via `--branch-tries`-style mining, which folds into the single-line refutation book idea above `[Next]` {cpu: hours, dev: medium}
- Interpret board analysis
  - Which piece is most impactful to the current evaluation? `[Later]` {cpu: minutes, dev: high}
  - What's the cheapest strategy to beat each given bot/parameters, even if overfitted? `[Dream]` {cpu: minutes, dev: high}
  - What strategies could a human devise to beat a bot? `[Dream]` {cpu: minutes, dev: high}
  - Is attacking the center or attacking the edge the best? `[Dream]` {cpu: minutes, dev: high}
  - Is advancing through the center or the edge the best? `[Dream]` {cpu: minutes, dev: high}
  - Is keeping the hind pieces in place the best? `[Dream]` {cpu: minutes, dev: high}
- Build a search tool to bound/compute **distance-to-win**: the true, rules-respecting
  number of plies to a forced win from a position, as opposed to `Docs/axioms.md` Lemma
  B's naive "capacity" sum (which ignores blocking and whether a piece can actually reach
  its goal). First validation target: the standard start, since axioms.md D13 already
  hand-proves its minimum game length is exactly 11 plies (a witnessed tight bound) --
  a computational search should reproduce that number before trusting it on anything
  harder. The state space is too large to explore exhaustively, so the tool needs pruning:
  - Depth-first search so it returns a usable answer sooner rather than needing to finish
  - Explore non-capture moves before captures at each node (this cuts against the engine's
    normal capture-first move ordering, which is tuned for alpha-beta pruning under a
    different objective -- pin down and justify the reasoning for this tool specifically
    before relying on it)
  - A transposition table deduping by position so a position reached by multiple move
    sequences is only explored once. Pin this down carefully, it is easy to get backwards:
    depending on whether the goal is a short witness line or a proof of a lower bound, the
    table may need to keep the position's BEST (fewest plies) or WORST (most plies)
    arrival, not just whichever was seen last
  - Prove each pruning optimization sound (it cannot cause the search to miss a valid
    shorter game or invalidate a claimed bound) before relying on it, and prioritize
    optimizations with the biggest expected computation savings first
  - Embedded hypothesis to test once the tool exists: capturing an opponent piece that is
    one ply from winning is always an optimal reply, except when it is the last piece.
    (Ambiguous as stated -- clarify whether "it" is the threatened piece being the
    opponent's LAST piece, in which case the capture wins outright via A9/D6 rather than
    merely defusing a threat, or the capturing piece being the defender's OWN last piece
    needed elsewhere. Filed as theory 17 in `Docs/theories.md`.) `[Later]` {cpu: minutes, dev: high}

## Budget-parity rebuild (PLANNED, not started)

- **Re-run every agent-production regime under matched training compute, on both
  compute tracks.** Full plan: `plans/budget-parity-plan-1-steady-meridian.md`
  (written 2026-09-01, nothing executed yet). 8 regimes in scope, 3 deferred.
  Developer decisions recorded in the plan's "Decisions taken" section.
  **The finding that drives it: neither track currently constrains compute.**
  Measured over 623,774 rows of `ranking/matches.jsonl`, the node track runs at
  21-46% of its 200k budget and the time track at 7.5-12% of its 150ms budget,
  because `deep=6` caps iterative deepening before either budget binds
  (`src/ai_minimax.cpp`). The same core costs 41,841 nodes/move on the node head
  and 41,819 on the time head, so the two tracks are today the same instrument
  measured twice. Quantified per core 2026-09-03 (`rank.exe nodeprofile`, 12 games
  each, 15 cores): on `ab(deep=6,tt,ord,nodes=200k)@2` the node cap binds on a mean
  of **3.8%** of plies, from 0.0% on `classic(chip=100)@2` to 14.8% on
  `learned(model=113,...,mlp)@1`. At `deep=7` it binds on 37% to 79%, so the roster
  head sits just below the knee. `[Now]` {cpu: days, dev: high}
  - ~~**The `rem=N` gate exists but is unrated.**~~ **Rated 2026-09-04.**
    `ab(...,rem=N,nodes=...)` declines a deepening iteration unless N% of the node
    budget is unspent, which is what lets a budget be raised without paying for
    iterations that cannot finish. Measured: the last completed iteration is 64-83%
    of everything spent to reach it on all 15 cores, `rem=70` saves 35-39% of nodes
    for 2-3% of plies losing a ply. **Elo, pinned fit, 1,228 games/cell, delta
    against `ab(deep=6,tt,ord,nodes=200k)@2` on the same core:** at the same 200k
    budget the gate is a wash for 29-38% less CPU (-4 m169, -13 m349, -61 classic,
    +34 m113, -17 m97), and spending the saving gives +35/+42 (m169), +51/+41
    (m349), +122/+163 (m113 MLP), +0/+10 (m97) at `nodes=300k`/`550k`.
    `classic(chip=100)@2` is the one core it hurts (-66/-49). Full grid and the
    untested TT-seeding hypothesis for the classic exception:
    `plans/budget-parity-results-2-steady-meridian.md`
  - ~~**Search-technique Elo grid.**~~ **Done 2026-09-04.** 55-agent cohort
    (5 cores x 11 heads) via Workflow A into the main store, 40,517 games at
    8 games/pair, working roster `ranking/q5/roster_rem.txt`, cohort
    `ranking/q5/cohort_all.txt`, then `rate --pin ranking/q5/pin_no_d12.tsv`.
    Holds the CORE fixed and varies the HEAD, the mirror image of `CHAMPION.md`
    rule 6. Headline: `part` is **-53 to -513** on every core and should never be
    used. `tt`+`ord` together are worth 49-173 Elo, but which of the two carries it
    flips by core. `margin=100` is within noise on all five. Grid in
    `plans/budget-parity-results-2-steady-meridian.md`
  - **The node track's definition is now an open decision.** `rem=70,nodes=300k`
    or `nodes=550k` beats today's `deep=6,nodes=200k` head on 4 of 5 cores while the
    current head's cap binds on only 3.8% of plies. Redefining the node track means
    re-rostering and re-certifying every node-track category, so it is a developer
    call, not an automatic follow-on `[Next]` {cpu: days, dev: medium}
  - ~~**`retain` is built but unrated.** `ab(...,rem=70,retain,nodes=...)` banks a
    move's unspent node budget and adds it to the same side's next move, so the cap
    is conserved per GAME rather than per move. Verified at `rem=70,nodes=200k`: the
    chip counter goes 108,032 -> 165,058 mean nodes/move (429,035 peak) with plies
    reaching depth >= 7 rising 19.7% -> 36.8%, and the tdleaf_self linear `model=169`
    goes 116,712 -> 174,098 (501,604 peak), 16.2% -> 23.7%. Elo cohort in flight:
    40 cells, 5 cores x 4 budgets (100k/200k/300k/400k) x {`rem=70`,
    `rem=70,retain`}, roster `ranking/q6/roster_retain.txt`, cohort
    `ranking/q6/cohort_retain.txt`. Read it as the slope of Elo on log2(budget):
    the hypothesis is that `retain` flattens it. The 550k and 1m rungs were dropped
    on developer instruction (2026-09-05) as too expensive for the value, and their
    partial cells are benched `off` in the working roster so a low-game-count row
    cannot reach a fit~~ **Done 2026-09-05.** Slope of Elo on log2(budget) drops
    from +103.3 to +36.7 Elo/doubling on tdleaf_self lin `model=169` (z=-5.45) and
    +81.3 to +24.0 on `model=349` (z=-4.83), but that slope difference is ENTIRELY
    the 100k rung: refit on 200k/300k/400k only and every core is under z=1.4. The
    surviving claim is narrow. `retain` is worth +142+-17 and +120+-17 at 100k on
    those two cores, equal to one doubling of budget, and roughly nothing from 200k
    up. Grid, rung-to-rung steps and the depth-saturation reading:
    `plans/budget-parity-results-2-steady-meridian.md`
  - ~~**Ladder cohort runs instead of guessing `--games`.**~~ **Done 2026-09-05.**
    `rank.exe play` and `tools/run_rank.ps1` ladder rungs 2, 4, 8, ... N by default
    (`--no-ladder` / `-NoLadder` to opt out, `-PinEachRung` to fit after each rung).
    Verified to play the identical multiset of (white, black, seed) as one pass, on
    the serial path, the 3-shard path, and in a unit test. Rung 1 is the widest, so
    the true multiplier to `--games 16` is 6.5x for a full round robin and 4.7x for
    a cohort cell, not 8x, and the tool prints the exact remaining count instead.
    Results: `plans/ranking-run-scheduling-results-1-tidal-lantern.md`
  - **Validate the ladder's stopping rule offline against the finished `retain`
    store.** Costs no new games. Subset its rows to 2/4/8/16 games per pair and find
    the rung at which each of the five slope contrasts first reached its final sign
    and magnitude. If a contrast that later moved would have passed the rule early,
    the SE multiplier is too loose. This is the one part of the ladder design the
    equivalence proof says nothing about `[Next]` {cpu: low, dev: medium}
  - ~~**`rem=0` baseline for the retain study.**~~ **Done 2026-09-06.** 40 cells,
    1,398-1,548 games each. The control passes (`rem=0,retain` vs `rem=0`, max |z| =
    1.75 over 20 cells, so `retain` is inert with no gate). The gate alone is free:
    Elo-neutral in all 20 cells while saving 29-48% of the wall clock. And `retain` is
    CPU-neutral against the DEFAULT at 0.85x-1.01x ms/move, not the 1.2x-1.9x it looks
    like against `rem=70`, so its +137+-17 and +104+-16 at 100k on the tdleaf_self
    cores come at 1.00x the default's compute. Grid:
    `plans/budget-parity-results-2-steady-meridian.md`
  - **Ladder the position-oracle labeling too.** `posgen`/`label` spend a fixed
    playout budget per position. The same `p(1-p)` argument applies and is stronger
    there: rather than skipping settled pairs, the later rungs would REDIRECT
    playouts onto positions whose label sits near 0.5 `[Next]` {cpu: low, dev: medium}
  - **Successive halving for `hill_climb.ps1` and the training playbook's Pass 3.**
    Uniform doubling spends equally on a candidate that is already clearly last.
    Racing (drop the worst half per rung, double the survivors) is the right shape
    when the question is "which of these" rather than "how big is this effect"
    `[Later]` {cpu: low, dev: medium}
  - **3 torn rows in `ranking/matches.jsonl`** (lines 631710, 641823, 655232), one
    logical game split across two lines by interleaved appends during the 2026-09-04
    40k-game run, out of 906,983 rows. The reader skips them with a WARNING and no
    result depends on them. Worth deciding whether the shard merge should be made
    atomic before the next large parallel run `[Later]` {cpu: none, dev: low}
  - **No time-bound ladder exists, so the two tracks cannot be compared.** Only
    `time=150ms` is rostered, at two depths, and `ab(deep=12,tt,ord,time=150ms)@2`
    carries just 2 of 5 cores over 1,792 games. Its ordering inverts the node track
    (chip counter 1279 over tdleaf_self lin `model=169` at 959, where the node track
    has tdleaf ahead by about 200), which is `PINNED AT LOW GAME COUNT` and must not
    be read. The `deep=6` time rows realize 11-362 ms against a 150 ms flag, which is
    the pre-fix overshoot rather than a wall-clock instrument. To answer "is ms
    comparable across the tracks", run a time ladder parallel to the node grid on the
    fixed binary: `ab(deep=12,tt,ord,time=Xms)@2` x the same 5 cores x
    {25, 50, 100, 200, 400ms}, giving Elo against realized ms per core, directly
    overlayable on the node grid's Elo-and-ms table. Note these pairs are no longer
    capped at 2 games, since `rankAgentIsDeterministic` now treats `time=` as
    stochastic `[Now]` {cpu: hours, dev: low}
  - ~~**`retain` has no time-budget analogue, and the gap is the interesting one.**~~
    **Built 2026-09-06.** `g_timeCarry[2]` (`src/globals.cpp`) fed from the new
    `g_lastSearchMs`, read and written by the same `agentChooseMove` block as the node
    purse and independent of it, cleared by `retainResetCarry()`, parked by `agreeStep`.
    Two unit tests assert the purse actually fills, never goes negative, holds only the
    unspent part, is per side and per game, and that a node-only `retain` agent leaves
    it at zero. Theory 68. Elo unmeasured until the ladder study below runs. NOTE:
    `rank.exe` could not be relinked while the `rem=0` job held it, so rebuild every
    binary (`breakthrough.exe`, `tests.exe`, `rank.exe`, `train.exe`) before the study
  - ~~**Wall-clock ladder study: what ms is a reasonable bound?**~~ **Ran
    2026-09-06, ANSWER INCOMPLETE BY DESIGN.** 40 cells, 2,136 games each.
    Headline: the wall-clock track normalizes compute (1.1x-1.6x cross-core spread
    in realized ms) and the node track does not (18.0x-36.4x), so `CHAMPION.md`
    needs rewording. The knee is NOT in range for half the cores: `position_elo mlp
    model=113` still gains +77 (plain) and +41 (`retain`) in the last doubling, as
    predicted before the run. Best candidate in range is `time=200ms` with
    `retain`, where realized spend is 165-177ms across all four cores and two of
    four have plateaued, but it is a candidate, not a conclusion.
    `plans/time-ladder-results-1-copper-vireo.md`
  - **Extend the time ladder to 800 and 1600ms, RETAIN ONLY.** LAUNCHED 2026-09-06.
    Required before any ms bound can be defended, because the study's own criterion
    is unmet for `position_elo mlp model=113` and `pool_games lin model=97`. 28
    cells, existing `retain` cells stay and the scheduler plays only the deficit.
    `[Now]` {cpu: hours, dev: low}
  - ~~**Retire the plain condition from every budgeted study.**~~ **Done
    2026-09-06, developer instruction.** `retain` is carried and plain is not, on
    both tracks. Backing: plain realizes 0.432 of its `time=` flag (sd 0.053)
    against `retain`'s 0.854 (sd 0.028) over 20 cells, and the Elo effect pools to
    +52.5+-7.2 at nodes=100k (z=7.25, free at 0.85x-1.01x CPU) and +67.3+-6.5 at
    time=25ms. Theory 70. `tools/make_time_ladder_roster.py` is retain-only, the 20
    first-pass plain cells are benched `off` in
    `ranking/q7/roster_timeladder.txt` rather than deleted, and the pooled analysis
    lives in `analysis/pool_retain_effect.py`
  - **Decide whether the STANDING roster moves to `retain` heads.** Not done, and
    not doable unilaterally: adding `retain` to the 228-agent roster mints an
    entirely new identity per agent, so the whole pool needs games from scratch and
    every `ranking/CHAMPION.md` category has to be re-certified on the new heads.
    The studies above establish that `retain` is the better head. They do not
    establish that the cost of migrating the ladder is worth paying. Developer call
    `[Now]` {cpu: days if taken, dev: medium}
  - **Neither `retain` purse is capped.** `src/agents.cpp` banks the whole unspent
    remainder every move with no ceiling, so on a head where the DEPTH cap binds
    instead of the budget the purse grows by nearly a full flag per move and is
    never drawn down. Inert while the depth cap keeps binding, which is why it has
    not surfaced, but one position that searches deeper could spend the whole
    accumulation at once. Check it on the 800/1600ms rungs now running: if the chip
    counter's realized ms is flat across 400 / 800 / 1600ms, depth 12 is binding
    and its purse is unbounded. Decide then whether to cap the purse at a small
    multiple of the flag `[Next]` {cpu: none, dev: low}
  - **Round 4: both compute tracks migrated, and the roster is now core-major.**
    LAUNCHED 2026-09-07. New heads `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3`
    and `ab(deep=12,tt,ord,retain,time=25ms)@3`, chosen because 25ms/100k =
    0.25 us/node and the three cheap cores measure 0.23-0.26, so both tracks are
    one operating point for most of the roster. 37 cores x 3 divisions x 2 tracks
    = 222 category agents, all six cells equal for the first time. `ranking/cores.txt`
    names each evaluator once, `ranking/tracks.txt` holds the cross, and
    `rankCategoryOf` now requires an EXACT head match, which drops the 19
    ablation-head rows that had drifted into title races. Stopping rule is
    pre-registered in `plans/track-migration-plan-1-slate-kestrel.md` and applied
    by `analysis/rung_convergence.py`: floor of 32 games/pair, then stop when
    order is stable (Spearman >= 0.99, no core moving > 3 places), all 6
    champions are unchanged and moved < 1 combined SE, and median pm fell by
    1.30-1.55x. Rungs 1 to 3 completed 2026-09-09, 8 games/pair, 562,952 rows in
    2,645.7 min on 10 workers. Rung snapshots go in `ranking/rungs/` because a
    snapshot records a past game count and cannot be regenerated. The core set
    then dropped 37 -> 33: measured on rung 1's own rows, the six
    `position_elo mlp mu_shape=129-512-8-1` seed replicates were 46.9 of the
    run's 96.9 core-hours, since a node budget fixes nodes and not time and that
    recipe runs 4.15 us/node for 292-413 ms/move against ~23 ms on the cheap
    cores. Four are benched, keeping the two lowest model numbers, a rule fixed
    before reading any fit. Cutting games instead was rejected on measurement,
    not taste: Bradley-Terry information per game is p(1-p), and the pool
    averages 0.1642 against a 0.2500 ideal with 82% of pairs inside 400 Elo, so
    the round robin is only 1.5x off optimal pairing and holds no pile of
    foregone conclusions. Rungs 4 and 5 (16 and 32 games/pair) launched
    2026-09-09 on `ranking/q8/cohort_round4b.txt`, 198 agents
    `[Now]` {cpu: days, dev: low}
  - **Rewrite `ranking/CHAMPION.md` for the new tracks once Round 4 converges.**
    Retire the `TIME BUDGET NOT ENFORCED` banner in the same edit, stating in the
    Round 4 entry why it no longer applies rather than letting it vanish. Move
    the 2026-08-30 summary and the six old-head detail sections to "Superseded
    summaries". Collapse the six detail sections to one `ranking/cores.tsv`-shaped
    table `[Next]` {cpu: none, dev: medium}
  - **Reword `ranking/CHAMPION.md`'s track descriptions.** The node track is not a
    compute-normalization track. It measures strength per node, which isolates
    evaluator quality from evaluator speed. The wall-clock track is the compute one.
    Theory 69 `[Now]` {cpu: none, dev: low}
  - **Re-specify the wall-clock track's head on `retain`, then re-certify.** Follows
    from the entry above and from theory 70. Measured on the current pinned fit
    (`ranking/standings_pinned.tsv`, 45 rostered `time=150ms` rows at 320 games
    each): the 43 `deep=6` rows realize a MEAN OF 20.5 ms/move (range 7.5 to 75.9)
    and the 2 `deep=12` rows 61.5 ms (57.2, 65.7). So on the deep=6 heads the depth
    cap binds long before the clock does and `time=150ms` is close to decorative,
    while on deep=12 the clock binds and plain leaves about 60% of it unspent.
    Fixing this is two changes, `retain` and a depth cap that does not pre-empt the
    budget, and both mint new identities for every agent in the track, so it is a
    re-certification and not an edit. Blocked on the 800/1600ms rungs landing, since
    the bound and the head should be set in one act `[Next]` {cpu: hours, dev: low}
  - **Superseded plan text for the study above:** Queued behind the
    `rem=0` run and the `@2 -> @3` bump, in that order. Criterion (developer choice
    2026-09-06): the KNEE of Elo against REALIZED ms, not against the flag, since a
    time-budgeted search finishes at roughly 40% of its allowance. 40 cells, 4 cores x
    {25, 50, 100, 200, 400ms} x {plain, `retain`}, generated by
    `tools/make_time_ladder_roster.py --abver 3`. `tdleaf_self lin model=349` is
    dropped as a near-duplicate of `model=169`. Accepted limitation: `position_elo mlp
    model=113` costs 252-1431 ms/move in the node grid and may still be climbing at
    400ms, in which case report no knee rather than fitting one. These pairs are not
    capped at 2 games, since `time=` counts as stochastic `[Now]` {cpu: hours, dev: low}
  - Mechanism reference for the two entries above:
    `retain` was inert without a node budget by construction
    (`retaining = a.retainBudget && a.nodeBudget != 0`, `src/agents.cpp`). `rem=` is
    likewise inert on a time head: `nodeIterationWorthStarting` returns true whenever
    there is no node deadline. The time side has its own separate gate,
    `nextIterationFits` (`src/ai_minimax.cpp`), which predicts the next iteration's
    cost from this search's own last-two-iteration growth ratio rather than applying a
    static threshold. Because that gate DECLINES iterations, a time-budgeted search
    also leaves budget unspent, which is exactly the condition `retain` exploits on
    the node side. Banking unspent milliseconds into the same side's next move is the
    direct analogue and is not built `[Next]` {cpu: hours, dev: medium}
  - **Why `rem=70` costs `classic` Elo.** The only core the gate hurts, and the one
    with the most budget left after depth 6 (31% of a 200k cap against 60-62%
    elsewhere). Hypothesis: its cheap doomed depth-7 iteration seeds the TT for the
    next move. Test: `ab(deep=12,noTT,ord,nodes=200k)@2` against
    `ab(deep=12,noTT,ord,rem=70,nodes=200k)@2` on `classic(chip=100)@2`, where the
    hypothesis predicts the gap closes `[Next]` {cpu: hours, dev: low}
  - ~~Cross-cutting prerequisites, all blocking (plan Part 1): raise `deep=` to a
    non-binding ceiling; fix `time=` enforcement (the one-line pre-iteration check
    in the item below is NOT sufficient once the depth cap lifts, see plan P2);
    instrument node/leaf counters in `src/ai_gumbel.cpp`; add wall-clock rungs to
    every trainer; fix the `games=` provenance bug (plan P5); add training-compute
    instrumentation; refresh `ranking/climb_roster.txt`~~ **All 8 landed 2026-09-01**,
    results in `plans/budget-parity-results-1-steady-meridian.md`. Part 2 is unblocked.
  - **Pending developer decision: is the `ab` explorer's code version bumped `@2` ->
    `@3` for the `time=` enforcement fix?** Not bumped so far. The fix changes what a
    `time=` head computes (7.14% of stored games involving one no longer replay, versus
    0.0% for games involving none), but module versions have no finer grain than
    per-module, so a bump re-identifies all 211 active `ab(...)` lines and orphans
    814,817 of 815,597 stored games to correct the 33.6% that carry a `time=` head. The
    node track is provably byte-identical across the fix. Full tradeoff, and the
    contrast with the 2026-08-27 TT bump that did fire, in `TIME BUDGET NOT ENFORCED`,
    `Docs/corrections.md`. **Developer decision 2026-09-05: bump eventually, but
    MIGRATE rather than orphan.** Games where neither side carries a `time=` head are
    provably byte-identical across the fix, so the store rewrite must renumber their
    `ab(...)@2` ids to `@3` in place and discard only the `time=` rows. That saves
    roughly 540,000 of 815,597 games instead of losing all of them. Big lift, small
    value add, so it ranks below anything currently running `[Later]`
    {cpu: none, dev: medium}
  - ~~**`rankAgentIsDeterministic` misclassifies every `time=` agent, and P1 makes it
    worse.** It derives determinism from whether an agent draws `rand()`, and a `time=`
    agent draws none, so `pairGameTarget` pins any two of them at exactly 2 games as
    both floor and ceiling. But a wall-clock search stops where machine load puts it,
    so those 2 games are SAMPLES of a noisy process, not replays of one game. Measured
    2026-09-01 on the fixed binary: `rank.exe determinism --replicas 3 --only
    "time=150ms"` exits 2 with 31 of 34 subject-colours reproducing, and the 3 that do
    not are exactly the ones where the budget BINDS -- both colours of the new
    `ab(deep=12,tt,ord,time=150ms)@2.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`
    and White of the `deep=6` model=113 core. Since P1's whole purpose is to make the
    budget bind on every core, the `deep=12` time track is genuinely stochastic and its
    pairs need real game targets. Theory 59 in `Docs/theories.md`. Changing the
    classifier raises those pairs' targets and re-opens the 3 time-track
    certifications, so it is a deliberate scheduling decision, not a slip-in fix~~
    Classifier fixed 2026-09-02: `rankAgentIsDeterministic` now checks `timeBudgetMs > 0`
    first (`src/ranking.cpp`). The raised game targets and the 3 category
    re-certifications this unlocks are still open, tracked below (see "Re-certify the
    3 `time=150ms` categories")
  - **Re-certify the 3 `time=150ms` categories.** Their titles were decided under a
    budget that did not bind (see `TIME BUDGET NOT ENFORCED`, `Docs/corrections.md`).
    Holders are not vacated, but the three time rows in `ranking/CHAMPION.md` are not a
    wall-clock-normalized comparison until refilled and refit on the fixed binary
    `[Now]` {cpu: hours, dev: low}
  - ~~**`ranking/climb_roster.txt` is dead**: 8 of its 10 lines are `@1` identities
    retired by the TT version bump, so a hill climb today silently falls back to 2
    live opponents~~ Rebuilt 2026-09-01: 11 all-stochastic rungs spanning Elo 0 to
    1093, ceiling raised so a d6-head candidate is bracketed rather than sweeping
  - ~~**The `games=` field in every TD-Leaf model header is wrong.** Provenance is
    written before the training loop from `cfg.games`, and `tools/tdleaf_study.ps1`
    passes the ladder's last rung, so every checkpoint from one run claims the same
    count. slot169 says `games=4000` but trained on 1,500 (`ranking/roster.txt:421`).
    Confirmed twice, also on slot131. The openless x node champion's training cost
    is misstated 2.67x~~ Fixed 2026-09-01: provenance is now written after the loop
    from what was actually spent, and carries `secs=` and `nodes=` alongside `games=`.
    Checkpoints written before that date still carry the wrong count
  - **`pool_games` is not one regime.** The tag is emitted for any
    `teacher=replay:<path>` (`src/ranking.cpp:430`) and spans ~195 Elo between
    found-data replay (slot99, 934) and pairgen arms (slot96, 1129). Split into
    three regimes for any comparison `[Now]` {cpu: seconds, dev: low}
  - **position_elo's label store is discarded, not salvaged.** 41.7% of its
    1,247,684 games are `tt`-vs-`tt`, and 100% of its labels depend on a ratings
    snapshot frozen 2026-07-19 over a contaminated store. Developer decision
    2026-09-01: redesign the labeling campaign to fit a wall-clock budget
    (see `TT CROSS-AGENT CONTAMINATION`, `Docs/corrections.md`) `[Now]` {cpu: hours, dev: medium}
  - ~~Open questions carried to the plan's Part 5, needing developer answers before
    execution: the sigma head's fate, whether the time-track control drops `tt`,
    whether the fast-tanh leaf-tail optimization is in scope, what happens if a
    regime is still improving at the 8h rung, and whether contaminated-era
    checkpoints stay rostered~~ All 5 answered by the developer 2026-09-01, inline in
    the plan's Part 5

## Models (value head: board -> scalar)
- Convolutional NN value model (board as an 8x8xC grid; local spatial filters for walls/columns/forwardness) `[Later]` {cpu: days, dev: high}
- NNUE-style value model (efficiently updatable; should plug into the incremental `g_evalPos`).
  Concrete next step now that `MLPModel` (full-scan) ships and beats the linear PST ceiling on
  offline equal-material calibration (theory 24): make its FIRST layer an incremental accumulator
  -- widen the scalar `g_mlAcc` that the linear inner already maintains into a vector, and
  recompute only the hidden units touched by the 2-3 changed inputs per make/unmake -- so the MLP
  can compete at FIXED compute. Motivated directly by the residual-mlp results' full-scan Elo
  caveat (its per-node eval is strong but per-second it is handicapped). `[Next]` {cpu: hours, dev: medium}
- Residual skip design space (follow-up to the shipped HARD frozen chip skip; theory 24,
  `plans/residual-mlp-results-1-tingly-chipmunk.md`). The linear residual HELPED + stabilized
  equal-material calibration but the MLP residual was a WASH, so probe whether a softer/richer
  skip or more capacity changes that split: (a) SOFT / regularized skip -- a learnable material
  weight initialized at the material-only logistic fit and penalized for drifting, vs the hard
  frozen one (theory 24 Q1); (b) a BROADER hand-crafted baseline as the skip (e.g. the Advanced
  linear mix) instead of the literal chip differential (theory 24 Q2); (c) a wider / DEEPER MLP
  capacity sweep (2 hidden layers, more widths) to map where added capacity stops improving
  calibration. All measurable with the existing stratified-loss printout + the generalized
  `sweep_pst_v2.ps1` groups. `[Next]` {cpu: hours, dev: medium}
- Transformer value model (squares as tokens) -- teacher / label generator only, not in-search `[Dream]` {cpu: days, dev: high}
- ~~Incrementalize an ML model (e.g. MLP/NNUE) so a move recomputes only the few inputs it changed
  instead of the whole forward pass.~~ Shipped 2026-07-22 as the NNUE-style first-hidden
  accumulator for an MLP mu head (`g_mlAccDim`/`g_mlAccVec`/`g_mlL0ByInput`; the vector
  generalization of `g_mlAcc`). The first layer is maintained across make/unmake; only the
  remaining layers run per leaf. Measured **1.78x** per-node speedup for the wide head
  (256/128) at fixed depth (36.1 -> 20.2 us/node at d4), bounded by the first-layer share as
  predicted; the layers past the first ReLU are irreducible for a fixed architecture. The d6
  MLP rating was NOT actually blocked (it was already done last session, 720+ games/agent:
  dist_lin 1031 > MLPs 974/967/931, all below the champion -- theory 27 holds); the
  incremental agent is the same identity + eval-equivalent, so those numbers carry over.
  See `plans/nnue-incremental-mlp-results-1-crystalline-taco.md`. `[done]`
  - **Second-accumulated-layer (dead-ReLU delta).** Superseded by the sparse leaf-tail
    forward above for these heads: an op-count showed the delta approach is both slower here
    (it pays an update AND an undo every make, and the side-to-move bit flips every ply,
    perturbing many units, so the "changed" set isn't much smaller than the live set) and
    more complex (H2-dim reversible accumulator state) than just recomputing the
    already-sparse tail per leaf. Would only be worth it if the second layer were much wider
    than the live-unit count, or for a head where per-move churn (not just static sparsity)
    is the bottleneck. `[Dream]` {cpu: hours, dev: medium}
  - **Sparsity-training penalty** (`dist-value` L1 / non-clamped-fraction loss): now lower
    priority still -- the sparse leaf-tail forward already captures the ~90%-sparse heads'
    speed win without changing training at all. Would only matter if a future architecture
    trains denser. `[Later]` {cpu: hours, dev: medium}
  - **Set `/arch:AVX2` as the native-build baseline?** Would realize the general ~1.2x leaf
    speedup in production (helps the standard head we would actually use). Requires an AVX2 CPU
    (~2013+) for the shipped native binaries; not the web/WASM build. A one-line flag add per
    native build script + a portability note. Developer decision. `[Next]` {cpu: seconds, dev: medium}
  - **int16 quantization of the accumulator.** The full NNUE throughput recipe (32 int16 per
    AVX-512 register vs 16 floats), but it multiplies SIMD lanes by a constant too, so it will
    not change the shape ranking either -- only worth it for the general leaf speedup, and it
    breaks bit-identicality and needs a retrain or post-training calibration. `[Later]` {cpu: hours, dev: medium}
  - **Full multi-seed 32-game d6 campaign for the dist MLPs**, now that d6 is affordable
    (~0.57 s/move for the wide head): re-confirm the existing 720-game standings (dist_lin
    1031 > MLPs 974/967/931) hold at tighter error bars, now that cost is no longer a
    constraint on games/pair. `[Next]` {cpu: hours, dev: low}
- Joint value + policy + next-value model trained to minimize its own recomputation `[Later]` {cpu: days, dev: high}
  - With ReLU units, a hidden unit that remains clamped at 0 before and after a move contributes
    no changed downstream value (equivalently, the derivative through the ReLU pre-activation is
    0 except at the kink). So add a penalty (L0, or an L1/sigmoid surrogate) on the count of
    hidden units whose output changes across the move the policy head picks. The model must
    jointly learn the board value, the best move, and the successor value, because which units
    must recompute depends on which move is played: it co-learns playing strength and its own
    recomputation cost, preferring representations where the lines it likes are also cheap.
  - Caveats: the sparsity term must stay a light regularizer or the model will prefer cheap moves
    over good ones. Alpha-beta explores all moves at a node, so architectural locality (conv or
    locally connected first layer) is what bounds worst-case per-move cost, while the learned
    clamping cheapens the chosen lines on top. Training needs the Python track (a custom
    three-head loss is beyond the C++ SGD in `ml_train.cpp`); inference plugs into the same
    `g_mlAcc`-style accumulator seams shipped above.
- Position-oracle MLP variant (developer, 2026-07-21): a lighter-weight version of the sparsity
  idea directly above, scoped to the dist model specifically rather than a new joint
  value+policy architecture -- add a sparsity penalty (L1 on weights, or a penalty on the
  fraction of ReLU units NOT clamped to 0) to `dist-value`'s loss, so the trained mu/sigma
  heads naturally have more zero weights and more dead ReLU units. Directly reduces the
  incrementalization work above once it exists (fewer nonzero paths to touch per move) and may
  also cut the current FULL-SCAN cost on its own (skip zero-weight multiplies even without an
  accumulator) `[Later]` {cpu: hours, dev: medium}
- Position-oracle MLP variant, more left-right symmetry (developer, 2026-07-21): either double
  each training batch with mirrored copies of each board, tie mirror-pair weights together, or
  add a symmetry-forcing loss term. Developer's own prediction going in: this will NOT do as
  well Elo-wise on this specific pool, and that prediction is well-grounded -- theories 23/30/32
  found the pool has a real, exploitable left-file tie-break bias, and mirror-symmetrizing a
  linear outcome-trained model already cost real Elo (theory 30) rather than being a free
  variance cut. Interesting BECAUSE it is expected to fail on playing strength: this is a
  chance to test whether the same left-bias-is-real-signal finding holds for a model trained to
  PREDICT position strength rather than to play well, or whether prediction quality and
  playing strength diverge again here too (see theory 27, freshly reconfirmed by this
  campaign). Concrete design already proposed and unbuilt for the general case: theory 32's
  5-way unflipped/flipped/averaged/left-onto-both/right-onto-both comparison, reusable for the
  dist model's mu head with `mlv2MirrorIndex` `[Later]` {cpu: hours, dev: medium}

## Models (policy head: board + move -> score / move-rater)
- MLP policy `[Later]` {cpu: hours, dev: medium}
- Transformer policy `[Dream]` {cpu: days, dev: high}
- Softmax / temperature sampling over move scores (for exploration + diverse self-play) `[Now]` {cpu: minutes, dev: low}
- Repurpose the position-oracle's labeled data for a policy signal (developer question,
  2026-07-21: "is that just the same thing?"). Answer: not quite, but three real, distinct
  paths exist, in order of how much new work each needs: (1) NOTHING new to build -- the dist
  model's mu head already scores positions, and running it through the existing Greedy explorer
  (1-ply argmax over an evaluator) or AlphaBeta already turns "which move leads to the
  highest-mu resulting position" into a policy for free, this is just using the model the way
  every other value model already gets used, not a new signal; (2) a genuinely NEW signal:
  instrument the labeler to record which move each ladder agent actually played from a labeled
  position (currently `rankLabel` plays to conclusion and keeps only the outcome, discarding
  the move), then train a move-rater on (position, move actually played by a HIGH-Elo ladder
  rung, weighted by the position's own measured mu/sigma) -- this is a real new dataset the
  raw store cannot currently produce without a labeler change, distinct from a value model's
  1-ply lookahead; (3) policy distillation -- train a fast direct move-scorer to mimic what
  "argmax over the dist model's mu across every legal move" would pick, without paying for the
  per-move mu evaluation at inference time. (2) is the interesting new-signal answer to the
  question as posed; (1) and (3) are real but are repackaging the existing model, not a new
  learning signal `[Later]` {cpu: hours, dev: medium}

## Models (difficulty head: board -> how hard is this position)
- Blunder labeling by branch replay: play a game between two strong deterministic
  agents, rewind to a move by the eventual winner, substitute a random different move,
  and replay from there. If the winner changes, label the substitute a blunder. Also a
  per-turn difficulty probe: turn difficulty = how few of the legal moves preserve the
  win. `[Later]` {cpu: hours, dev: medium}
- Position difficulty as a learned target: position difficulty = an aggregate of this
  turn's and the following turns' difficulty. A strong move lowers your future
  difficulty; a risky move raises it while still winning. Computing it exactly needs
  near-exhaustive tree exploration (intractable except trivial endgames), which makes
  it a natural ML target: generate labels by branch replay / sampling where it IS
  computable, train a model to predict it anywhere. `[Later]` {cpu: hours, dev: high}
  - Uses: difficulty-aware move choice (prefer low-difficulty winning lines against a
    tricky opponent), rating the difficulty of puzzles/positions for humans,
    difficulty-aware time allocation in search.

## Adversarial / Opponent-Modeling Agents
- Counter-agent trained against one specific opponent: mine that opponent's alpha-beta
  search for moves it pruned early (cut off before full evaluation) and check whether
  continuing into that line actually wins. Train a policy that preferentially steers
  into an opponent's under-explored lines. Deliberately overfits to one opponent's
  build/pruning behavior, so it is fragile against everyone else by design. `[Later]` {cpu: hours, dev: high}
  - Purpose is not a standalone strong agent: keep counter-agents in the rank.exe pool
    on purpose. They force every other agent, especially the reigning #1, to be robust
    not only to raw strength but to opponent-specific exploitation, the same way the
    dilution ladder forces robustness to weaker/noisier play.

## Board-State Evaluators (BSEFs)
- Ensemble / blended evaluator (average or weighted mix of several evaluators/models) `[Later]` {cpu: hours, dev: low}
- Phase-conditioned mixture of experts: a lightweight router (start with a hand-fixed classifier
  on total material/piece count: high piece count = opener, low piece count = endgame, graded
  band in between) that dispatches evaluation to a phase-specialized model (separate opener/
  midgame/endgame value or policy models) instead of one model covering the whole game. Distinct
  from the Tapered/phase-split PST idea in Training Regimes below, which smoothly interpolates
  ONE linear model's weights by piece count: this is hard routing between genuinely separate
  models (potentially different architectures per phase), a phase-conditioned specialization of
  the Ensemble/blended evaluator idea above (routing instead of uniform blending) and akin to the
  "separate opener model" idea in the Agent Track above. Router could start as the fixed
  material-band classifier and later become learned/soft (e.g. blending adjacent-phase experts
  near the boundary instead of a hard cutoff). Motivated by the developer's hypothesis that
  Breakthrough has genuinely distinct phases with different best strategies, not just a smoothly
  varying one. See theory 25, `Docs/theories.md` `[Now]` {cpu: days, dev: high}

## Heuristic Evaluator Feature Ideas (Classic / Experimental)
New candidate terms for the hand-crafted evaluators, alongside the existing chip/wall/column/
forward mix. ~~Each is a single-place edit in `g_evaluators` (`src/ai_eval.cpp`) plus wiring the
term into the incremental `evalPosLocal` delta the same way wall/column/forward already are.~~
Batch 1 shipped 2026-07-11 as the **Advanced** evaluator (`adv`, 16 params, all terms below
plus the D14 RaceWin detector; see `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md`).
- ~~Reward defended pieces: a diagonal-adjacency analog of the existing wall (orthogonal) and
  column (same-file) structure terms, since a diagonal neighbor is what actually recaptures
  after a capture in this game~~ Shipped (Support, `d`), merged with the phalanx idea below --
  both describe the same diagonal-pair geometry; a saturating per-piece variant stays open
- ~~Race-distance differential: (opponent's closest piece to your back rank) minus (your closest
  piece to theirs), a cheap proxy for who wins a pure race, ignoring tactics.~~ Shipped (Race,
  `r`, from incrementally maintained per-row piece counts), plus a stronger sibling: RaceWin
  (`g`), the exact D9/D14 decided-race sentinel detector (proven sound, ~8% us/move, no
  measurable Elo at d6 -- theory 21). The capacity generalization is settled analytically:
  capacity advantage == forwardSum - 7*chipDiff exactly (code-verified identity), so it is
  linearly dependent on existing terms and was NOT added as a weight; theory 18's
  outcome-correlation half stays open, `capacityWhite/Black()` are the helpers for it
- Per-term incremental routing: let each Advanced term declare whether it is maintained in
  `g_evalPos` or recomputed at the leaf based on which weights are enabled, so sparse mixes
  (e.g. chip+mobility) stop paying delta overhead (from the ladder pricing above) `[Next]` {cpu: seconds, dev: low}
- "Cluster": a Wall/Column variant restricted to the middle rows only (excluding the 2 rows
  nearest each side's home row and the 2 nearest the goal row, avoiding overlap with the
  existing Hole/Control/RaceWin terms that already own that territory). Motivated by theory
  31 (`Docs/theories.md`): quiescence may induce a "posturing" style (deferring an even trade
  until it lands exactly at the search horizon, since only pending-capture leaves get a
  deeper look), and a middle-only clustering term might reward the same pattern statically.
  Test by hill-climbing the Advanced weight mix twice, once with `qs` off and once on, and
  checking whether Cluster's (and Race/RaceWin's) climbed weight shifts between the two runs
  `[Next]` {cpu: hours, dev: medium}

## Move Choosers / Policies (direct, no search)
- Greedy-by-eval (1-ply pick of the move maximizing a BSEF) **(P1, via the Greedy explorer)** {cpu: seconds, dev: low}
- Softmax/temperature sampling policy (probabilistic move choice) `[Now]` {cpu: minutes, dev: low}

## Move-Tree Explorers (search)
- MCTS / PUCT (pairs a policy head with a value head). Shipped 2026-08-16 as
  Gumbel MCTS (Danihelka et al. 2022; `Docs/works-cited.md`): the `GumbelMCTS`
  explorer (`src/ai_gumbel.cpp`, Gumbel-top-k root sampling + Sequential
  Halving) and the `joint` value+policy model type (`src/ml_model.h`),
  `gaz(sims=N)@1` in the roster ID grammar. **Self-play trainer (Pass 1
  sanity) shipped 2026-08-17** as the `gumbelzero` regime
  (`src/ml_gumbelzero.cpp`): value head trained against
  `GumbelRootInfo::searchValue` (the search's own improved value estimate,
  bootstrapped, not game outcome -- the developer's stated preference) and
  policy head against `gumbelImprovedPolicy`'s Gumbel-improved target,
  strictly online updates drawn from a replay buffer, from scratch only,
  linear heads only. Validated by unit tests (`tests/test_gumbelzero.cpp`)
  and a manual 40-game run published to `models/sweep/slot650.txt`/
  `slot651.txt` (regime tag `gumbel_self`), round-tripping through
  `rank.exe check`. **Still no Elo measured** -- Pass 1 is sanity only, per
  `Docs/model-training-playbook.md`'s certification gate this agent must not
  be described as strong or promoted until Pass 2 (a broad hyperparameter
  sweep, not yet scoped) and Pass 3 produce a checkpoint that clears a
  full-roster refit. See `ML.md`'s "Gumbel MCTS" section for the full
  picture `[Now]` {cpu: seconds, dev: high}
  - ~~**Pass 2 (broad sweep)** shipped 2026-08-17
    (`plans/gumbel-mcts-results-3-amber-thicket.md`): Round A (21 draws,
    random search over sims/lr/l2/replay/batch/open-plies, 1 seed, rungs
    100/400/1500) + Round B (top 8 draws seed-replicated x3, extended to
    rung 4000) + a compute-matched sims follow-up. Findings: sims has an
    interior optimum under fixed training compute (100-400 productive, 25
    too low, 800 too high -- theory 48); l2=0.0 is the best value found and
    survives a sims-confound check; lr and open-plies show apparent trends
    that are confounded with sims in the single-seed sample and are NOT
    settled; replay-capacity/warmup and batch-size show no clean trend.
    Leading configuration (R17: sims=200, lr=0.03, l2=0.0,
    replay=8000/128, batch=8, open=8) still rising at rung 4000 in 2 of 3
    seeds -- ceiling not reached. **Still screening-level only** (pinned
    fits, never an unpinned full-roster refit) -- no Elo is certified. A
    direct 32-game match of R17's best checkpoint against the plain
    `ab(deep=6)@1.classic(chip=100)@2` chip counter went 32-0 to the chip
    counter (both colors), at roughly 1/10th the compute per move, so this
    regime remains far from competitive even against the simplest
    non-learned baseline.~~ `[Done]` {cpu: hours, dev: high}
  - ~~Exposed `ai_gumbel.cpp`'s search-shape constants (root Gumbel-top-k
    breadth, `c_visit`/`c_scale`) as per-agent roster knobs, shipped
    2026-08-17: `gaz(sims=N,cvisit=C,cscale=S,m=M)@1`, only appended when
    non-default (50/10/16), no version bump needed. Implemented as new
    `g_gumbelCVisit`/`g_gumbelCScale`/`g_gumbelRootM` globals
    (`globals.h`/`.cpp`), set/restored by `agentChooseMove` from new
    `AgentSpec` fields, mirroring `AlphaBeta`'s `g_useTT`-style convention
    exactly (no signature changes needed anywhere, including
    `gumbelSearch`/`gumbelExplore`). Serving-time only in this pass: the
    Gumbel-Zero self-play trainer (`ml_gumbelzero.cpp`) still trains against
    the paper defaults, unaffected. Knob-validation test confirms `m=1`
    visibly narrows the root candidate set vs `m=16` under an identical sim
    budget.~~ `[Done]` {cpu: hours, dev: high}
  - ~~**Swept cvisit/cscale/m** (theory 49, `Docs/theories.md`), shipped
    2026-08-17: 5-round screening investigation on the R17/rung-4000
    checkpoint (slot746) at `sims=200`, self-contained round robins (never
    the canonical ladder), 3,440 games total. Found: `m<=2` costs 300+ Elo
    regardless of `cvisit`/`cscale` (not confounded -- a wide range was
    tried at `m=2`, all scored low); with `m=16` fixed, `cvisit` and
    `cscale` both independently rise past the paper defaults (50/1.0); Round
    5 rated the leading candidates together in one shared fit and found a
    broad, statistically flat plateau at the top rather than one sharp
    point -- `cvisit=1000,cscale=10.0` (893), `cvisit=500,cscale=5.0` at
    `m=16` (879) and `m=8` (875) all within one error band of each other,
    beating REF (793) by 85-100 Elo, non-confounded. Recommended default:
    `gaz(sims=200,cvisit=500,cscale=50)@1`. One checkpoint, one `sims`
    value -- not yet checked for generality across checkpoints.~~ `[Done]`
    {cpu: minutes, dev: medium}
  - **Pass 3 (optimize)**: not yet scoped. Candidates: an isolated
    fixed-sims lr sweep to settle the lr/sims confound from Pass 2,
    extending the Round B ladder past rung 4000 for R17/R3/REF (none had
    plateaued), and checking whether the cvisit=500/cscale=5.0 corner found
    above generalizes to other checkpoints/sims values, or is specific to
    R17 `[Next]` {cpu: hours, dev: high}
  - **Round 5 (joint training x search-shape sweep)** shipped 2026-08-18
    (`plans/gumbel-mcts-results-5-violet-harbor.md`): 101-draw random search
    (linear architecture only) x 4 rungs = 404 checkpoints, screened pinned
    against `ranking/roster_screening_pool.txt`. **Round 6 (mlp/conv
    architecture sweep)** shipped 2026-08-22
    (`plans/gumbel-mcts-arch-results-6-silver-thistle.md`): 92-draw random
    search over `--model-type linear|mlp|conv` (45 mlp + 45 conv + 2 REF, x4
    rungs = 368 checkpoints), same screening pool/method. mlp clearly beats
    conv at the population level (median 713 vs 685, max 1023 vs 806) and
    appears to beat round 5's best linear checkpoint (816). A follow-up
    3-seed replication (`tools/gumbelzero_arch_seedcheck.ps1`) found the
    single best mlp draw's 1023 Elo does NOT hold up (regression to the
    mean, 5 of 5 replicated blocks), but the architecture-level finding
    survives. Top 4 candidates by 3-seed mean, all within the seed-noise
    band of each other: `gaz(sims=500,cvisit=800,cscale=70,m=20)`+joint
    mlp32 (M34, mean 971.7), `gaz(sims=300,cvisit=600,cscale=100,m=12)`+
    joint mlp64 (M14, 955.0), `gaz(sims=500,cvisit=800,cscale=70,m=8)`+
    joint mlp64-32 (M10, 939.7), `gaz(sims=300,cvisit=1000,cscale=40,m=8)`+
    joint mlp32 (M31, 937.0). **Opener-division follow-up shipped same day**
    (`tools/gumbelzero_arch_opener_check.ps1`,
    `plans/gumbel-mcts-arch-opener-agents-6-silver-thistle.tsv`): wrapped all
    90 rung=4000 checkpoints with one opener per CHAMPION.md's 4 non-openless
    categories (book=15/16, rand moves=4/8) and screened all 360 variants the
    same pinned way. Result: the same top-4 (M34/M14/M10/M31) lead every
    division, no reshuffling, and mlp still beats conv in all 4 (one seed
    per variant, not seed-replicated under openers yet). M34/M14/M10/M31
    registered in `ranking/roster.txt` 2026-08-23 as `off` (benched, no new
    games) so the identities exist ahead of a future push. **Still not
    certified**: every number above is a screening-level pinned fit against
    the one hand-picked screening pool, never touching the other 135 roster
    agents or the d8/nb2m oracle -- a Workflow B unpinned refit (flip to
    `on`, run an unpinned `rank.exe rate`) is the remaining step if the
    developer wants to pursue certification, not yet started. **Peak-Elo /
    speed / efficiency predictors** (`analysis/predict_peak_elo.py`,
    extended 2026-08-23 to accept any target + `--minimize` + categorical
    features) run over the round-6 sweep: `modeltype` and `l2` dominate Elo,
    `modeltype` alone (mlp ~7ms/move vs conv ~91ms/move) dominates speed,
    and the Elo-optimal (`lr`=0.003) and efficiency-optimal (`lr`=0.01,
    `sims`=300) recipes diverge, unresolved pending a controlled follow-up
    -- see the results doc's new predictor section. **Conv-capacity
    follow-up shipped 2026-08-23** (new `--policy-mlp` flag,
    `src/ml_gumbelzero.h`/`.cpp`, `tools/train_main.cpp`, decouples the
    policy head's architecture from the value head's): round 6 gave every
    mlp draw nonlinear hidden layers on both heads but every conv draw a
    direct-linear value readout and an unconditionally-linear policy head.
    Fixing both on round 6's best conv recipe (block C15) raised its 3-seed
    mean at rung=4000 from 748.7 to 953.0 (+204.3, super-additive over the
    individual fixes' +72/+65), landing inside the seed-corrected mlp
    top-4's own range (937.0-971.7) -- best single checkpoint 1003 +/- 20,
    not yet added to `ranking/roster.txt`. Theory 52, `Docs/theories.md`.
    Training is confirmed game-count-paced not wall-clock-paced, and
    round 6's conv population was already flat (not still rising) from
    rung 1500 to 4000, so undertraining from conv's slower forward pass is
    ruled out as the explanation `[Now]` {cpu: hours, dev: medium}
  - **GAZ budget instrumentation is designed but not implemented.** The
    reference-class rule (revised 2026-08-23) already lets a `gaz(...)` agent
    compete for a category title once certified, but `gaz()` has no node- or
    time-budget concept yet -- only a simulation-count budget. A concrete
    design (cut Sequential Halving only at round boundaries, never mid-round,
    to preserve its fairness guarantee; node/leaf counters mirroring
    `ai_minimax.cpp`'s `nodes++`/`leafs++` convention; `nodes=`/`time=`
    grammar additions to `gaz()` reusing `ab()`'s existing machinery) was
    scoped 2026-08-24 in
    `plans/champion-category-restructure-plan-1-golden-painting-anchor.md`'s
    appendix -- not yet built. Needed before any `gaz(...)` agent can enter a
    category, independent of the Pass 3 certification item above `[Next]`
    {cpu: minutes, dev: medium}
  - **AB's `time=` budget does not correctly cap wall-clock cost for
    expensive-per-node evaluators.** Discovered 2026-08-24 while building the
    `x time` categories: two wide-MLP cores (`model=111`, `model=113`) run
    477-500 ms/move against a `time=150ms` budget, 2-3.3x over. Root cause:
    `src/ai_minimax.cpp`'s `budgetTripped()` checks the wall-clock deadline
    only once every 4096 nodes (`(nodes & 4095ULL) == 0`, a deliberate
    `Clock::now()`-overhead tradeoff), and the outer iterative-deepening loop
    (`miniMaxWhite`, the `for (int d = 1; d <= depth; d++)` loop) has no check
    before starting a new depth iteration -- so an expensive-per-node
    evaluator can start a whole iteration with no time left and run well past
    the deadline before the coarse in-recursion check catches it. Cheap
    evaluators are unaffected (confirmed: all 12 normal-cost cores in the same
    cohort stay under budget). Does not change any `CHAMPION.md` category's
    declared champion (the two affected cores are not top-2 anywhere), but the
    `x time` track's compute-parity guarantee is broken for those two cores'
    rows until fixed. Cheapest fix identified but not implemented: add a
    check before committing to the next depth iteration (mirrors the
    round-boundary check already designed for GAZ above), which would cut the
    worst-case overshoot (an entire doomed iteration) without touching the
    existing per-node check's granularity. See `ranking/CHAMPION.md`'s Summary
    section for the full writeup and measured numbers.
    ~~**FIXED, and the fix is `nextIterationFits` (src/ai_minimax.cpp:128),
    which is the proposed before-the-next-iteration check, already built and
    called at lines 520 and 696.** It is gated on `s_timeOn` alone, so it covers
    every time-budgeted search whether or not the agent carries `retain`.
    Confirmed by re-measuring the exact head and cores that defined the defect,
    2026-09-09, 4 boards against an `ab(deep=6,tt,ord,nodes=200k)@3` opponent,
    29 subject moves each:~~

    | head | core | ms/move | of flag | worst single move |
    |---|---|---|---|---|
    | `ab(deep=6,tt,ord,time=150ms)@3` | `classic(chip=100)@2` | 6.0 | 0.04x | 14.1 |
    | `ab(deep=6,tt,ord,time=150ms)@3` | `position_elo mlp model=113` | 6.9 | 0.05x | 15.9 |
    | `ab(deep=12,tt,ord,time=150ms)@3` | `classic(chip=100)@2` | 68.2 | 0.45x | 150.0 |
    | `ab(deep=12,tt,ord,time=150ms)@3` | `position_elo mlp model=110` | 64.3 | 0.43x | 150.1 |
    | `ab(deep=12,tt,ord,time=150ms)@3` | `position_elo mlp model=113` | 65.6 | 0.44x | 150.0 |
    | `ab(deep=12,tt,ord,time=25ms)@3` | `classic(chip=100)@2` | 13.0 | 0.52x | 34.7 |
    | `ab(deep=12,tt,ord,time=25ms)@3` | `position_elo mlp model=113` | 12.0 | 0.48x | 34.6 |

    `model=110`/`model=113` on `ab(deep=12,tt,ord,time=150ms)` now spend 64-66
    ms/move where the 2026-08-24 measurement recorded 477-500, and their worst
    single move is 150.1 ms against the 150 ms flag. The mean never exceeds the
    flag on any bare head. Independently, Round 4's own 562,952 rows put all 33
    cores on `ab(deep=12,tt,ord,retain,time=25ms)@3` between 16.2 and 18.6
    ms/move, a 1.15x cross-core spread, so the time track's compute-parity
    guarantee holds in live play too. **Remaining work is not this bug**: retire
    the `TIME BUDGET NOT ENFORCED` banner and re-check whether the two cost
    flags in the refutation-oracle target list are still warranted
    `[Next]` {cpu: minutes, dev: low}
  - **The `retain` purse, not the deadline check, is what now exceeds a time
    flag on a single move.** Same measurement as above. `retain` banks the whole
    unspent remainder with no ceiling (`src/agents.cpp:164-171`), so one move may
    spend several flags' worth while the per-GAME total stays bounded, which is
    the documented contract. Measured worst single move: `time=150ms,retain` with
    `tdleaf_self lin model=169` hit **553.7 ms, 3.7x the flag** (mean 141.0,
    0.94x), and `time=25ms,retain` with `classic(chip=100)@2` hit **95.6 ms,
    3.8x** (mean 21.9, 0.88x). Bare heads by contrast top out at 1.00x (150ms)
    and 1.39x (25ms), the latter being the granularity of the every-4096-nodes
    deadline test. This is the number the "cap the purse" decision below needs:
    decide whether a single move may spend 3.8 flags, and if not, cap the purse
    at a small multiple `[Now]` {cpu: none, dev: low}
- TT speedup is currently node-count-real but wall-clock-muddied by `positionKey`'s per-node string build; an incremental Zobrist hash would make the TT a wall-clock win too `[Next]` {cpu: seconds, dev: low}

## Training Regimes
- Eval-blended labels: label each position with lambda*outcome + (1-lambda)*sigmoid(teacherEval/scale)
  instead of outcome alone. The teacher already computes a root search score every move and throws
  it away; blending turns one noisy bit per game into a real-valued signal per position (the NNUE
  training recipe) and should also improve move ordering, where the PST prunes 3x worse than
  Classic `[Now]` {cpu: hours, dev: medium}
- ~~Weight symmetrization + seed-ensembling for linear models: after training, average each weight
  with its left-right mirror (exact symmetry projection, free variance cut), and average the
  weights of K seed-replicas (for a linear model the ensemble IS the average). Directly attacks
  the measured 50-150 Elo training-seed noise~~ Shipped 2026-07-17 as `train.exe ensemble`
  (`--models` list, `--mirror 0|1`). Verdict: REFUTED for playing strength (theory 30,
  `plans/dethrone-champion-results-4-wiggly-mitten.md`). Mirroring the champion's own weights
  cost 135 Elo (1079 -> 944), the 6-seed mirror ensemble 144 vs the seed mean -- likely because
  a symmetric model's extra evaluation ties get broken by the directional first-found rule
  (theory 23) and the learned asymmetry is doing useful anti-pool tie-breaking (theory 19), not
  just noise. Open follow-ups: a pure-average (mirror=0) control to isolate averaging from
  mirroring, the calibration-vs-strength check (theory 27 parallel), and seed SELECTION by Elo
  instead of averaging (the best of 6 seeds is +28 over the champion) `[done]`
  - Follow-up (theory 32, `Docs/theories.md`): is the harm from asymmetry PER SE being removed,
    or from removing THIS SPECIFIC learned asymmetry (fitted to how this pool's shared
    left-file tie-break bias, theory 23, actually plays)? Extend `train.exe ensemble`'s
    `--mirror` flag with 3 more modes beyond off/average -- flip (full reflection, no
    averaging), left-onto-both (copy each mirror pair's left-column value onto both squares),
    right-onto-both (same, mirrored) -- and rate all 5 variants (unflipped/flipped/averaged/
    left/right) against each other. If flipped ~ unflipped >> averaged, asymmetry itself is
    what matters, not its direction; if unflipped >> flipped, the specific learned direction is
    fitted to something real about the pool `[Next]` {cpu: hours, dev: medium}
- Extraction quality controls in rank.exe extract: --min-elo floor or Elo-confidence weighting
  (label quality), --exclude held-out agents (measure pool-style overfitting by comparing Elo vs
  held-in against held-out opponents; low risk for linear models, must exist before MLP/NNUE),
  and positionKey-based dedup / repeat capping (openings are massively overrepresented) `[Next]` {cpu: hours, dev: medium}
  (partly superseded: the position-oracle pipeline sidesteps found-data label quality entirely
  by playing designed fresh games per position; posgen ships the positionKey dedup for pools.
  extract's own controls still matter for the outcome-label training path)
  - Test the low-Elo-data-quality hypothesis (theory 26, `Docs/theories.md`) using the --min-elo /
    Elo-filter controls above: retrain the existing value-model recipes on replay data (a) EXCLUDING
    low-Elo agents' games, (b) EXCLUDING mixed high-vs-low games, (c) EXCLUDING high-Elo games (the
    control), and compare the trained models' Elo. Also try an Elo-weighted reward (stronger label
    signal from higher-Elo games). Use seed replicas so the deltas clear the training-seed noise
    band (theory 8) `[Next]` {cpu: hours, dev: medium}
- ~~Vs-champion training regime (first pairgen study): train value models on games
  involving the reigning champion, sourced every plausible way (learner vs champ,
  diluted champ vs champ, oracle vs champ, champion-loss cherry-picks, branch-mined
  winning lines), and compare against the replay/self-play baselines.~~ (done, see
  `plans/vs-champion-training-results-1-cozy-forest.md`. Headlines: diluted-champion
  vs clean-champion games are the best value-training data found so far (beats
  replay), oracle-vs-champ close behind, and the best model ties the champion at d6
  (1137 vs 1140). Cherry-picked one-sided datasets (champloss, branch-wins) fail from
  degenerate labels, and a d2 generator on one side drags data quality below the
  self-play control. Theory 1 (out-of-distribution fragility) refuted in the current
  pool, Theory 2 (dilution data can't approach the champ) refuted on strength but
  head-to-head unresolved at n=8.) Standing longitudinal check: after each future
  batch of diverse agents joins the pool, re-run `tools/train_vs_champion.ps1
  -AnalysisOnly` to re-test the out-of-distribution theory `[Now]` {cpu: minutes, dev: medium}
- Tapered / phase-split PST: separate opening/endgame weight tables interpolated by piece count
  (piece count changes only on capture, so it stays fully incremental). The natural capacity step
  before MLP `[Next]` {cpu: hours, dev: medium}
- PV/leaf position harvesting: train on positions from inside the teacher's search tree labeled
  by subtree value, matching the off-path distribution the eval actually sees in search `[Later]` {cpu: hours, dev: medium}
- Active / hard-example mining: oversample positions where the current model most
  disagrees with the teacher label or with a deeper search, instead of uniform
  sampling `[Later]` {cpu: hours, dev: medium}
- ~~TD-Leaf(lambda) self-play bootstrap (value)~~ Shipped 2026-07-29 as `src/ml_tdleaf.cpp`
  + `train.exe tdleaf`. The project's first ONLINE, bootstrapped value regime: the target for a
  position is the model's own evaluation of a later position backed up through the search, applied
  at the principal-variation leaf, with weights moving during play. lambda=1 provably reduces to
  outcome-supervised training on PV leaves (unit-tested closed form). PV leaves come from TT probes
  along the played line, so `ai_minimax.cpp` is untouched and no rated agent's us/node changes.
  Strength MEASURED at screening level 2026-07-29 (38-agent cohort, `tools/tdleaf_study.ps1`,
  pinned fit at 8 games/pair, `plans/tdleaf-results-1-amber-pangolin.md`): peaks at **1051 vs its
  791 initialisation, +260 Elo**, at the same 130-param linear architecture and head. The
  lambda=1 control (provably == outcome-supervised on PV leaves) scored 713, BELOW the init, so
  the gain is specifically the BOOTSTRAP. Game count has an interior optimum ~1000 and declines
  past it (-36, all 4 seeds same sign). NOT certified: a pinned fit cannot dethrone and 8
  games/pair is half the standard `[Now]` {cpu: seconds, dev: low}
- ~~**Certify the TD-Leaf peak.** Append the top rungs to `ranking/roster.txt`, fill contenders to
  32 games/pair, run an unpinned refit (`Docs/ranking-workflow.md` Workflow B). Best screening
  agent is `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=131,18bfb7a0,tdleaf_self,lin,shape=129-1)@1` (A-base seed
  1001 at 1000 games) at 1056, vs the pinned openless champion's 1012~~ Done 2026-08-01, though
  via a different route than planned: the certification refit happened as a side effect of
  dropping the Pass-2 screening games from the fit. A TD-Leaf agent,
  `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1`, **took the
  openless title** at 1044 +/- 11 over s76's 1007 +/- 8, boosted to 0 pending at 32 games/pair.
  See `ranking/CHAMPION.md`, which records two caveats: 32 rows/pair is ~22.6 DISTINCT games/pair
  (0.706 distinct/row measured), so the gap is 2.3 combined SE rather than 2.7; and s169 gained
  the title while LOSING 36% of its games, which is not understood `[Now]` {cpu: seconds, dev: low}
- **Still unexplained: why s169 GAINED the openless title while losing 36% of its games.**
  The chip counter's fall is now accounted for, its rise is not `[Now]` {cpu: hours, dev: high}
- **Decide whether `rate --regime-balanced` should become the canonical fit** `[Now]` {cpu: seconds, dev: high}.
  Implemented 2026-08-01 and writing `ranking/*_balanced.*`; not promoted, because promoting
  it re-certifies every champion. It moves the table a lot: at head `ab(deep=6,tt,ord,nodes=200k)@1`
  the openless order becomes s602 / s169 / s76 / s349 / classic@2 (1259 / 1249 / 1240 / 1239 /
  1213, all +/-8 except classic's +/-14), i.e. a 4-way statistical tie at the top and
  **`classic(chip=100)@2` back to rank 5 from rank 35**. Open questions before promoting:
  whether the anchor (`rand@1`, regime `nonlearning`, 5 agents) should be exempt from
  balancing; whether SEs remain interpretable after reweighting (they are effective-sample
  SEs now); and whether category champions should be declared on the balanced or pooled fit
- **Old (superseded) framing of the chip-counter question, kept so the reasoning is not
  re-derived:** `s169` rose to the openless title on 36% fewer games, while
  `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2` fell from openless rank 2 to rank 35 under the
  same change. Bradley-Terry accounts for opponent strength, so "it farmed weak candidates" is
  not an explanation and should not be repeated as one. Candidate tests: refit with only the
  cohort games REMOVED from classic@2's record but kept for everyone else; check whether the
  cohort was a non-transitive matchup for classic@2 specifically (head-to-head vs the 9 dropped
  candidates); check whether the effect is connectivity (pairs lost) or volume (games lost) by
  topping the surviving pairs back up to the old game counts `[Now]` {cpu: hours, dev: medium}
- Bracket the TD-Leaf game-count peak: the ladder jumps 500/1000/2000 so the optimum is located
  only within 2x, and the post-peak decline is unexplained. Add 700/1400 rungs + a decayed-lr arm `[Next]` {cpu: hours, dev: low}
- Extend the TD-Leaf from-scratch arm past 2000 games: it was still climbing (+134 from 500->2000)
  while champ-init had already peaked, and the gap had narrowed from -222 to -103 `[Next]` {cpu: hours, dev: low}
- Re-run the TD-Leaf lr and generator-depth arms at 4+ seeds: both are n=1, so the lr ordering and
  the d4 == d6 equivalence (theory 44) are suggestive only `[Next]` {cpu: hours, dev: low}
- Run the TD-Leaf batched-update path (`--batch`), implemented but never exercised; the
  online-vs-batched question is still open `[Next]` {cpu: hours, dev: low}
- Run a TD-Leaf MLP arm: the whole cohort was the 130-param linear model `[Next]` {cpu: hours, dev: medium}
- Backfill `Docs/hyperparameter-log.md`: only the `tdleaf` section is populated so far.
  Transcribe real values (not re-derived from memory) from `plans/training-sweep-results-1-luminous-snail.md`
  (the 78-candidate sweep's full axis list, `--gen-random`/`--gen-random-floor`/`--gen-random-decay-plies`,
  `--residual-skip`, `--val-split`), the position-oracle pipeline (posgen/label/labelfit ladder design,
  `--elo-se`, calibration sample size), and `tools/hill_climb.ps1`'s step-size/reset/flip-probability
  history in `plans/heuristic-eval-overhaul-results-1-buzzing-floyd.md` `[Next]` {cpu: seconds, dev: low}
- Population / other-play tournaments as a data source, including an evolutionary variant:
  each round, mutate the top-couple-Elo agents (unique random perturbations of their weights)
  into new agents, add them to the round-robin, drop the weakest, and iterate -- so the
  population crawls the weight surface by selection `[Now]` {cpu: days, dev: high}
- ~~Explore new agent types built from the mu/sigma distribution instead of just mu-as-eval:
  (a) SAMPLE a value from N(mu, sigma) at each leaf instead of using mu directly -- a
  genuinely new stochasticity source (the model's own learned uncertainty) distinct from the
  dilution/jitter mechanisms already in the ladder; (b) mean +/- k*sigma variants (an
  "optimistic" agent that scores mu+sigma, a "cautious" one at mu-sigma) -- ties directly to
  the sigma-as-search-time-signal idea two lines up (prefer high-sigma/high-upside lines when
  behind is exactly a mu+sigma-style leaf score). Needs a new accessor or explorer variant
  since `mlValueScore`/`mlLeafScore` currently read mu only (`mlValueScoreDist` already
  exposes both mu and sigma, so the plumbing exists, just not wired into move choice)~~ (b)
  shipped 2026-08-06 as LearnedValue's Risk weight (`mlValueScoreRisk`, ID `risk=<tenths>`,
  see `plans/risk-weight-plan-1-dusty-kestrel.md` / `risk-weight-results-1-dusty-kestrel.md`,
  theory 47). Screened on the s76 core only (developer scoping decision) at k in
  {-1.0,-0.5,-0.2,-0.1,0.1,0.2,0.5,1.0}: REFUTED cleanly, every nonzero k rated 66-228 Elo below
  the mu-only baseline (1007), monotonically worse with |k|, negative k less damaging than
  positive at matched magnitude. (a) sampling from N(mu,sigma) remains open, and so does
  screening (b) on the other 9 rostered position_elo cores and at k below 0.1 (see the results
  doc's Future Work) `[Next]` {cpu: hours, dev: medium}
- Activate --elo-se (rating-SE variance is plumbed, off by default); an adaptive second
  labeling pass (per position, add pairings whose gap centers on -mu_hat from the first
  labels, where sigma is best identified); relabel-free retrain after each future ratings
  refit (rerun labelfit + dist-value on the same raw stores, documented in ML.md) `[Next]` {cpu: hours, dev: medium}
- Position-oracle input-feature alternatives (developer question 2026-07-20, feature v2's
  129 raw piece-square bits are not the only reasonable encoding). Test candidates on the
  CHEAP linear model first via the same log-triplet position-count-sweep discipline used to
  find the ~425-1700 saturation point, since it isolates whether a feature helps independent
  of the architecture-capacity confound the MLP sweep just surfaced, before spending compute
  confirming a winner on the MLP:
  - Material/piece advantage as an explicit input (not a frozen skip). `matDiffFromFeatures`
    + `ResidualModel` already exist and were tested this way for the OUTCOME-trained linear/MLP
    value models -- refuted at 6 seeds (theory 24: the effect fell inside seed noise at every
    capacity). That result does NOT settle this case: theory 24 forced material in as a frozen
    additive skip bypassing the model, this idea is a plain feature the model can freely weight
    and combine with everything else, and it was tested where data was plentiful, not where a
    model is capacity-starved relative to its data (which the MLP position-count sweep just
    showed is true here for the mlp128-64/mlp32 config below ~6800 positions). Worth a fresh,
    honestly-scoped test, not assumed refuted by the old result `[Next]` {cpu: hours, dev: medium}
  - Ternary board encoding (-1/0/+1 per square, 64 features instead of 128) -- considered and
    NOT recommended, reasoning captured so it is not re-proposed blind: a single shared weight
    per square forces white-here and black-here to be exact negatives of each other, which is
    wrong for this game specifically (white advances row0->row7, black row7->row0, so e.g. row6
    is nearly-won for White and merely unadvanced for Black -- nowhere close to sign-flipped
    values on the same square). Would encode an incorrect symmetry and likely hurt, not help
  - Mover's-perspective (color-canonicalized) encoding: recolor every position to "my piece /
    empty / opponent piece" oriented to the mover's own forward direction, instead of literal
    White/Black. The rules genuinely have this symmetry (axioms.md color-swap symmetry) --
    DISTINCT from the LEFT-RIGHT mirror fold this pipeline deliberately does NOT apply
    (theories 30/32: that asymmetry is real pool-specific signal, not noise; do not conflate
    the two axes). Canonicalizing this way makes a White-played position and its exact
    color-swapped mirror the SAME input, roughly doubling usable data for free -- directly
    attacks the MLP's demonstrated data appetite. Untested anywhere in the project; the
    strongest candidate on this list `[Next]` {cpu: hours, dev: medium}
  - Forward-progress sum per side (Advanced evaluator's Forward term / the capacity axiom,
    `Docs/axioms.md` Lemma B) -- a race-tempo signal distinct from raw material
  - Per-row piece histogram -- the engine already maintains this incrementally (`g_rowCountW`/
    `g_rowCountB` in globals.h, used by the Race/RaceWin terms) so it costs nothing extra to
    also expose as a feature; gives race structure without inferring it from 128 raw bits
  - Support/structure count (Advanced's Support term, diagonal same-color adjacency, Lemma C)
    as a defensive/piece-safety proxy
  - Mobility (Advanced's Mobility term, legal-move count per side)
  - The decided-race sentinel (Advanced's Race/RaceWin, the exact D9/D14 detector) as an
    explicit input -- particularly relevant to the SIGMA head: a decided race should read as
    near-zero volatility, and handing that fact directly rather than making the model
    rediscover it could sharpen exactly the number this project cares about most
  - Ply / game-phase as an input -- already tracked in the position pool (`"ply"` field) but
    never fed to the model; volatility plausibly differs by phase (quiet openings, sharp
    midgames, near-decided endgames), so this may matter more for sigma than for mu
- Distillation from deep search or from a teacher model `[Later]` {cpu: hours, dev: medium}

## Weight optimization / geometry mapping
A single per-weight sweep is insufficient: each weight only matters RELATIVE to the others
(if chip=300, then forward 1 vs 2 vs 10 is indistinguishable), interactions are non-obvious
(forward may need to be HIGHER when structure is high, to offset the structure lost by
advancing; forward could even be NEGATIVE to keep pieces back and advance together), and the
optimum is a surface, not a point. Replace single sweeps with a search that maps the geometry:
- Report a response surface, not a single recommended value `[Next]` {cpu: hours, dev: medium}

## Strength Dilution (to spread an Elo ladder)
- Measure how often stochastic depth dilution (`dil(rP,dN)`) actually changes the move: for a
  sample of real positions, run both the full depth and the diluted shallower depth (e.g. depth
  6 vs depth 8, same node budget, tt+ord on, Classic eval) and compare chosen moves directly
  (diff the board before/after each search). Developer hypothesis to test alongside this: ODD
  dilution depths (5, 7) diverge from the full depth MORE than even depths (4, 6, 8), because an
  odd-depth search's leaf lands on the opponent's reply rather than after a complete move/response
  round trip, a horizon-style asymmetry -- compare odd vs even depth agreement rates to check.
  Motivated by a question about `dil(prob=15,deep=6)`/`dil(prob=30,deep=6)` (the position-oracle campaign's d8
  ladder rungs, `Docs/Memories/position-oracle-campaign.md`) but stands alone as a general
  dilution-quality question, deliberately deferred to its own session `[Later]` {cpu: minutes, dev: medium}

## Elo / Tournaments
- Recreate the `s9` linear v2 value model retired 2026-07-30 (`ranking/roster.txt`,
  `ranking/roster_top.txt`, `ranking/CHAMPION.md`): `models/sweep/slot9.txt` was
  accidentally overwritten by a test using an unverified slot number, and since
  `models/sweep/*.txt` is gitignored the original weights are gone. Its exact
  provenance (recipe/seed) lived in the file's own `teacher=` line and is lost with
  it, so this can only be a NEW agent trained to fill a similar role (linear v2,
  same head family), not a literal reproduction of the old weights/identity. Low
  priority: the roster is fine without it, and its historical Elo/match record
  already stands as-is regardless `[Later]` {cpu: hours, dev: low}
- **Same loss recurred for `models/sweep/slot6.txt` and `slot7.txt`, found
  2026-08-16.** Both are live, active (`on`) roster agents
  (`ab(deep=6,tt,ord,nodes=200k)@1.learned(model=6,eac8ab99,...)@1` and
  `model=7,c7f7ce61`). `tests/test_ranking.cpp`'s `rankLoadAgentModels` test
  uses those exact slot numbers as scratch (overwrites slot 6, deletes slot 7)
  to exercise its load-success/load-failure cases, with no check that they
  were already live -- the identical unverified-slot-number defect class as
  the `s9` entry above. Neither file is git-tracked, so neither is
  recoverable. Developer decision 2026-08-16: accept the loss the same way as
  `s9` rather than fix the test now. Historical Elo/match record for both
  agents stands as-is; no NEW games can be played for either until the roster
  lines are repointed at retrained models or the weights are restored from
  outside this repo. Full detail: `plans/gumbel-mcts-results-1-hidden-greeting-mist.md`
  ~~`[Later]` {cpu: hours, dev: low}~~
  ~~**Root-cause defect class (test picking an unverified slot number) fixed
  2026-08-16.**~~ The reserved-scratch-range mechanism below closes this
  category structurally: it is no longer possible for a test to pick a slot
  number that lands on a live roster agent, because the scratch range resolves
  to a directory (`models/scratch/`) no roster identity is ever written into.
  **Severity correction, same day:** the paragraph above undersold the
  consequence. Confirmed by running `rank.exe check` against the live repo: the
  hash mismatch on `models/sweep/slot6.txt` does not just block new games for
  the two affected agents, it makes `rankLoadRoster` reject `ranking/roster.txt`
  IN FULL (a single bad line aborts the whole parse, `src/ranking.cpp`), so
  every `rank.exe` subcommand that loads the roster -- `check`, `play`, `rate`,
  `gauntlet`, all of them -- currently fails outright. This has been true since
  the 2026-08-16 loss and was not previously verified end to end (see the
  project's own "Validate the instrument before quoting the reading" rule,
  `CLAUDE.md`). Repair requires a roster-data decision (repoint `model=6`/
  `model=7` at retrained replacements, or restore the original weights from
  outside this repo) that stays with the developer; not attempted here.
  `[Next]` {cpu: hours, dev: low}
- ~~**Give the rating path real sample diversity.**~~ Done 2026-07-26, via a second
  pool rather than by changing the first. `ranking/roster_open.txt` holds 14 agents
  each wearing `.opener(rand,moves=4)@1`, played with `rank.exe ... --paired-openings` into
  `ranking/matches_open.jsonl`. Result: **median distinct-trajectory ratio 1.000, min
  1.000** across all 91 pairs (the fixed-start pool is 0.438 median, 0.062 min), and
  error bars scale as 1/sqrt(n) exactly as independent samples should (median pm 53 ->
  38 -> 28 -> 20 at 4 -> 8 -> 16 -> 32 games/pair). Defect 3 does not exist in this
  pool. Rank-order stability across fills: Spearman rho 0.974 (4->8), 0.969 (8->16),
  0.987 (16->32), with agents changing rank 6, 6, then 3 of 14. Converging but not
  converged at 32.
  Remaining sub-items: option 3 below (an effective-n column in `rank.exe rate`) is
  still worth doing for the FIXED-START pool, which keeps its defect. Option 2
  (`ttClear()` per game) is also still open and is what would make the fixed-start
  pool reproducible.
- **Can a book be mined to RECOVER from bad random openings? `[Next]`** {cpu: hours, dev: low}
  Developer question, 2026-07-26. Partially addressed by `cbook`
  (`plans/cluster-book-plan-1-noble-swimming-scott.md`): fuzzy nearest-cluster
  matching has no exact-hash requirement, so a position reached via a random
  opening still matches SOME mined cluster for its half-move, unlike `book`'s
  exact hash which a random opening essentially never reaches. Not yet tested
  against THIS specific claim, though: Pass 1a mined only from `matches.jsonl`
  (the fixed-start pool), not `matches_open.jsonl` (the diversified one), and
  the "recovers from a bad start" framing below is still the sharper, more
  specific test. In the diversified pool a book is inert, because it
  is keyed on exact position hashes that a random opening never reaches, so book
  agents were left out. But that assumes a book mined the way `bookgen` mines them:
  from the standard start, forward. The interesting variant is a book keyed on
  positions that arise AFTER a random opening, mined from games the owner won from a
  bad start. That is a different artifact and a different claim: not "replay my best
  line" but "here is the refutation once I am already worse". If it works it is a
  genuinely transferable skill rather than the memorization theory 38 found. Test:
  mine from `ranking/matches_open.jsonl` (which now has 2912 diversified games) with
  `--plies` covering the post-opening window, roster the result, and see whether it
  beats its own bare core in the diversified pool. Cheap, the games already exist.
- **Grow the diversified pool. `[Next]`** {cpu: hours, dev: medium}
  Currently 14 agents, chosen as one strongest representative per evaluator family
  plus the scale (see the header of `ranking/roster_open.txt` for the selection rule).
  Candidates to add: more seed replicas per recipe so the training-seed-noise band is
  visible inside this pool, the remaining `adv` hill-climb finds, and a second dist
  seed (only `learned(model=111,...)` is in). Also decide whether `.opener(rand,moves=4)` is the
  right depth: 4 own half-moves means 8 plies of random play, never swept.
- **Give the rating path real sample diversity (original entry, superseded above).** {cpu: hours, dev: low}
  Found 2026-07-26 (`plans/book-opener-audit-results-1-vivid-lantern.md`, defect 3 in
  `Docs/benchmarking.md`). `rankSchedule` seeds every game, but `rand()` is only consumed
  by dilution and random-move agents, so for a pair with no `dil(...)` and no `rand`
  opener the seed is inert and every game with the same colour assignment is
  byte-identical. `playOneGame` also never calls `ttClear()`, so for a `tt` head the sole
  source of variation is which games ran earlier in the same process. Measured: median
  distinct-trajectory ratio 0.438 across 190 pairs with >= 16 games, worst cases 32 games
  yielding 2 distinct games (all `ab(deep=6,ord,nodes=200k)`, the no-TT head). A null control of
  two identical deterministic agents went 32-0 as White and 1-31 as Black over 64 games.
  Consequence: `pm` in `ratings.tsv` / `standings.tsv` is understated by roughly 1.5x for
  a typical pair and much more for deterministic ones.
  Options, to be decided before the next certification refit:
  1. Add `--open-plies K` / `--open-side` to `rank.exe play` and `run` (the plumbing
     already exists in `playoutCapture`, only the CLI and `rankPlay` signature are
     missing). K random opening half-moves make `rand()` live, so each seed is a real
     game. This changes what the ladder measures, from "strength at the standard start"
     to "strength across openings", and it is the standard fix in engine testing. It also
     invalidates comparison against the existing store, so it needs a new board tag or a
     fresh store rather than mixing rows.
     **Option 1 alone is not sufficient.** Measured 2026-07-26: `--open-plies` makes the
     seed live but leaves cross-game TT state intact, and splitting the same 64
     diversified games into four 16-game processes moved a booked pair by 17pp. Options
     1 and 2 are one change, not alternatives.
  2. Add `ttClear()` per game in `playOneGame` for reproducibility, which is the
     `--reset-state` repair theory 14 and theory 19 both ask for. On its own it makes
     sample size WORSE (it removes the only current diversity source), so it must land
     together with option 1, which supplies real diversity to replace it.
  3. Report effective sample size instead of fixing it: have `rank.exe rate` emit a
     distinct-trajectory count per pair and an effective-n column, so a reader can see
     that a 32-game pair is 2 games. Cheapest, and worth doing regardless of 1 and 2.
- **Boost the category-champion pools to 32 games/pair. `[Next]`** {cpu: hours, dev: low}
  ~~openless x node and openless x time DONE 2026-08-27~~: boosted to 32
  games/pair (top-12 of 37 contenders for x node via `ranking/roster_top.txt`,
  the full 14-core round-3 cohort for x time via `ranking/roster_top_time.txt`),
  refit unpinned, both champions held (`ranking/CHAMPION.md`). Remaining under
  the current 6-category system (`ranking/CHAMPION.md`'s 2026-08-24
  restructure): **opener8 x node** still carries round 1/2's 1840-game
  screening fill; **dil20 x node**, **opener8 x time**, **dil20 x time** are
  all still round-3-only at 8 games/pair nominal. None of those four
  declarations should be treated as settled until boosted the same way (build
  a `ranking/roster_top_<name>.txt` top-N contender list per category, play to
  32/pair, plain unpinned `rank.exe rate`, update `CHAMPION.md`).
  Historical note (pre-restructure, 5-category system): round 1 (2026-07-28,
  24 agents, 116->140 active) and round 2 (2026-07-29, 18 more agents incl.
  `s3`/`adv` own-books at 4/8-ply, 140->158 active) both screened at only
  8-11 games/pair; only 4-book/8-book cleared ~2 combined SE over their
  runner-up, and growing the roster in round 2 made 4-random/8-random's gaps
  SMALLER, not larger. Those categories are now demoted to
  `ranking/CHAMPION.md`'s "Deferred categories" appendix pending a
  self-maximizing book-mining redesign, not part of this item's remaining
  scope. Remaining book-category growth candidates (if that redesign lands):
  `s4`/`s9`/`s10`/`s94`/`s95`/`s97`/`s99` don't have an established own-book
  pair yet (would need a fresh bookgen source, unlike `s3`/`adv` which reused
  book6/book3's existing target); the wide dist-mlp cores (`s77`/`s78`/`s79`/
  `s110`/`s112`/`s114`/`s115`, 350-1670 ms/move) were deliberately skipped from
  the random
  categories for cost, same call as `book5`'s exclusion.
- **Explain the 30-ply book depth rung. `[Next]`** {cpu: hours, dev: medium}
  Book depth was varied for the first time (6/16/30/60 ply, `models/book7..12`). On the
  `classic` core the lift rises with depth (+54, +45, +71, +110 over bare). On the `s98`
  core it does not (+70, +82, +9, +55), and the 30-ply rung sits 73 Elo below its 16-ply
  neighbour at +/- 10 each, far outside the bars. Untested hypothesis: handing off to
  the brain mid-middlegame is worse than handing off early or carrying to the endgame.
  Test by mining intermediate depths (20, 24, 36, 44) on the same pair and looking for a
  trough, and by instrumenting `openerBook` with a per-game in-book ply counter.
- **Re-evaluate every Elo claim under the new comparison hygiene, then clear its banner. `[Now]`** {cpu: hours, dev: high}
  On 2026-07-25 two defects were found in this project's Elo reporting: (a) numbers read
  from `ranking/ratings.tsv` mixed RETIRED agents (`active = gone`, superseded `@N`
  identities frozen at old game counts) in with live ones, and (b) agents were compared
  across different SEARCH HEADS, which are different agents. Measured instance: a retired
  `classic` row reads 1081 where its live identity reads 990, a 91-Elo phantom gap that
  inverted one conclusion. Fixes shipped the same day: `rank.exe rate` now writes
  `ranking/standings.tsv` (active only, grouped by head), `CLAUDE.md` gained ranking-claim
  hygiene rules (5) and (6), and `Docs/benchmarking.md` has the full explanation under
  "Elo comparison hygiene". A third defect was added 2026-07-26: nominal stored games are
  not distinct games (see the diversity item above), so every re-check below must also
  count distinct trajectories before trusting a record or an error bar.
  Progress: `Docs/theories.md` theories 14 and 33 corrected 2026-07-26, and
  `ranking/CHAMPION.md` flagged. Banners stay until the whole document is re-verified.
  **37 documents were flagged with an `[ELO HYGIENE UNVERIFIED]` banner** (all Elo-citing
  files in `plans/`, plus `ML.md`, `Docs/agents.md`, `Docs/axioms.md`, `Docs/theories.md`,
  `ranking/CHAMPION.md`). For each one: re-check its numbers against `standings.tsv` within
  a single fit at a fixed head, confirm or correct each finding it accepted or refuted,
  then delete that document's banner. A finding that flips must also be corrected in
  `Docs/theories.md` and, if it touches the throne, in `ranking/CHAMPION.md`.
  Suggested order (highest claim density / most load-bearing first):
  1. `ranking/CHAMPION.md` (the throne declaration itself)
  2. `Docs/theories.md` (the theory ledger other docs cite)
  3. `ML.md` (the shipped-models tables)
  4. `plans/dethrone-champion-results-*` (5 docs, all champion-relative claims)
  5. `plans/position-oracle-results-1`, `plans/heuristic-eval-overhaul-results-1`,
     `plans/bounded-jitter-results-1`, `plans/vs-champion-training-results-1`
  6. the remainder of `plans/`
- **Loadout-parity study: stop comparing bare cores against an equipped champion. `[Next]`** {cpu: days, dev: medium}
  The reigning champion is a `classic` core wearing one loadout item (`.opener(book,book=2)`),
  and that item is worth **+124 Elo** on that core (990 bare -> 1114 equipped, 2026-07-25
  fit, head `ab(deep=6,tt,ord,nodes=200k)`). Every learned/dist/hill-climbed agent it has been
  measured against is **bare**, so those comparisons have been reading the loadout, not
  the core. Report all three numbers instead of one: (1) **bare vs bare** (the honest core
  comparison), (2) the **lift** of each loadout item on each core (same core, with and
  without), and (3) **equipped vs equipped** (best achievable build of each core). See
  `Docs/terminology.md` for core / loadout / bare / equipped / loadout-matched / lift.
  **Critical implementation constraint, already established:** books are NOT portable
  across cores. Theory 14 was refuted in its naive foreign-book form and theory 33
  confirmed the self-mined form -- a book must be mined from the WEARER'S OWN wins.
  Current-fit evidence at the same head: classic + its own book2 = 1114 (**+124**), classic
  + the foreign book1 = 982 (**-8**), s98 + book1 = 1066 (+23 on the core the book was
  mined against). So do NOT hand the champion's `book2` to a learned agent and call it a
  fair build. Mine each core its own book via `rank.exe bookgen` from that agent's own
  wins, give it a fresh slot (book files are immutable and not hashed into the ID), and
  rate the equipped identity separately.
  Loadout items to sweep per core, each already an ID-level toggle: own-mined opening book,
  quiescence (`qs`), transposition table (`tt`), move ordering (`ord`), aspiration window
  (`asp`). Expected payoff beyond fairness: if lifts are roughly additive, the strongest
  agent is the best core wearing every positive-lift item, which no one has actually built
  yet -- the champion wears exactly one.
- Standing project loop: on every new agent, check whether it lowers the current #1's
  Elo, by outrating it outright or by countering its specific build (see the
  adversarial counter-agent idea above). Treat "dethrone the champion" as the
  recurring success criterion, not just "raise some Elo" in isolation `[Now]` {cpu: seconds, dev: high}
- Roster curation policy (interim, until the classifier below exists): keep `on` the
  anchor, the dilution ladder, the reigning champion family, one oracle, the best agent
  per distinct data-source family (replay, self-play, vs-champion, oracle-mimic,
  branch-mined), and any agent with a distinctive opponent-bucket profile (e.g. a
  counter-agent that beats the champ but loses broadly). Retire (`off`) near-duplicates
  whose head-to-head profiles match an existing agent, since their games stay in
  `matches.jsonl` forever `[Now]` {cpu: minutes, dev: high}
- Agent behavioral classifier: characterize agents by how they PLAY, not just Elo, and
  use it to decide which agents are interesting enough to keep active. Features:
  position-distribution overlap between agents (shared `positionKey` histograms over
  their stored games, so two agents reaching the same positions 99% of the time rate as
  near-identical), a left-right symmetry measure, and responses against a fixed
  discriminator agent as a feature vector. Cluster k-means-style and keep the most
  interesting few per cluster. Big project, deliberately deferred `[Later]` {cpu: days, dev: high}

## Books (openers and mid-game)

- **[IN PROGRESS, 2026-08-27] Refutation oracle: one book that beats every
  deterministic agent in the roster.** The repaired form of theory 14 that the
  theory's own notes name and that theory 33 did not cover: a book carrying the
  full winning continuation, so the live brain never plays and there is no
  brain-portability handoff to fail. Plan + Phase-0 results:
  `plans/refutation-oracle-plan-1-quiet-lodestone.md` /
  `plans/refutation-oracle-results-1-quiet-lodestone.md`.
  - ~~**Phase 0, reproducibility gate.** Does a deterministic agent actually
    replay? Built as `rank.exe determinism`. PASSED: 238/238 subject-colours
    reproducible within and across processes, 210/210 node-budgeted in all 4
    passes. Only `model=111`/`model=113` on the `time=150ms` head ever varied,
    the two lines already `# cost flag`ged, and only once in four passes.~~
  - ~~**Phase 1-2, the miner (`rank.exe refute`).** With both sides deterministic
    each (book, opponent, colour) pair is ONE game, so beating 119 agents is
    winning 238 specific games and finding each is a ONE-PLAYER depth-first
    search over our own moves with backtracking, not a minimax. Built: stage 1
    mines every target over one shared book with the oracle as fallback, stage 2
    repairs the losers by walking each losing line backwards and re-searching
    with the tried moves filtered out of the search root.~~
  - ~~**Phase 3, merge conflicts.** `openerBook` keys on `positionKey(side).hash`
    with no ply and no path, so two lines needing different moves from one
    position cannot both be expressed. Handled by construction (the book is
    shared from the first game onward, so a later line inherits an earlier one's
    move) plus an ownership check that refuses to change a position a won line
    depends on, surfacing the conflict as a `blocked_shared` row.~~
  - ~~**Phase 4, verification.** Replay the merged book against all 238.
    Built as two passes: a coverage audit that reports how many lines LEFT the
    book, and an `openerBook` replay through `playOneGame`.~~
  - ~~**The transposition table was handing each agent the other agent's search
    results** (theory 54, `TT CROSS-AGENT CONTAMINATION` in `Docs/corrections.md`).
    Found here because a book plays back without searching, so it exposes any
    dependence on the miner having searched: 132 of 238 mined lines stopped
    reproducing, all 132 against a `,tt,` opponent and 0 of 77 against non-`tt`
    ones. Fixed by mixing a searcher context into the TT key. Same 8 targets,
    same `tt` oracle: 8 of 8 lines left the book before, 0 of 8 after.~~
  - ~~**Run a clean full mine on the fixed `@2` binary.** Done 2026-08-29,
    `models/book21.txt`, 3657 entries, log `ranking/refute_book21_postfix.log`,
    per-target detail `ranking/refute_book21.tsv`. **230 of 238 lines are won by
    the book alone, end to end**, which is the memorization result. The mining
    stage's own 238/238 is NOT that number: 7 lines left the book (5 won on the
    wearer's brain, 1 lost, 1 disagreed) and 1 fully covered line failed to
    reproduce. Both miner-versus-`openerBook` disagreements are `model=111` and
    `model=113`, the two cost-flagged `time=150ms` cores that do not replay, so
    excluding them the result is 230 of 236. Stage 1 took 143 by the oracle line
    alone, stage 2 repaired all 95 remaining with 0 conceded.~~
  - ~~**Close the ownership hole.**~~ Fixed 2026-08-29. Stage 3 found 1 position
    of 3657 where two winning lines recorded different moves: the check gated
    which position a repair may BRANCH at but then committed the winning line's
    whole path unconditionally, so a repair could overwrite a position an earlier
    winner owns. Stage 2 now validates the full path against `owners` before
    committing and falls through to the next candidate move instead, and the
    summary reports how many wins were refused so the cost of the check is
    visible. Stage 1 needed no change, it writes insert-only. Two diagnostics
    were added with it: `ovr_ply`/`ovr_key` report the first position where the
    written book serves a move DIFFERENT from the one mined for that line, which
    measures an overwrite instead of inferring it, and `oob_key` records where
    the book fell silent in the same form `models/book<N>.txt` uses.
  - **The miner is not reproducible run to run, so book21 vs book22 is not a
    valid A/B.** Slot 22 (`ranking/refute_book22.log`) re-mined on the fixed
    binary, same frozen snapshot and same oracle, and came out worse: 86 of 95
    repaired against 95 of 95, 16 lines out of book against 7. That is NOT
    attributable to the fix. The new check never fired (0 rejects, 0 collisions),
    and STAGE 1, which is identical code in both binaries, already diverged:
    3532 book entries against 3530, first differing between targets 181 and 200.
    A 2-entry difference at the end of stage 1 is enough to change which
    positions stage 2 finds owned, and stage 2 is path dependent.
  - ~~**Clean A/B with `--skip-timed`.**~~ Done 2026-08-29. Slot 23 on the fixed
    binary and slot 24 on a pre-fix binary built from `src/ranking.cpp` at commit
    `3d945c2` (`rank_refute_pre.exe`, sources in `build/pre/`), both 210 targets
    with the 14 wall-clock-budgeted agents excluded, same snapshot and oracle.
    **The two runs are identical at every stage**: 130/210 stage 1, 3505 entries
    kept of 3899, 203/210 mined, 202/210 audited with 12 out of book, 202-8-0
    verified, 1736 mining games, 0 collisions in both. `models/book23.txt` and
    `models/book24.txt` are byte-identical apart from the slot number in the
    header comment, and the reports agree on all 210 rows. Three results:
    (a) the miner is exactly reproducible once the `time=` agents are excluded,
    which localises the irreproducibility to them, (b) the fix is
    behaviour-neutral, it never fired, and (c) **the ownership hole does NOT
    explain lines leaving the book** -- 12 left with 0 collisions, so that
    hypothesis is refuted.
  - **Study the shared transposition table instead of only fixing it (theory 58).**
    Developer proposal, 2026-08-31. The claim: a shared table degrades play through
    its ORDERING channel alone, separately from the value channel, and the damage
    concentrates on evaluators with large tie sets. Two code facts make it sharp.
    `ttProbe` fills the move hint BEFORE the depth test and returns false after it
    (`src/transposition.cpp`, "usable for ordering regardless of depth"), so the
    ordering channel fired on entries too shallow for a cutoff and had strictly
    more contact surface than the value channel. And `if (eval > best)` in
    `src/ai_minimax.cpp` is strictly greater, so among tied moves the FIRST found
    wins, which is what ordering decides.
    - **Instrument needed, does not exist yet.** An ordering-only TT mode: keep the
      `ttFrom`/`ttTo` hint, suppress the score-return path. Roughly a flag around
      the `if (ttProbe(...)) return sc;` line at both probe sites. Plus a way to
      re-share the key (undo the `s_ttCtx` mix) for the sharing arm.
    - **Design.** 2x2 of {shared key, separated key} x {ordering-only, full TT},
      crossed with an evaluator that has huge tie sets (`classic(chip=100)@2`) and
      one that has few (a learned continuous-valued model). Prediction: the effect
      concentrates on the chip counter.
    - **Make "many ties" a number, not an assertion.** Instrument the search to
      count, per node, how many children share the best score. That per-evaluator
      tie rate is the covariate that should predict effect size, and it converts
      the explanation from a story into a fitted relationship.
    - **Run the poisoned control first.** Store the WORST move as the hint rather
      than a foreign agent's best. It is the true adversarial case, it is cheaper
      than the full grid, and it bounds the effect: if maximally poisoned ordering
      costs little Elo, the theory is dead and the grid is unnecessary.
    - Separate the two mechanisms by budget: unbudgeted fixed depth isolates
      tie-breaking, `nodes=200k` adds the completed-plies effect.
    `[Next]` {cpu: hours, dev: medium}
  - ~~**DEFECT (indicated, not yet confirmed): a deterministic opponent does not
    reproduce its replies when our side stops searching.**~~ **NOT CONFIRMED.
    The direct run this entry asks for was done 2026-09-09 and the deduction
    does not survive it.** A node-budgeted or fixed-depth agent reproduces its
    replies exactly when the other side stops searching. What does not reproduce
    is a `time=` agent, which is not in the deterministic class at all
    (`rankAgentIsDeterministic` returns false for `timeBudgetMs > 0`, so the
    2-game cap never applied to it and nothing about the scheduler is wrong).
    - **What was run.** `tests/test_determinism.cpp`, now in the suite, plus a
      wider scratch sweep. Reference game, then three perturbations: White never
      searches and replays its recorded move, White is silent for its first 8
      plies (the `.opener(rand,moves=8)` shape), and both sides search normally
      but on a table another game already dirtied. Compared per ply by
      `positionKey` hash. Swept over 5 boards x 11 configurations: chip counter
      and `tdleaf_self lin model=169` and `position_elo mlp model=110`, with and
      without `tt`, with and without `qs`, on `ab(deep=6,tt,ord,nodes=200k)@3`,
      on both new `deep=12` heads, and on the budget-BOUND case where the node
      cap actually cuts the search mid-iteration.
    - **Result.** 0 divergences in every node-budgeted and fixed-depth cell.
      The only divergences were on `ab(deep=12,tt,ord,retain,time=25ms)@3`, 1 of
      5 boards under a silent White and 1 of 5 under a dirty table, and they
      moved between runs, which is wall-clock sensitivity rather than a state
      leak. The project's own gate agrees over a wider set than the sweep:
      `rank.exe determinism --replicas 4` across the WHOLE deterministic roster
      (140 of 426 active agents, 2 colours, 4 replicas, 1,120 games) returns
      **280/280 subject-colours reproducible**, and `--only deep=12` returns
      70/70. That set covers the fixed-depth agents the sweep did not, since
      every case there carried a `nodes=` or `time=` budget. Output kept at
      `ranking/determinism_full.tsv`.
    - **The instrument was validated before the reading was quoted.** Two
      positive controls, both in the test file: replaying the reference
      identically must match (it does), and a `.dil(prob=20)@1` opponent must
      diverge (it does, at ply 1). Without the second, every "no divergence"
      above would be unfalsifiable.
    - **Why the shared table does not leak.** `setTTContext`
      (`src/ai_minimax.cpp:196`) salts the key with root side, evaluator and
      eval params, so a foreign entry is a miss, not a false hit, and
      `ttStore`'s always-replace eviction costs time rather than correctness.
      The root move loop is unordered, `orderMoves` runs only at interior nodes,
      so table-driven ordering cannot reorder the root candidates that a tie is
      broken among. This is why the eviction mechanism guessed below does not
      bite, and it is worth keeping: the property depends on the root staying
      unordered, so a future root-ordering change would need this test rerun.
    - **What the original observation most likely was.** The `Phase 0`
      reproducibility gate already recorded that only `model=111` and
      `model=113` on the `time=150ms` head ever varied, and both
      miner-versus-`openerBook` disagreements in the book21 run were those same
      two cores. A `time=` opponent explains the residual without a new defect.
      The 5 slot-23 lines with `ovr_ply = -1` were never checked for whether
      their opponent was time-budgeted, and that is the one loose end.
    - The superseded mechanism guess and the original deduction are kept below,
      because a later reader meeting them quoted somewhere needs to recognise
      them.
    - **What was measured.** On the clean A/B (slot 23), 12 lines left the book.
      Seven are conceded lines with no coverage to lose. The other five were
      mined as WINS and all five report `ovr_ply = -1`.
    - **What that deduces.** `ovr_ply = -1` means no position exists where the
      replay stands on a position the mine recorded and the book returns a
      different move. So our moves reproduced the mined line exactly up to the
      ply the book fell silent, and the book was not overwritten. The line still
      reached a position the book had never seen, which leaves the opponent's
      reply as the only thing that can have changed.
    - **Why it is only INDICATED.** Every step above is inference from a negative
      result. The opponent replying differently has never been watched directly.
    - **Direct confirmation, one run.** Play the same deterministic opponent from
      the same position twice, once with our side searching and once with our
      side replaying book moves, and diff its chosen move per ply. That either
      exhibits the defect or kills the deduction. Do this BEFORE any mechanism
      work.
    - **Blast radius, now measured rather than feared.** The worry was that if
      stored games between an opener-wearing agent and a searcher were affected,
      the consequence would be the same class as theory 54, which invalidated
      every stored `tt`-vs-`tt` game and forced the `ab@1 -> @2` bump. The
      `.opener(rand,moves=8)` shape was tested directly and shows 0 divergences,
      so the 77 `.opener(...)` rows in `ranking/roster.txt` are not affected and
      no stored game is invalidated. No code version bump is needed.
    ~~`[Now]` {cpu: minutes, dev: medium}~~ CLOSED 2026-09-09, regression test in
    `tests/test_determinism.cpp` {cpu: none, dev: none}
  - **The ownership check has never fired, so it is untested.** Correct by
    construction and behaviour-neutral on every workload run so far. A targeted
    test would construct two winning lines that genuinely want different moves at
    one shared position and assert the second is refused rather than committed.
    `[Next]` {cpu: seconds, dev: medium}
  - ~~**The `ovr` diagnostic had a false-positive mode.**~~ Fixed 2026-08-29
    before the A/B landed. It compared the written book against the target's
    stored path without checking that the target WON. A conceded target's stored
    path is its last failed attempt, which was never written to the book, so every
    difference is expected. All 4 lines slot 22 reported as overwritten were
    conceded (`mined=0`). Now gated on `t.status == 1`.
  - Note for the re-mine comparison: the seven `oob_key` values on book21 are all
    DISTINCT, and that does NOT weigh against the guess that one overwritten
    position explains them, since lines crossing it face different opponents and
    so reach different successors.
  - ~~**Check that the book21 audit reproduces.**~~ Done 2026-08-29, an
    independent `--verify-only` replay against the same snapshot. The audit
    matched exactly (237/238 won, the same 7 lines out of book at the same
    plies), but the verified record moved 235-3-0 to 236-2-0 on a single row:
    `model=111` at `time=150ms` as Black, which flipped L to W and moved its own
    `oob_first` from 13 to 16. The 236 non-timed rows reproduced exactly, so the
    memorization result is stable and the instability is confined to the two
    cores that cannot be mined by construction. A timed line's verified result
    should not be quoted to the unit.
  - ~~**Work out whether the book can be certified.**~~ Answered 2026-08-29, and
    the reference-class rule is NOT the reason. `CHAMPION.md`'s divisions are
    openless (no `.opener(...)` and no `.dil(...)` segment), opener8
    (`.opener(rand,moves=8)@1`) and dil20 (`.dil(prob=20)@1`). An agent carrying
    `.opener(book,book=21)@1` matches none, and the file says any other opener
    combination, naming the retired 4-ply and 8-ply book categories, is ladder
    and study data holding no title. So a book agent can be rostered and rated
    but holds no title, by category membership. Reference class is defined by
    COMPUTE, and a book is not more compute, so it does not apply. Two further
    reasons no cheaper wearer fixes: the book was mined against the same 119
    deterministic agents it would be rated against, so its pooled Elo measures
    memorization of this pool, and it is maximally intransitive (near-perfect
    against those 119, no coverage against the 52 dilution and 66 random-opener
    agents that never reproduce a line).
  - ~~**Does the book transfer to a cheap brain?**~~ Yes, measured 2026-08-29.
    Replaying `models/book21.txt` through `--verify-only` with three brains (the
    d8/nb2m oracle, `ab(deep=6,tt,ord,nodes=200k)@2.classic(chip=100)@2`, and the
    same head with `learned(model=459,...,tdleaf_self,lin,shape=129-1)@1`) gives
    **coverage differing on 0 of the 210 node-budgeted targets**, all three at 205
    fully covered and 205 won. Every coverage difference is on a `time=150ms`
    target. Verification differs on 4 node-budgeted rows, all of them lines that
    LEFT the book, which is the expected split: the book decides covered lines,
    the brain decides the rest. **The target-class number is 205 of 210, not the
    230 of 238 headline**, which belongs to the reference-class d8/nb2m wearer and
    counts the 28 timed targets. This is the campaign's actual goal met: the lines
    are carried with less live compute than the oracle that found them, since on a
    covered line the wearer never searches.
  - **Roster candidates staged, ready for another session to play.**
    `ranking/roster_book_candidates.txt`, 9 agents, all passing `rank.exe check`.
    One book (book21) on six fallback brains plus two bookless twins. Notes that
    matter: `deep` is a CAP on iterative deepening when a budget is set, so the
    deep=10 rows carry `nodes=2m` (deep=10 at 200k would spend the same compute as
    deep=6 at 200k), and the deep=10 bookless twins are included because lift is
    per-core and unmeasurable without them. Merge into `ranking/roster.txt` (pure
    additions), `check`, play, rate. `[Next]` {cpu: hours, dev: low}
  - **Held-out mine, the only design that makes this a strength claim.** Mine
    against a SUBSET of the roster, rate against all of it, and report the
    held-out agents separately. Nothing run so far distinguishes "this book
    generalises" from "this book memorised 205 specific games". `[Next]`
    {cpu: hours, dev: medium}
  - Open risk compute cannot remove: a black-side win against the strongest
    openless agents may not exist. Report the unwinnable set explicitly rather
    than dropping it.

- ~~Decide what the TT fix means for the roster and the stored match history.~~
  **Resolved 2026-08-28.** Measured the Elo consequence first (replayed a
  3000-game `tt`-vs-`tt` sample: 30.9% outcome-mismatch under the fixed binary,
  vs 13.5% baseline on a same-size non-`tt`-vs-`tt` control sample, z approx
  6.4 -- see `Docs/corrections.md`'s `TT CROSS-AGENT CONTAMINATION` entry for
  the full numbers). The gap was judged too large to leave unaddressed, so the
  `ab` explorer's code version was bumped `@1` -> `@2` (`src/ranking.cpp`'s
  `g_rkExplorers`), all four binaries rebuilt, `ranking/roster.txt`'s 216
  active alpha-beta lines bumped to match (`rank.exe check` passes), and the
  canonical fit re-run. Developer pre-authorized this specific action
  ("if you notice any impact, do that next") ahead of the measurement.

- ~~**Re-certify all 6 category champions under the `ab(...)@2` identities.**~~
  Resolved 2026-08-30: rather than boosting a per-category contender subset,
  filled the ENTIRE roster (`.\tools\run_rank.ps1 -Workers 10 --games N`, no
  `--roster` filter) on a rung ladder N = 8, 16, 32, snapshotting
  `ranking/standings.tsv`/`ratings.tsv` after each rung into
  `ranking/recert_snapshots/`. Rank order was stable from rung 8 through rung
  32 for every category's top field (openless x node's top-5 held identical
  rank across all three rungs), so 32 was taken as converged. `rank.exe check
  --games 32` confirms 0 pending across all 23,653 pairs (deterministic pairs
  remain capped at their 2-game floor, unaffected by the target). This clears
  CLAUDE.md rule 2 for all 6 categories in one fit, not just the 2 that had a
  `roster_top*.txt` file. All 6 declarations in `ranking/CHAMPION.md` updated
  from this fit; several reordered materially under `@2` (the TT fix's whole
  point). `roster_top.txt`/`roster_top_time.txt` were bumped to `@2` IDs but
  NOT used as the boost mechanism this round and now list a stale top-N (the
  full-roster fill superseded them) -- leaving them as-is; the next
  contender-only study should rebuild them from the current standings rather
  than trust their listed order.

- **Does the same replay mismatch confound `bookgen`'s measured book lift?**
  `bookgen` mines the line owner's moves out of stored games in which the owner
  DID search, and the resulting book is worn by an agent that does not. That is a
  second, independent explanation for theory 38's book collapse alongside position
  novelty, and it is only partly addressed by the TT fix: the fix stops the
  opponent reading OUR entries, but the owner's own entries still shaped the moves
  that got mined. Test: replay a `book13`/`book14` wearer against the exact core
  the book was mined from and report the half-move at which it first leaves book,
  separately for `tt` and non-`tt` opponents. Not measured. {cpu: low, dev: low}

- **Deterministic pairs are still scheduled as if they were samples.** 119 of 218
  active agents are deterministic, and Phase 0 above confirms a post-fix
  deterministic pair yields exactly 2 distinct games however many are scheduled.
  A 32-games/pair boost round therefore spends most of its compute on replays.
  Options: skip known-replay pairs in `rankSchedule`, or store an 8-byte per-ply
  trace hash per row so distinct-game counts (and error bars) become exact
  instead of inferred from `(plies, result, node totals)` -- a tuple that also
  OVERCOUNTS for `time=` agents (`Docs/benchmarking.md` defect 3). Either changes
  what "games/pair" means in `CHAMPION.md`, so it needs a decision first.
  {cpu: none, dev: medium}

- **[IN PROGRESS, 2026-08-26] Opening book chosen by win rate over the roster's own
  game history.** The pooling half of this is now built as `cbook` (`rank.exe
  cbookdump`/`cbookfit`, `src/ml_cluster.h`, `.opener(cbook,<N>[,ply=M])@1`), which
  also generalizes past exact-position-hash matching to nearest-cluster matching
  (SMARTSTART, applied to alpha-beta as a root-move filter rather than a played
  move). Pass 1a (plumbing sanity, one core's own wins) is complete and the
  fuzzy-match-survives-diversification hypothesis is confirmed on a hit-rate
  curve: the exact-hash book's hit rate collapses to 0% by half-move 3 against a
  diversified opponent while `cbook`'s fuzzy match keeps firing (53-100%) through
  half-move 15. Elo-expected weighting (below) is NOT implemented -- `cbookfit`
  weights by raw move count within a cluster, not by the winner's Elo-expected
  score -- and the universal-vs-per-regime scope decision (Pass 1b) has not run.
  Plan + Pass-1a results: `plans/cluster-book-plan-1-noble-swimming-scott.md` /
  `plans/cluster-book-results-1-noble-swimming-scott.md`. Original iterations still
  worth trying, now against `cbook` rather than a from-scratch design:
  - Restrict the source games to random-opener agents, so the book is built from
    diverse openings rather than the few lines deterministic pairs replay.
  - Weight each win by the Elo-EXPECTED win rate of that matchup rather than counting
    it flat, so beating a much stronger opponent counts for more. `rank.exe matchup`
    already computes actual minus Elo-expected, so the per-game expectation is on hand.
  - Compare against the existing per-pair mined books. Theory 38
    (`plans/book-opener-audit-results-1-vivid-lantern.md`) found a mined book's lift is
    a memorized-line artifact that collapses under opening diversification. A win-rate
    book drawn from many agents may or may not inherit that failure, and finding out
    is most of the value here. `[Next]` {cpu: hours, dev: medium}

- **Mid-game book: identify difficult positions and the best response.** Key the book on
  positions where an agent went on to lose, ranked by how many games the correction
  would have saved across the roster.
  - Only positions where it is the eventual loser's turn.
  - Only positions reached at least 16 times by DIFFERENT agent pairs, lowering that
    threshold iteratively once the candidates are exhausted.
  - Exclude the first 4 moves, which the opening book already covers.
  - For each agent pair that reached the position, replay with a different move
    substituted for the loser and keep it if the loser now wins. `pairgen
    --branch-tries` already rewinds to a snapshot and substitutes a move, so the
    replay machinery exists.
  - Sort entries by games saved, so the book can be truncated to whatever is worth
    carrying. `[Next]` {cpu: hours, dev: medium}

## Position and agent analysis

- **Do strong agents recognise the lines a random agent beat them on?** Collect games
  where a random agent beat an agent rated 500+ Elo above it, then check whether the
  strong agents' own evaluators score those lines as strong. If they do not, the losses
  are an evaluation or search blind spot rather than variance. `[Next]` {cpu: minutes, dev: high}

- **Position similarity score.** For a given position, find or generate another position
  with a similar win rate for EVERY agent pair. A position's signature is the whole
  vector of pairwise win rates, not one number: agent A may beat B from it 90% of the
  time but beat C only 60%.
  - Some positions likely have win rates that barely depend on who is playing. Weight
    the score by how common that distribution of pairwise win rates is, so an
    unremarkable signature counts for less.
  - Worth splitting: the likelihood an agent REACHES a position versus the likelihood
    it WINS from it. Different questions, possibly uncorrelated. `[Later]` {cpu: hours, dev: high}

- **Agent similarity score: the proportion of positions where two agents play the same
  move.** Scores agents on how they PLAY rather than on how they were trained, which is
  what actually determines a matchup. `rank.exe posgen` already builds deduped, ply and
  material stratified pools, so the benchmark set exists. Would settle empirically what
  provenance cannot, for example whether a TD-Leaf model initialised from another
  agent's weights belongs with its ancestor or with the other TD-Leaf models. `[Next]` {cpu: hours, dev: medium}

- **Model to predict which agent pair generated a board position.**
  - Baseline first, no model: search the history for that exact position, or the one
    the least piece-distance away, and return the agent pair that played it.
  - Then test whether a trained model beats that baseline. Open question is how much
    data diversity it needs. Unique 1-distance positions may suffice, or it may need
    1-distance and beyond to generalise to novel positions.
  - Hold out ENTIRE GAMES, not individual positions: positions from one game are not
    independent samples.
  - Related measurement this would need anyway: how different does a random opening
    actually make the games? `[Later]` {cpu: days, dev: high}

## Agent Composition + Play
- Determinism classification: classify each agent component (explorer, evaluator, chooser,
  dilution) as deterministic or not, and classify each agent as the LEAST deterministic of
  its components (any random part makes the whole agent non-deterministic). For a
  deterministic-vs-deterministic pairing, one game per color is provably sufficient --
  repeats are guaranteed identical -- so the scheduler (`rankSchedule`) and `pairgen` could
  skip redundant games instead of replaying the same game `gamesPerPair` times. Motivated by
  a real bug found this session: an early `pairgen` head-to-head eval used no dilution/opening
  randomness and silently replayed only 2 distinct games ~120 times each, producing a
  misleadingly exact 50/50 split. Unit test: build a randomly-configured deterministic agent,
  play it against itself/an opponent twice, assert repeated games are identical (final
  position at minimum; the full move sequence if cheap, since compute is dominated by the
  search itself, not the check) `[Next]` {cpu: seconds, dev: low}

## Data + Infrastructure
- **`posgen` pools are not reproducible** `[Next]` {cpu: none, dev: small}.
  `rank.exe posgen` replays a seeded sample of stored games and drops any replay whose
  outcome differs from the stored one. The guard is correct and makes the tool SAFE, but
  a `time=` agent's replay depends on machine load, so WHICH games get dropped varies and
  the pool varies with it. Measured over `ranking/matches.jsonl` (623,774 rows, 274,260 of
  them involving a `time=` agent): mismatches ran 2 idle and 1 to 6 under load, and 2 of 3
  loaded pools differed from the idle baseline (see `POSGEN POOL NOT REPRODUCIBLE`,
  `Docs/corrections.md`). Deliberately left unfixed (developer decision 2026-09-02: document,
  change no code). The fix when it is wanted: skip games whose either agent carries a live
  wall-clock budget BEFORE replaying, under its own counter, which is what `rank.exe refute
  --skip-timed` already does. Retiring the `time=150ms` roster lines makes every newly stored
  row replayable, so the defect is bounded by rows already stored and shrinks on its own.
  ~~Consequence: the test `rank posgen - deduped, stratified, deterministic pools` asserted
  byte-identical reproduction over the live store and so failed intermittently under CPU
  load, roughly 1 run in 8 against a concurrent `rank.exe play`.~~ Fixed 2026-09-03: split
  into `rank posgen - deduped and stratified pools` (the invariant checks, kept against the
  live store) and `rank posgen - byte-identical pools on a time=-free fixture store` (the
  determinism claim, moved onto a fixture store filtered to rows where neither agent id
  contains `time=`, which posgen genuinely reproduces). Reproduced the original failure
  twice in two tries under a 12-way concurrent `rank.exe play` load on the pre-fix test,
  then ran the fixed test 10/10 clean under the same load. `posgen` itself is untouched by
  this fix and remains not reproducible; see `POSGEN POOL NOT REPRODUCIBLE`,
  `Docs/corrections.md`, for its still-open state.
- **Make the Bradley-Terry SE honest, then schedule to a target SE** `[Now]` {cpu: hours, dev: medium}. The fit
  counts stored rows as independent samples, but a deterministic pair replays one game
  per colour, so store-wide only 0.438 distinct games per row -- every printed `pm` is
  understated by roughly 1.5x. Fix the fit to weight a pair by DISTINCT games, then
  replace `--games N` with a target ("every active agent at SE <= 8 Elo") and allocate
  greedily to whichever pair most reduces the worst SE. Fisher information per game is
  `p(1-p)`, so a lopsided pair contributes almost nothing and uniform games/pair spends
  most of its compute on settled matchups. Sparsity then falls out instead of being
  hand-tuned. Scheduling half is done (deterministic pairs pinned at 2); the fit half
  is not.
- **Regime-level pooling: ordering confirmed, magnitude not** `[Next]` {cpu: hours, dev: medium}. Measured
  2026-08-03 over 209 agents' matchup-residual profiles: same regime + same opener
  r=0.150, same regime different opener r=0.059, different regime same opener r=-0.010,
  different both r=-0.045. So the grouping is real and REGIME separates more strongly
  than opener (0.160 vs 0.091 drop), which is the opposite of the initial guess. But
  r=0.150 is far too weak to treat regime members as exchangeable, and the residuals
  are measured on 8-game samples whose noise attenuates correlation toward zero, so
  the true value is an unknown amount higher. Re-run after the SE fix before building
  a hierarchical fit or regime-sparse scheduling on it.
- **Running the test suite overwrites `models/manifest.{json,md}`** `[Next]` {cpu: seconds, dev: low}. The
  ML tests call `writeManifest` with their own scratch rows, so `tests.exe`
  replaces the real committed manifest with a single `build\dist_model_ckpt30.txt`
  entry. Noticed 2026-08-01 while reviewing a diff before committing, and the
  corruption is easy to commit by accident since it looks like an ordinary
  modification. Fix by pointing the tests at a scratch manifest path, or by
  having them restore what they overwrote.
- ~~Screening cohorts flood the permanent ladder~~ Fixed 2026-08-01:
  `tools/tdleaf_study.ps1 -ScreenStore` plays cohorts into
  `ranking/matches_screen.jsonl` instead. The pinned screening fit is unchanged
  (cohort-vs-roster games live in that store and the roster enters pinned). On
  promotion, `rank.exe split` over the screening store separates the kept
  agents' games for merging into the ladder. **Other study scripts still write
  to the permanent store** -- `sweep_pst_v2.ps1`, `train_vs_champion.ps1`, and
  `hill_climb.ps1`'s promote path should be audited for the same problem `[Next]` {cpu: minutes, dev: low}
- Python analysis layer (DuckDB queries: top Elo, fairest positions, avg eval per position) `[Later]` {cpu: minutes, dev: low}
- Python training (PyTorch) for MLP/NNUE/transformer, exporting C++-format weights `[Later]` {cpu: days, dev: high}
- Optional Weights & Biases tracking (local metrics by default, W&B opt-in) `[Dream]` {cpu: minutes, dev: low}
## GUI
- Add MLP models to the GUI (developer, 2026-07-21). `DrawPlayerConfig` generates its controls
  from the evaluator registry the same way the console's `getEvaluatorSettings` does (root
  `CLAUDE.md` Architecture Notes), so this is likely a model-slot-picker generalization rather
  than new UI machinery -- needs checking `gui/main_gui.cpp` for whatever currently limits
  LearnedValue selection to linear models specifically `[Now]` {cpu: seconds, dev: high}
- Set a dist MLP (`dist_mlp_wide` once incrementalized, or any dist model meanwhile) as the
  GUI's default on-screen board-state evaluator `[Now]` {cpu: seconds, dev: high}
- Give the on-screen board-state evaluator iterative deepening: currently (needs confirming
  against `gui/main_gui.cpp`) it looks like a static immediate leaf eval; make it a live,
  progressively-deepening background search the way a chess GUI's analysis panel updates as it
  thinks, rather than a single flat number `[Now]` {cpu: seconds, dev: high}
- Sharding to evaluate multiple lines at once in the GUI without freezing it (a multi-PV-style
  analysis view). Real architectural tension to resolve first: the engine's board/eval state is
  global (root `CLAUDE.md` Architecture Notes), which is exactly why every other parallel
  workload in this project (rank.exe, tournaments) shards across separate OS processes rather
  than threads. A GUI analysis panel showing several lines at once likely needs the same
  process-per-line approach with results streamed back to the GUI process, not an in-process
  thread pool over the current global-state engine `[Now]` {cpu: minutes, dev: high}
