# tools/ Reference

Build/run scripts, the trainer and ranker CLIs, study scripts, and the
artifact directories they write. Loaded when working on files in `tools/`.
The always-loaded overview and the most common command lines live in the root
`CLAUDE.md`. Engine/ML/ranking internals live in `src/CLAUDE.md`.

## Common commands

Copy-paste forms beyond the root's short list:

```powershell
.\tools\run_train.ps1 -Build selfplay-supervised --games 250 --epochs 6 --feature-version 2 --out models/pst_value
.\tools\run_train.ps1 tournament --games 10                # single-process, default depth ladder
.\tools\run_train.ps1 docs                                 # regenerate ML.md AUTODOC region + registries
.\rank.exe history --agent "ab(d4"                         # per-opponent record for one agent
.\rank.exe run --games 8                                   # serial play (live progress) then rate
.\rank.exe pairgen --a "<challenger>" --b "<champion>" --games 80 --open-plies 6 --open-side a --out data/pg.jsonl   # asymmetric opener: only agent a plays random
.\rank.exe opener-bias --a "<champion>" --b "<id>" --judge "<learned id>" --games 60   # how much the random opener degrades agent a's position
.\rank.exe opener-swap --a "<id>" --b "<champion>" --games 20 --open-plies 6   # same opener snapshot, colors swapped: position bias vs agent skill
.\tools\train_vs_champion.ps1               # 10-arm vs-champion training study (resumable; -AnalysisOnly reprints the bucket tables)
.\tools\opener_bias_study.ps1               # Theory 6: opener-inflation sensitivity sweep + mechanism measure (Layers 1+2)
.\tools\opener_bias_retrain.ps1             # Theory 6: retrain the oracle arm on asymmetric-opener data (Layer 3; -DryRun for a tiny check)
.\tools\hill_climb.ps1 -Iters 20 -Promote -PromoteTop 2    # climb, then promote winners to the roster
.\train.exe tdleaf --out models/sweep/tdl --init models/pst_value.txt --ckpt-at "100,250,500,1000,2000" --lambda 0.7 --lr 0.01 --seed 1001
.\tools\tdleaf_study.ps1 -Workers 12 -Phase all --GamesPerPair 8   # the full TD-Leaf cohort study
.\rank.exe play --roster ranking/roster_tdleaf.txt --cohort ranking/cohort_tdleaf.txt --games 32
.\rank.exe rate --roster ranking/roster_tdleaf.txt --pin ranking/standings.tsv   # screen on a FROZEN scale
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

**A pinned fit is screening, never certification.** The champions' ratings are
inputs to it, so it can never dethrone one. Certify by choosing which cohort
agents to keep, appending them to `ranking/roster.txt`, and running a plain
unpinned `rank.exe run` (`ranking/CHAMPION.md` rule 1).

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
root `CLAUDE.md`, process-sharded across all CPUs, then rated). Under the hood
it runs `train.exe tournament-play --shard i --of K ...` (each shard writes
`data/tourney.jsonl.<i>`) then `train.exe tournament-rate ...` (merges, fits Elo, prints the
`Elo | ms/move | max ms | games | agent` table, writes `agents/champion*.txt`). Threads are
not used because the engine's board/eval state is global; processes each get their own copy.
Add `-Only "name1,name2,..."` to restrict the roster to those agent names (include their
depths in `-Depths` so the names exist); a subset run leaves `agents/library.txt` +
`champion*.txt` untouched. Every run is archived under `runs/<id>/` (`config.json`,
`elo.tsv`, `notes.md`, `results.jsonl`), logged in `runs/index.jsonl`, and folded into the
agent registry (`agents/registry.{jsonl,md}`, a union with a `spec_hash` that flags retrains
/ changes); `-Note "..."` records a pre-run note and `train.exe run-note --run <id> --note
"..."` attaches one later.

## Ranker (`rank.exe`)

The persistent agent Elo-ranking system is a third binary, independent of both
`breakthrough.exe` and `train.exe` (it links the engine sources plus `src\ranking.cpp`,
NOT `src\ml_train.cpp` or `src\settings.cpp`). Common invocations are in the root
`CLAUDE.md`. Raw build: `.\build_rank.bat`.

Agents are identified by a canonical ID string, e.g.
`ab(d6,tt,ord,nb200k)@1.classic(t2,c10,w3,l2)@1` (grammar in `src\ranking.h`), which is
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
(per-game export) + `ranking/report.md` (W-L split by color, avg plies, end-piece margin, cpu/move,
`eff` = Elo / log2(1 + cpu_us/move), and an Elo-vs-CPU pareto-frontier table). Learned
agents embed an 8-hex model-file content hash in the ID and roster load hard-errors on
a mismatch (a retrain is a new identity). Full internals (ID codec, store row format,
scheduler, BT fit, every subcommand, slot conventions): `src/CLAUDE.md`'s
`ranking.cpp` entry.

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
ladder (random-move `dil(rP)` + stochastic-depth `dil(rP,dN)`) so the top of the table
is well-resolved and the climber has non-deterministic opponents.

## Script details

| File | Purpose |
|---|---|
| `run_tests.ps1` | Build and run the Catch2 test suite in one step: `.\tools\run_tests.ps1 -Build`. Use the `/run-tests` skill to invoke this correctly from Claude sessions. Calls `build_tests.bat` (repo root), which uses `vswhere` to locate VS automatically. |
| `run_train.ps1` | Build (`-Build`) and run `train.exe`, passing args through. |
| `run_tournament.ps1` | Mint a UTC `RunId`, write `runs/<id>/` config via `run-config`, launch K `tournament-play` shards in parallel (one process each, own output file), then merge + `tournament-rate --run <id>`. Params include `-Only`, `-Note`, `-RunId`. |
| `run_rank.ps1` | Build (`-Build`) and run `rank.exe`, passing args through. `-Workers K` shards `play` across K processes (per-shard `<store>.<s>` files appended to the store only after every worker exits cleanly), then rates once. Uses `PositionalBinding=$false` so `--key value` passthrough args are not captured by named params. |
| `sweep_pst.ps1` | Small training sweep for v2 PST models: train a teacher-depth x games grid, then rate each candidate serially via `rank.exe gauntlet` (one shared `models/pst_value.txt` slot). Superseded for large studies by `sweep_pst_v2.ps1`. |
| `sweep_pst_v2.ps1` | General model-sweep harness (was PST-only). A candidate is a `Group`/`Slot`/`Meta`/`Args` object where `Args` is a raw `train.exe` arg array trained into `models/sweep/slot<N>.txt`, so ANY train.exe flag (feature version, `--model-type`, `--residual-skip`, ...) drops into a candidate with no scaffolding change; a candidate may also carry an optional `Wrapper` (its own search shell, e.g. the cheaper `$MlpWrapper` for full-scan MLP cells). Groups: A teacher grid, B dilution decay, C self-play bootstrap chains, D replay extraction, E L2, **F linear residual chip-skip baseline (plain vs auto-skip, theory 24)**, **G MLP capacity comparison (`--model-type mlp` x hidden x skip {off,auto}, theory 24)**. `-Groups "F,G"` selects a subset (default all; only selected groups consume slots, so `F,G` alone fits slots 3..14); F/G share one replay extract (`data/replay_v2_residual.jsonl`). Trains each candidate, captures the `Stratified loss by |matDiff|` printout into the report (`Loss0`/`Loss1`/`Loss2`, the theory-24 equal-material calibration measure), appends candidates to the roster (idempotent), rates everyone in ONE `rank.exe run` (sharded via `-Workers`), and writes `models/sweep/report_v2.csv`. `-NoRate` stops after training + stratified loss (skips the roster/matches append + rating -- cheap and reversible; run the full rating later). `-Only N` dry-runs the first N candidates. After a rating study, trim the sweep lines back out of the roster (their games stay in `matches.jsonl` as retired history). First run's findings (groups A-E): `plans/training-sweep-results-1-luminous-snail.md`; F/G (residual/MLP): `plans/residual-mlp-results-1-tingly-chipmunk.md`. |
| `train_scaling.ps1` | Resumable data-scaling study for v2 value models: pins the sweep-validated recipe (d2 teacher, dilution decay), doubles the self-play game count until the mean screening Elo gain over its seed replicas drops below `-ConvergeElo`, then runs a replay-data arm (`rank.exe extract` at 4k/8k games) and an epoch probe, and d6-confirms the best cell. Appends to `models/sweep/scaling.csv` and skips cells already recorded there, so an interrupted run resumes. First run's result: replay training beat single-teacher self-play by ~250 Elo (best model d6 Elo 920, promoted to `models/pst_value.txt`). **[SELF-PLAY CONVERGENCE UNSUPPORTED - see `Docs/corrections.md`]** Its `-ConvergeElo` stop is NOT a convergence detector and its self-play arm never converged. The arm is 4 rows total (250 and 500 games, 2 seeds each); it stopped at 500 on a +16 mean gain under a 20-Elo threshold, while the seed spread WITHIN a size was 94 and 72 Elo, and no size above 500 was ever tested. The stop threshold is smaller than the noise it is thresholding, so it fires at the first rung by construction (theory 45). `plans/training-sweep-results-1-luminous-snail.md` item 3 already said this and warned "do not trust 'self-play plateaus at 500'"; the caveat is repeated here because this row is where the result gets quoted from. Before reusing this script, either raise the seed count until the seed band is below `-ConvergeElo`, or replace the stop rule with a fixed ladder rated end to end (what `tdleaf_study.ps1` does via `--ckpt-at`). |
| `train_vs_champion.ps1` | Vs-champion training-data study: generates `rank.exe pairgen` datasets from games involving the reigning champion (learner-vs-champ, diluted-champ-vs-champ, d8 oracle-vs-champ, champion-loss cherry-picks, branch-mined winning lines), trains linear v2 PST cells per dataset (seed replicas), gauntlet-screens at the d4 wrapper into `models/sweep/vs_champ.csv` (resumable), gates a bootstrap arm, promotes each family's best to reserved slots 94..99, d6-confirms, appends the d6 IDs + the oracle to the roster, re-rates the pool, and prints the opponent-bucket residual analysis (champion / classic-like / diverse) that answers the two recorded theories. `-DryRun` for a tiny pipeline check, `-AnalysisOnly` to recompute the bucket tables later (the standing longitudinal theory re-check). First run's result (`plans/vs-champion-training-results-1-cozy-forest.md`): diluted-champion-vs-champion and oracle-vs-champion data beat the replay recipe, the best model ties the champion at d6 (1137 vs 1140 on the shared fit), one-sided cherry-picked datasets fail from degenerate labels. |
| `opener_bias_study.ps1` | Theory 6 test (`Docs/theories.md`), Layers 1+2: for each promoted challenger (champdil s96, oracle s98) vs the champion, plays the d6 head-to-head under three opener configs -- S (`--open-side both`, the symmetric baseline), C (`--open-side a`, challenger random / champion true policy), P (`--open-side b`, champion random) -- and reads the win tally from each pairgen `.meta.json`, then runs `rank.exe opener-bias` with a learned judge for the mechanism measure. Writes `data/opener_bias/` (gitignored) + `sensitivity_sweep.csv`. First run: champdil 65% (S) -> 40% (C), oracle 58.8% (S) -> 66.2% (C). Results: `plans/opener-bias-results-1-synchronous-stearns.md`. |
| `opener_bias_retrain.ps1` | Theory 6 test, Layer 3: regenerates the oracle training set with `--open-side a` (only the oracle plays the random opener; the champion plays its own opening) into `data/pg_oracle_champ_asym.jsonl`, retrains the 3-seed oracle cell (`--from-data`), gauntlet-screens at the d4 wrapper, d6-confirms the best, and compares to the symmetric baseline (screen mean 785 / d6 1137). Resumable via `models/sweep/opener_bias_retrain.csv`; archives models to `models/sweep/vsc_oracle-asym_<seed>.txt` (does NOT overwrite the symmetric `vsc_oracle-vs-champ_*.txt` or touch the roster). `-DryRun` for a tiny pipeline check. |
| `hill_climb.ps1` | Stochastic hill climber over the Advanced eval weight mix (13 weights: c,w,l,f,d,e,m,h,b,o,r,x,n), optimizing Elo at a fixed depth via `rank.exe gauntlet` as the fitness function. Turn pinned at `-Turn` (20), `-NoiseSeed`/`-RaceWin` pinned (1/1), absolute weight values renormalized to sum `-Sum`-`-Turn` (80) so the search varies the mix not the scale and candidates dedupe. Greedy-from-best with `{1,3,5}`-unit simplex steps + occasional drastic chip reset; `-AllowNegative` adds sign-flip mutations and signed resets; id-keyed cache. `-Roster` defaults to `ranking/climb_roster.txt`; `-Promote` appends the top finds to `ranking/roster.txt` and runs a full refit. Logs every candidate to `ranking/climb_adv_<mode>_d<depth>_<stamp>.tsv` (gitignored). |
| `tdleaf_study.ps1` | TD-Leaf(lambda) self-play study orchestrator (phases: train, roster, play, screen; resumable via `models/sweep/tdleaf_study.csv`). Trains 13 runs into a 38-agent cohort on slots 128..165, one axis at a time around a base config (`init=models/pst_value.txt`, lambda 0.7, lr 0.01, d6/nb200k, 4 random opener plies per side): **A** learning curve + seed band (4 seeds x rungs 100/250/500/1000/2000 = 20 agents), **B** lambda in {0, 1} vs the base's 0.7, **C** from-scratch init, **D** lr in {0.003, 0.03}, **E** a d4 generator-depth control. **The game count is not an input**: `--ckpt-at` writes a rung ladder per run and every rung is rated as its own agent, so the learning curve is an output. Then appends the cohort to `ranking/roster_tdleaf.txt`, writes the id list `ranking/cohort_tdleaf.txt`, plays with `--cohort` (so only pairs touching a cohort agent are scheduled), and screens with `rate --pin ranking/standings.tsv`. **Screening only** -- the pinned fit cannot dethrone anything; certification is a separate, deliberate unpinned refit once the agents to keep are chosen. Rungs of ONE run share a training trajectory and are NOT independent replicates; only distinct seeds are. |
| `label_study.ps1` | Position-oracle labeling campaign orchestrator (phases: prep, posgen, label-train, label-eval, fit, train, eval, rate), resumable via a CSV ledger of done cells (`data/labels/study.csv`). prep appends + rates the two depth-diluted d8 ladder rungs and freezes `data/labels/ratings_snapshot.tsv` (the study's fixed Elo basis, never overwritten). The label phases chunk positions across `-Workers` rank.exe shards (`--resume --done` exact top-up, shard merge, meta carry) and detect completion from the shard metas' positions_touched; leftover shard files from an interrupted chunk are salvaged into the master (exact-line dedup, sound because deterministic seeds make replayed rows byte-identical) before the chunk relaunches, so a kill at any moment loses nothing already played. train launches the dist configs in parallel (lin + two mlp seeds + a wide mlp); eval runs `train.exe dist-eval` per model; rate wires slots 76..79 into the roster at both standard heads (d4 + d6/nb200k) and refits. (The dist MLP mu heads are incrementally scored since the NNUE-style accumulator + sparse leaf-tail forward shipped, so a d6/nb200k MLP leaf is affordable; the earlier d4-only-for-MLP restriction is gone.) `-DryRun` runs everything on a tiny pool with a d2 ladder in minutes under `data/labels/dry/`. Ladder spec files (`ladder_train.txt`, `ladder_eval.txt`) are written once with the default design and are hand-editable. |
| `smoke_test_gui.ps1` | Standard GUI smoke test: build/launch/screenshot/close, exits non-zero on crash (run from project root). See `gui/CLAUDE.md`. |
| `gui_capture.ps1` | Targeted screenshot helper: finds the `GLFW30` window by process id and crops its client area for inspecting individual widgets (complements `smoke_test_gui.ps1`). |
| `train_main.cpp` | `train.exe` CLI: subcommands `selfplay-supervised`, `ensemble`, `imitate`, `dist-value`, `score`, `dist-eval`, `tournament`, `tournament-play`, `tournament-rate`, `turn-swing`, `speed`, `run-config`, `run-note`, `docs`, all `--key value` (incl. `--only`, `--run`, `--note`, `--node-budget`, `--time-budget-ms`, `--budgets`, `--ablate`, `--forward-study`, `--gen-eval`/`--gen-params`, `--teacher-eval`/`--teacher-params`, `--feature-version`, selfplay-supervised's `--model-type linear|mlp` + `--mlp-hidden "32"|"32,16"` + `--residual-skip <f>` (0 off / >0 fixed / <0 auto-calibrate the frozen chip skip), ensemble's `--models <comma-list>` + `--mirror 0|1` + `--out`, and `turn-swing`'s `--chip/--wall/--col/--forward`). |
| `rank_main.cpp` | `rank.exe` CLI: subcommands `check`, `play`, `rate`, `run`, `seal`, `split`, `history`, `gauntlet`, `extract`, `bookgen`, `pairgen`, `opener-bias`, `opener-swap`, `posgen`, `label`, `labelfit`, all `--key value` (`--roster`, `--in`, `--out`, `--board`, `--games`, `--seed`, `--shard`/`--of`, `--agent`, `--last`, `--id`, `--keep`, seal's `--max-mb`, split's `--group`/`--max-mb`/`--apply` (**dry run unless `--apply`**), extract's `--feature-version`/`--sample`, bookgen's `--a` (line owner) `--b` (target) `--plies` `--out`, pairgen's `--a`/`--b`/`--dil-apply`/`--dil-start`/`--dil-floor`/`--dil-decay-plies`/`--open-plies`/`--open-side`/`--filter`/`--branch-tries`, opener-bias's `--a`/`--b`/`--judge`/`--open-plies`/`--games`, opener-swap's `--a`/`--b`/`--open-plies`/`--games`, posgen's `--out-train`/`--out-eval`/`--train`/`--eval`/`--per-game`/`--min-ply`/`--max-ply`, label's `--pool`/`--ladder`/`--out`/`--resume`/`--done`/`--max-positions`, labelfit's `--in`/`--pool`/`--ratings`/`--out`/`--min-rows`/`--rating-se`). |
| bookgen (subcommand) | Mine an opening/refutation book from stored games between two agents. Replays every stored `--a` vs `--b` game, keeps positions + the move `--a` played (first `--plies` half-moves) from A's WINS only, writes `models/book<N>.txt` (a `#` provenance header + `<positionKey hex16> <sx> <sy> <dx>` lines). The `book` opener (`src/ai_random.cpp` `g_openers[]`) plays those replies via `.opener(book,<N>)@1`. First use: the s98 refutation book (dethrone plan phase 2, `plans/dethrone-champion-results-3-wiggly-mitten.md`). The book file is NOT hashed into the agent ID (unlike `learned()` models), so treat a book slot as immutable and give a regenerated book a new slot number. **Read `plans/book-opener-audit-results-1-vivid-lantern.md` (theory 38) before quoting any book Elo:** a book is a memorized line keyed by position hash, so its measured lift only holds while the opponent reproduces its previous replies, and it collapses under `pairgen --open-plies`. |

**Mined books.** Slot numbers are immutable, a regenerated book gets a new slot.
`--plies` is the book DEPTH (how many of the line owner's half-moves are stored).

| Slot | Line owner (`--a`) | Target (`--b`) | Depth | Entries | Kept replays |
|---|---|---|---|---|---|
| `book1` | `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2` (oracle) | `learned(s98,5801570e)` | 60 | 553 | 29 of 32 |
| `book2` | `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` | `learned(s98,5801570e)` | 60 | 134 | 7 of 32 |
| `book3` | `ab(d6,ord,nb200k)@1.adv(t20,c77,...)@1` | champion | 60 | 24 | 16 of 32 |
| `book4` | `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1` | `classic@2` | 60 | 519 | 25 of 32 |
| `book5` | `ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974)@1` (dist) | champion | 60 | 145 | 6 of 8 |
| `book6` | `ab(d6,tt,ord,nb200k)@1.learned(s3,68364898)@1` | champion | 60 | 162 | 7 of 8 |
| `book7` | `classic@2` (same pair as book2) | `learned(s98,...)` | 6 | 13 | 7 of 32 |
| `book8` | same | same | 16 | 38 | 7 of 32 |
| `book9` | same | same | 30 | 73 | 7 of 32 |
| `book10` | `learned(s98,...)` (same pair as book4) | `classic@2` | 6 | 41 | 25 of 32 |
| `book11` | same | same | 16 | 134 | 25 of 32 |
| `book12` | same | same | 30 | 279 | 25 of 32 |
| `book13` | `classic@2` (same pair as book2/7-9) | `learned(s98,...)` | 4 | 8 | 7 of 32 |
| `book14` | same | same | 8 | 18 | 7 of 32 |
| `book15` | `learned(s98,...)` (same pair as book4/10-12) | `classic@2` | 4 | 22 | 25 of 32 |
| `book16` | same | same | 8 | 60 | 25 of 32 |
| `book17` | `learned(s3,68364898)` (same pair as book6) | `classic@2.opener(book,2)@1` | 4 | 10 | 23 of 32 |
| `book18` | same | same | 8 | 42 | 23 of 32 |
| `book19` | `ab(d6,ord,nb200k)@1.adv(t20,c77,...)@1` (same pair as book3) | `classic@2.opener(book,2)@1` | 4 | 2 | 16 of 32 |
| `book20` | same | same | 8 | 4 | 16 of 32 |

`book13`-`book16` (2026-07-28) and `book17`-`book20` (2026-07-29) fill the 4-ply
and 8-ply rungs of the four existing own-book ladders, for the category-champion
split (`ranking/CHAMPION.md`, "4-book" and "8-book" categories). `book17`/`book18`
(s3-own) and `book19`/`book20` (adv-own) are mined against the fixed pre-split
single champion identity (`classic@2.opener(book,2)@1`), the same target book3/
book6 used, so mining needed no new games. `adv`'s books live at a DIFFERENT
search head (`ab(d6,ord,nb200k)@1`, no `tt`) so they add Elo diversity to the
roster but are never category-eligible for the 4-book/8-book titles (one-head
rule). `book11` was the single-champion era's reigning book until the 2026-07-28
split; under the new taxonomy (exactly 4-ply and 8-ply) it isn't a member of
either book category and holds no title, but stays rostered as depth-ladder
data. Books 1-4, 6, 7-20 are rostered in `ranking/roster.txt`; `book5`
is not, because its `learned(s111,...)` dist core costs 370 ms/move and would add
hours of roster play for one row.

Two things measured on the full-roster refit that are easy to get wrong
(`plans/book-opener-audit-results-1-vivid-lantern.md`, theory 38):

- **Book size does not predict book strength.** The 553-entry oracle book is the
  worst loadout on the `classic` core (-12 Elo), the 24-entry `adv` self-book is the
  worst anywhere (-107), and the best is 134 entries.
- **A self-mined book is not reliably better than a borrowed one.** Own beats
  borrowed for `classic`, ties for `s98`, and LOSES for `learned(s3,...)` (-33 vs +1)
  and `adv(t20,c77,...)` (-107 vs +15). Do not assume core-specificity.

Book size also tracks the mining pair's diversity rather than its skill: `book3` is
tiny (24 entries, 0 replay drift) because its no-TT head is fully deterministic and
its 16 wins collapse to about 7 distinct games, while `book4` is large (519 entries,
12 drifted) because the `tt` head's cross-game state made its 25 wins varied.

## Artifact directories (repo root)

| Dir | Purpose |
|---|---|
| `ranking/` | The persistent Elo-ranking state: `roster.txt` (hand-edited `anchor|on|off <id>` lines, incl. a dense diluted-d6 ladder), `CHAMPION.md` (the reigning-champion declaration, single source of truth + certification methodology), `roster_top.txt` (the reusable top-resolution boost roster: contenders played to >= 32 games/pair before any top-of-table claim), `climb_roster.txt` (a small mostly-stochastic opponent pool for the hill climber), and the **match store** (below). The rating outputs `ratings.tsv` (full historical fit, includes retired agents), `standings.tsv` (**read this one for current standings**: active agents only, grouped by search head), `games.tsv` and `report.md` are all **gitignored**: `rank.exe rate` rebuilds them from the store in about 20 seconds (measured 2026-08-01, 649,434 games) and the fit is deterministic, so a fresh clone runs a rate before reading standings. Shard temps `matches.jsonl.*`, `gauntlet.jsonl` scratch, and `climb_*.tsv` logs are gitignored too. |
| `ranking/` (store) | **The match store is a set of PARTS plus a live tail**, not one file. `matches.index.txt` (committed) lists the parts in load order, one filename per line, `#` comments allowed; a listed part whose file is absent is skipped rather than being an error. Writers always append to `matches.jsonl`, the tail, which is loaded last. Parts are grouped by WHO played the game (`rank.exe split`). **Committed:** `matches.roster.NNNN.jsonl` (games between agents currently in the roster, 132,769 rows / 55 MB) and `matches.retired_other.NNNN.jsonl` (59,054 rows / 23 MB, kept tracked so re-rostering one of those agents needs no file transfer). **Gitignored:** `matches.retired_tdleaf_self.NNNN.jsonl`, the TD-Leaf Pass-2 candidates that were screened and never promoted, at 457,611 rows / 193 MB -- 89% of all retired rows and 71% of the store, which is why the split exists. They stay on disk and nothing is deleted. To take a group out of the ratings, delete its line from the index; to put it back, restore the line. **A retired agent's games are not dead weight:** Bradley-Terry fits every rating jointly, so a game against a retired agent is evidence about the ROSTERED agent that played it, and rating without the untracked part moves 153 of 170 rostered agents (Spearman rho 0.9526, mean error bar 7.2 -> 10.2 Elo, measured 2026-08-01). `rank.exe seal` separately rolls an oversized tail into `matches.NNNN.jsonl` shards (and appends them to the index when one exists). Both cap every part with `--max-mb` so no single file can outgrow what a host accepts (GitHub rejects blobs over 100 MB). |
| `ranking/` (2nd pool) | **Diversified-opening pool**, a self-contained second instrument added 2026-07-26: `roster_open.txt` (14 agents, each wearing `.opener(rand,4)@1`), `matches_open.jsonl` (its own store, never mixed with `matches.jsonl`), and generated `ratings_open.tsv` / `standings_open.tsv` / `games_open.tsv` / `report_open.md`. Rating outputs are named after the store, so `matches<X>.jsonl` writes `ratings<X>.tsv` and so on, and the default store keeps the historical unsuffixed names. |
| `runs/` | Per-run archive (one timestamped dir per tournament): `config.json` (exact config + pre-run note), `elo.tsv` (that run's ranked table), `notes.md` (pre-run + `run-note`-appended notes), `results.jsonl` (gitignored copy). `runs/index.jsonl` is the master log, one summary line per run. |
| `data/`, `models/`, `agents/` | ML outputs: append-only JSONL datastore, model checkpoints + `manifest.{json,md}` + `registries.json`, the Elo-rated `agents/library.txt` (full-roster snapshot), and the agent registry `agents/registry.{jsonl,md}` (union of every agent ever rated, with a `spec_hash`). |
| `data/labels/` | Position-oracle campaign home: committed pools (`pool_train/eval.jsonl`), ladder specs, fitted labels (`labels_train/eval.jsonl`), raw-store `.meta.json` sidecars (the frozen rung-id mapping), `ratings_snapshot.tsv` (the study's fixed Elo basis), and `study.csv` (the resume ledger). The raw stores themselves (`raw_train/eval.jsonl`, ~hundreds of MB, the durable asset that re-labels under any future ratings fit) are gitignored -- back them up outside git. `dry/` and `logs/` are scratch. |
