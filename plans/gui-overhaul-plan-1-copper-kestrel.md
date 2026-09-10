# GUI overhaul: agent selection, recommended-move arrows, a non-blocking engine, and a web page

## Context

Developer goals for this session (2026-09-10), in their priority order:

1. Overhaul the GUI to give full control over selecting agents, evaluators, and
   their settings.
2. Recommended-move arrows.
3. No GUI element may block on background work (an agent's move, board
   evaluation). Earlier versions froze while the engine computed.
4. A web-hostable version that is much simpler: play against a strong agent at
   three difficulties (Hard = the strongest agent, easier levels via a random
   opener or dilution so games vary and are reasonable for a human), or watch
   two top agents.

Constraints: other sessions are training agents and editing ranking code in the
same working tree, so touch `src/` only where unavoidable. Never open a visible
GUI window for testing (the developer may be in a full-screen game). Taste
decisions are the developer's: pick defaults, build them, and collect the
questions for the end.

## Design

### Engine service (`gui/gui_engine.h/.cpp`)

The engine's state is process globals (board, counts, accumulators, TT, killers,
model slots), so only one thread may touch it. One engine service thread owns it.
The UI thread keeps the game position as a `GuiPos` and uses pure rule helpers
(`guiIsLegal`, `guiApplyMove`, `guiWinner`) that read only that struct.

Jobs carry a position copy:
- Move job: mirror `src/ranking.cpp`'s `playOneGame` dispatch (opener first, else
  `agentChooseMove`, clear the root filter), recover the move by board diff,
  report the agent's static eval, predicted eval, nodes, effective depth.
  `engNewGame` clears the TT and retain purses as `playOneGame` does per game.
- Analysis job: iterative deepening where each step scores one root move exactly
  (apply it, `miniMaxWhite/Black(d-1)` from the opponent side). Every root move
  gets a real score at each depth. Salt the TT context through the unused last
  evaluator parameter so analysis entries never collide with an agent's.
- Priority: a move request aborts the running analysis step by forcing
  `g_nodeDeadline = 1` under the lock while that step is running. TT stores are
  already skipped once the budget trips.
- Web (no pthreads): same API, work runs inline from `engTick()`, a move job in
  one frame, analysis in ~10 ms slices.

### Agent library (`gui/gui_library.h/.cpp`)

Sources of canonical ids: `ranking/standings.tsv`, `ranking/CHAMPION.md`'s summary
table, a new `gui/presets.txt` (roles easy / medium / hard / watch_white /
watch_black), favorites, and a recent-id history. A background model-catalog scan
over every slot file. GUI settings in a key=value file.

### Front end (`gui/main_gui.cpp`, rewritten)

- Panel tabs: Play (side cards with Human/Agent, name, Elo, Library / Edit / Save,
  Swap, board, New Game / Undo, pacing, status, move log), Analysis (arrow count,
  reply arrow, labels, eval bar, evaluator and model, copy a player's evaluator,
  depth and time limits, TT / ordering / quiescence, line list), View (piece and
  board themes, flip, coordinates, last move, legal-move dots, readouts, slider
  style, simple mode).
- Modals: agent library, agent editor (canonical id box + structured fields),
  model picker, save favorite.
- Board: arrows by rank color and width with score labels, dashed reply arrow,
  eval bar, last-move highlight, drag and click moves, turn text in the top bar.
- Simple mode (the web page's only mode): Play White / Play Black / Watch, Easy /
  Medium / Hard, hints toggle, New Game / Undo.
- Capture mode: `--capture <png>` renders in a hidden window and saves a frame,
  with `--scenario` and `--moves` to reach any state without code edits.

### Presets (all on `ab(deep=6,tt,ord,nodes=200k)@3`, Elo from the 2026-09-06 standings fit)

| Role | Canonical id | Why |
|---|---|---|
| easy | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.dil(prob=20)@1` | dil20 x node champion (660): 20% random moves |
| medium | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=10,fead67b7,weight_merge,lin,shape=129-1)@1.opener(rand,moves=8)@1` | 883: random first 8 moves, then full strength |
| hard | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` | openless x node champion (1227) |
| watch_white | same as medium | varied games |
| watch_black | `ab(deep=6,tt,ord,nodes=200k)@3.learned(model=76,ef183148,position_elo,lin,mu_shape=129-1,sigma_shape=129-1)@1.opener(rand,moves=8)@1` | opener8 x node champion (880) |

### Builds and tooling

- `build_gui.bat` and `build_web.bat` link the two new GUI files. The web build
  links `src/ranking.cpp` (it compiles under Emscripten), outputs to `build\web\`
  (a `docs\` output collides with the tracked `Docs\` on Windows), and bundles the
  preset model files.
- `tools/smoke_test_gui.ps1` defaults to hidden capture. New `tools/gui_shot.ps1`.

### Hosting (decided by the developer at the end of the session)

GitHub Pages through GitHub Actions: a workflow builds the page on an Ubuntu
runner with a Linux twin of `build_web.bat` and deploys `build/web`. The preset
model files the page bundles must reach the runner byte-identical to the files
their ids hash.

## Verification

- Hidden-capture screenshots of every scenario, read by eye.
- Test suite (only if `src/` changes).
- Web: serve `build\web` locally and screenshot with headless Chrome.
