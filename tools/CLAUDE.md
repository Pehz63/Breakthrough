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
```

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
`rate` writes `ranking/ratings.tsv` + `ranking/games.tsv` (per-game export) +
`ranking/report.md` (W-L split by color, avg plies, end-piece margin, cpu/move,
`eff` = Elo / log2(1 + cpu_us/move), and an Elo-vs-CPU pareto-frontier table). Learned
agents embed an 8-hex model-file content hash in the ID and roster load hard-errors on
a mismatch (a retrain is a new identity). Full internals (ID codec, store row format,
scheduler, BT fit, every subcommand, slot conventions): `src/CLAUDE.md`'s
`ranking.cpp` entry.

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
| `train_scaling.ps1` | Resumable data-scaling study for v2 value models: pins the sweep-validated recipe (d2 teacher, dilution decay), doubles the self-play game count until the mean screening Elo gain over its seed replicas drops below `-ConvergeElo`, then runs a replay-data arm (`rank.exe extract` at 4k/8k games) and an epoch probe, and d6-confirms the best cell. Appends to `models/sweep/scaling.csv` and skips cells already recorded there, so an interrupted run resumes. First run's result: replay training beat single-teacher self-play by ~250 Elo (best model d6 Elo 920, promoted to `models/pst_value.txt`). |
| `train_vs_champion.ps1` | Vs-champion training-data study: generates `rank.exe pairgen` datasets from games involving the reigning champion (learner-vs-champ, diluted-champ-vs-champ, d8 oracle-vs-champ, champion-loss cherry-picks, branch-mined winning lines), trains linear v2 PST cells per dataset (seed replicas), gauntlet-screens at the d4 wrapper into `models/sweep/vs_champ.csv` (resumable), gates a bootstrap arm, promotes each family's best to reserved slots 94..99, d6-confirms, appends the d6 IDs + the oracle to the roster, re-rates the pool, and prints the opponent-bucket residual analysis (champion / classic-like / diverse) that answers the two recorded theories. `-DryRun` for a tiny pipeline check, `-AnalysisOnly` to recompute the bucket tables later (the standing longitudinal theory re-check). First run's result (`plans/vs-champion-training-results-1-cozy-forest.md`): diluted-champion-vs-champion and oracle-vs-champion data beat the replay recipe, the best model ties the champion at d6 (1137 vs 1140 on the shared fit), one-sided cherry-picked datasets fail from degenerate labels. |
| `opener_bias_study.ps1` | Theory 6 test (`Docs/theories.md`), Layers 1+2: for each promoted challenger (champdil s96, oracle s98) vs the champion, plays the d6 head-to-head under three opener configs -- S (`--open-side both`, the symmetric baseline), C (`--open-side a`, challenger random / champion true policy), P (`--open-side b`, champion random) -- and reads the win tally from each pairgen `.meta.json`, then runs `rank.exe opener-bias` with a learned judge for the mechanism measure. Writes `data/opener_bias/` (gitignored) + `sensitivity_sweep.csv`. First run: champdil 65% (S) -> 40% (C), oracle 58.8% (S) -> 66.2% (C). Results: `plans/opener-bias-results-1-synchronous-stearns.md`. |
| `opener_bias_retrain.ps1` | Theory 6 test, Layer 3: regenerates the oracle training set with `--open-side a` (only the oracle plays the random opener; the champion plays its own opening) into `data/pg_oracle_champ_asym.jsonl`, retrains the 3-seed oracle cell (`--from-data`), gauntlet-screens at the d4 wrapper, d6-confirms the best, and compares to the symmetric baseline (screen mean 785 / d6 1137). Resumable via `models/sweep/opener_bias_retrain.csv`; archives models to `models/sweep/vsc_oracle-asym_<seed>.txt` (does NOT overwrite the symmetric `vsc_oracle-vs-champ_*.txt` or touch the roster). `-DryRun` for a tiny pipeline check. |
| `hill_climb.ps1` | Stochastic hill climber over the Advanced eval weight mix (13 weights: c,w,l,f,d,e,m,h,b,o,r,x,n), optimizing Elo at a fixed depth via `rank.exe gauntlet` as the fitness function. Turn pinned at `-Turn` (20), `-NoiseSeed`/`-RaceWin` pinned (1/1), absolute weight values renormalized to sum `-Sum`-`-Turn` (80) so the search varies the mix not the scale and candidates dedupe. Greedy-from-best with `{1,3,5}`-unit simplex steps + occasional drastic chip reset; `-AllowNegative` adds sign-flip mutations and signed resets; id-keyed cache. `-Roster` defaults to `ranking/climb_roster.txt`; `-Promote` appends the top finds to `ranking/roster.txt` and runs a full refit. Logs every candidate to `ranking/climb_adv_<mode>_d<depth>_<stamp>.tsv` (gitignored). |
| `smoke_test_gui.ps1` | Standard GUI smoke test: build/launch/screenshot/close, exits non-zero on crash (run from project root). See `gui/CLAUDE.md`. |
| `gui_capture.ps1` | Targeted screenshot helper: finds the `GLFW30` window by process id and crops its client area for inspecting individual widgets (complements `smoke_test_gui.ps1`). |
| `train_main.cpp` | `train.exe` CLI: subcommands `selfplay-supervised`, `imitate`, `tournament`, `tournament-play`, `tournament-rate`, `turn-swing`, `speed`, `run-config`, `run-note`, `docs`, all `--key value` (incl. `--only`, `--run`, `--note`, `--node-budget`, `--time-budget-ms`, `--budgets`, `--ablate`, `--forward-study`, `--gen-eval`/`--gen-params`, `--teacher-eval`/`--teacher-params`, `--feature-version`, selfplay-supervised's `--model-type linear|mlp` + `--mlp-hidden "32"|"32,16"` + `--residual-skip <f>` (0 off / >0 fixed / <0 auto-calibrate the frozen chip skip), and `turn-swing`'s `--chip/--wall/--col/--forward`). |
| `rank_main.cpp` | `rank.exe` CLI: subcommands `check`, `play`, `rate`, `run`, `history`, `gauntlet`, `extract`, `pairgen`, `opener-bias`, `opener-swap`, all `--key value` (`--roster`, `--in`, `--out`, `--board`, `--games`, `--seed`, `--shard`/`--of`, `--agent`, `--last`, `--id`, `--keep`, extract's `--feature-version`/`--sample`, pairgen's `--a`/`--b`/`--dil-apply`/`--dil-start`/`--dil-floor`/`--dil-decay-plies`/`--open-plies`/`--open-side`/`--filter`/`--branch-tries`, opener-bias's `--a`/`--b`/`--judge`/`--open-plies`/`--games`, opener-swap's `--a`/`--b`/`--open-plies`/`--games`). |

## Artifact directories (repo root)

| Dir | Purpose |
|---|---|
| `ranking/` | The persistent Elo-ranking state: `roster.txt` (hand-edited `anchor|on|off <id>` lines, incl. a dense diluted-d6 ladder), `climb_roster.txt` (a small mostly-stochastic opponent pool for the hill climber), `matches.jsonl` (append-only ID-keyed match history, committed, the never-recomputed asset), and generated `ratings.tsv` + `games.tsv` + `report.md`. Shard temps `matches.jsonl.*`, `gauntlet.jsonl` scratch, and `climb_*.tsv` logs are gitignored. |
| `runs/` | Per-run archive (one timestamped dir per tournament): `config.json` (exact config + pre-run note), `elo.tsv` (that run's ranked table), `notes.md` (pre-run + `run-note`-appended notes), `results.jsonl` (gitignored copy). `runs/index.jsonl` is the master log, one summary line per run. |
| `data/`, `models/`, `agents/` | ML outputs: append-only JSONL datastore, model checkpoints + `manifest.{json,md}` + `registries.json`, the Elo-rated `agents/library.txt` (full-roster snapshot), and the agent registry `agents/registry.{jsonl,md}` (union of every agent ever rated, with a `spec_hash`). |
