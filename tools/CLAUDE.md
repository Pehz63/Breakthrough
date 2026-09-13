# tools/ Reference

Build/run scripts, the trainer and ranker CLIs, study scripts, and the
artifact directories they write. Loaded when working on files in `tools/`.
The always-loaded overview and the most common command lines live in the root
`CLAUDE.md`. Engine/ML/ranking internals live in `src/CLAUDE.md`.

**`-Workers` default: leave 2 cores free.** Scripts that fan out parallel
`train.exe`/`rank.exe` processes (`gumbelzero_*_study.ps1`, `label_study.ps1`,
`opener_bias_study.ps1`, `opener_bias_retrain.ps1`, `tdleaf_study.ps1`,
`train_vs_champion.ps1`, `run_tournament.ps1`) default `-Workers` to
`[Math]::Max(1, [Environment]::ProcessorCount - 2)` rather than a hardcoded
number or full core count, so the machine stays usable for other work during a
multi-hour or multi-day run. Pass `-Workers` explicitly to override.
Developer instruction, 2026-08-19, after a hardcoded `-Workers 12` run on this
project's 12-core machine left nothing free.

`run_rank.ps1` and `sweep_pst_v2.ps1` are the exception: their `-Workers`
default is `1` on purpose (serial play gives clean, unshared ms/move timing;
see `run_rank.ps1`'s own header comment), and parallel play is an explicit
opt-in via `-Workers N`, not the default to begin with. That default is
unrelated to core-count headroom, so it is not changed by this rule.

## Common commands

Copy-paste forms beyond the root's short list:

```powershell
.\tools\run_train.ps1 -Build selfplay-supervised --games 250 --epochs 6 --feature-version 2 --out models/pst_value
.\tools\run_train.ps1 tournament --games 10                # single-process, default depth ladder
.\tools\run_train.ps1 docs                                 # regenerate ML.md AUTODOC region + registries
.\rank.exe history --agent "ab(deep=4"                         # per-opponent record for one agent
.\rank.exe run --games 8                                   # serial play (live progress) then rate
.\rank.exe pairgen --a "<challenger>" --b "<champion>" --games 80 --open-plies 6 --open-side a --out data/pg.jsonl   # asymmetric opener: only agent a plays random
.\rank.exe opener-bias --a "<champion>" --b "<id>" --judge "<learned id>" --games 60   # how much the random opener degrades agent a's position
.\rank.exe opener-swap --a "<id>" --b "<champion>" --games 20 --open-plies 6   # same opener snapshot, colors swapped: position bias vs agent skill
.\rank.exe agree --a "<fixed-depth id>" --b "<node-budget id>" --games 6 --open-plies 6   # % of moves two budget rules on ONE core pick identically
.\rank.exe nodeprofile --id "<id>" --games 12 --out prof.tsv                           # cumulative nodes at each depth iterative deepening finished
.\tools\train_vs_champion.ps1               # 10-arm vs-champion training study (resumable; -AnalysisOnly reprints the bucket tables)
.\tools\opener_bias_study.ps1               # Theory 6: opener-inflation sensitivity sweep + mechanism measure (Layers 1+2)
.\tools\opener_bias_retrain.ps1             # Theory 6: retrain the oracle arm on asymmetric-opener data (Layer 3; -DryRun for a tiny check)
.\tools\hill_climb.ps1 -Iters 20 -Promote -PromoteTop 2    # climb, then promote winners to the roster
.\train.exe tdleaf --out models/sweep/tdl --init models/pst_value.txt --ckpt-at "100,250,500,1000,2000" --lambda 0.7 --lr 0.01 --seed 1001
.\train.exe tdleaf --out models/sweep/tdl --init models/pst_value.txt --wall-ckpt-at "7200,14400,28800" --seed 1001   # wall-clock rungs: _t7200/_t14400/_t28800 + a hard stop at the last mark
.\train.exe tdleaf --out models/sweep/tdl2 --resume models/sweep/tdl.txt --wall-stop 57600 --seed 1001   # continue ONE cumulative ladder (2x rung, plan Part 5 answer 4)
.\train.exe tdleaf --out models/sweep/slot1858 --init "" --depth 12 --node-budget 100000 --rem 70 --retain --backup treestrap --lr 0.0005 --seed 1001   # replication-study switches: --backup td-leaf|td-directed|rootstrap|treestrap, --tree-min-depth N, --terminal winloss|depth, --augment mirror, --explore-dist eps|ordinal (--ordinal-start/--ordinal-end/--ordinal-games)
.\train.exe gumbelzero --out models/sweep/slot650 --games 40 --sims 50 --ckpt-at "20,40" --seed 1001   # Pass-1 sanity only, no Elo certified
.\train.exe gumbelzero --out models/sweep/slot650 --games 40 --sims 50 --model-type mlp --mlp-hidden "64,32" --seed 1001   # mlp applies to both heads
.\train.exe gumbelzero --out models/sweep/slot650 --games 40 --sims 50 --model-type conv --conv-channels "16,16" --seed 1001   # conv: value head only, policy head stays linear
.\tools\tdleaf_study.ps1 -Workers 12 -Phase all --GamesPerPair 8   # the full TD-Leaf cohort study
.\rank.exe play --roster ranking/roster_tdleaf.txt --cohort ranking/cohort_tdleaf.txt --games 32
.\rank.exe rate --roster ranking/roster_tdleaf.txt --pin ranking/standings.tsv   # screen on a FROZEN scale
.\rank.exe play --roster <roster> --cohort <cohort> --paired-openings --common-openings --games 400   # every cohort agent meets each panel agent on the SAME opening couples (common random numbers)
.\tools\replication_pass2.ps1 -Step curve -Panel ranking/panel_rep1.txt   # then -Step noise -NoiseArms B0,A3 -NoiseCpu <s>, -Step tune -TuneCpu <s> -BaseLambda <B0's>
python analysis/replication_stage1.py selftest   # Stage 1 analysis (analyze / export / verify-store / selftest), must recover its synthetic effects before any real reading
```

**Rating a new cohort without moving the existing scale.** A normal refit
re-solves every rating at once, so adding a cohort shifts the whole table and
every number in `ranking/CHAMPION.md` stops being comparable mid-study. Two
flags avoid that:

- **`play --cohort <id list>`** schedules only pairs touching a listed agent.
  Without it, a `--games 32` pass also tops up every existing pair: measured
  2026-07-29 the store holds 27,265 pairs at a median of 6 games, so that is
  about 702,000 games that have nothing to do with the cohort.
- **`rate --pin <ratings/standings tsv>`** holds those agents at their listed
  Elo and solves only for the rest (`rankFitBTPinned`). Uses cohort-vs-cohort
  games too, so it resolves the cohort's internal order, which per-candidate
  gauntlets cannot. Writes its own `ranking/*_pinned.*` family (gitignored) and
  leaves the canonical files untouched.
- **`rate --prior <X>`** sets the virtual games at 50% added to every played
  pair (default 0.5, `g_rankPriorGames`) for that run, pinned or not, and
  writes a `ranking/*_prior<X>*.*` family. The prior's share of a pair is
  X / (games + X), so it matters when schedules differ in games per pair
  (`Docs/benchmarking.md`, "Elo scale drift across fits").

**A pinned fit is screening, never certification.** The champions' ratings are
inputs to it, so it can never dethrone one. Certify by choosing which cohort
agents to keep, appending them to `ranking/roster.txt`, and running a plain
unpinned `rank.exe run` (`ranking/CHAMPION.md` rule 1).

**Screening cohorts play into their own store** (`ranking/matches_screen.jsonl`,
`tdleaf_study.ps1 -ScreenStore`, added 2026-08-01), never the permanent ladder.
A cohort is mostly candidates that get discarded, and `--cohort` tops up every
cohort-vs-roster pair, so writing them to the shared store floods it: the Pass-2
run left 457,611 such rows, 71% of the whole store, with some rostered agents
taking 60% of their games against candidates that were then thrown away.
Removing them afterwards is not free either, because the fit is joint -- those
games were evidence about the ROSTERED agent too. The pinned screening fit still
works unchanged, since the screening store holds cohort-vs-roster games and the
roster enters at its pinned Elo. **On promotion**, run `rank.exe split` over the
screening store: its `roster` bucket is exactly the games between agents that
are now rostered, which is what gets appended to the permanent store and listed
in `matches.index.txt`. Discarded candidates' games stay behind.

### Regimes, matchups, and why pooled Elo depends on the pool

A **regime** is how an agent was produced, already stamped into every canonical
id (`tdleaf_self`, `pool_games`, `position_elo`, `teacher_games`, `model_games`,
`weight_merge`, plus `classic`/`exp`/`adv`/`nonlearning` for the non-learned).
No extra labelling is needed to group by it.

```powershell
.\rank.exe matchup --min-games 500      # regime x regime: actual vs Elo-expected, and the residual
.\rank.exe rate --regime-balanced       # fit with every regime bloc weighted equally
```

Bradley-Terry gives each agent ONE strength number, so it assumes transitivity
and cannot represent "A is strong but happens to lose to B". `matchup` measures
where that assumption breaks: **residual = actual score rate minus Elo-expected
score rate** for a regime pair, both colours combined. A large residual is a
matchup the pooled rating is actively misreporting.

Measured 2026-08-01, and the reason this exists. The TD-Leaf self-play cohort
was initialised from a learned champion and self-played, so it specialised
against that champion's style: it beat `learned-other` agents **70.5%** but the
chip counter only **63.6%**. That bloc was ~30% of the store, so the fit set the
chip counter's rating to explain those 196,767 games and mispriced it elsewhere
-- `classic` vs `pool_games` ran a **-6.0** residual. Dropping the cohort cut
that to **-2.7**, and the chip counter fell 33 places in the openless standings
purely from the population change. No number of games fixes this; it is a
modelling failure, not sampling noise.

**Roster composition is therefore part of the measurement.** The 2026-08-01
active roster is 30.6% `classic` + 30.0% `pool_games`, so "strong" and "good
against those two" are nearly the same claim. `rate --regime-balanced` weights
each pair by `1/(agents in A's regime * agents in B's regime)`, rescaled to the
original total, so every regime bloc counts equally however many agents wear it.
It answers "strong against the space of strategies" rather than "strong against
this pool", and it moves the table a lot: `classic(chip=100)@2` goes from
openless rank 35 to rank 5. Like `--pin`, it writes its own `ranking/*_balanced.*`
family and never the canonical files, because promoting it to canonical would
re-certify every champion and is a deliberate act.

### Keeping the match store a size a host will accept

The store only grows, so left alone it eventually stops being pushable: GitHub
rejects any file over 100 MB outright, and a single growing file also deltas
badly, so every commit re-stores the whole thing. Two subcommands manage it, and
both are safe to run repeatedly.

```powershell
.\rank.exe split                      # DRY RUN: what would move where, and how big each group is
.\rank.exe split --max-mb 90 --apply  # regroup the store into parts + write matches.index.txt
.\rank.exe seal --max-mb 90           # roll an oversized live tail into sealed shards
```

Sealing the committed store is automatic. `rank.exe play` (unsharded) and
`rank.exe gauntlet --keep` seal the store they appended to before exiting, and
`tools/run_rank.ps1` runs `seal --max-mb 90` after every rung's merge. Both act
only on a store with a part index (`<stem>.index.txt`), which marks the
committed store. Screening and scratch stores are gitignored and stay single
files. `seal` defaults to `--max-mb 90` (`RANK_STORE_SEAL_MB` in
`src/ranking.h`). Commit new shards together with the index lines `seal`
appends, since a clone with the index but not the shards loads without them.
The backstop is `.githooks/pre-commit`, which refuses any staged file over 95
MiB (enable once per clone with `git config core.hooksPath .githooks`).

`split` groups rows by who played the game into `roster` (both agents in the
roster), `retired_<group>` (the `--group` substring, default `tdleaf_self`), and
`retired_other`, capping each part at `--max-mb`. Nothing is deleted and every
part stays listed in `matches.index.txt`, so **ratings do not change until an
index line is removed**. That was verified on the real 649,434-game store: after
the 2026-08-01 split, `ratings.tsv` and `standings.tsv` came back byte-identical
and `games.tsv` was a confirmed pure reordering.

Two things to know before using it:

- **Removing a group is not free.** An archived row was also evidence about the
  rostered agent that played in it, so dropping a group moves the Bradley-Terry
  fit for agents you kept. Delete the index line, refit, and compare standings
  before deciding the change is acceptable.
- **`split` rewrites all parts,** so run it before `seal`, and expect a prune to
  re-commit the roster part. Sealed shards are otherwise immutable, which is the
  property that keeps the committed history from growing: only the small tail
  changes between runs.

## Trainer (`train.exe`)

The modular ML toolchain is a separate binary (does not touch `breakthrough.exe`).
Notes:

- The linear value model overfits past ~6-8 epochs on outcome labels (loss climbs
  back toward 0.69); keep `--epochs` small (~6).
- `selfplay-supervised` takes `--feature-version 2` to train on the sparse
  piece-square layout (the incremental-search substrate, e.g. `--out models/pst_value`).
- `train.exe speed` benchmarks the v1 full-scan learned leaf against the v2
  incremental one side by side. It also runs the heuristic eval-level ladder
  (`g_evalLevel` 1/2/3: full chip rescan / incremental chip + full structure
  scan / fully incremental) at nonzero structure weights, with `--reps` fixed
  timed reps, `--warmup` discarded passes, interleaved levels, mean/median/min
  us/move, and an equivalence self-check (same end board + node count across
  levels). Measurement methodology: `Docs/benchmarking.md`.
- Raw build: `.\build_train.bat` (mirrors `build_tests.bat`). See `ML.md` for the
  full system and the "how to add more" workflow.

**Parallel depth-laddered tournament** (the `run_tournament.ps1` command in the
root `CLAUDE.md`, process-sharded across all CPUs, then rated). This is the old,
frozen tournament system, superseded by `rank.exe` / `ranking/` below (see
`Docs/ranking-workflow.md`) -- it has not run since its 2026-06-28 introduction and
its Elo scale is not comparable to the ranking pool's. Kept documented here only
because the binary still works; prefer `rank.exe` for any new agent evaluation.
Under the hood it runs `train.exe tournament-play --shard i --of K ...` (each shard
writes `data/tourney.jsonl.<i>`) then `train.exe tournament-rate ...` (merges, fits Elo,
prints the `Elo | ms/move | max ms | games | agent` table, writes `agents/champion*.txt`).
Threads are not used because the engine's board/eval state is global; processes each get
their own copy. Add `-Only "name1,name2,..."` to restrict the roster to those agent names
(include their depths in `-Depths` so the names exist); a subset run leaves
`agents/library.txt` + `champion*.txt` untouched. Every run is archived under `runs/<id>/`
(`config.json`, `elo.tsv`, `notes.md`, `results.jsonl`), logged in `runs/index.jsonl`, and
folded into the agent registry (`agents/registry.jsonl`, a union with a `spec_hash` that
flags retrains / changes; its regenerated rollup `agents/registry.md` has been deleted as
stale); `-Note "..."` records a pre-run note and `train.exe run-note --run <id> --note
"..."` attaches one later.

## Ranker (`rank.exe`)

The persistent agent Elo-ranking system is a third binary, independent of both
`breakthrough.exe` and `train.exe` (it links the engine sources plus `src\ranking.cpp`,
NOT `src\ml_train.cpp` or `src\settings.cpp`). Common invocations are in the root
`CLAUDE.md`. Raw build: `.\build_rank.bat`.

Agents are identified by a canonical ID string, e.g.
`ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=10,wall=3,column=2)@1` (grammar in `src\ranking.h`), which is
the permanent key of the append-only match store `ranking/matches.jsonl` (committed,
never regenerated). Every module segment carries its code version as `@N`, a constant in
the codec tables in `src\ranking.cpp`: bump one constant when that module's code changes
behavior and only the agents using it get new identities (a stale `@N` in the roster
fails the canonical check and prints the fix). The roster `ranking/roster.txt` is
hand-edited (`anchor|on|off <id>` lines, exactly one anchor). The scheduler plays only
each active pair's missing games (color-balanced, per-game srand seeds derived from the
pair + game ordinal, so shard splits and re-runs reproduce identical games), which makes
adding one agent O(N) games. Each game records per-side wall ms, process-CPU ms
(GetProcessTimes deltas, honest under parallel contention), node totals, effective
depth, plies, and end piece counts. Ratings are an anchored Bradley-Terry MM refit
(anchor = Elo 0, 0.5 virtual games prior per played pair, Fisher standard errors);
`rate` writes `ranking/ratings.tsv` (the full historical fit, every agent ever rated
including retired `gone` rows) + `ranking/standings.tsv` (**the file to read for current
standings**: the same fit filtered to active agents and grouped by search head, so a
retired identity or a mixed-head comparison cannot slip into a claim; its `eff_evaluator`
column also elides the turn weight `t` when the search cannot act on it (no `qs`/`part`),
since at fixed depth `t` shifts every leaf by one constant and reorders nothing -- compare
cores on `eff_evaluator`, while `evaluator`/`id` keep the exact canonical form) + `ranking/games.tsv`
(per-game export) + `ranking/report.md`. Active agents (2026-08-31 redesign) get three views
instead of one flat table: **"Ratings by category"** groups into `ranking/CHAMPION.md`'s 6
division x track buckets (`rankCategoryOf()`, pattern-matches the id -- mechanical fact only, NOT
the reference-class eligibility check, so the d8/nb2m oracle still shows `openless`/`node` despite
being excluded from that title) and sorts by Elo WITHIN each group, so it never mixes incomparable
heads/loadouts into one order the way a flat sort does; per-row columns are just Elo, `white edge`
(= white win% - black win%), the track's own compute column, `eff`, and id. **"All active agents
(flat)"** is a quick global scan by raw Elo, explicitly marked not a valid cross-category ranking.
**"Notable exceptions"** is where SE/games/margin/avg-plies live now -- not shown per row at all,
only listed when an agent deviates from its own group's median/mean (high SE, few games, margin or
plies outlier). The OLD full-column table (color-split win%, avg plies, margin, `ms/move`, `eff`,
`wall/mv`, `nodes/mv`, `state`) still covers **"Inactive and retired agents"**, where exhaustive
detail is the actual use case. `games`/`nodes/mv` are comma-formatted (`fmtInt()`) throughout since
both can run into six figures.
**`report.md`'s `id` column is `rankReportId()`, a human-readable simplification, NOT the
canonical id**: it drops `nodes=200k` (the default node budget) and drops `tt`/`ord`
unconditionally, since they are assumed on for nearly the whole roster -- but the minority
of active agents that deviate (a `tt`/`ord` ablation study) print an explicit `noTT`/`noOrd`
marker rather than silently rendering identically to the standard config (no marker under
`noab`, where `tt`/`ord` are inapplicable rather than off). Any other flag (a different
track, `qs`, `margin=`, a different node/time budget value) is printed in full. It also
rewrites a `learned(...)` core to lead with its training regime name and drop the content
hash. Never quote a `report.md` id anywhere that requires the canonical
form (`CLAUDE.md`'s ranking-claim hygiene rules, `ranking/CHAMPION.md`) -- read the `id`
column of `ranking/standings.tsv`/`ranking/ratings.tsv` for that instead, which are
untouched by this simplification. Learned agents embed an 8-hex model-file content hash in
the (canonical) ID and roster load hard-errors on a mismatch (a retrain is a new identity).
Full internals (ID codec, store row format, scheduler, BT fit, every subcommand, slot
conventions): `src/CLAUDE.md`'s `ranking.cpp` entry.

### Does a deterministic agent actually replay? (`determinism`)

`rankAgentIsDeterministic` answers whether an agent draws from `rand()`. That is
not the same as whether it replays. An agent can consume no randomness and still
play a different game on a second run, because search state outliving a move (the
transposition table) or a budget measured against the wall clock makes its choice
depend on something other than the position.

`rank.exe determinism` measures the stronger property. It replays every active
deterministic roster agent against a fixed deterministic probe, both colours,
`--replicas` times, and compares exact per-ply position traces. Replicas are
interleaved agent-minor so each is preceded by a different game sequence, and the
seed varies per replica. Cross-PROCESS reproducibility is a separate question: run
it twice and diff the `traceset` column. Exit code 2 when any subject-colour did
not repeat, so it works as a script gate.

**Run `--include-stochastic` before believing a clean sweep.** It adds agents that
DO draw from `rand()`, which must come back non-reproducible. A probe that only
ever prints "reproducible" is indistinguishable from one that cannot detect
anything, and this exact failure happened while the subcommand was being written
(a pinned seed made dilution agents replay, reporting a meaningless 56/56).

Measured 2026-08-27 (`plans/refutation-oracle-results-1-quiet-lodestone.md`):
238/238 subject-colours reproducible across two independent processes, 210/210 for
the node-budgeted subset in all four passes. The only agents that ever varied are
`model=111` and `model=113` on the `time=150ms` head, the two lines already
carrying `# cost flag` in `ranking/roster.txt`, and `model=113` failed once in
four passes. Note that for a `time=` agent the node total is NOT a trajectory
fingerprint: identical move sequences came back with node totals differing by
~5,000, so `Docs/benchmarking.md` defect 3's distinct-game tuple overcounts there.

### Mining one book that beats every deterministic agent (`refute`)

`rank.exe refute` builds a single position-keyed book intended to win every game
against every deterministic agent on the roster, both colours. It is the repaired
form of theory 14 that theory 33 did not cover: the book carries the full
continuation to the win, so the wearer's own brain is never consulted on a covered
line and there is no brain-portability handoff to fail.

**Why it is a one-player search.** With both sides deterministic a (book,
opponent, colour) triple produces exactly ONE game, so beating N opponents is
winning 2N specific games rather than 2N distributions. Finding each is depth-first
search over our own moves with the opponent as a fixed reply function. No minimax,
no sampling.

**Why every trial replays from move 1.** The opponent is a fixed function of the
game PATH, not of the position: `playOneGame` clears the transposition table once
per game, not per ply, so a `tt` agent's reply at ply k depends on everything
played before it. An alternative move therefore cannot be tried by un-playing the
last one, and the miner replays the whole game each time. That is the dominant
cost and the reason stage 1 is greedy.

**Why it is one book and not 2N books.** `openerBook` keys on
`positionKey(sideToMove)` alone, with no ply and no path, so one entry serves every
line reaching that position and two lines needing different moves from one position
cannot both be expressed. The miner avoids that conflict by construction rather
than repairing it afterwards: the book is shared from the first game onward, and
every move our side plays is committed immediately, so a later target inherits an
earlier one's choice. Deriving its own would not agree anyway, since the oracle's
own TT state at the same position differs by which opponent led it there.

**This command is how the transposition table's cross-agent contamination bug was
found.** Mining exposed it because a mined line is played back by a book that does
no searching at all, so any dependence of the opponent's replies on OUR searching
shows up immediately as the line failing to reproduce. Before the fix, 132 of 238
mined lines stopped reproducing, and the split was exactly on the opponent's own
flag: all 132 had a `,tt,` opponent, and 0 of the 77 non-`tt` opponents were
affected. The cause and the fix are in `src/ai_minimax.cpp`'s searcher-context
comment and in `Docs/corrections.md` under `TT CROSS-AGENT CONTAMINATION`. With the
fix in place the same command, same `tt` oracle, same 8 targets goes from 8 of 8
lines leaving the book to 0 of 8.

Stages:

1. Play every target once with the shared book, falling back to `--oracle` where
   the book is silent. A win claims ownership of every position on its line.
2. Repair the losers. Walk each losing line backwards from the deepest of OUR
   moves, and at any position no won line owns, re-search with the already-tried
   moves filtered out of the search root (the same one-shot whitelist `cbook`
   uses) and replay. Positions a won line owns are left alone, which is how a real
   merge conflict surfaces as a `blocked_shared` row instead of quietly breaking a
   solved line. **Ownership is enforced at the write, not only at the branch.** A
   repair that branches at a free position can still, further down its new
   continuation, cross a position an earlier winner owns and want a different move
   there, so a win is committed only after its WHOLE path is checked against the
   owners map. A win that would overwrite an owned position is refused and the
   search falls through to the next candidate move at that node (the `rejected`
   column, and a `reject:` line naming the position). Stage 1 needs no such check,
   it writes insert-only. The count is reported because refusing a win is a real
   cost: a line that ends up conceded may have been solvable at the price of
   breaking another.
3. Prune to the entries a winning line actually walks, write
   `models/book<slot>.txt`, audit, then verify. Exit code 2 unless the book wins
   every line, on its own, through `openerBook`.

**Read the `audit` line, not the `mined` line.** A mining win only says a line was
found while our side was still searching at every unbooked ply. The audit replays
each target against the WRITTEN book and reports how many lines LEFT the book:
those were carried by the wearer's brain and are not a memorization result
whatever their W/L says. The audit runs on every invocation, because the first
version of this command reported 238/238 mined and 226-12 verified while 132 of the
238 lines had silently fallen out of book. `--verify-only` runs the same audit plus
verification against an existing book without mining or rewriting it, which is how
that was diagnosed (`oob_first` in the TSV names our first unserved move, and
`oob_key` gives that position's hash in the same 16-digit form the book file uses,
so it can be grepped straight out of `models/book<N>.txt`).

**`ovr_ply` / `ovr_key` measure an overwrite instead of inferring one.** During the
audit the replay stands on the same positions the mine recorded for that line, so
if the book hands back a move DIFFERENT from the mined one at a position the line
is replaying correctly, that line was overwritten by a later winner. Those two
columns name the first ply where it happens and the position's hash. They are
NOT COMPUTED under `--verify-only`, which loads a book with no mined path to
compare against, nor on a conceded line, and in both of those cases they still
print -1, the same value a line that WAS compared and found clean prints. So -1
alone does not mean the comparison ran. Reach for these before theorising about
why a line left the book: a
stage-3 collision count says only that a merge conflict happened somewhere, and
several lines broken by ONE overwritten position will still show DISTINCT
`oob_key` values, because each faces a different opponent and so reaches a
different successor after the wrong move.

**`off_ply` / `off_by` say WHOSE move left the mined path**, which `ovr_ply`
does not. `ovr_ply` compares the written book against the mined path, so
`ovr_ply = -1` on a line that left the book was read as "our moves reproduced the
mine, therefore the opponent replied differently". That does not follow.
`off_ply` is the first of OUR plies whose POSITION differs from the mined one at
the same index, and `off_by` is 1 if our own move at the previous ply differed
and 2 if ours matched, so the opponent is what moved. The console prints the
split. Read `off_ply` WITH `oob_first`, not instead of it: `oob_first` says where
the BOOK fell silent and `off_ply` says where the GAME left the path, and a line
can leave the path while the book is still serving.

**A reproducible audit is not a determinism problem.** Before treating lines that
leave the book as a state leak, re-run the audit: `--verify-only` on an existing
book replays the same 210 games without mining. `models/book23.txt` audited on
the `@3` binary on 2026-09-09 reproduced the `@2` run of 2026-08-29 exactly (3,
7, 11, 12 lines out at 120 / 160 / 200 / 210 audited, 202 won, verify 202-8-0),
so those 12 are a systematic difference between the mining condition, where our
side searches at unbooked plies, and the audit condition, where the book answers
instantly. Four other explanations were tested and are dead, and the record is
`plans/determinism-audit-results-1-quiet-transom.md`.

```powershell
.\rank.exe refute --slot 21 --tries 4 --out ranking/refute_book21.tsv
.\rank.exe refute --slot 22 --wearer "greedy@1.classic(chip=100)@2" --only "<substring>"
```

Book slots are immutable (the opener ID does not hash the file), so `refute`
refuses to overwrite an existing `models/book<N>.txt` without `--force`.

**Two checks the tool runs on itself, both worth reading in the output.** It
compares the miner's own verdict against the `openerBook` replay and warns when
they disagree, because a book that only wins inside the tool that mined it has
proved nothing about the agent that will wear it. And it detects a search that
ignores the root whitelist and returns an excluded move, which would otherwise make
stage 2 silently re-try the move that already lost. Coverage is reported directly by the audit, and
a deliberately weak `--wearer` is the independent check on it: if the book really
covers a line, a weak brain never decides anything and the record is unchanged.

**What a clean sweep would and would not establish.** Memorization, not strength.
Such an agent belongs in `ranking/CHAMPION.md`'s reference class, not in a category
race: a Bradley-Terry fit assigns one strength parameter per agent, and this one is
maximally intransitive, going undefeated against agents that reproduce a line while
rating ordinarily against the dilution and random-opener agents that never do. Its
pooled Elo would be a fit artifact of the kind `rank.exe matchup` exists to expose.

### Two rating pools

The ranker now maintains two independent pools. They answer different questions and
their Elo scales are NOT comparable (different fits, different priors).

| | Fixed-start pool | Diversified pool |
|---|---|---|
| Roster | `ranking/roster.txt` (116 agents) | `ranking/roster_open.txt` (14 agents) |
| Store | `ranking/matches.jsonl` | `ranking/matches_open.jsonl` |
| Outputs | `ratings.tsv`, `standings.tsv`, ... | `ratings_open.tsv`, `standings_open.tsv`, ... |
| Every game starts from | `boards/board1.txt` | a random position 8 plies in |
| Books | meaningful, this is where they live | inert, so no book agents |
| Distinct games / stored rows | median **0.438**, min 0.062 | median **1.000**, min 1.000 |
| Answers | "strength at the standard start" | "strength across openings" |

Run the diversified pool with `-Store` (NOT `--in`/`--out`, which the driver owns):

```powershell
.	ools
un_rank.ps1 -Workers 12 -Store ranking/matches_open.jsonl --roster ranking/roster_open.txt --games 32 --paired-openings
```

**`--paired-openings`** is what makes this pool worth having. The scheduler emits each
pair as colour-swapped couples, and the flag gives both games of a couple ONE seed
derived from the canonically ordered pair. The random opener draws its moves from
`rand()` and no brain is consulted during the opener window, so an identical `rand()`
stream produces an identical opening line: the couple plays the SAME position with the
colours reversed. Agents are compared on how they recover from equal ground rather
than on which of them drew the kinder start. The flag is inert for agents that consume
no `rand()`, so it cannot change a deterministic fixed-start pool.

Measured payoff: error bars scale as `1/sqrt(n)` (median pm 53, 38, 28, 20 at 4, 8, 16,
32 games/pair), which is the signature of genuinely independent samples and is exactly
what the fixed-start pool does not do (`Docs/benchmarking.md`, defect 3). Rank-order
stability across those fills: Spearman rho 0.974, 0.969, 0.987, with 6, 6, then 3 of 14
agents changing rank. Converging at 32, not converged.

The **hill climber** (`tools/hill_climb.ps1`) optimizes the Advanced weight mix at a
fixed depth using `gauntlet` as fitness: 13 climbed weights (chip, wall, column,
forward, support, center, mobility, hole, control, open, race, overext, noise), turn
pinned at `-Turn` (20), noise seed and RaceWin pinned (`-NoiseSeed` 1 / `-RaceWin` 1),
absolute values renormalized to sum 80 (so the mix, not the scale, is searched and
candidates dedupe), greedy-from-best with `{1,3,5}`-unit simplex steps + drastic chip
resets, id-keyed cache. `-AllowNegative` adds sign-flip mutations and signed drastic
resets (weights may go negative; the only way to reach e.g. the capacity direction of
positive forward + negative chip). It plays the small stochastic pool
`ranking/climb_roster.txt` by default; `-Promote` appends the top finds to
`ranking/roster.txt` and does a full refit. The roster also carries a dense diluted-d6
ladder (random-move `dil(prob=P)` + stochastic-depth `dil(prob=P,deep=N)`) so the top of the table
is well-resolved and the climber has non-deterministic opponents.

## Script details

| File | Purpose |
|---|---|
| `run_tests.ps1` | Build and run the Catch2 test suite in one step: `.\tools\run_tests.ps1 -Build`. Use the `/run-tests` skill to invoke this correctly from Claude sessions. Calls `build_tests.bat` (repo root), which uses `vswhere` to locate VS automatically. |
| `run_train.ps1` | Build (`-Build`) and run `train.exe`, passing args through. |
| `run_tournament.ps1` | Mint a UTC `RunId`, write `runs/<id>/` config via `run-config`, launch K `tournament-play` shards in parallel (one process each, own output file), then merge + `tournament-rate --run <id>`. Params include `-Only`, `-Note`, `-RunId`. |
| `run_rank.ps1` | Build (`-Build`) and run `rank.exe`, passing args through. `-Workers K` shards `play` across K processes (per-shard `<store>.<s>` files appended to the store only after every worker exits cleanly), then rates once. **Ladders by default:** plays rungs 2, 4, 8, ... `--games N`, relaunching the shards and merging between each, which is the same total games as one pass but readable minutes in. `-NoLadder` for a single pass, `-PinEachRung <ratings.tsv>` to fit after every rung (writes only `ranking/*_pinned.tsv`). The merge drops structurally torn lines with a warning rather than appending them. Uses `PositionalBinding=$false` so `--key value` passthrough args are not captured by named params. |
| `sweep_pst.ps1` | Small training sweep for v2 PST models: train a teacher-depth x games grid, then rate each candidate serially via `rank.exe gauntlet` (one shared `models/pst_value.txt` slot). Superseded for large studies by `sweep_pst_v2.ps1`. |
| `sweep_pst_v2.ps1` | General model-sweep harness (was PST-only). A candidate is a `Group`/`Slot`/`Meta`/`Args` object where `Args` is a raw `train.exe` arg array trained into `models/sweep/slot<N>.txt`, so ANY train.exe flag (feature version, `--model-type`, `--residual-skip`, ...) drops into a candidate with no scaffolding change; a candidate may also carry an optional `Wrapper` (its own search shell, e.g. the cheaper `$MlpWrapper` for full-scan MLP cells). Groups: A teacher grid, B dilution decay, C self-play bootstrap chains, D replay extraction, E L2, **F linear residual chip-skip baseline (plain vs auto-skip, theory 24)**, **G MLP capacity comparison (`--model-type mlp` x hidden x skip {off,auto}, theory 24)**. `-Groups "F,G"` selects a subset (default all; only selected groups consume slots, so `F,G` alone fits slots 3..14); F/G share one replay extract (`data/replay_v2_residual.jsonl`). Trains each candidate, captures the `Stratified loss by |matDiff|` printout into the report (`Loss0`/`Loss1`/`Loss2`, the theory-24 equal-material calibration measure), appends candidates to the roster (idempotent), rates everyone in ONE `rank.exe run` (sharded via `-Workers`), and writes `models/sweep/report_v2.csv`. `-NoRate` stops after training + stratified loss (skips the roster/matches append + rating -- cheap and reversible; run the full rating later). `-Only N` dry-runs the first N candidates. After a rating study, trim the sweep lines back out of the roster (their games stay in `matches.jsonl` as retired history). First run's findings (groups A-E): `plans/training-sweep-results-1-luminous-snail.md`; F/G (residual/MLP): `plans/residual-mlp-results-1-tingly-chipmunk.md`. |
| `train_scaling.ps1` | Resumable data-scaling study for v2 value models: pins the sweep-validated recipe (d2 teacher, dilution decay), doubles the self-play game count until the mean screening Elo gain over its seed replicas drops below `-ConvergeElo`, then runs a replay-data arm (`rank.exe extract` at 4k/8k games) and an epoch probe, and d6-confirms the best cell. Appends to `models/sweep/scaling.csv` and skips cells already recorded there, so an interrupted run resumes. First run's result: replay training beat single-teacher self-play by ~250 Elo (best model d6 Elo 920, promoted to `models/pst_value.txt`). **[SELF-PLAY CONVERGENCE UNSUPPORTED - see `Docs/corrections.md`]** Its `-ConvergeElo` stop is NOT a convergence detector and its self-play arm never converged. The arm is 4 rows total (250 and 500 games, 2 seeds each); it stopped at 500 on a +16 mean gain under a 20-Elo threshold, while the seed spread WITHIN a size was 94 and 72 Elo, and no size above 500 was ever tested. The stop threshold is smaller than the noise it is thresholding, so it fires at the first rung by construction (theory 45). `plans/training-sweep-results-1-luminous-snail.md` item 3 already said this and warned "do not trust 'self-play plateaus at 500'"; the caveat is repeated here because this row is where the result gets quoted from. Before reusing this script, either raise the seed count until the seed band is below `-ConvergeElo`, or replace the stop rule with a fixed ladder rated end to end (what `tdleaf_study.ps1` does via `--ckpt-at`). |
| `train_vs_champion.ps1` | Vs-champion training-data study: generates `rank.exe pairgen` datasets from games involving the reigning champion (learner-vs-champ, diluted-champ-vs-champ, d8 oracle-vs-champ, champion-loss cherry-picks, branch-mined winning lines), trains linear v2 PST cells per dataset (seed replicas), gauntlet-screens at the d4 wrapper into `models/sweep/vs_champ.csv` (resumable), gates a bootstrap arm, promotes each family's best to reserved slots 94..99, d6-confirms, appends the d6 IDs + the oracle to the roster, re-rates the pool, and prints the opponent-bucket residual analysis (champion / classic-like / diverse) that answers the two recorded theories. `-DryRun` for a tiny pipeline check, `-AnalysisOnly` to recompute the bucket tables later (the standing longitudinal theory re-check). First run's result (`plans/vs-champion-training-results-1-cozy-forest.md`): diluted-champion-vs-champion and oracle-vs-champion data beat the replay recipe, the best model ties the champion at d6 (1137 vs 1140 on the shared fit), one-sided cherry-picked datasets fail from degenerate labels. |
| `opener_bias_study.ps1` | Theory 6 test (`Docs/theories.md`), Layers 1+2: for each promoted challenger (champdil s96, oracle s98) vs the champion, plays the d6 head-to-head under three opener configs -- S (`--open-side both`, the symmetric baseline), C (`--open-side a`, challenger random / champion true policy), P (`--open-side b`, champion random) -- and reads the win tally from each pairgen `.meta.json`, then runs `rank.exe opener-bias` with a learned judge for the mechanism measure. Writes `data/opener_bias/` (gitignored) + `sensitivity_sweep.csv`. First run: champdil 65% (S) -> 40% (C), oracle 58.8% (S) -> 66.2% (C). Results: `plans/opener-bias-results-1-synchronous-stearns.md`. |
| `opener_bias_retrain.ps1` | Theory 6 test, Layer 3: regenerates the oracle training set with `--open-side a` (only the oracle plays the random opener; the champion plays its own opening) into `data/pg_oracle_champ_asym.jsonl`, retrains the 3-seed oracle cell (`--from-data`), gauntlet-screens at the d4 wrapper, d6-confirms the best, and compares to the symmetric baseline (screen mean 785 / d6 1137). Resumable via `models/sweep/opener_bias_retrain.csv`; archives models to `models/sweep/vsc_oracle-asym_<seed>.txt` (does NOT overwrite the symmetric `vsc_oracle-vs-champ_*.txt` or touch the roster). `-DryRun` for a tiny pipeline check. |
| `hill_climb.ps1` | Stochastic hill climber over the Advanced eval weight mix (13 weights: c,w,l,f,d,e,m,h,b,o,r,x,n), optimizing Elo at a fixed depth via `rank.exe gauntlet` as the fitness function. Turn pinned at `-Turn` (20), `-NoiseSeed`/`-RaceWin` pinned (1/1), absolute weight values renormalized to sum `-Sum`-`-Turn` (80) so the search varies the mix not the scale and candidates dedupe. Greedy-from-best with `{1,3,5}`-unit simplex steps + occasional drastic chip reset; `-AllowNegative` adds sign-flip mutations and signed resets; id-keyed cache. `-Roster` defaults to `ranking/climb_roster.txt` (see the artifact-directory entry below for the two properties that pool is built around, and re-check them before editing it); `-Promote` appends the top finds to `ranking/roster.txt` and runs a full refit. Logs every candidate to `ranking/climb_adv_<mode>_d<depth>_<stamp>.tsv` (gitignored). |
| (trainer flags, not a script) | **Wall-clock rungs, hard stop, and resume** are available on `train.exe tdleaf` and `train.exe gumbelzero` (`src/train_budget.h`). `--wall-ckpt-at "7200,14400,28800"` writes a checkpoint at each cumulative SECOND mark, named for the nominal mark (`<out>_t7200.txt`) so one filename finds the same rung across every seed and regime, with the true elapsed time in the file's own provenance. `--wall-stop <secs>` is the hard cumulative stop, defaulting to the last mark, and it makes `--games` optional: pass neither and wall clock alone governs the length. `--resume <checkpoint>` continues the SAME cumulative ladder, reading the prior spend out of the checkpoint's provenance and skipping the marks it already passed, so a 16h run is an 8h run plus a resumed 8h rather than a restart (plan Part 5 answer 4). It refuses a checkpoint carrying no spend stamp rather than silently continuing from zero and mislabelling the result. Every checkpoint's `teacher=` line carries `games=`/`secs=`/`nodes=` for what THAT rung actually cost, written after the training loop. Nodes are what make a spend claim portable off this machine. **Checkpoints written before 2026-09-01 carry a `games=` copied from the command line's `--games`, which for a laddered run is the LAST rung's count on every rung** (slot169's header says 4000 for a model trained on 1,500). Do not quote a pre-2026-09-01 `games=` as a training cost. |
| `tdleaf_study.ps1` | TD-Leaf(lambda) self-play study orchestrator (phases: train, roster, play, screen; resumable via `models/sweep/tdleaf_study.csv`). Trains 13 runs into a 38-agent cohort on slots 128..165, one axis at a time around a base config (`init=models/pst_value.txt`, lambda 0.7, lr 0.01, d6/nb200k, 4 random opener plies per side): **A** learning curve + seed band (4 seeds x rungs 100/250/500/1000/2000 = 20 agents), **B** lambda in {0, 1} vs the base's 0.7, **C** from-scratch init, **D** lr in {0.003, 0.03}, **E** a d4 generator-depth control. **The game count is not an input**: `--ckpt-at` writes a rung ladder per run and every rung is rated as its own agent, so the learning curve is an output. Then appends the cohort to `ranking/roster_tdleaf.txt`, writes the id list `ranking/cohort_tdleaf.txt`, plays with `--cohort` (so only pairs touching a cohort agent are scheduled), and screens with `rate --pin ranking/standings.tsv`. **Screening only** -- the pinned fit cannot dethrone anything; certification is a separate, deliberate unpinned refit once the agents to keep are chosen. Rungs of ONE run share a training trajectory and are NOT independent replicates; only distinct seeds are. |
| `replication_pass1_sanity.ps1` | Replication study Stage 1, Pass 1 (`plans/replication-study-plan-1-brass-lectern.md`). Trains every arm (B0, A1..A8) at the study head `ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@3` from scratch to a 10/20-game ladder, twice with the same seed and once at `--lr 0` for 3 games, then checks: each rung's provenance (`games=` per rung, `cpu=`/`nodes=` growing, the arm's switch token, `init:scratch`), byte-identical repeats apart from `secs=`/`cpu=`, identical play at lr 0, the logs' PV depth and TreeStrap coverage, and a `rank.exe pairgen` load test of each rung-20 checkpoint published to slots 1858..1875 (ledger `models/sweep/replication_stage1_pass1.csv`). Phases `train`, `check`, `play`, `all`, exit code 2 on any failure, summary in `models/sweep/rep1_p1/pass1_summary.txt`. **Pass `--games` with any `--ckpt-at` ladder below 500 games**: the trainer runs to `max(--games, top rung)` and `--games` defaults to 500. Results: `plans/replication-study-results-1-brass-lectern.md`. |
| `replication_lr_probe.ps1` | Replication study Stage 1, Pass 2's learning-rate probe (plan, "Hyperparameter fairness"). Every arm at every half-decade learning rate from 1e-8 to 1, one calibration seed (default 3001), 50 games at the study head, checkpoints every 10 games. Per arm it reports D (the lowest rate whose weights exceed max \|w\| 5 at any checkpoint), L (the lowest rate whose weights move a mean of 0.01 from the initialization) and the tuning range [D / 10^2.5, D / 10^0.5], the shared width placed at each arm's own D. Weight-based, rates nothing. Phases `train` (resumable, skips runs whose last rung exists), `analyze`, `all`. Rows in `models/sweep/replication_lr_probe.csv`, summary in `models/sweep/rep1_lrprobe/probe_summary.txt`. |
| `replication_pass2.ps1` | Replication study Stage 1, Pass 2's rated steps. `-Step curve` (every arm, 1 seed, game ladder `-CurveRungs`, at the middle of its locked learning-rate range: curve shape and CPU seconds per game), `-Step noise` (`-NoiseArms B0,<arm>`, `-NoiseSeeds` seeds at `-NoiseCpu` matched CPU seconds, converted to each arm's game count off its curve run: sigma_seed and sigma_meas), `-Step tune` (the same `-Draws` random draws for every arm over its locked range, twice as many joint draws for arms with an arm-specific hyperparameter: lambda for B0/A1, d_min in {1,2,4,8} for A3, epsilon in [0.01, 0.3] for A7, ordinal start e for A8, with A5..A8 at `-BaseLambda`, B0's tuned lambda). Every rated checkpoint is published to a slot (ledger `models/sweep/replication_stage1_pass2.csv`, from 1876), wears `-Opener` (default `.opener(rand,moves=8)@1`, so panel pairs are never deterministic) and plays ONLY the `-Panel` roster: one `rank.exe play --paired-openings --common-openings` process per agent, into its own part under `ranking/matches_rep1/`, listed in `ranking/matches_rep1.index.txt`. One `rate --pin <-PanelPin>` over the study store then rates everything (`ranking/standings_rep1_pinned.tsv`), and the report prints levels tables, exports `analysis/replication_stage1.py` runs and runs `verify-store`. Phases `train`, `publish`, `play`, `rate`, `report`, `all`, all resumable. Every panel id must be in `-PanelPin`. |
| `gumbelzero_sample_pass2.ps1` | Gumbel-Zero Pass-2 draw sampler: `Get-GumbelZeroPass2Draws([N],[SweepSeed])` is the single source of truth for the random-search grid (random search over the joint space, not one-axis-at-a-time, per Bergstra & Bengio 2012), dot-sourced by `gumbelzero_study.ps1` so the study that trains/rates can never drift from what got reviewed. Axes: sims {25,50,100,200}, lr {0.003,0.01,0.03}, l2 {0.0,0.0003,0.001}, (replay-capacity,replay-warmup) paired presets {(500,16),(2000,32),(8000,128)}, batch {8,32,64}, open-plies {0,4,8} (grounded on `ranking/CHAMPION.md`'s openless/4-random/8-random categories, not arbitrary -- sims is sampled once per draw and used for BOTH the self-play generator and the certification head, per `Docs/model-training-playbook.md`'s generator/search-depth-matching guidance). REF row = the Pass-1 sanity recipe, always included as a control. Run directly (not dot-sourced) it only samples and exports `models/sweep/gumbelzero_pass2_draws.csv`, never trains/plays/rates. |
| `gumbelzero_study.ps1` | Gumbel-Zero Pass-2 Round A driver (phases: train, roster, play, screen, all; resumable via `models/sweep/gumbelzero_pass2_study.csv`). Trains the 21 sampled draws (slots 660..722, see `src/CLAUDE.md`'s slot ledger) to a shared rung ladder (100/400/1500), builds `ranking/roster_gumbelzero_pass2.txt`/`cohort_gumbelzero_pass2.txt`, plays the cohort into its own screening store (`ranking/matches_screen_gz.jsonl`, never the canonical ladder), and screens with `rate --pin ranking/standings.tsv`. First run's result: `plans/gumbel-mcts-results-3-amber-thicket.md` -- most draws (12/21) were still rising monotonically at rung 1500, motivating Round B. |
| `gumbelzero_study_roundb.ps1` | Gumbel-Zero Pass-2 Round B driver: seed-replicates (original seed + 2 offsets) and extends the rung ladder to 4000 for the top 8 Round-A draws by rung-1500 Elo (slots 723..882). Same phase structure as `gumbelzero_study.ps1`. **Known gotcha already hit and fixed in the script**: building the 3-seed list inline inside an `@(...)` array literal (`@($row.Seed, $row.Seed + 10000, $row.Seed + 20000)`) is a PowerShell parsing trap that silently expands to 5 elements, not 3 -- precompute the offsets into local variables first. First run's result: `plans/gumbel-mcts-results-3-amber-thicket.md` -- R17 (sims=200,lr=0.03,l2=0.0,replay=8000/128,batch=8,open=8) leads at every rung and is still rising in 2 of 3 seeds even at rung 4000. |
| `gumbelzero_joint_sample.ps1` | Gumbel-Zero joint sweep's draw sampler: `Get-GumbelZeroJointDraws([N],[SweepSeed])` extends `gumbelzero_sample_pass2.ps1`'s pattern with 3 more axes -- cvisit {400,600,800,1000}, cscale {40,70,100,130} (TENTHS), m {8,12,16,20} -- that apply ONLY at the certification head `gaz(sims=N,cvisit=C,cscale=S,m=M)@1` built by `gumbelzero_joint_study.ps1`'s roster phase, never passed to `train.exe` (Gumbel-Zero's self-play generator does not read them). One joint draw therefore fixes both how a checkpoint trains (6 training axes, eligible sets widened from Pass 2: sims {300,400,500}, batch {8,32,128}) and how it is later screened. REF row snaps the Pass-1 sanity recipe and the Slice-2 search-knob study's practical recommendation (`plans/gumbel-mcts-results-4-copper-lantern.md`) into this study's eligible sets. Run directly, only samples/exports `models/sweep/gumbelzero_joint_draws.csv`. |
| `gumbelzero_joint_study.ps1` | Gumbel-Zero joint sweep driver (phases: train, roster, play, screen, export, all; resumable via `models/sweep/gumbelzero_joint_study.csv`; `-OnlyDraws K` limits to the first K draws for a 1-arm timing check). Trains 101 draws (slots 998..1401, `src/CLAUDE.md`'s slot ledger) to a shared 100/400/1500/4000 rung ladder. Linear architecture, from-scratch init only -- `train.exe gumbelzero` had no `--model-type`/`--mlp-hidden`/`--init` flag when this study launched (confirmed 2026-08-18); a larger 2-init x 2-architecture design was planned first (hence `ML_SLOTS` jumping straight to 4096) and dropped back to this scope before launch, MLP/init deferred to a follow-up round. `--model-type`/`--mlp-hidden` were added to the trainer later that same day, and `--model-type conv`/`--conv-channels` (value head only, `ConvModel`) followed shortly after (see `src/CLAUDE.md`'s `ml_gumbelzero.cpp` entry), so an MLP or conv follow-up round no longer needs trainer work first; a second `--init` flag is still unimplemented. Screening is **pool-only gauntlet games** (`rank.exe gauntlet --roster ranking/roster_screening_pool.txt --keep`, launched in parallel per-checkpoint via per-worker batch files merged after, NOT `play --cohort`): a pinned-cohort play (this project's usual Workflow A pattern) also schedules cohort-vs-cohort games, which is quadratic in cohort size and measured at ~48hr for this cohort (404 checkpoints, 1.3M internal games) vs ~5.6hr pool-only (175K games) -- the "pinning beats N gauntlets" guidance in `Docs/ranking-workflow.md` was written for cohorts of ~20-50 (TD-Leaf scale) and inverts at this size. **`Start-Process -PassThru`'s `.ExitCode` gotcha**: reads back empty/null after `WaitForExit()` unless `.Handle` is touched immediately after the process starts (hit 2026-08-18: all 4 timing-check shards printed correct results but were flagged "failed" from a null exit code) -- the script now does `[void]$p.Handle` right after `Start-Process` and has each worker batch file `exit /b` its real errorlevel explicitly. `rank.exe canon --roster <file>` (not documented elsewhere in this table) rewrites every id in a roster/id-list file to today's canonical spelling via `rankUpgradeId`, used by the roster phase to turn its short-form `learned(s<slot>,<hash>)` ids into the full arch-descriptor form `rank.exe check` requires. `export` calls `export_cohort_results.ps1` (below) to write `plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv`, the per-checkpoint training-config x results table. Rating output: `ranking/standings_screen_gzjoint_pinned.tsv` (screening pool frozen at `ranking/standings.tsv`, cohort agents connect to it only through pool games, never each other). Results: `plans/gumbel-mcts-results-5-violet-harbor.md`. |
| `gumbelzero_arch_sample.ps1` | Gumbel-Zero mlp/conv architecture sweep's draw sampler: `Get-GumbelZeroArchDraws([N],[SweepSeed])` reuses `gumbelzero_joint_sample.ps1`'s 9 shared training/search-shape eligible sets unchanged, adding 2 architecture axes -- mlp hidden layers {16, 32, 64, "64,32"} and conv channels {"8,8", "16,16", "16,16,16", "32,32"}. Stratified sampling: draws two separate blocks (`M1..MN`, `C1..CN`) from one continuing RNG stream rather than a per-draw coin-flip, so the mlp/conv split is exact, not merely expected. Conv's own FC head (`--mlp-hidden`, reused by the trainer for conv's post-flatten layer) is fixed empty, not swept. Two REF rows (one per architecture) at that architecture's own built-in trainer default. Run directly, only samples/exports `models/sweep/gumbelzero_arch_draws.csv`. |
| `gumbelzero_arch_study.ps1` | Gumbel-Zero mlp/conv architecture sweep driver (phases: train, roster, play, screen, export, all; resumable via `models/sweep/gumbelzero_arch_study.csv`; `-OnlyDraws K` limits to the first K draws for a timing check). Trains 92 draws (2 REF + 90 random, 45 mlp + 45 conv; slots 1402..1769, `src/CLAUDE.md`'s slot ledger) to the same 100/400/1500/4000 rung ladder as the joint sweep. Same pool-only-gauntlet screening mechanism as `gumbelzero_joint_study.ps1`, reused unchanged. **Ledger CSV comma gotcha**: `--mlp-hidden`/`--conv-channels` values like `"16,16"` contain a literal comma, so the ledger row writer CSV-quotes that field (`` `"$($c.Arch)`" ``) -- an earlier unquoted version silently shifted every later column (rung/slot/model) on `Import-Csv` re-read, breaking both `$done` resumability and `export_cohort_results.ps1`'s join, caught by a 2-draw Pass-1 timing check before the full run. **Play-phase non-idempotency**: re-running the `play` phase replays a checkpoint's full gauntlet regardless of whether it was already screened (no per-checkpoint games-already-played check), so a checkpoint screened in more than one invocation (e.g. a timing check followed by the full run) accumulates extra games in the screening store -- not a correctness bug, but its `games`/`elo_pm` won't match the rest of the cohort's; flagged as future work in the results doc rather than fixed this round. Results: `plans/gumbel-mcts-arch-results-6-silver-thistle.md`. |
| `gumbelzero_arch_seedcheck.ps1` | Seed-replication follow-up to the mlp/conv architecture sweep: re-trains the top 5 mlp blocks' exact recipes (read from `plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.tsv`, never hand-transcribed) at 2 additional seeds each (+10000/+20000 offsets, matching Round B's convention), same 4-rung ladder, slots 1770..1809. Same phase structure (train, roster, play, screen, export) as the other `gumbelzero_*_study.ps1` drivers. Finding: the single best mlp draw's headline Elo (M34, 1023) does not hold up under replication (3-seed mean 971.7), regression to the mean since each block's original seed was picked FOR being the best-of-45 draw. The architecture-level mlp-vs-conv gap survives. Folded into `plans/gumbel-mcts-arch-results-6-silver-thistle.md`'s "Seed-replication follow-up" section. |
| `gumbelzero_arch_opener_check.ps1` | Opener-division follow-up to the mlp/conv architecture sweep: wraps all 90 rung=4000 checkpoints (45 mlp + 45 conv, no retraining) with one opener per `ranking/CHAMPION.md`'s 4 non-openless categories (book=15/16, rand moves=4/8) and screens all 360 variants the same pinned way, to check whether the openless top-4 (M34/M14/M10/M31) also lead the book/random divisions. `export_cohort_results.ps1` is NOT reused here (its join is by model SLOT alone, and 4 opener variants sharing one base slot would collapse to one) -- a custom `ExportResults` joins by the full canonical id via a side table (`models/sweep/gza_opener_rows.csv`) instead. Result: same top-4 lead every division, mlp still beats conv in all 4. Folded into `plans/gumbel-mcts-arch-results-6-silver-thistle.md`'s "Opener-division follow-up" section, full data in `plans/gumbel-mcts-arch-opener-agents-6-silver-thistle.tsv`. |
| `gumbelzero_conv_capacity_check.ps1` | Conv-capacity follow-up to the mlp/conv architecture sweep, investigating WHY conv underperformed: round 6 gave every mlp draw 2 nonlinear hidden layers on both heads but every conv draw 0 hidden layers on the value readout (`--mlp-hidden` never passed to conv) and an unconditionally-linear policy head (no flag could give a conv run's policy head MLP capacity). Crosses C15's exact recipe (round 6's best conv checkpoint, 806 Elo) with FC head {none,32} x policy head {linear, new `--policy-mlp` flag} x 3 seeds (8975/+10000/+20000), same 4-rung ladder, slots 1810..1857. Same phase structure. The `--policy-mlp` flag itself (`src/ml_gumbelzero.h`/`.cpp`, `tools/train_main.cpp`) decouples policy-head architecture from value-head architecture, purely additive (default off, preserves prior behavior byte-for-byte, verified against the full test suite, 3703 assertions, and a live gauntlet smoke test through the real search path). |
| `export_cohort_results.ps1` | **Standard last step of any cohort study**, generic across all of them: joins a study's per-checkpoint ledger CSV (any `models/sweep/*_study.csv`, identified only by needing a `slot` column -- the schema of every other column is passed through untouched, so it never goes stale as new studies sweep new axes) against a pinned-fit rating output (`ranking/standings_screen_<name>_pinned.tsv`, joined on the model slot number embedded in each agent's `learned(model=<slot>,...)` id) plus that fit's `report_screen_<name>_pinned.md` (for the avg-plies / end-piece-margin / W-L-by-color / cpu-per-move / nodes-per-move columns rank.exe already aggregates per agent -- no new measurement, just surfaced). Writes one flat tab-separated file: training config columns (verbatim from the ledger) followed by a fixed set of result columns, so a study's full "what was trained, how it was rated, what happened" always lives in one file instead of split across the ledger/standings/report trio. A ledger row with no matching standings row (training outran rating) is still written with blank result columns rather than dropped. `-PinnedReport` defaults to `-PinnedStandings` with `standings`->`report`/`.tsv`->`.md` (what every study's screen phase actually writes); pass `-PinnedReport ""` to skip playstyle columns explicitly. `-HeaderComment` lets a calling study stamp study-specific caveats as `#` lines at the top of the output (`gumbelzero_joint_study.ps1`'s `export` phase uses this to flag that its cvisit/cscale/m columns are rating-time only, since Gumbel-Zero self-play never reads them -- see that row above). `-GroupBy <cols> -WideBy <col> -WideOut <path>` additionally writes a wide pivot: one row per `-GroupBy` key (e.g. `block,seed`, a training run) instead of one row per checkpoint, with `-WideBy`'s distinct values (e.g. `rung`) spread into suffixed columns (`elo_rung100`, `elo_rung1500`, ...) for every column that is NOT constant within a group -- constant columns (training config, anything that doesn't vary across a run's checkpoints) collapse to a single column automatically, no need to list them. First used for the joint sweep: `plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv` (404 rows, one per checkpoint) + `.wide.tsv` (101 rows, one per draw). Reused unchanged for the architecture sweep: `plans/gumbel-mcts-arch-sweep-agents-6-silver-thistle.tsv` (368 rows) + `.wide.tsv` (92 rows). |
| `label_study.ps1` | Position-oracle labeling campaign orchestrator (phases: prep, posgen, label-train, label-eval, fit, train, eval, rate), resumable via a CSV ledger of done cells (`data/labels/study.csv`). prep appends + rates the two depth-diluted d8 ladder rungs and freezes `data/labels/ratings_snapshot.tsv` (the study's fixed Elo basis, never overwritten). The label phases chunk positions across `-Workers` rank.exe shards (`--resume --done` exact top-up, shard merge, meta carry) and detect completion from the shard metas' positions_touched; leftover shard files from an interrupted chunk are salvaged into the master (exact-line dedup, sound because deterministic seeds make replayed rows byte-identical) before the chunk relaunches, so a kill at any moment loses nothing already played. train launches the dist configs in parallel (lin + two mlp seeds + a wide mlp); eval runs `train.exe dist-eval` per model; rate wires slots 76..79 into the roster at both standard heads (d4 + d6/nb200k) and refits. (The dist MLP mu heads are incrementally scored since the NNUE-style accumulator + sparse leaf-tail forward shipped, so a d6/nb200k MLP leaf is affordable; the earlier d4-only-for-MLP restriction is gone.) `-DryRun` runs everything on a tiny pool with a d2 ladder in minutes under `data/labels/dry/`. Ladder spec files (`ladder_train.txt`, `ladder_eval.txt`) are written once with the default design and are hand-editable. |
| `make_time_ladder_roster.py` | Builds `ranking/q7/roster_timeladder.txt` + `cohort_timeladder.txt` for the wall-clock ladder study: 4 cores x `RUNGS` on the head `ab(deep=12,tt,ord,retain,time=Xms)@<abver>`. **RETAIN ONLY** since 2026-09-06 (developer instruction, theory 70): a plain `time=` head realizes 0.432 of its flag (sd 0.053) against `retain`'s 0.854 (sd 0.028), so it is not a wall-clock instrument and is no longer carried. The 20 plain cells of the first pass are emitted as `off` bench lines rather than dropped, so their games stay on record without entering a fit beside the `retain` cells. `PLAYED_PLAIN` lists which rungs get a bench line, so a rung plain never ran is not implied to have games. Pass `--abver` to match the live `ab` explorer version or `rank.exe check` rejects every id. |
| `migrate_ab_v3.py` | One-shot store migration for the `ab` explorer's `@2 -> @3` bump (2026-09-06). The bump exists because `time=` was not enforced before it, so a `time=` game recorded under `@2` is not a game the `@3` binary would play. Match rows are DROPPED when either side's id contains `time=` and renumbered otherwise (kept 685,210, dropped 324,570, 3 torn lines swept). Rosters renumber and keep their `time=` lines, since those agents still exist and simply need replaying. Pin files renumber and drop `time=` rows, since a pinned Elo from unenforced games would freeze a wrong number into a fit. Derived fit outputs are deleted rather than migrated, because `rank.exe rate` regenerates them in about 20 seconds. Retained here as the reference pattern for the next codec bump, not as something to re-run. |
| `smoke_test_gui.ps1` | Standard GUI smoke test: build, run the GUI hidden (`--capture`), save `build\gui_smoke.png`, exit non-zero on a crash or hang. `-Visible` opens a real window and refuses while a Steam or Epic game runs. See `gui/CLAUDE.md`. |
| `gui_shot.ps1` | Hidden-window GUI screenshots by scenario (`-Scenario library,red -Moves c1c,f6f`, or `-All`) into `build\gui_shots\`. Never shows a window. |
| `web_preloads.ps1` | Prints the `--preload-file` arguments `build_web.bat` and `build_web.sh` bundle: the default model slots plus every model file `gui/presets.txt` names, taken from the byte-exact copies in `gui/web_models/` and mounted at their `rankSlotFile` paths. Checks each copy's hash against the preset id that names it and fails on a mismatch. `-Sync` first copies the files from `models/` (run it after changing a preset). |
| `web_shot.ps1` | Real-time screenshots of the built web page: serves `build\web`, opens each page (`-Pages "name=query"`) in its own hidden headless Chrome, waits `-Seconds`, and saves `build\web_shots\<name>.png` through the DevTools protocol. Use it instead of `--virtual-time-budget`, under which the page's clock-paced moves stall. `-Device WxH` emulates a phone with touch, and `-Steps` runs touch taps, drags, waits, and extra shots on every page (`TESTING.md`, "Web build check"). |
| `gui_capture.ps1` | Targeted screenshot helper: finds the `GLFW30` window by process id and crops its client area for inspecting individual widgets (complements `smoke_test_gui.ps1`). |
| `train_main.cpp` | `train.exe` CLI: subcommands `selfplay-supervised`, `ensemble`, `imitate`, `dist-value`, `score`, `dist-eval`, `tournament`, `tournament-play`, `tournament-rate`, `turn-swing`, `speed`, `run-config`, `run-note`, `docs`, all `--key value` (incl. `--only`, `--run`, `--note`, `--node-budget`, `--time-budget-ms`, `--budgets`, `--ablate`, `--forward-study`, `--gen-eval`/`--gen-params`, `--teacher-eval`/`--teacher-params`, `--feature-version`, selfplay-supervised's `--model-type linear|mlp` + `--mlp-hidden "32"|"32,16"` + `--residual-skip <f>` (0 off / >0 fixed / <0 auto-calibrate the frozen chip skip), ensemble's `--models <comma-list>` + `--mirror 0|1` + `--out`, and `turn-swing`'s `--chip/--wall/--col/--forward`). |
| `rank_main.cpp` | `rank.exe` CLI: subcommands `check`, `play`, `rate`, `run`, `seal`, `split`, `history`, `gauntlet`, `determinism`, `refute`, `extract`, `bookgen`, `cbookdump`, `cbookfit`, `pairgen`, `opener-bias`, `opener-swap`, `agree`, `nodeprofile`, `posgen`, `label`, `labelfit`, all `--key value` (`--roster`, `--in`, `--out`, `--board`, `--games`, `--seed`, `--shard`/`--of`, `--agent`, `--last`, `--id`, `--keep`, seal's `--max-mb`, split's `--group`/`--max-mb`/`--apply` (**dry run unless `--apply`**), extract's `--feature-version`/`--sample`, bookgen's `--a` (line owner) `--b` (target) `--plies` `--out`, cbookdump's `--a`/`--core`/`--regime` (at most one, none = universal) `--min-elo`/`--ratings` `--max-plies`/`--sample`/`--out`, cbookfit's `--in`/`--clusters`/`--keep`/`--mirror`/`--min-per-cluster`/`--out-slot` (`--clusters`/`--keep` take comma-separated lists, one book file per pair from a single read), pairgen's `--a`/`--b`/`--dil-apply`/`--dil-start`/`--dil-floor`/`--dil-decay-plies`/`--open-plies`/`--open-side`/`--filter`/`--branch-tries`, opener-bias's `--a`/`--b`/`--judge`/`--open-plies`/`--games`, opener-swap's `--a`/`--b`/`--open-plies`/`--games`, agree's `--a`/`--b`/`--open-plies`/`--games`, nodeprofile's `--id`/`--games`/`--open-plies`/`--out` (one agent self-plays; every non-forced ply emits its whole depth ladder plus the BudgetKind that ended the search), posgen's `--out-train`/`--out-eval`/`--train`/`--eval`/`--per-game`/`--min-ply`/`--max-ply`, label's `--pool`/`--ladder`/`--out`/`--resume`/`--done`/`--max-positions`, labelfit's `--in`/`--pool`/`--ratings`/`--out`/`--min-rows`/`--rating-se`, determinism's `--probe`/`--replicas`/`--only`/`--include-stochastic`/`--out`, refute's `--slot`/`--oracle`/`--wearer`/`--tries`/`--colours`/`--only`/`--max-games`/`--skip-timed`/`--force`/`--out` (its TSV also carries off_ply/off_by, naming whose move left the mined path)). |
| bookgen (subcommand) | Mine an opening/refutation book from stored games between two agents. Replays every stored `--a` vs `--b` game, keeps positions + the move `--a` played (first `--plies` half-moves) from A's WINS only, writes `models/book<N>.txt` (a `#` provenance header + `<positionKey hex16> <sx> <sy> <dx>` lines). The `book` opener (`src/ai_random.cpp` `g_openers[]`) plays those replies via `.opener(book,<N>)@1`. First use: the s98 refutation book (dethrone plan phase 2, `plans/dethrone-champion-results-3-wiggly-mitten.md`). The book file is NOT hashed into the agent ID (unlike `learned()` models), so treat a book slot as immutable and give a regenerated book a new slot number. **Read `plans/book-opener-audit-results-1-vivid-lantern.md` (theory 38) before quoting any book Elo:** a book is a memorized line keyed by position hash, so its measured lift only holds while the opponent reproduces its previous replies, and it collapses under `pairgen --open-plies`. |
| cbookdump + cbookfit (subcommands) | Mine a fuzzy, nearest-cluster-matched opening book -- SMARTSTART (Steinmetz & Gini, IJCAI 2015) applied to Breakthrough, generalizing `bookgen` past its theory-38 collapse. Two steps because replay is the only expensive part and clustering depends only on what got recorded: `cbookdump` replays winning games and writes `data/cbook_<scope>.jsonl`, one `(half-move, difference-from-start vector, move)` triple per winner ply, deduplicated to distinct games first and dropping replays that drift from the stored result. Scope is at most one of three, none meaning universal (every winner in the store): `--a <id>` (that exact agent id's own wins only), `--core <id>` (that id's wins under ANY opener or none -- the winner's id with its trailing `.opener(...)@N` segment stripped must equal `<id>`, so this pools a core's bare/`book`/`rand`/`cbook`-wearing wins together), or `--regime <tag>` (every winner whose `rankAgentRegime` matches, the broadest of the three: e.g. `classic` matches any classic-evaluator agent regardless of weights, dilution, or opener). `--min-elo`/`--ratings` gates additionally on the winner's rating. `cbookfit` reads a dump (no replay) and writes one `models/cbook<N>.txt` per `(--clusters, --keep)` combination via spherical k-means (`src/ml_cluster.h`). The `cbook` opener (`src/ai_random.cpp`) matches the live position to its nearest mined cluster per half-move and narrows the search's ROOT move list to that cluster's moves via `.opener(cbook,<N>[,ply=M])@1`, rather than playing a move outright the way `book` does -- the brain still searches every surviving candidate, so this is a compute-for-depth trade, not a memorized line. Design rationale, the raw-vector clustering failure mode, and the measured grounding for its mining parameters: `plans/cluster-book-plan-1-noble-swimming-scott.md`, results (Pass 1a): `plans/cluster-book-results-1-noble-swimming-scott.md`, results (scope comparison, Pass 1a-extended): `plans/cluster-book-results-2-noble-swimming-scott.md`. |

**Mined books.** Slot numbers are immutable, a regenerated book gets a new slot.
`--plies` is the book DEPTH (how many of the line owner's half-moves are stored).

| Slot | Line owner (`--a`) | Target (`--b`) | Depth | Entries | Kept replays |
|---|---|---|---|---|---|
| `book1` | `ab(deep=8,tt,ord,nodes=2m)@1.classic(chip=100)@2` (oracle) | `learned(model=98,5801570e)` | 60 | 553 | 29 of 32 |
| `book2` | `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2` | `learned(model=98,5801570e)` | 60 | 134 | 7 of 32 |
| `book3` | `ab(deep=6,ord,nodes=200k)@1.adv(chip=77,...)@1` | champion | 60 | 24 | 16 of 32 |
| `book4` | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=98,5801570e,pool_games,lin,shape=129-1)@1` | `classic@2` | 60 | 519 | 25 of 32 |
| `book5` | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=111,78ef6974)@1` (dist) | champion | 60 | 145 | 6 of 8 |
| `book6` | `ab(deep=6,tt,ord,nodes=200k)@1.learned(model=3,68364898)@1` | champion | 60 | 162 | 7 of 8 |
| `book7` | `classic@2` (same pair as book2) | `learned(model=98,...)` | 6 | 13 | 7 of 32 |
| `book8` | same | same | 16 | 38 | 7 of 32 |
| `book9` | same | same | 30 | 73 | 7 of 32 |
| `book10` | `learned(model=98,...)` (same pair as book4) | `classic@2` | 6 | 41 | 25 of 32 |
| `book11` | same | same | 16 | 134 | 25 of 32 |
| `book12` | same | same | 30 | 279 | 25 of 32 |
| `book13` | `classic@2` (same pair as book2/7-9) | `learned(model=98,...)` | 4 | 8 | 7 of 32 |
| `book14` | same | same | 8 | 18 | 7 of 32 |
| `book15` | `learned(model=98,...)` (same pair as book4/10-12) | `classic@2` | 4 | 22 | 25 of 32 |
| `book16` | same | same | 8 | 60 | 25 of 32 |
| `book17` | `learned(model=3,68364898)` (same pair as book6) | `classic@2.opener(book,book=2)@1` | 4 | 10 | 23 of 32 |
| `book18` | same | same | 8 | 42 | 23 of 32 |
| `book19` | `ab(deep=6,ord,nodes=200k)@1.adv(chip=77,...)@1` (same pair as book3) | `classic@2.opener(book,book=2)@1` | 4 | 2 | 16 of 32 |
| `book20` | same | same | 8 | 4 | 16 of 32 |

`book13`-`book16` (2026-07-28) and `book17`-`book20` (2026-07-29) fill the 4-ply
and 8-ply rungs of the four existing own-book ladders, for the category-champion
split (`ranking/CHAMPION.md`, "4-book" and "8-book" categories). `book17`/`book18`
(s3-own) and `book19`/`book20` (adv-own) are mined against the fixed pre-split
single champion identity (`classic@2.opener(book,book=2)@1`), the same target book3/
book6 used, so mining needed no new games. `adv`'s books live at a DIFFERENT
search head (`ab(deep=6,ord,nodes=200k)@1`, no `tt`) so they add Elo diversity to the
roster but are never category-eligible for the 4-book/8-book titles (one-head
rule). `book11` was the single-champion era's reigning book until the 2026-07-28
split; under the new taxonomy (exactly 4-ply and 8-ply) it isn't a member of
either book category and holds no title, but stays rostered as depth-ladder
data. Books 1-4, 6, 7-20 are rostered in `ranking/roster.txt`; `book5`
is not, because its `learned(model=111,...)` dist core costs 370 ms/move and would add
hours of roster play for one row.

Two things measured on the full-roster refit that are easy to get wrong
(`plans/book-opener-audit-results-1-vivid-lantern.md`, theory 38):

- **Book size does not predict book strength.** The 553-entry oracle book is the
  worst loadout on the `classic` core (-12 Elo), the 24-entry `adv` self-book is the
  worst anywhere (-107), and the best is 134 entries.
- **A self-mined book is not reliably better than a borrowed one.** Own beats
  borrowed for `classic`, ties for `s98`, and LOSES for `learned(model=3,...)` (-33 vs +1)
  and `adv(chip=77,...)` (-107 vs +15). Do not assume core-specificity.

Book size also tracks the mining pair's diversity rather than its skill: `book3` is
tiny (24 entries, 0 replay drift) because its no-TT head is fully deterministic and
its 16 wins collapse to about 7 distinct games, while `book4` is large (519 entries,
12 drifted) because the `tt` head's cross-game state made its 25 wins varied.

**Mined cluster books.** Slot numbers are immutable, a regenerated fit gets a new
slot. Distinct from `book<N>.txt` (a position-hash dictionary): `cbook<N>.txt` is a
per-half-move-bucket set of clusters, each an XOR-difference-from-start centroid
plus a count-ranked move list, matched by nearest cosine similarity rather than
exact hash. `models/cbook1.txt` is this feature's Pass-1a plumbing artifact:

| Slot | Scope | Dump | Clusters | Keep | Mirror | Kept replays | Positions |
|---|---|---|---|---|---|---|---|
| `cbook1` | `--a` `ab(deep=6,tt,ord,nodes=200k)@1.classic(chip=100)@2` | `data/cbook_classic.jsonl` (max-plies 32) | 4 | 6 | canon | 1520 of 2173 (380 drifted, 273 unparseable/stale ids) | 22408 |
| `cbook2` | `--a` `...classic(chip=100)@2` (re-mined) | `data/cbook_dump_classic_exact.jsonl` | 16 | 6 | canon | 1541 of 2173 (359 drifted, 273 unparseable/stale ids) | 22622 |
| `cbook3` | `--core` `...classic(chip=100)@2` (any opener) | `data/cbook_dump_classic_core.jsonl` | 16 | 6 | canon | 14435 of 17906 (2440 drifted, 1031 unparseable/stale ids) | 214819 |
| `cbook4` | `--regime classic` | `data/cbook_dump_classic_regime.jsonl` | 16 | 6 | canon | 52432 of 78360 (9381 drifted, 16547 unparseable/stale ids) | 764840 |
| `cbook5` | `--a` `...learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | `data/cbook_dump_linear_exact.jsonl` | 16 | 6 | canon | 1380 of 1592 (146 drifted, 66 unparseable/stale ids) | 21765 |
| `cbook6` | `--core` `...learned(model=169,...)@1` (any opener) | same dump as `cbook5` | 16 | 6 | canon | identical to `cbook5` | identical to `cbook5` |
| `cbook7` | `--regime tdleaf_self` | `data/cbook_dump_linear_regime.jsonl` | 16 | 6 | canon | 13577 of 17849 (3647 drifted, 625 unparseable/stale ids) | 210216 |

`cbook1` (2026-08-26) was the Pass-1a sanity vehicle: one core, one scope, the
smallest candidate cluster count (the plan's `--a`-mode density is two orders of
magnitude thinner than the universal/per-regime pools Pass 2 targets, so
`clusters=4` states the intent honestly rather than requesting a count the
per-cluster floor would immediately clamp down anyway). Ply-0's bucket collapses to
one real cluster (every game's ply-0 difference from the start is the zero vector,
so the other 3 requested clusters end up empty by construction, see
`src/ml_cluster.h`'s zero-vector note); later buckets show genuine multi-cluster
structure (ply 8: 4 clusters, sizes 146-216, mean intra-cluster cosine 0.62-0.70,
visibly different move distributions per cluster). Full mining rationale, the
measured grounding behind every parameter choice, and the Pass-1a verification
results (including the fuzzy-match-fires-where-exact-wouldn't hit-rate curve):
`plans/cluster-book-plan-1-noble-swimming-scott.md` /
`plans/cluster-book-results-1-noble-swimming-scott.md`.

`cbook2`-`cbook7` (2026-08-27) are a scope comparison: two cores (the top-Elo bare
`classic` chip counter and the top-Elo bare linear `tdleaf_self` model on
`ab(deep=6,tt,ord,nodes=200k)@1`) each mined under 3 scopes (`--a` exact id,
`--core` that id under any opener, `--regime`), all fit with the same
`--clusters 16 --keep 6 --mirror canon --seed 1` so scope is the only varying
input. `cbook6` is byte-identical to `cbook5` because `learned(model=169,...)@1`
has no opener-wearing wins in the store, so its `--core` and `--a` scopes select
the same game set. Screened via `rate --pin ranking/standings.tsv` against
`ranking/roster_screening_pool.txt` at `ply=16`, 16 games/pair: **every cbook
variant of both cores rated BELOW its bare baseline** (classic: 923 bare vs
899/915/895 for exact/core/regime; linear-169: 1031 bare vs 964 for exact/core).
Full numbers and caveats: `plans/cluster-book-results-2-noble-swimming-scott.md`.

## Artifact directories (repo root)

| Dir | Purpose |
|---|---|
| `ranking/` | The persistent Elo-ranking state: `roster.txt` (hand-edited `anchor|on|off <id>` lines, incl. a dense diluted-d6 ladder), `CHAMPION.md` (the reigning-champion declaration, single source of truth + certification methodology), `roster_top.txt` (the reusable top-resolution boost roster: contenders played to >= 32 games/pair before any top-of-table claim), `roster_screening_pool.txt` (the default gauntlet opponent pool for rating a new/candidate agent during training or screening: 26 agents + `rand@1`, regime-diverse and Elo-spread across all 5 opener categories from 1000+ down to ~100, built 2026-08-17; see `Docs/ranking-workflow.md`'s "Default gauntlet pool"), `climb_roster.txt` (the hill climber's own small opponent pool, rebuilt 2026-09-01: 11 rungs, EVERY member drawing from `rand()` via dilution, the `rand` opener, or SmartRandom, spanning Elo 0 to about 1100 with no gap wider than ~200. Both properties are load-bearing. A deterministic member replays one game per colour however many are requested, which turns the fitness into a step function, and `gauntlet`'s 1-D MLE is unidentified when a candidate beats every opponent, so the ceiling rung has to sit above the strongest agent a climb can reach), and the **match store** (below). The rating outputs `ratings.tsv` (full historical fit, includes retired agents), `standings.tsv` (**read this one for current standings**: active agents only, grouped by search head), `games.tsv` and `report.md` are all **gitignored**: `rank.exe rate` rebuilds them from the store in about 20 seconds (measured 2026-08-01, 649,434 games) and the fit is deterministic, so a fresh clone runs a rate before reading standings. Shard temps `matches.jsonl.*`, `gauntlet.jsonl` scratch, and `climb_*.tsv` logs are gitignored too. |
| `ranking/` (store) | **The match store is a set of PARTS plus a live tail**, not one file. `matches.index.txt` (committed) lists the parts in load order, one filename per line, `#` comments allowed; a listed part whose file is absent is skipped rather than being an error. Writers always append to `matches.jsonl`, the tail, which is loaded last. Parts are grouped by WHO played the game (`rank.exe split`). **Loaded and committed:** `matches.roster.NNNN.jsonl` (games between rostered agents, 132,769 rows / 55 MB) and `matches.retired_other.NNNN.jsonl` (59,054 rows / 23 MB, tracked so re-rostering one of those agents needs no file transfer). **Not loaded, not committed:** `matches.retired_tdleaf_self.NNNN.jsonl` (457,611 rows / 193 MB), the TD-Leaf Pass-2 candidates that were screened and never promoted. Their index lines were removed on 2026-08-01 by developer decision: a permanent ladder should not be dominated by games against transient candidates, and 60% of some rostered agents' games were against that cohort. **Those files were DELETED 2026-08-02** to reclaim the space. They were never committed and never pushed, so no copy survives: that population is unrecoverable except by replaying the Pass-2 campaign. Re-adding their index lines would name files that no longer exist, which the loader skips silently. **This was a methodology change that moved the ranking**, so it required re-certifying `ranking/CHAMPION.md`; do not re-add the lines without re-certifying in the same session. **A retired agent's games are not dead weight:** Bradley-Terry fits every rating jointly, so a game against a retired agent is evidence about the ROSTERED agent that played it. Dropping this part moved 153 of 170 rostered agents (Spearman rho 0.9526, mean error bar 7.2 -> 10.2 Elo). Screening cohorts now play into their own store instead (see below), so this cannot recur. `rank.exe seal` separately rolls an oversized tail into `matches.NNNN.jsonl` shards (and appends them to the index when one exists), and every writer of an indexed store runs it automatically. Both cap every part with `--max-mb` so no single file can outgrow what a host accepts (GitHub rejects blobs over 100 MB). |
| `ranking/` (2nd pool) | **Diversified-opening pool**, a self-contained second instrument added 2026-07-26: `roster_open.txt` (14 agents, each wearing `.opener(rand,moves=4)@1`), `matches_open.jsonl` (its own store, never mixed with `matches.jsonl`), and generated `ratings_open.tsv` / `standings_open.tsv` / `games_open.tsv` / `report_open.md`. Rating outputs are named after the store, so `matches<X>.jsonl` writes `ratings<X>.tsv` and so on, and the default store keeps the historical unsuffixed names. |
| `runs/` | Per-run archive (one timestamped dir per tournament): `config.json` (exact config + pre-run note), `elo.tsv` (that run's ranked table), `notes.md` (pre-run + `run-note`-appended notes), `results.jsonl` (gitignored copy). `runs/index.jsonl` is the master log, one summary line per run. |
| `data/`, `models/`, `agents/` | ML outputs: append-only JSONL datastore, model checkpoints + `manifest.{json,md}` + `registries.json`, the Elo-rated `agents/library.txt` (full-roster snapshot), and the old tournament system's agent registry `agents/registry.jsonl` (union of every agent ever rated, with a `spec_hash`; frozen since 2026-06-28, its `registry.md` rollup deleted as stale -- superseded by `ranking/`). |
| `data/labels/` | Position-oracle campaign home: committed pools (`pool_train/eval.jsonl`), ladder specs, fitted labels (`labels_train/eval.jsonl`), raw-store `.meta.json` sidecars (the frozen rung-id mapping), `ratings_snapshot.tsv` (the study's fixed Elo basis), and `study.csv` (the resume ledger). The raw stores themselves (`raw_train/eval.jsonl`, ~hundreds of MB, the durable asset that re-labels under any future ratings fit) are gitignored -- back them up outside git. `dry/` and `logs/` are scratch. |
