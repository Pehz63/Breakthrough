# CLAUDE.md - Breakthrough Project Reference

## Standing Instructions

- **Never run `git commit` or `git push`.** The developer makes all commits manually.
- **Writing style:** Avoid semicolons and em dashes. Use a comma or period instead, restructuring the sentence if needed. Avoid special Unicode characters like arrows or comparison signs. Use standard keyboard equivalents instead, such as `->` for a right-pointing arrow and `>=` for a greater-than-or-equal sign.
- **After every functional change:**
  1. Update `README.md` for any section affected by the change (build command, game rules, AI descriptions, etc.)
  2. Update this file (`CLAUDE.md`) to reflect new files, renamed functions, or changed behavior
  3. Tell the developer **how to test** the change and **what new behavior to expect**
  4. Suggest 2-3 **candidate commit messages** (message text only, not the full `git commit` command), then give a **top recommendation**
     - Before suggesting, check `git status` to see what is actually uncommitted. If this change will be bundled with other uncommitted work from the session, write the message to cover **all** of those changes together, not just the latest one
     - Use `Add` for files being committed for the first time, `Update` only if the file was already in a prior commit

---

## Project Overview

**Breakthrough** is an 8x8 abstract board game implemented in C++ with a console UI and multiple AI difficulty levels. White pieces start at rows 6-7 and move upward. Black pieces start at rows 0-1 and move downward. A player wins by advancing a piece to the opposite back row or capturing all opponent pieces.

- **Language:** C++ (C++11)
- **Compiler:** MSVC (`cl`), the primary build tool
- **Alternative build:** CMake (`CMakeLists.txt`), which is not the primary workflow
- **Entry point:** `.\breakthrough.exe` from project root

---

## Build & Run

### Build
```
cl src\main.cpp src\board_io.cpp src\settings.cpp src\board_analysis.cpp src\moves.cpp src\ai_eval.cpp src\ai_random.cpp src\ai_minimax.cpp /I src /EHsc /Fo"build\\" /Fe:breakthrough.exe
```

### Run
```
.\breakthrough.exe
```
When prompted for a board file, enter e.g. `boards\board1.txt`.

---

## File Structure

### Root
| File | Purpose |
|---|---|
| `README.md` | User-facing docs: build, run, game rules, move notation |
| `CLAUDE.md` | Claude reference and workflow instructions (this file) |
| `CMakeLists.txt` | Alternative CMake build (not primary) |
| `minimax_params.txt` | Saved MiniMax weights, loaded automatically when MiniMax player is selected |
| `.gitignore` | Excludes `.exe`, `.obj`, `build/` directory |

### `src/`
| File | Purpose |
|---|---|
| `globals.h` | Master header: macros (`EMPTY='.'`, `WHITE='W'`, `BLACK='B'`, `SIZE=8`), enums, `extern` globals, and all function prototypes. Included by every `.cpp`. |
| `main.cpp` | Top-level game loop: seeds RNG, loads board, calls `getSettings()`, dispatches turns, tracks per-player time, and accumulates scores over multi-game sets. It also hosts parameter test-sweep mode. |
| `board_io.cpp` / `board_io.h` | Board file I/O and display: `getBoard()`, `reloadBoard()`, `printBoard()`, `loadMinimaxParams()` |
| `settings.cpp` / `settings.h` | Interactive CLI configuration. `getSettings()` prompts for player types, AI weights, game count, and verbosity. `printVictor()` displays winner and timing. |
| `moves.cpp` / `moves.h` | All move logic: dispatch (`moveWhite`/`moveBlack`), human input parsing, full validation (`tryMoveWhite/Black`), fast AI validation (`tryMoveQuickWhite/Black`), execution (`playMoveWhite/Black`), reversible simulation (`simulateMove`/`unsimulateMove`) |
| `board_analysis.cpp` / `board_analysis.h` | Chip counting, row-level chip difference, one-step win detection: `findWinWhite/Black()`, `canWinWhite/Black()` |
| `ai_eval.cpp` / `ai_eval.h` | `evaluateBoard()`, the heuristic leaf-node score for minimax: near-end win detection (rows `SIZE-2` and `1`), turn bonus, wall/column structure bonuses, chip-diff base score. End-row win detection is handled before calling this function. |
| `ai_random.cpp` / `ai_random.h` | Three random AI strategies (`pureRandomMove`, `tieredRandomMove`, `smartRandomMove`) and opening-sequence logic (`playOpenerWhite/Black`) |
| `ai_minimax.cpp` / `ai_minimax.h` | Alpha-beta minimax: `miniMaxWhite/Black()` top-level search, `maxAlphaBeta`/`minAlphaBeta` recursive pruning, capture-first move ordering, win-decay for fastest wins |

### `boards/`
| Files | Purpose |
|---|---|
| `board1.txt` - `board5.txt` | Standard starting configurations |
| `puzzle1.txt` - `puzzle13.txt` | Mid-game tactical positions for testing AI |

---

## Key Global State

All globals are declared `extern` in `globals.h` and defined in `main.cpp`.

| Variable | Type | Meaning |
|---|---|---|
| `board[SIZE][SIZE]` | `char[8][8]` | Board grid indexed `[col][row]`. Values are `'W'`, `'B'`, `'.'`. |
| `g_whiteCount` | `int` | Live white piece count, updated on every capture |
| `g_blackCount` | `int` | Live black piece count, updated on every capture |
| `g_chipDiff` | `int` | `g_whiteCount - g_blackCount`, used directly in `evaluateBoard()` |
| `g_whiteAtEnd` | `int` | Count of White pieces currently on row `SIZE-1`. Updated by `simulateMove`/`unsimulateMove`. |
| `g_blackAtEnd` | `int` | Count of Black pieces currently on row `0`. Updated by `simulateMove`/`unsimulateMove`. |
| `nodesWhite` / `nodesBlack` | `int` | Minimax nodes visited this turn, printed for perf analysis |
| `PRNT` | `int` | Verbosity: `0`=silent, `1`=moves only, `2`=full board states |

---

## AI Player Types (`PlayerEnum`)

| Value | Name | Description |
|---|---|---|
| 0 | Human | Console input. Move format `c1d` (source col, source row, dest col). |
| 1 | UniformRandom | All legal moves equally likely |
| 2 | TieredRandom | Prioritizes winning moves first, then captures, then normal moves |
| 3 | SmartRandom | Like TieredRandom but restricts candidates to the furthest N pieces (`p1` parameter) |
| 4 | MiniMax | Alpha-beta search with configurable depth, chip/turn/wall/column weights, and opener |

### Opening Strategies (`OpenerEnum`)
| Value | Name | Description |
|---|---|---|
| 0 | StandardOpener | No opening sequence, plays normal moves immediately |
| 1 | OffensiveOpener | Edge pieces attack diagonally, center pieces push forward |
| 2 | DefensiveOpener | Corner-focused, protects corner pieces first |

Openers are disabled automatically once the opponent advances into the player's half of the board.

---

## Architecture Notes

- **Simulate/unsimulate pattern:** `simulateMoveWhite/Black` and `unsimulateMoveWhite/Black` apply and reverse moves in-place so minimax avoids copying the board.
- **Two validation tiers:** `tryMoveWhite/Black` gives full validation with user-readable error messages for human input. `tryMoveQuickWhite/Black` skips bounds checks for AI inner loop performance.
- **Capture-first move ordering:** In minimax, moves for each piece are tried in strict priority order: actual captures (diagonal to an enemy piece), then empty diagonal advances, then the forward move. This ordering maximizes alpha-beta cutoffs. End-row win detection (`canWinWhite`/`canWinBlack`) uses `g_whiteAtEnd`/`g_blackAtEnd` for O(1) checks instead of scanning a row.
- **Win decay:** When minimax finds a forced win, the score decreases by 1 per level, incentivizing the fastest possible victory path.
- **`minimax_params.txt`:** Key-value config file (`key=value`, `#` comments) for persisting preferred AI weights between sessions.

---

## Verification Checklist

Use this after any change to confirm nothing is broken:

1. Build succeeds with the `cl` command above (no errors or warnings introduced)
2. `.\breakthrough.exe` launches and shows the settings prompts
3. Enter `boards\board1.txt` when asked for a board file. Confirm the board displays correctly.
4. Run a quick Human vs. UniformRandom game (a few moves) to confirm basic flow
5. **For AI changes:** run MiniMax (depth 3) vs. MiniMax (depth 3) with `PRNT=1`. Confirm `nodesWhite`/`nodesBlack` stats print and the game completes.
6. **For eval/weight changes:** compare win rates over a 10-game batch before and after
