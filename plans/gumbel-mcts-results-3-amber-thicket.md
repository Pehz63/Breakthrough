# Gumbel AlphaZero bot -- Slice 2 results: Pass 2 broad hyperparameter sweep

Companion to `plans/gumbel-mcts-plan-3-amber-thicket.md`. Read that first for
the design; this document records what actually ran, what was found, and the
gotchas hit along the way.

## Round A: 21-draw random search

Ran to completion: 63 checkpoints (21 draws x 3 rungs: 100/400/1500) trained
into slots 660..722, rostered as `ranking/roster_gumbelzero_pass2.txt` /
`cohort_gumbelzero_pass2.txt`, played into `ranking/matches_screen_gz.jsonl`,
and screened via a pinned fit against `ranking/standings.tsv`.

### Full configuration table (rung 1500, the most-trained rung each draw reached)

| block | elo | +/-pm | sims | lr | l2 | replay cap | replay warm | batch | open |
|---|---|---|---|---|---|---|---|---|---|
| R17 | 704 | 10 | 200 | 0.03 | 0.0 | 8000 | 128 | 8 | 8 |
| R3 | 655 | 10 | 100 | 0.03 | 0.0 | 2000 | 32 | 8 | 8 |
| REF | 595 | 10 | 50 | 0.01 | 0.0 | 2000 | 32 | 32 | 4 |
| R8 | 580 | 10 | 200 | 0.01 | 0.0003 | 500 | 16 | 32 | 4 |
| R19 | 465 | 10 | 100 | 0.01 | 0.0003 | 2000 | 32 | 8 | 0 |
| R18 | 464 | 10 | 200 | 0.003 | 0.001 | 2000 | 32 | 8 | 4 |
| R7 | 450 | 10 | 200 | 0.01 | 0.001 | 8000 | 128 | 32 | 0 |
| R14 | 427 | 10 | 100 | 0.01 | 0.0003 | 2000 | 32 | 64 | 4 |
| R15 | 425 | 10 | 25 | 0.003 | 0.0 | 8000 | 128 | 8 | 0 |
| R16 | 417 | 10 | 25 | 0.003 | 0.0003 | 8000 | 128 | 64 | 4 |
| R9 | 415 | 10 | 50 | 0.01 | 0.0003 | 8000 | 128 | 32 | 0 |
| R6 | 393 | 11 | 100 | 0.003 | 0.001 | 2000 | 32 | 32 | 0 |
| R1 | 392 | 11 | 50 | 0.01 | 0.001 | 8000 | 128 | 64 | 8 |
| R20 | 392 | 11 | 25 | 0.003 | 0.001 | 2000 | 32 | 64 | 8 |
| R5 | 383 | 11 | 25 | 0.003 | 0.0003 | 500 | 16 | 64 | 0 |
| R10 | 358 | 11 | 25 | 0.01 | 0.0003 | 2000 | 32 | 64 | 8 |
| R12 | 357 | 11 | 100 | 0.03 | 0.001 | 500 | 16 | 8 | 4 |
| R11 | 327 | 11 | 25 | 0.003 | 0.0003 | 8000 | 128 | 8 | 0 |
| R4 | 284 | 12 | 25 | 0.003 | 0.001 | 8000 | 128 | 8 | 8 |
| R2 | 281 | 12 | 25 | 0.01 | 0.001 | 2000 | 32 | 8 | 4 |
| R13 | 248 | 12 | 25 | 0.01 | 0.001 | 500 | 16 | 32 | 0 |

### Group means (rung 1500 only, pooled)

| axis | values (rung-1500 mean Elo) |
|---|---|
| sims | 25: 346, 50: 467, 100: 459, 200: 550 |
| lr | 0.003: 386, 0.01: 421, 0.03: 572 |
| l2 | 0.0: 595, 0.0003: 422, 0.001: 362 |
| open | 0: 388, 4: 446, 8: 464 |
| batch | 8: 440, 32: 447, 64: 395 |
| replay cap | 500: 372, 2000: 447, 8000: 384 |

### Learning-curve check (100 -> 400 -> 1500)

Monotonic rising: 12/21. Monotonic falling: 2/21. Interior peak at 400: 3/21.
Other/mixed: 4/21. Most draws, including REF (351 -> 499 -> 595), were still
rising at rung 1500 -- exactly the situation `Docs/model-training-playbook.md`
warns a ladder stopping before the peak will understate. This, reviewed with
the developer, drove Round B's design (seed replication + a 4th rung).

## Mid-study blocker: roster corruption

`rank.exe check` failed outright (`model hash mismatch for
models/sweep/slot6.txt`), blocking every `rank.exe` subcommand including
Round A's own roster build. Pre-existing damage from a 2026-08-16 incident
(`todo.md`), not caused this session, but it now actively blocked Pass 2
work. Per `todo.md`'s own note that this is a roster-data decision for the
developer, stopped and asked rather than deciding unilaterally; developer
chose "deactivate the 8 broken lines."

First fix attempt (`on` -> bare `off`) still failed `rank.exe check`,
because a plain `off <id>` line is still hash-validated by the parser --
only a fully `#`-commented line is skipped. Corrected to match the
pre-existing `s9` convention exactly (`# off <id>  # DEACTIVATED
2026-08-17: ...`). Verified: `rank.exe check` -> 162 active agents, OK.

## Round B: seed replication + extended ladder

Ran to completion: 96 checkpoints (8 promoted draws x 3 seeds x 4 rungs:
100/400/1500/4000), rostered as `ranking/roster_gumbelzero_pass2_roundb.txt`
/ `cohort_gumbelzero_pass2_roundb.txt`, played into the same screening store,
screened via the same pinned-fit mechanism.

### Bug found and fixed during the run

`tools/gumbelzero_study_roundb.ps1`'s original seed-list construction,
`@($row.Seed, $row.Seed + 10000, $row.Seed + 20000)`, is a PowerShell
parsing trap: building the arithmetic inline inside an `@(...)` array
literal silently expands to 5 elements
(`$row.Seed, $row.Seed, 10000, $row.Seed, 20000`), not 3. Verified via an
isolated `PowerShell` tool test. Consequence: each draw's original seed got
trained 3 redundant (but byte-identical, deterministic) times, and the two
"new" seeds were the literal values `10000`/`20000` for every draw instead
of per-draw offsets. Fixed at the source (precompute into local variables
before building the array; the fix is now the version of the script in
`tools/gumbelzero_study_roundb.ps1`). The already-generated ledger data
(161 lines including duplicates) was deduplicated by keeping the first
occurrence per `(block,seed,rung)` key via a one-off Python script rather
than re-training; verified exactly 3 distinct seeds per block and 96 total
rows before building the real Round B roster from the deduplicated set.

### Results (mean +/- population sd across 3 seeds)

| block | sims | r100 | r400 | r1500 | r4000 |
|---|---|---|---|---|---|
| R17 | 200 | 502+/-63 | 706+/-36 | 736+/-18 | 691+/-63 |
| R3 | 100 | 395+/-45 | 432+/-1 | 623+/-28 | 654+/-12 |
| REF | 50 | 313+/-36 | 429+/-16 | 577+/-16 | 618+/-12 |
| R8 | 200 | 461+/-19 | 534+/-8 | 560+/-11 | 563+/-28 |
| R19 | 100 | 299+/-10 | 384+/-16 | 422+/-8 | 437+/-22 |
| R7 | 200 | 450+/-38 | 408+/-22 | 412+/-24 | 407+/-27 |
| R18 | 200 | 318+/-25 | 339+/-15 | 414+/-21 | 395+/-5 |
| R14 | 100 | 352+/-23 | 396+/-5 | 360+/-3 | 329+/-8 |

R17 leads at every rung. Its 1500 -> 4000 mean (736 -> 691) is misleading
read as a group decline: 2 of its 3 seeds are still climbing (664 -> 738,
701 -> 733) while one seed dropped sharply (762 -> 603). That single
outlier's swing (a 159-Elo range across seeds at rung 4000) falls inside
this project's documented 50-150 Elo training-seed-noise band, at the high
end of it. R3 and REF are both still rising at rung 4000. R8/R7/R18/R14 look
roughly flat or slightly declining by rung 4000.

## Cross-serving investigation (superseded methodology)

Before the compute-matched design below, an initial investigation tested
whether a checkpoint trained at one sims value performs as well when SERVED
(not retrained) at a different sims value, using the fact that `sims` lives
purely in the `gaz(sims=N)@1` head wrapper and not in the weight file, so any
checkpoint can be rated under any sims value with zero retraining. R15
(trained at sims=25) and R17 (trained at sims=200) were each rated at
multiple sims levels. This surfaced two methodology problems, both corrected
in place before drawing any conclusion: an initial 2x3 design was missing
one cell (R17 served at sims=100), caught and filled; and comparing
checkpoints at the same RUNG (game count) is not a fair comparison once
sims itself costs more compute per game, caught when the developer pointed
out that "the higher sims does more training time compute even at the same
rungs." That second correction is what produced the compute-matched design
below, which is the result to cite -- the intermediate rung-matched numbers
from this phase are not repeated here since they are superseded by it.

## Compute-matched sims comparison (theory 48)

One fixed configuration (REF's hyperparameters: lr=0.01, l2=0.0,
replay-capacity=2000, replay-warmup=32, batch=32, open-plies=4, seed=6001),
sims as the only varying axis. Per-game cost measured directly at each sims
value (games=300 under the fixed config): 11.22/18.64/40.45/78.88/150.08
ms/game at sims=50/100/200/400/800. Game counts chosen so
`games x ms_per_game` matches a shared target (~60,675 ms-equivalent, the
sims=200 @ 1500-games point): 5400/3250/1500/770/400 games respectively.
Trained into `models/sweep/slot910.txt`..`slot914.txt` (hashes `e5036e7d`,
`e74b5740`, `cf24b0f0`, `929c88b8`, `a23e4469`), training wall times
54.7/62.3/49.1/57.5/57.0s (confirming the compute-matching held in practice,
not just on paper). Rated at each checkpoint's own native sims via
`rank.exe gauntlet --games 2` (2 games against every active pool agent, 324
games total per checkpoint):

| sims | games | ms/move at training | Elo (native-sims gauntlet) |
|---|---|---|---|
| 50 | 5400 | 11.22 | 585 +/- 22 |
| 100 | 3250 | 18.64 | 646 +/- 21 |
| 200 | 1500 | 40.45 | 673 +/- 21 |
| 400 | 770 | 78.88 | **683 +/- 21 (peak)** |
| 800 | 400 | 150.08 | 617 +/- 21 |

Elo rises from 50 to 400 sims, then drops at 800, outside 400's error band.
This rules out 25 sims (485 +/- 24, measured earlier at its own native rung)
as unproductive and 800 sims as past the peak under this compute budget.
Filed as `Docs/theories.md` theory 48 (generalizes theory 46's TD-Leaf
game-count interior optimum to a second, tradeable axis). Gauntlet caveat:
324 games at 2/opponent is screening-level, well under this project's
32-games/pair certification standard, adequate for ranking these 5
checkpoints against each other, not for certifying any of their absolute
Elo numbers.

## lr/l2 confound check

Round A's raw group means (lr: 386/421/572 for 0.003/0.01/0.03; l2:
595/422/362 for 0.0/0.0003/0.001) both look like clean, large effects. Since
Round A is single-seed random search (not a factorial grid), checked whether
either bucket's mean also carries a higher mean `sims` (a known, confirmed
~90-100 Elo lever across this range):

| axis bucket | mean sims in bucket | mean Elo |
|---|---|---|
| lr=0.003 | 56.2 | 386 |
| lr=0.01 | 82.5 | 421 |
| lr=0.03 | 133.3 | 572 |
| l2=0.0 | 93.8 | 595 |
| l2=0.0003 | 68.8 | 422 |
| l2=0.001 | 83.3 | 362 |

The lr buckets climb in mean sims right alongside Elo (56 -> 82 -> 133), so
a substantial part of lr=0.03's apparent lead could be its bucket having
drawn higher sims, not lr itself. The lr finding is NOT settled. l2's
extreme buckets have similar mean sims (93.8 vs 83.3), so its 233-Elo gap
survives this check and is the more trustworthy Round-A finding: l2=0.0 is
the best value found (also the tested floor). The same sims-correlation
check on open-plies found open=4's bucket carries a higher mean sims (100)
than open=0 (68.8) or open=8 (70.8), so open=4's contribution is suspect
even though open=0 vs open=8 (similar mean sims, 68.8 vs 70.8, 76-Elo gap)
looks comparatively cleaner. Neither lr nor open-plies should be treated as
settled from Round A alone.

## Direct match: R17's best checkpoint vs the plain chip counter

To ground how far this regime is from competitive, played
`ab(deep=6)@1.classic(chip=100)@2` (plain fixed-depth-6 alpha-beta, no
tt/ord/node budget, pure chip-count evaluator, no learning at all) against
`gaz(sims=200)@1.learned(s746,3de6dd38)@1` (Round B's R17, seed 2117, rung
4000, the single best Gumbel-Zero checkpoint by Elo). 32 fresh games (16 per
color assignment) into an isolated scratch store
(`ranking/matches_ab6_vs_gz.jsonl`), never touching the canonical ladder.

| agent | win rate as White | win rate as Black | wall ms/move | CPU ms/move |
|---|---|---|---|---|
| ab(deep=6).classic(chip=100) | 16/16 (100%) | 16/16 (100%) | 6.08 | 5.90 |
| gaz(sims=200).learned(R17) | 0/16 (0%) | 0/16 (0%) | 0.43 | 0.60 |

A clean 32-0 sweep to the chip counter, in both colors, at roughly 10x less
compute per move. This is consistent with, though not required by, the
pooled Elo gap against this project's openless champion (1044 +/- 11,
`ranking/CHAMPION.md`, 2026-08-01 fit) versus R17's own pooled range
(~691-738 across seeds/rungs) -- a ~300-350 Elo gap that would predict
roughly 88% expected score under a transitive Bradley-Terry read, not 100%.
The pooled fit and this direct pairing are not required to agree exactly:
`rank.exe matchup`'s own regime-residual analysis already documents that
Bradley-Terry's transitivity assumption can fail badly for a specific pair.

## Slot collision incident (caught before committing)

Discovered while preparing to commit: Round A's slot range (660..722)
silently overwrote 8 pre-existing, git-tracked model files
(`models/sweep/slot700.txt`..`slot707.txt`), the 2026-08-02
"regime-diversity backfill" checkpoints (`teacher_games`/`model_games`
regimes), explicitly whitelisted in `.gitignore` as permanent assets. Same
class of incident as the slot6/slot7 corruption from 2026-08-16, this time
self-inflicted by claiming a new sweep-slot range without cross-checking
`.gitignore`'s exception list (only `src/CLAUDE.md`'s slot-ledger prose was
checked, and it was silent on 700..707 since that range was never entered
there either).

No data was actually lost, since nothing had been committed yet -- git
still held the original blobs. Fix: copied the 8 overwritten Gumbel-Zero
checkpoints (R13 rungs 400/1500, R14 rungs 100/400/1500, R15 rungs
100/400/1500) to slots 990..997 (verified identical content hashes before
and after the copy), restored slots 700..707 to their original committed
content via `git checkout`, and repointed Round A's ledger CSV and
roster/cohort files at the new slot numbers. No retraining was needed.
The 8 relocated checkpoints' already-played Round-A screening games remain
valid history under their original (now orphaned relative to the current
roster file) ids -- none of R13/R14/R15 were promoted to Round B, so
nothing downstream depends on re-verifying them. `src/CLAUDE.md`'s slot
ledger now states the rule this incident violated: check `.gitignore`'s
exception list, not just the ledger prose, before claiming a new range.

## Commit

`git commit`, message covering the Pass-2 study, the roster fix, the
compute-matched sims finding, the direct match, and the slot collision fix
and its repair.

## Future Work

- **lr is not settled.** Round A's apparent lr effect (386/421/572 across
  0.003/0.01/0.03) is confounded with sims in the single-seed sample. An
  isolated sweep (fixed sims, only lr varied, matching the compute-matched
  sims study's design) would settle it and, by extension, test whether the
  l2=0.0 finding still holds at a properly isolated lr.
- **open-plies is not settled** for the same reason as lr (open=4's bucket
  also carries elevated mean sims); open=0 vs open=8 looks cleaner but
  wasn't isolated either.
- **No leading configuration has reached its ceiling.** R17, R3, and REF
  were all still rising in most seeds at Round B's final rung (4000). The
  interior-optimum finding for sims (theory 48) doesn't establish an
  interior optimum for game count within one training run at fixed
  hyperparameters -- that's still open, unlike TD-Leaf's confirmed
  interior optimum (theory 46).
- **The Elo gap to a non-learned baseline is large and only measured once.**
  The 32-0 result against `ab(deep=6).classic(chip=100)` used one
  checkpoint (R17, one seed) against one opponent core. It is not a general
  verdict on the regime, and wasn't extended to more games since 32-0 is
  already close to as unambiguous a signal as that sample size can give.

## Ideas This Inspired

- **Exposing `ai_gumbel.cpp`'s hardcoded search-time constants as agent-level
  knobs**: root Gumbel-top-k breadth, `kGumbelCVisit`/`kGumbelCScale`, and
  the Sequential-Halving round schedule are currently fixed internals, not
  per-agent. Mechanically the same kind of change that turned `sims` from a
  hardcoded 50 into `gaz(sims=N)@1`. Root breadth and the halving schedule
  are checkpoint-agnostic (testable directly against already-trained
  checkpoints, no retraining needed); `cVisit`/`cScale` control how fast the
  search trusts its own backed-up values over the raw policy prior, which
  plausibly matters more while the network is this weak. Would need new ID
  grammar, a version bump on the `gaz()` head, and roster/slot plumbing
  before any sweep could start.
- **Using the trained value head under classical alpha-beta search.**
  `JointModel::forward()` delegates to its value head, which is exactly what
  the existing `LearnedValue` evaluator consumes, so the current trained
  weights could be dropped into `ab(deep=K,tt,ord,...)@1.learned(...)@1`
  today with no new code, giving a genuine alpha-beta depth lever for the
  same network. This tests the value head's quality under strong search in
  isolation from the trained policy head and Gumbel-Zero's own search
  algorithm, a different question from "is this self-play regime good."
