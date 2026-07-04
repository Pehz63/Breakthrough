# Persistent Agent Elo-Ranking System (rank.exe)

## Context

The existing tournament (`train.exe tournament-*`) recomputes Elo from scratch every run from one transient results file, has no per-pair match history, mean-centers ratings at 1500 with no anchor (which is why weak agents drift to negative Elo), defines agents only in C++ code (`buildTournamentRoster`), and prints one line per shard at the end. The user wants a new, separate ranking system that:

- Persists every game forever, keyed by a modular human-readable agent ID, so adding one new agent costs O(N) games (only its missing pairings), never replaying old ones.
- Fixes negative Elo by anchoring UniformRandom at Elo 0.
- Uses an easily hand-editable roster document with on/off toggles.
- Prints live per-game progress, exposes full per-agent match history, and reports both strength (Elo +/- SE) and compute cost (ms/move, nodes/move).
- Serves as deterministic ground truth for ML training and a future hill climber (elo vs compute pareto), via a fixed-pool `gauntlet` mode that rates one candidate in O(N).

Decisions confirmed with the user: **Bradley-Terry ML refit** over full stored history (deterministic, order-independent), anchored at UniformRandom = 0 with a small virtual-draw prior. **Compact functional ID style** (`ab(d6).classic(t2,c10,w3,l2).v1`). **Plain text on/off roster** with a generated read-only report beside it. The old tournament stays untouched. New separate binary `rank.exe` (consistent with the user's "mostly separate work" note).

## New files

| File | Purpose |
|---|---|
| `src/ranking.h` | Public API: `RankAgent {AgentSpec spec; std::string id; bool active; bool anchor;}`, `MatchRow`, ID codec (`rankAgentId`/`rankAgentFromId`), roster loader, match store, scheduler, BT fit, subcommand entries. ID->AgentSpec is public so future tools (hill climber, GUI) can instantiate an agent from its ID string. |
| `src/ranking.cpp` | Full implementation (~1100 lines). Does NOT link `ml_train.cpp`. Replicates its tiny static helpers locally (`ensureDir`, `fnv1a64`, `jsonStr`/`jsonNum`, `nowUtcString`, registry-index-by-name), since they are `static` there and exporting them would couple old and new systems. |
| `tools/rank_main.cpp` | `rank.exe` CLI, modeled on `tools/train_main.cpp` (`--key value` parsing). |
| `build_rank.bat` | Mirrors `build_train.bat` (vswhere + vcvars64): `tools/rank_main.cpp` + engine sources + `src/ranking.cpp`, excluding `ml_train.cpp`, `settings.cpp`, `main.cpp`. Creates `ranking/` dir. Output `rank.exe`. |
| `tools/run_rank.ps1` | `-Build` + passthrough driver. `-Workers K` shards `play` across K processes (per-shard files `ranking/matches.jsonl.<s>`, append-merged after all exit 0, then deleted), then runs `rate`. Serial is the default (clean timing). |
| `ranking/roster.txt` | Seed roster (below). Committed. |
| `tests/test_ranking.cpp` | Catch2 tests (below). |

Existing files edited: `build_tests.bat` (add `src\ranking.cpp` + `tests\test_ranking.cpp`), `.gitignore` (ignore `ranking/matches.jsonl.*` shard temps and `ranking/gauntlet.jsonl` scratch; matches.jsonl itself is precious and NOT ignored), `README.md`, `CLAUDE.md`, `ML.md` (short manual section: tournament = one-off experiments, ranking = permanent incremental ladder), `todo.md` (strike through "Gauntlet vs fixed anchors" and "BayesElo-style rating with uncertainty"). `CMakeLists.txt` unchanged (it never built train.exe either).

## Agent ID codec

Canonical invariant: `rankAgentId(rankAgentFromId(id).spec) == id`, enforced by tests and `rank.exe check`. Non-canonical IDs are rejected with the canonical form printed for pasting.

```
id      := head { "." segment } "." version
head    := "rand" | "tiered" | "smart(" N ")" | "policy"         (policy brains)
         | "greedy" | "ab(" "d" N { "," flag } ")"               (search brains)
flag    := "noab" | "tt" | "ord" | "part" | "asp" N
         | "nb" budget | "tb" N "ms" | "cap" N                   (budget: 200k, 2m, or raw)
segment := evalseg | dilseg
evalseg := "classic(" weights ")" | "exp(" weights ")"           (search brains only)
         | "learned(s" slot "," hash8 ")"                        (LearnedValue)
         | "linpol(s" slot "," hash8 ")"                         (policy head model)
weights := letter int { "," letter int }                         ALL params, registry order
dilseg  := "dil(r" pct ")"                                       r5 = 5%, r2.5 ok; future args reserved, v1 rejects
version := "v" int                                               required, always last, user-bumped identity salt
```

Examples: `rand.v1`, `smart(4).v1`, `greedy.classic(t1,c4,w0,l0).v1`, `ab(d6).classic(t2,c10,w3,l2).v1`, `ab(d8,tt,ord,nb200k).exp(t2,c10,w3,l2,f2).dil(r5).v1`, `greedy.learned(s0,ab12cd34).v1`, `policy.linpol(s1,9f3e21aa).v1`.

Key rules:
- All tokens map to registry indices by NAME lookup (never hardcoded indices). Agents built via `agentMakeSearch`/`agentMakePolicy` (src/agents.cpp) then overridden, so registry defaults stay single-sourced.
- Weight letters come from an explicit hand-maintained codec table in ranking.cpp: Classic -> `t/c/w/l` (turn/chip/wall/column), Experimental (`exp`) adds `f` (forward). A test asserts every `g_evaluators` entry has a codec row with unique letters, so a new evaluator without one fails loudly.
- Canonical rule: ALL evaluator params always written, registry order, even when default. IDs stay stable if registry defaults change.
- `ab()` flags appear only when non-default, in fixed grammar order. Maps: `noab` -> `useAlphaBeta=false`, `tt`/`ord`/`part` -> toggles, `asp` -> `aspirationWindow`, `nb`/`tb` -> budgets, `cap` -> `depthCap`.
- Learned model hash = first 8 lowercase hex of FNV-1a-64 over the model file bytes (slot 0 = `models/lin_value.txt`, slot 1 = `models/lin_policy.txt`). Roster load hashes the file and hard-errors on mismatch (retrained model = new identity, history stays truthful).
- `AgentSpec.name[48]` is too short for IDs, so the ID lives in `RankAgent::id` and `name` gets a truncated debug copy. The ID string IS the identity (no spec hash).
- Rating and `history` treat store IDs as opaque strings, so legacy/unknown IDs in the store still rate and display.

## Data formats (all under `ranking/`)

**roster.txt** (hand-edited): `anchor|on|off  <id>  [# comment]` per line, blank/comment lines ok, CRLF tolerated. Loader enforces exactly one `anchor` (implicitly active), no duplicate IDs, canonical IDs. `off` keeps history in the fit but excludes from scheduling.

**matches.jsonl** (append-only, the precious store), one flat JSON object per game:
```
{"t":"g","w":"<whiteId>","b":"<blackId>","r":"W|B|D","plies":57,
 "wms":812.4,"bms":1.9,"wmv":29,"bmv":28,"wnod":181233,"bnod":0,
 "seed":3141592653,"board":"boards/board1.txt","par":1,"ts":"<utc>","run":"<utc-stamp>"}
```
`D` = 400-half-move cap (scored 0.5). `wnod/bnod` accumulate `g_lastNodes` for search brains (guard `g_lastNodes > 1` like ml_train.cpp:664). `par` = worker count the game ran under (1 = clean timing). Malformed lines are skipped with a counted warning.

**ratings.tsv** (generated): `rank elo pm games wins losses draws ms_move nodes_move active id`, Elo-descending. The machine-friendly ground-truth label file.

**report.md** (generated): header (fit settings, legend: +/- is one SE), main table for active agents (`rank | Elo | +/- | games | W-L-D | ms/move | nodes/move | id`, anchor marked), inactive/retired agents table, head-to-head matrix (`score% (n)` cells in rank order), then per-agent sections: per-opponent table `opponent | games | W-L-D | actual score | expected score | delta` (expected from fitted Elo gap). This is the visible match-history fix. `rate` also prints the main table to console.

## Scheduler (the incremental O(N) core)

1. Active agents sorted lexicographically by ID. Pairs (i, j), i < j, in that order (roster line order never matters).
2. Per pair, color-specific targets: ID-smaller agent gets White in ceil(G/2), Black in floor(G/2) (G = `--games`, default 8). Pending per color = max(0, target - already in store for that exact ID pair + color). Lopsided history gets rebalanced, never deleted.
3. Global pending list, shard s plays indices with `p % K == s`.
4. Per-game seed = 32-bit truncation of FNV-1a over `"<whiteId>|<blackId>|<pairOrdinal>|<runSeed>"` (pairOrdinal = existing pair game count + emission index). `srand(seed)` right before each game, so games are self-contained and identical regardless of order or sharding (fixes the old order-sensitive global rand stream). `--seed` defaults to 1.

Play loop copies the `tournamentPlay` pattern (src/ml_train.cpp:620-699): `PRNT=0`, `mlLoadSlot` for referenced slots, `reloadBoard` per game, 400-half-move cap, per-move `steady_clock` timing, victor codes from `agentChooseMove`. One row appended + flushed per game, with a live progress line:
```
[  17/132] ab(d4).classic(t1,c4,w0,l0).v1 (W) vs rand.v1 : W in 57 plies, 0.8s | pair 3-0-1
```
plus a start banner (N active, P pending, shard s/K) and `[s<shard>]` prefix when sharded.

## Bradley-Terry fit (~100 lines, no deps)

1. Aggregate rows into per-pair counts via ordered `std::map` (order-independent): `w_ij` = i's score sum vs j, `n_ij` = games. Agent set = union of history IDs and roster IDs.
2. Prior: 0.5 virtual games (0.25 win each way) per pair that has at least one real game. Keeps undefeated agents finite (8-0 fits to roughly +600, not infinity), adds no phantom edges. Connectivity safety net: union-find; components not containing the anchor rate relative to their own mean = 1000 and are marked provisional with a warning.
3. MM fixed point (Hunter's algorithm): `gamma_i' = W_i / sum_j (n_ij / (gamma_i + gamma_j))`, renormalize anchor gamma = 1 each sweep, converge at max |delta log gamma| < 1e-9 or 5000 sweeps.
4. Elo = (400/ln 10) * ln(gamma). Anchor exactly 0. If no anchor is available (legacy store rated standalone), fix mean at 1000 with a WARNING.
5. SE from Fisher diagonal: `I_ii = sum_j n_ij * p_ij * (1-p_ij)`, `+/- = (400/ln 10)/sqrt(I_ii)`.

Negative Elo then only means genuinely worse than UniformRandom, which is the honest reading.

## CLI (rank.exe)

| Command | Key options | Behavior |
|---|---|---|
| `check` | `--roster`, `--games` | Validate roster (canonical IDs, single anchor, dupes, model hashes, prints expected hash for learned agents), print pending-game count. Non-zero exit on error. |
| `play` | `--roster --games --shard/--of --out --seed --board --workers-tag` | Play this shard's pending slice, append rows, live progress. |
| `rate` | `--in --roster --board` | Load history, BT fit, write ratings.tsv + report.md, print table. |
| `run` | union | Serial play then rate. The everyday command. |
| `history` | `--agent <id or unique prefix> --last N` | Per-opponent aggregates + last N raw games. Ambiguous prefix lists candidates. |
| `gauntlet` | `--id <candidate> --games N --keep` | O(N) hill-climber path: play N games vs each active agent into `ranking/gauntlet.jsonl` (scratch, truncated per run), hold pool ratings FIXED (from ratings.tsv), solve the candidate's 1-D MLE by bisection, print `Elo +/- SE`. `--keep` appends to matches.jsonl instead and reminds to add the ID to the roster. |

Timing in reports uses serial rows (`par` 1) only, falling back to all rows with a `*` marker, so sharded runs do not pollute ms/move.

## Seed roster (curated, one axis varied at a time)

```
anchor  rand.v1                                        # UniformRandom, pinned at Elo 0
on      tiered.v1
on      smart(4).v1
on      greedy.classic(t1,c4,w0,l0).v1                 # 1-ply, registry defaults
on      greedy.classic(t2,c10,w3,l2).v1                # 1-ply, structure-weighted
on      ab(d2).classic(t1,c4,w0,l0).v1
on      ab(d4).classic(t1,c4,w0,l0).v1
on      ab(d6,nb200k).classic(t1,c4,w0,l0).v1
on      ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1   # feature delta vs previous line
on      ab(d4).exp(t1,c4,w0,l0,f2).v1                  # forward delta vs ab(d4)
on      ab(d4).classic(t1,c4,w0,l0).dil(r10).v1        # dilution delta vs ab(d4)
# off   policy.linpol(s1,00000000).v1                  # enable after training; rank.exe check prints the real hash
```
11 active agents = 55 pairs, 440 games at G=8 for the first full run.

## Tests (tests/test_ranking.cpp)

1. ID round-trip battery: all seed IDs plus full-feature `ab(d8,tt,ord,nb200k).exp(...).dil(r5).v1`, `ab(d3,noab,part,asp50,tb250ms,cap2).classic(...).v1`, `smart(4).dil(r2.5).v1`, negative weight, budget renderings `nb2m`/`nb1500`. Assert emit == input and decoded AgentSpec fields (including `randomMoveProb == 0.05`).
2. Learned IDs: temp model file in `build\`, hash, round-trip both learned heads, wrong hash rejected.
3. Non-canonical rejection: `smart(04).v1`, missing version, out-of-order weights, unknown flag/head, future dil args. Clean errors, no crash, canonical form in message where applicable.
4. Codec-table completeness vs `g_evaluators` (unique letters per evaluator).
5. Roster parse via istream overload: comments, CRLF, on/off/anchor, duplicate error, zero/two-anchor error.
6. Match-row format/parse round-trip, malformed line returns false.
7. Scheduler: 3 agents G=4 empty store -> 12 color-balanced games. Feed back as played, add 4th agent -> exactly 12 new games all involving it (the incremental guarantee). Rebalancing, determinism of pending list + seeds, shard slices partition.
8. BT fit synthetic: agents at true 0/200/400 Elo, sampled games with fixed srand -> fitted gaps within ~60 Elo, anchor exactly 0, bit-identical refits, undefeated agent finite with larger SE, draw-heavy pair near 0 gap.
9. Gauntlet 1-D fit: candidate scoring 0.5 vs a fixed 300-Elo pool fits ~300.

## Implementation phases

1. `src/ranking.h` + codec/roster half of ranking.cpp + tests 1-5 (front-loads the risky canonicalization logic, testable without playing a game).
2. Match store + scheduler + per-game seeds + tests 6-7.
3. Play loop with progress lines.
4. BT fit + Fisher SE + gauntlet 1-D fit + tests 8-9.
5. Reports (ratings.tsv, report.md, history view).
6. `tools/rank_main.cpp`, `build_rank.bat`, `tools/run_rank.ps1`, `ranking/roster.txt`, `build_tests.bat` edit.
7. Docs (README, CLAUDE.md, ML.md, todo.md, .gitignore) + verification + suggest commit messages (no committing, per standing instructions).

## Verification

1. `.\tools\run_rank.ps1 -Build` builds clean. `.\rank.exe check` validates the seed roster and prints pending count.
2. `.\tools\run_rank.ps1 run --games 4` end-to-end: watch progress lines, inspect report.md/ratings.tsv. Sanity: rand.v1 at 0 (anchor), Elo ordering ab(d4) > ab(d2) > greedy > tiered > rand, diluted agent below its twin, finite +/-, ms/move + nodes/move populated for search agents.
3. Add `on ab(d3).classic(t1,c4,w0,l0).v1` to roster, re-run: banner shows only (active-1) x 4 pending games, matches.jsonl grew by exactly those rows (the incremental guarantee, user-visible).
4. Toggle an agent off, `rate`: moves to inactive table, keeps rating.
5. `history --agent ab(d4` and `gauntlet --id ab(d5).classic(t1,c4,w0,l0).v1 --games 2` behave as specced (gauntlet leaves matches.jsonl untouched without `--keep`).
6. `.\tools\run_rank.ps1 play -Workers 4 --games 6` then rate: shard files merged + deleted, per-game seeds identical to a serial run.
7. `.\tools\run_tests.ps1 -Build` passes (104 existing + new assertions).
