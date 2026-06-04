# Breakthrough

A C++ console implementation of the Breakthrough board game by Zeph Johnson.

## Prerequisites

- Visual Studio Community with the **Desktop development with C++** workload installed

## Compiling

Run from the project root in any VS Code terminal (regular PowerShell works):

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl main.cpp board_io.cpp settings.cpp board_analysis.cpp moves.cpp ai_random.cpp ai_minimax.cpp /EHsc /Fe:breakthrough.exe'
```

This produces `breakthrough.exe` in the project root.

## Compile and Run

To recompile and immediately run the result:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cl main.cpp board_io.cpp settings.cpp board_analysis.cpp moves.cpp ai_random.cpp ai_minimax.cpp /EHsc /Fe:breakthrough.exe' ; if ($?) { .\breakthrough.exe }
```

## Source files

| File | Contents |
|---|---|
| `globals.h` | Shared macros, enums, extern globals, all forward declarations |
| `main.cpp` | Global variable definitions + `main()` game loop |
| `board_io.cpp` | `getBoard`, `reloadBoard`, `printBoard`, `loadMinimaxParams` |
| `settings.cpp` | `getSettings`, `printVictor` |
| `board_analysis.cpp` | `countChips`, `chipDiff`, `findWin*`, `canWin*` |
| `moves.cpp` | Move validation, execution, simulation, player input, move routing |
| `ai_random.cpp` | `evaluateBoard`, `playOpener*`, `pureRandom*`, `tieredRandom*`, `smartRandom*` |
| `ai_minimax.cpp` | `miniMax*`, `maxAlphaBeta`, `minAlphaBeta` |

## Running

Run from the project root (required so board file paths resolve correctly):

```powershell
.\breakthrough.exe
```

## Gameplay

The game runs interactively in the console. At startup you will be prompted to:

1. **Enter a board file** — e.g. `boards/board1.txt` through `boards/board5.txt`, or a puzzle like `boards/puzzle1.txt`
2. **Choose a player type** for White and Black:
   - `0` = Human
   - `1` = Uniform Random
   - `2` = Tiered Random
   - `3` = Smart Random
   - `4` = MiniMax (AI)
3. **Configure AI parameters** if applicable (search depth, weights, opener style)
4. **Choose number of games** to play and verbosity level (0 = silent, 1 = moves, 2 = full board)

### Human move format

Enter moves as `c1d` where:
- `c` = source column (letter)
- `1` = source row (number)
- `d` = destination column (letter)

Example: `d2e` moves the piece at column d, row 2 diagonally to column e.
