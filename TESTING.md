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

---

## GUI changes (`gui/`)

### Always run the smoke test

After any `gui/` change:

```powershell
.\tools\smoke_test_gui.ps1 -Build
```

This rebuilds `breakthrough_gui.exe`, launches it, captures a full-screen
screenshot to `build\gui_smoke.png`, and closes it. **Exit code 0** means it built
and stayed alive, non-zero means the build failed or it crashed on startup. Add
`-KeepOpen` to interact with the window by hand.

This proves the GUI *runs*. It does not prove it *looks right*.

### Always open and visually read the screenshot

Exit code 0 hides visual bugs that only a human (or a look at the image) catches:
wrong colors, invisible glyphs, bad layout, overlapping widgets. Real examples this
project hit, none of which changed the exit code:

- A custom speed glyph drawn in a light color on raygui's light button face, so it
  was invisible. Fix: draw custom glyphs in a **dark** color for contrast.
- Piece-count badges placed on the wrong side of the board, and a black circle that
  blended into a dark pill background.
- Speed buttons showing the wrong icon (a single left arrow instead of the intended
  slow-motion / fast-forward double arrows).

So: after the smoke test passes, **open `build\gui_smoke.png`** and confirm the
board, pieces, and controls actually render as intended.

### Targeted / zoomed capture of a single widget

`gui_smoke.png` is a full-screen grab. To inspect one widget closely, use the
committed helper:

```powershell
.\tools\gui_capture.ps1 -Out build\widget.png
```

It captures just the client area of the GUI window. To read small glyphs or text,
open the PNG and zoom with **nearest-neighbor** (no smoothing) so it stays crisp.

Why the helper is non-trivial: `FindWindow` by title returns 0 for raylib windows.
The helper instead enumerates top-level windows by process id and matches the
window **class `GLFW30`**, moves the window to a known size, then crops the
**client** rectangle. Keep captures under git-ignored `build\` so they never
clutter commits.

### Verifying conditional / matchup-gated UI

Some controls only appear for a specific setup. For example, the AI-vs-AI speed and
transport row is gated by `ClassifyMatchup` in `main_gui.cpp`, so a default Human
vs MiniMax launch never shows it. To screenshot it:

1. Temporarily set both player defaults to the needed types in `main()` (e.g. both
   `MiniMax` for the AI-vs-AI controls).
2. Build, run `gui_capture.ps1`, and inspect.
3. **Revert the temporary edit before finishing.** Do not leave the forced defaults
   in a commit.

Board orientation truth (useful when checking click-to-move and coordinates): on
`board1.txt`, **Black is at the top** (rows 6-7) and **White is at the bottom**
(rows 0-1), moving upward. This is what the screen actually shows.

### Rebuild lock (LNK1104)

A running `breakthrough_gui.exe` locks the output file and breaks the next rebuild
with `LNK1104`. Kill it first:

```powershell
Get-Process breakthrough_gui -EA 0 | Stop-Process -Force
```

Both `tools\smoke_test_gui.ps1` and `tools\gui_capture.ps1` already do this at startup.

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
  are custom-drawn by `DrawSpeedGlyph`. Custom glyphs need a dark fill to be
  visible on the light button face (see above).
