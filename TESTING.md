# TESTING.md - How to verify changes

This is the verification playbook for the Breakthrough project: the commands to run
after a change, and the hard-won lessons that are easy to miss. The condensed
checklist also lives in [CLAUDE.md](CLAUDE.md); this file is the long form.

All commands are run from the **project root** (`.\breakthrough.exe`,
`.\tests.exe`, and puzzle board paths all assume it).

---

## Console / engine changes (`src/`)

1. **Build clean.** Use the `cl` command from [README.md](README.md) (or
   [CLAUDE.md](CLAUDE.md)). No new errors or warnings.
2. **Run the test suite.** Build and run `tests.exe`, expect all assertions to pass
   (63 at last count). Run it from the project root so the puzzle boards resolve:
   ```
   .\tests.exe
   ```
3. **Launch and smoke-play.** Start `.\breakthrough.exe`, enter `boards\board1.txt`,
   confirm the board prints correctly, and play a few moves of Human vs.
   UniformRandom to confirm basic flow.
4. **For AI changes:** run MiniMax (depth 3) vs. MiniMax (depth 3) with `PRNT=1`.
   Confirm `nodesWhite` / `nodesBlack` stats print and the game completes.
5. **For eval / weight changes:** compare win rates over a 10-game batch before and
   after, not just a single game (one game is noise).

### Driving the console non-interactively (do it this way)

`breakthrough.exe` reads every setting with `cin`, so it **hangs forever** the
moment a prompt gets input it cannot parse. The right way to script it is to pipe a
newline-separated answer sequence through the **Bash tool** (raw bytes):

```bash
printf '1\n2\n0\n4\n0\n3\n0\n0\n1\n0\n0\n0\n1\n0\n1\n1\n' | ./breakthrough.exe 2>&1 | grep -E "eval:|has won"
```

Do **not** pipe input from PowerShell (`"...`n..." | .\breakthrough.exe` or
`Get-Content in.txt | .\breakthrough.exe`). Windows PowerShell encodes the stream
to the native exe with an encoding/BOM that corrupts the **first** `cin >>` read.
The first integer read fails, `useDefault` stays 0, the program drops into
`getBoard()`, every later answer is then misread as a bad filename, and it spins
printing "Invalid file..." until it has emitted millions of lines. If you see that,
the cause is the PowerShell pipe, not the game.

Tips for building the answer sequence:
- Answer `1` to "Use default board?" to skip the `getBoard()` filename prompt
  entirely (it is the easiest thing to get wrong).
- Picking **MiniMax** triggers a "Use these? (1=yes,0=no)" prompt because
  `minimax_params.txt` exists. Answer `0` and set a small depth (e.g. 3) so the
  game finishes in seconds. The saved depth in that file is large (slow).
- The trailing answers are game count, testing (0), `PRNT`, then `SHOW_EVAL`.
- Kill a stuck run with `Stop-Process -Name breakthrough -Force` before retrying.

When checking evaluation output specifically: a MiniMax side should print both
`now=` and `pred=`, a non-MiniMax side only `now=`, and forced wins must render as
`+WIN` / `-WIN` rather than the raw sentinel (`2147483646` / `-2147483647`). Make
sure **both** the immediate and predicted values go through the `+WIN`/`-WIN`
formatter, not just one of them.

---

## GUI changes (`gui/`)

### Always run the smoke test

After any `gui/` change:

```powershell
.\tools\smoke_test_gui.ps1 -Build
```

This rebuilds `breakthrough_gui.exe`, runs it in its `--capture` mode for 180
frames, and saves the last frame to `build\gui_smoke.png`. The window is created
hidden: nothing appears on screen and nothing takes focus, so it is safe to run
while the developer is playing a full-screen game. **Exit code 0** means it built,
ran, and exited cleanly. Non-zero means the build failed, it crashed, or it hung.
`-Visible` restores the old behavior (a real window and a full-screen grab), and
refuses to run while a Steam or Epic game is running.

This proves the GUI *runs*. It does not prove it *looks right*.

### Always open and visually read the screenshot

Exit code 0 hides visual bugs that only a human (or a look at the image) catches:
wrong colors, invisible glyphs, bad layout, overlapping widgets. Real examples this
project hit, none of which changed the exit code:

- A custom speed glyph drawn in the same shade as the button face, so it was
  invisible. Fix: draw custom glyphs in a color that contrasts with the current
  raygui style (light, since `ApplyDarkStyle` makes button faces dark).
- Text drawn in the app's light label color inside a raygui scroll panel or list
  whose background was raygui's default light gray, so move-log lines and editor
  labels were unreadable. Fix: one raygui palette for everything
  (`ApplyDarkStyle`), rather than mixing app-drawn and raygui-drawn colors.
- Piece-count badges placed on the wrong side of the board, and a black circle that
  blended into a dark pill background.
- Speed buttons showing the wrong icon (a single left arrow instead of the intended
  slow-motion / fast-forward double arrows).

So: after the smoke test passes, **open `build\gui_smoke.png`** and confirm the
board, pieces, and controls actually render as intended.

### Scenario screenshots (modals, tabs, matchup-gated UI)

Many controls only appear in a specific state: a modal window, a panel tab, the
agent-vs-agent pacing row, a finished game. `tools\gui_shot.ps1` reaches them
through the GUI's own `--scenario` and `--moves` flags, with no temporary code
edits:

```powershell
.\tools\gui_shot.ps1 -All                                  # every scenario -> build\gui_shots\*.png
.\tools\gui_shot.ps1 -Scenario editor                      # one scenario
.\tools\gui_shot.ps1 -Scenario "view,red,flip" -Moves "c1c,f6f"
```

Scenarios: `library`, `standings`, `presets`, `editor`, `models`, `analysis`,
`view`, `simple`, `aivai` (two presets at 4x), `red` (Red / Blue pieces), `flip`,
`nopanel`, `hard` (Black = the Hard preset). `--moves` plays moves in engine
notation first (`c1c` = c1 to c2). Capture mode starts from defaults and never
reads or writes `gui_settings.txt`, favorites, or history. To read small glyphs,
zoom the PNG with **nearest-neighbor** (no smoothing).

`tools\gui_capture.ps1` (client-area grab of a real, visible window, matched by
the window class `GLFW30` because `FindWindow` by title returns 0 for raylib)
still works for interactive sessions, but it shows a window, so do not use it
while the developer may be in a game.

### Web build check

```powershell
.\build_web.bat
python -m http.server 8765 --bind 127.0.0.1 -d build\web
```

Open `http://127.0.0.1:8765/?mode=watch&hints=1` to watch two presets play with
arrows on. For hands-off screenshots, `.\tools\web_shot.ps1` serves `build\web`
itself, loads each page in its own hidden headless Chrome, waits 20 s of real
time, and saves `build\web_shots\<name>.png`. Read them: Watch should show a
game well under way, Hard its first move and Black's arrows.

Two headless Chrome shortcuts give misleading pictures of this page:
- `--virtual-time-budget` freezes the clock inside each task, so the page's
  clock-paced agent moves and the analysis time limits stall or run away. A
  Watch page can sit on move 1 however large the budget.
- `--timeout=N --screenshot` does not wait N ms. The shot is taken about a
  second after load.

`web_shot.ps1` uses a fresh profile per run. When driving Chrome by hand, use a
fresh profile (or a cache-busting query) after every rebuild: a reused profile
can serve the previous `index.wasm` from its cache.

Board orientation truth (useful when checking click-to-move and coordinates): on
`board1.txt`, **Black is at the top** (rows 6-7) and **White is at the bottom**
(rows 0-1), moving upward. This is what the screen actually shows.

### Rebuild lock (LNK1104)

A running `breakthrough_gui.exe` locks the output file and breaks the next rebuild
with `LNK1104`. Kill it first:

```powershell
Get-Process breakthrough_gui -EA 0 | Stop-Process -Force
```

`tools\smoke_test_gui.ps1 -Build` and `tools\gui_capture.ps1` already do this at
startup.

---

## Build / coding gotchas (MSVC + raylib / raygui)

- **Aggregate init, not compound literals.** Use `Rectangle{ x, y, w, h }`, not the
  C99 compound literal `(Rectangle){ ... }`. MSVC's C++ compiler rejects the latter.
- **`WHITE` / `BLACK` macro collision.** raylib defines `WHITE` and `BLACK` as
  `Color` macros that collide with `globals.h`'s board macros. `main_gui.cpp`
  includes raylib/raygui first, `#undef`s `WHITE`/`BLACK`, then includes
  `globals.h` and draws with explicit `Color` literals. See the GUI notes in
  [CLAUDE.md](CLAUDE.md).
- **raygui icons** embed in widget text as `#NNN#` (e.g. `#131#` play, `#132#`
  pause, `#134#` step, `#211#` restart). There is **no** double-arrow rewind /
  fast-forward icon, so the slow-motion (`|>`) and fast-forward (`>>`) speed glyphs
  are custom-drawn by `DrawSpeedGlyph`, in a color that contrasts with the button
  face (see above).
- **`GuiToggleGroup`'s `bounds` is the size of ONE item, not the whole group.**
  Each subsequent item is placed at `bounds.x += bounds.width + GROUP_PADDING`
  (see raygui.h's implementation), so passing the full row/column width as
  `bounds` (as if it were the group's total span, the natural first guess)
  draws item 0 at full width and pushes every other item off past the edge of
  the panel/popup -- invisible, not merely misaligned, and the exit code and
  even a full-screen screenshot both look fine unless you look at exactly that
  region. Divide the intended total width by the item count first. Caught via
  the agent editor's Human/Agent and Search/Policy toggles (2026-08-08): a
  full-panel-width group showed only the active item, all row width, with
  no error anywhere.
- **`ml_model.h`'s `class Model` collides with raylib's own `struct Model`**
  (a 3D model asset, `raylib.h`). Unlike the `WHITE`/`BLACK` macro collision
  above, this is a type name, so `#undef` cannot fix it: including any header
  that pulls in `ml_model.h` (e.g. `ml_eval.h`, for its `ML_SLOTS` constant)
  into `main_gui.cpp` fails with `C2011: 'Model': 'struct' type redefinition`.
  The GUI works around it by keeping every model-slot operation in
  `gui/gui_engine.cpp`, which includes no raylib header, so `main_gui.cpp`
  never needs `ml_eval.h`.
- **An open `GuiDropdownBox` list must draw last**, over every control below it,
  and those controls must ignore the click that lands on the list. `main_gui.cpp`
  queues dropdowns with `DeferDropdown` and draws them in `FlushDropdowns` after
  the rest of the panel or modal, locking the other controls while one is open.
- **`MeasureText` is linear in raylib's glyph table per character**, so a list
  that re-truncates thousands of rows every frame (the model picker) can drop the
  frame rate to a crawl. Cache row strings and truncate with a binary search
  (`FitText`).
