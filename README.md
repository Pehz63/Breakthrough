# Breakthrough

A C++ implementation of the Breakthrough board game by Zeph Johnson, with a
console interface and a graphical interface (raylib + raygui) that also compiles
to the web via WebAssembly.

## Prerequisites

- Visual Studio Community with the **Desktop development with C++** workload installed
- For the graphical interface: **raylib** (and, for the web build, the Emscripten
  SDK). See [INSTALL.md](INSTALL.md) for exact download/setup steps.

## Compiling

Run from the project root in any VS Code terminal (regular PowerShell works):

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\datastore.cpp src\transposition.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe'
```

This produces `breakthrough.exe` in the project root. Intermediate `.obj` files go into `build/`.
(The `ml_*` files provide the learned `LearnedValue` evaluator; the engine links them
even when you do not select a learned evaluator.)

## Compile and Run

To recompile and immediately run the result:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl src\main.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\datastore.cpp src\transposition.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe' ; if ($?) { .\breakthrough.exe }
```

## Testing

Build and run the unit and integration test suite (uses [Catch2 v2](https://github.com/catchorg/Catch2/tree/v2.x), header already included in `tests/`):

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl tests\test_main.cpp tests\test_move_validation.cpp tests\test_win_detection.cpp tests\test_eval.cpp tests\test_ai_integration.cpp tests\test_game_outcomes.cpp tests\test_ml.cpp tests\test_ranking.cpp src\globals.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp src\ml_features.cpp src\ml_model.cpp src\ml_eval.cpp src\explorers.cpp src\choosers.cpp src\agents.cpp src\datastore.cpp src\transposition.cpp src\ml_train.cpp src\ranking.cpp /I src /I tests /EHsc /Fo"build\\" /Fe:tests.exe'
```

The preferred one-liner is `.\tools\run_tests.ps1 -Build` (see [CLAUDE.md](CLAUDE.md)).

Then run from the project root (required so puzzle board paths resolve correctly):

```powershell
.\tests.exe
```

For the GUI, the smoke test (`.\tools\smoke_test_gui.ps1 -Build`) and the targeted
screenshot helper (`.\tools\gui_capture.ps1`) are the main tools. See
[TESTING.md](TESTING.md) for the full verification playbook, including what to look
for visually and how to capture matchup-gated controls.

## Source files

| File | Contents |
|---|---|
| `globals.h` | Shared macros, enums, extern globals, all forward declarations |
| `globals.cpp` | Global variable definitions (board, counters, etc.) |
| `main.cpp` | `main()` game loop |
| `board_io.cpp` | `getBoard`, `reloadBoard`, `printBoard`, `loadMinimaxParams` |
| `settings.cpp` | `getSettings`, `printVictor` |
| `board_analysis.cpp` | `countChips`, `chipDiff`, `findWin*`, `canWin*` |
| `moves.cpp` | Move validation, execution, simulation, player input, move routing |
| `ai_eval.cpp` | Evaluator registry (`g_evaluators`) + each evaluator's scoring function, and `evaluateBoard`, the board heuristic used by minimax and the move dispatcher |
| `ai_random.cpp` | `playOpener*`, `pureRandom*`, `tieredRandom*`, `smartRandom*` |
| `ai_minimax.cpp` | `miniMax*`, `maxAlphaBeta`, `minAlphaBeta`, iterative deepening + node/time budgets, opt-in transposition/ordering/aspiration/quiescence |
| `ml_features.cpp` | Board (value) + move (policy) feature extraction and legal-move generation; value features v1 (dense aggregates) and v2 (sparse piece-square) |
| `ml_model.cpp` | `Model` base + `LinearModel`, model-type registry, save/load (`type=` format) |
| `ml_eval.cpp` | Model slots, `mlValueScore` (LearnedValue), `mlRateMoves` (policy), incremental v2 accumulator (`mlIncrementalBegin`/`mlLeafScore`) |
| `explorers.cpp` | Move-tree explorer registry (`Greedy`, `AlphaBeta`) |
| `choosers.cpp` | Direct move-chooser registry (random family + `LearnedPolicy`) |
| `agents.cpp` | `AgentSpec` composition + `agentChooseMove` (search/policy + dilution) |
| `datastore.cpp` | Append-only JSONL writer + canonical position keys (also the TT hash) |
| `transposition.cpp` | Opt-in transposition table (`g_useTT`): probe/store + best-move ordering hint |
| `ml_train.cpp` | Training regimes, Elo, tournaments, checkpoints, manifest + doc export |
| `ranking.cpp` | Persistent agent Elo ranking: canonical agent-ID codec, roster file, append-only match store, incremental scheduler, anchored Bradley-Terry fit, reports |
| `tools/train_main.cpp` | `train.exe` CLI front end |
| `tools/rank_main.cpp` | `rank.exe` CLI front end |
| `gui/main_gui.cpp` | raylib + raygui front end: window, per-frame state machine, board rendering, click-to-move, widget panel, move log |
| `gui/raygui.h` | Vendored single-header raygui widget library |
| `gui/shell.html` | Emscripten HTML shell for the web build |

## Running

Run from the project root (required so board file paths resolve correctly):

```powershell
.\breakthrough.exe
```

## Graphical interface (GUI)

The GUI is a front end built with [raylib](https://www.raylib.com/) and
[raygui](https://github.com/raysan5/raygui). It uses the same C++ game logic and AI
as the console app. The same source builds a native Windows window and a
WebAssembly version for the web.

First-time setup (download raylib, plus Emscripten for the web build) is described
in [INSTALL.md](INSTALL.md).

### Native desktop build

```powershell
.\build_gui.bat
.\breakthrough_gui.exe
```

`build_gui.bat` finds Visual Studio automatically, so it runs from a plain
terminal. It produces `breakthrough_gui.exe` in the project root.

To rebuild and run in one line (only runs if the build succeeds):

```powershell
.\build_gui.bat; if ($?) { .\breakthrough_gui.exe }
```

If a previous window is still open it will lock the executable and block the
rebuild; close it first, or kill it inline:

```powershell
Get-Process breakthrough_gui -EA 0 | Stop-Process -Force; .\build_gui.bat; if ($?) { .\breakthrough_gui.exe }
```

### Web build (GitHub Pages)

```powershell
.\build_web.bat            # release -> docs\index.html (+ .wasm/.js/.data)
python -m http.server -d docs
```

Commit `docs\` and enable GitHub Pages on the `/docs` folder to host it. See
[INSTALL.md](INSTALL.md) for details.

### Using the GUI

- The window is **resizable** and the board **scales to fill it** (kept square and
  centered).
- The options live in a narrow panel on the **left**. The board sits **beside** it
  (it is not covered). Toggle the panel with the **Options** / **Hide** button in
  the top-left, or the **Tab** key. Hiding it lets the board grow to fill the
  window.
- Pick a player type for **White** and **Black** from the dropdowns
  (Human / Uniform Random / Tiered Random / Smart Random / MiniMax), choose each
  side's **opener** from a dropdown (Standard / Offensive / Defensive), and adjust
  **Smart Random** / **MiniMax** parameters with the controls that appear for those
  types. The default matchup is **Human (White) vs MiniMax (Black)**.
- **Evaluator (MiniMax):** an **Eval** dropdown picks which board-state evaluator
  the MiniMax search uses (e.g. Classic / Experimental). Each evaluator defines its
  own set of weights, and the parameter sliders below the dropdown change to match
  the selected evaluator. Switching evaluator resets that side's weights to the new
  evaluator's defaults.
- **Slider designs:** the numeric parameters use prototype controls that each show
  both a bar and the number and step with a "+" (up) above a "-" (down) button.
  Each row demonstrates a different design (Bar+number, Segments, Number+bar,
  Handle, Ruler) so you can compare them, and you can also click or drag a bar to
  set its value directly. The **Sliders** switcher at the top of the panel forces
  one design across all rows ("Per-row" restores the mixed view).
- **MiniMax depth:** the Depth control (the Number+bar design) lets you type an
  exact depth, step it with +/-, or drag its bar (which tops out at 25, though the
  typed/stepped value can go higher). Large depths get very slow. The default depth
  is 8.
- The game starts automatically when the GUI opens using the default board and
  matchup. To change the board file, type a new path in the **Board** box and press
  **New Game** to apply it.
- **Changing settings mid-game:** if you adjust any player option while a game is in
  progress, a "Settings changed." notice appears above the **New Game** button.
  Press **New Game** to restart with the new settings.
- **Piece counts** are shown on the board itself as small badges (a piece icon plus
  the count) on each side, so they stay visible even with the options panel hidden.
- **Board-state evaluation** is shown under each side's count badge. `now` is the
  immediate static evaluation of the position that side faced; for a **MiniMax**
  side a second line `pred` shows the AI's predicted best-line ("downstream")
  evaluation. Numbers are white-centric: a positive value favors White, and a
  forced win shows as `+WIN` / `-WIN`. Turn off the readouts with the **Show
  evaluations** checkbox or the **E** key (useful for a hint-free PvP / PvC game).
- **Human moves:** click one of your pieces to select it (legal destinations are
  highlighted), then click the destination square one row forward.
- **Pacing** adapts to the matchup:
  - **Human vs a strong AI** (MiniMax depth > 5): no pacing controls, the AI's own
    search sets the pace.
  - **Human vs a fast AI** (shallow MiniMax or a random AI): a **Min 2s per AI
    move** checkbox so the AI does not snap back instantly.
  - **AI vs AI:** the full set, slow-motion (`|>`) / fast-forward (`>>`) buttons
    that step the speed presets (Step / 0.25x / 1x / 4x / Instant) shown between
    them, plus icon buttons for **play/pause**, **step**, and **restart**.
- The **Move Log** scrolls through the move history.

## Gameplay

The game runs interactively in the console. At startup you will be prompted to:

1. **Enter a board file**, e.g. `boards/board1.txt` through `boards/board5.txt`, or a puzzle like `boards/puzzle1.txt`
2. **Choose a player type** for White and Black:
   - `0` = Human
   - `1` = Uniform Random
   - `2` = Tiered Random
   - `3` = Smart Random
   - `4` = MiniMax (AI)
3. **Configure AI parameters** if applicable. For MiniMax this includes the search
   depth, the **evaluator** to use (Classic / Experimental / ...), that evaluator's
   weights (prompted one at a time by name), and the opener style.
4. **Choose number of games** to play and verbosity level (0 = silent, 1 = moves, 2 = full board)
5. **Show board evaluations?** (0 = no, 1 = yes). When enabled, after each move a
   line prints that side's `now` (immediate static eval) and, for a MiniMax side,
   its `pred` (predicted best-line eval). Values are white-centric (positive favors
   White); forced wins print as `+WIN` / `-WIN`.

### Board-state evaluators

The MiniMax AI scores leaf positions with a selectable **evaluator**. Each evaluator
is one entry in the `g_evaluators` registry in `src/ai_eval.cpp`: a name, a list of
parameters (display name, save-file key, default, and min/max), and a scoring
function. To add or change an evaluator you edit that one table entry and its
function body. The new evaluator and its parameters then appear automatically in
both the console prompts and the GUI's Eval dropdown / sliders, and can be saved per
side in `minimax_params.txt` via `<side>_eval` plus each weight's `<side>_<key>`.

Four evaluators ship:

- **Classic**: turn, chip (material), wall, and column weights. The original
  heuristic and the reigning ranked champion's evaluator.
- **Experimental**: Classic plus a forward weight (rewarding advancement).
  Identical to Classic when forward is 0.
- **Advanced**: Experimental plus eleven more terms. Weights may be negative.
  - *Support*: diagonal same-color pairs (the diagonal-behind piece is what
    recaptures in this game).
  - *Center*: advancement scaled by file centrality (a center piece keeps two
    escape diagonals).
  - *Mobility*: legal-move count per piece.
  - *Hole*: back-rank columns whose two guard squares are empty, admitting an
    unstoppable enemy outpost.
  - *Control*: occupancy of the two rows nearest the opponent's home row.
  - *Open*: files with no own piece on the own half while the enemy occupies
    the file.
  - *Race*: closest-piece distance differential (who wins a pure race).
  - *Overext*: advanced pieces with no diagonal-behind defender.
  - *Noise*: the sign selects one of two seeded-random forms (NoiseSeed = the
    seed, both deterministic per seed). Noise > 0 is a random piece-square
    table: each (color, square) gets a fixed value in [-n, +n], so the board
    total scales with piece count and acts as a persistent square bias -- a
    strength weight, and a bad one at material scale. Noise < 0 is a bounded
    per-position jitter of magnitude -n: the leaf score becomes
    realEval * 256 + jitter, so the jitter can never reverse a strict
    preference between two positions -- it only reorders exact evaluation
    ties, re-rolling pseudo-randomly every move. Use the jitter form for
    deterministic play diversity; note a jittered agent's displayed
    evaluations are 256x the native scale.
  - *RaceWin* (0/1): an exact decided-race detector. When a side has a passed
    runner that provably wins the race under the game's rules, the leaf returns
    a win score immediately, deciding races many plies before search could.
- **LearnedValue**: delegates to a trained value model (see the ML section).

MiniMax evaluates the board incrementally: a move changes only two squares, so each
score term (material, wall, column, advance, and the Advanced terms) updates just
those squares and their neighbors instead of rescanning all 64 at every leaf. Terms
with zero weight are skipped entirely. It returns the same score as a full
recompute, so it affects speed only, not the moves played.

### Machine learning system

Beyond the hand-written evaluators, the project has an ML system whose
interchangeable parts combine into agents. Agents can play tournaments, be
Elo-rated, and generate training data. The four
pluggable axes are board-state **evaluators** (value), move-tree **explorers**
(search), move **choosers/policies** (direct, no-lookahead, including a learned
**move-rater**), and the **models** behind the learned parts (linear now;
MLP/NNUE/transformer registered for later). A `LearnedValue` evaluator appears in
the console prompts and GUI Eval dropdown automatically once a model is trained.

Inference is pure C++ with no runtime dependencies. Model training and analysis use
optional Python. Build and use the trainer:

```powershell
.\tools\run_train.ps1 -Build selfplay-supervised --games 250 --epochs 6   # linear value model
.\tools\run_train.ps1 imitate --out models/lin_policy.txt --games 150      # linear policy move-rater
.\tools\run_train.ps1 docs                                                 # regenerate ML.md tables
```

Value models come in two feature layouts. Version 1 is 30 dense board aggregates
(material, rank counts, structure counts, mobility): informative, but every leaf of a
search recomputes all of them, including two full move generations. Version 2
(`--feature-version 2`) is a sparse piece-square layout: one binary input per (color,
square) plus side to move. A linear model over it is a piece-square table. Because a
move changes only 2 or 3 of those inputs, the engine keeps the model's output in a
running accumulator during search, updated by a few weight additions per make/unmake
instead of a rescan, the same pattern the heuristic evaluators use for their
positional term. This drops the learned leaf's cost per node by roughly 9x (measured
with `train.exe speed`, which benchmarks both layouts side by side). The same route
now also carries NNUE-style MLP value heads: the scalar accumulator is widened to a
vector for the first hidden layer, maintained across make/unmake, with only the
remaining layers run per leaf (measured ~1.78x per node for a 256/128 head). Train a
linear one with:

```powershell
.\tools\run_train.ps1 selfplay-supervised --out models/pst_value --feature-version 2 --games 250 --epochs 6
```

The trainer's data source and schedule are configurable: `--gen-random-floor` +
`--gen-random-decay-plies` decay the teacher's random-move dilution over the game
(explore early, play late moves for real), `--gen-model` makes a previously-trained
model the self-play generator, `--l2` adds weight decay, and `--from-data` fits on
positions replayed from the real ranked match history by `rank.exe extract`
(`rank.exe extract --out data/replay_v2.jsonl --feature-version 2 --sample 3000`).
`tools/sweep_pst_v2.ps1` combines all of it into a training-hyperparameter sweep
whose candidates are rated together in one `rank.exe run`. The first 78-candidate
study's findings are in `plans/training-sweep-results-1-luminous-snail.md`: seed
noise dominates, more data helps, teacher depth does not, dilution decay is the
best default, and replay extraction beats bespoke single-teacher self-play (the
follow-up scaling study `tools/train_scaling.ps1` measured ~+250 Elo from
replay-training on the full 46k-game match history; the best model reached
d6 gauntlet Elo 920 and is the current `models/pst_value.txt`).

**Rate everything in a depth-laddered round-robin** (process-sharded across all CPUs,
since the engine's state is global and can't share a process safely):

```powershell
.\tools\run_tournament.ps1 -Workers 12 -Depths "2,4,6,8,10" -Games 10 -NodeBudget 500000
```

This pits every agent (the random/heuristic family, learned policy, and Greedy +
AlphaBeta over several evaluator weight presets at each depth) against each other and
prints an `Elo | ms/move | max ms | games | agent` table. A per-move **node budget**
with iterative deepening bounds deep searches, so depths up to 10 stay tractable.
Strength rises with depth, then plateaus once the budget binds. The
top agent is saved to `agents/champion.txt` and `agents/champion_params.txt` (a
`minimax_params.txt` block you can drop in to play the champion in the console/GUI).

**Restrict the field and archive the run.** Add `-Only "name1,name2,..."` to run just a
subset (e.g. the strongest few plus the learned policy); include their depths in
`-Depths` so the names exist. A subset run leaves the full-roster `agents/library.txt` /
`champion*.txt` snapshot untouched. Every run is archived, timestamped, under
`runs/<id>/` (`config.json`, `elo.tsv`, `notes.md`, plus a results copy), summarized in
`runs/index.jsonl`, and folded into the always-complete agent registry
(`agents/registry.{jsonl,md}`). Pass `-Note "..."` to record why a run was made, and
`train.exe run-note --run <id> --note "..."` to attach a realization later (e.g. a
throttled CPU). See **[ML.md](ML.md)** for the full archive layout.

```powershell
.\tools\run_tournament.ps1 -Depths "4,6,8,10" -NodeBudget 1000000 `
  -Only "AB6-Classic-chip,AB8-Classic-chip,AB10-Classic-chip,LearnedPolicy" `
  -Note "strongest few + learned policy at 1M budget"
```

Trained models, the Elo-rated agent library, and an append-only JSONL datastore are
written under `models/`, `agents/`, and `data/`; the optional `analysis/analyze.py`
(DuckDB) answers questions like the highest-Elo agent, the most fairly-matched
positions, and the average evaluation of a board state. See **[ML.md](ML.md)** for
the full design and the "how to add more" workflow.

### Agent Elo ranking (rank.exe)

Unlike the one-off tournament runner above, `rank.exe` maintains a persistent,
incremental ladder: every game is appended to `ranking/matches.jsonl` and never
overwritten, keyed by a canonical human-readable **agent ID** that
encodes the whole agent, for example:

```
rand@1                                             uniform random (the Elo-0 anchor)
smart(4)@1                                         SmartRandom over the furthest 4 pieces
ab(d6,tt,ord,nb200k)@1.classic(t2,c10,w3,l2)@2     alpha-beta d6 + TT + ordering + node budget,
                                                   Classic eval with those weights
ab(d4)@1.classic(t1,c4,w0,l0)@2.dil(r10)@1         10% random-move dilution
ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.dil(r30,d3)@1   30% depth-3 dilution of a d6
greedy@1.learned(s0,7cc8a70d)@1                    learned value model (content-hashed)
```

The head names the move choice / search, then the evaluator with all its weights,
then optional `dil()` dilution. Search heads take opt-in flags: `tt`
(transposition table), `ord` (move ordering), `qs` (quiescence: a captures-only
stand-pat extension at depth leaves), `aspN` (aspiration window), `noab` (full
minimax), plus `nb`/`tb` budgets. Dilution weakens an agent to spread the Elo ladder:
`dil(rP)` plays a fully random move `P`% of the time, while `dil(rP,dN)` instead
plays a shallower depth-`N` search `P`% of the time (a plausible-but-weaker move
rather than a blunder, `0 < N <` the agent depth). Every module segment carries its own `@N` **code
version**, a constant in the codec tables in `src/ranking.cpp`. When a module's
behavior changes, bump its constant. Only agents using that module get new
identities and fresh history, and the rest are unaffected. Learned segments embed a model-file
content hash instead, so a retrain is automatically a new identity. IDs are
canonical: `rank.exe check` rejects a stale version or non-canonical spelling
and prints the exact form to paste.

Agents live in the hand-edited `ranking/roster.txt` (`anchor|on|off <id>` per
line). The scheduler only plays each active pair's **missing** games, so adding
one agent to an N-agent pool costs N pairings, and nothing is ever recomputed.
Ratings are a deterministic **Bradley-Terry maximum-likelihood refit** of the
full store, anchored so `rand@1` = Elo 0 (an agent rates negative only when it is
worse than uniform random), with a `+/-` standard error per agent.

```powershell
.\tools\run_rank.ps1 -Build check        # validate the roster, print model hashes
.\tools\run_rank.ps1 run --games 8       # play pending games (live progress), then rate
.\tools\run_rank.ps1 -Workers 8 --games 8   # same, process-sharded across 8 workers
.\rank.exe history --agent "ab(d4"       # one agent's record vs every opponent
.\rank.exe gauntlet --id "ab(d5)@1.classic(t1,c4,w0,l0)@2" --games 4
```

Each game records wall time per side, **process CPU time** per side (via
GetProcessTimes deltas, so it stays honest under parallel contention), end piece
counts, ply count, node totals, and effective search depth. `rate` writes three
files:

- `ranking/ratings.tsv`: machine-readable ratings, usable as a label file for training.
- `ranking/games.tsv`: one row per stored game, for pandas/DuckDB.
- `ranking/report.md`: the ranked table (W-L split by color, average plies,
  end-piece margin, cpu/move, and `eff` = Elo / log2(1 + cpu_us/move), the Elo
  bought per doubling of per-move compute), plus a **compute-efficiency table**
  marking the Elo-vs-CPU pareto frontier, a head-to-head matrix, and per-agent
  match history with actual vs expected scores.

`gauntlet` rates one candidate against the frozen pool in O(N) games without
touching the store (add `--keep` to persist them). This is the cheap evaluation
step for weight hill-climbing along the pareto frontier.

`pairgen` plays fresh games between any two canonical agent IDs and captures
labeled value-model training data in the same format `extract` writes and
`train.exe --from-data` reads, plus a `.meta.json` sidecar recording the full
generation recipe. Knobs: `--dil-apply a|b|both|none` overrides one side's
random-move dilution (`--dil-start`/`--dil-floor`/`--dil-decay-plies`) so a
deterministic agent's games vary without changing its identity, `--open-plies K`
plays a uniform-random opening (`--open-side a|b|both` chooses which agent plays
it, so an asymmetric opener handicaps only one side; default `both`),
`--filter winner=a|b` keeps only one agent's wins, and `--branch-tries T` rewinds
each kept win to a random ply, substitutes a different legal move, and keeps the
tail if the winner wins again (mined alternative winning lines). `--shard`/`--of`
split generation across processes. First use: the vs-champion training study
(`tools/train_vs_champion.ps1`), which trains value models on champion-sourced
data (learner vs champion, diluted champion vs itself, a deeper oracle vs
champion, champion-loss cherry-picks, branch-mined wins) and compares them
against the replay and self-play baselines.

```
.\rank.exe pairgen --a "ab(d2)@1.classic(t1,c4,w0,l0)@2" --b "ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2" --games 200 --dil-apply a --out data/pg.jsonl
```

`bookgen` mines an opening/refutation book from stored games: it replays every
match between `--a` and `--b`, keeps the positions and moves `--a` played in its
wins (first `--plies` half-moves), and writes them to `models/book<N>.txt` keyed
by canonical position hash. An agent plays the book via the `book` opener ID
segment (`.opener(book,<N>)@1`): the stored reply while the position is in book,
its own brain otherwise. Book files are not content-hashed into the agent ID, so
treat a book slot as immutable and give a regenerated book a new slot number.

**Opener-bias study (Theory 6).** The vs-champion head-to-heads used a symmetric
random opener (`--open-plies 6` on both sides), which forces the deterministic
champion to play random opening moves it would never choose. `--open-side` and the
`rank.exe opener-bias` subcommand test whether that inflated the "beats the
champion" results. `tools/opener_bias_study.ps1` replays the head-to-heads under
symmetric vs asymmetric openers, and `tools/opener_bias_retrain.ps1` retrains the
oracle arm on asymmetric-opener data. See
`plans/opener-bias-results-1-synchronous-stearns.md`.

**Hill-climbing eval weights.** `tools/hill_climb.ps1` searches the Advanced
evaluator's weight mix for the highest Elo at a fixed search depth, using
`gauntlet` as its fitness function. It climbs the 13 strength weights (chip, wall,
column, forward, support, center, mobility, hole, control, open, race, overext,
noise). Turn is pinned at 20, the noise seed and the RaceWin detector are pinned
(they are not mix weights), and the 13 weights are renormalized so their absolute
values sum to 80 (total 100), so the search varies the relative mix rather than the
scale (the evaluator is scale-invariant for move selection) and scalar-duplicate
candidates dedupe. Each step mutates the best-so-far, with an occasional drastic
reset of the chip weight, and accepts on an Elo gain. `-AllowNegative` lets weights
go negative (sign-flip mutations, signed resets), which is the only way to reach
mixes like a negative chip weight. It plays against a fast, mostly-stochastic pool
(`ranking/climb_roster.txt`), so a candidate earns a smooth win rate instead of
replaying one deterministic line against a deterministic opponent. `-Promote`
appends the top finds to `ranking/roster.txt` and runs a full refit so they are
rated on the shared scale.

```powershell
.\tools\hill_climb.ps1 -Build -Iters 4 -Games 2 -Depth 2   # quick smoke
.\tools\hill_climb.ps1 -Iters 60 -Games 4                  # real climb at d4
.\tools\hill_climb.ps1 -Iters 60 -Games 4 -AllowNegative   # signed-weight climb
.\tools\hill_climb.ps1 -Iters 20 -Promote -PromoteTop 2    # then rank the winners
```

To keep the top of the ladder well-resolved (and to supply the climber with
non-deterministic opponents), `ranking/roster.txt` also carries a dense ladder of
diluted variants of the strongest pruned d6 agent, both random-move (`dil(rP)`) and
stochastic-depth (`dil(rP,d3)` / `dil(rP,d4)`).

### Human move format

Enter moves as `c1d` where:
- `c` = source column (letter)
- `1` = source row (number)
- `d` = destination column (letter)

Example: `d2e` moves the piece at column d, row 2 diagonally to column e.
