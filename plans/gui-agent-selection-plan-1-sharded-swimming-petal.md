# GUI: full agent-ID selection

## Context

The GUI currently drives AI moves through the old narrow path
`moveWhite/moveBlack(playerType, w1, evaluator, params, opener)` (`gui/main_gui.cpp`
`PlayerConfig`, `LaunchAIMove`/`AiWorker`): 5 fixed `PlayerEnum` types, one evaluator
dropdown, three scripted `OpenerEnum` openers, and no node/time budgets, no
transposition table, no dilution, no learned-model slots, no policy brains. Every
axis the ranked roster (`ranking/roster.txt`, 170 active agents) actually uses lives
only in `AgentSpec` + `agentChooseMove()` (`src/agents.h/.cpp`), which the GUI never
calls. Roster agents are addressed by a canonical ID string with its own grammar
(documented in full in `src/ranking.h`) and codec (`rankAgentId`/`rankAgentFromId` in
`src/ranking.cpp`) -- the same codec `rank.exe` uses to validate `roster.txt`.

The developer asked to overhaul the GUI so either side can be set to **any** agent
currently in the roster: a validated free-text canonical-ID box with persisted
history, plus dropdowns/sliders for every field, generic over the grammar. Measured
against the live roster: 106/170 active agents use a learned model, 62/170 carry an
identity-level opener (`.opener(rand,...)`/`.opener(book,...)`), 29/170 dilute -- so
partial coverage (e.g. parsing the ID but ignoring dilution or the opener) would
silently misplay a third of the roster. Confirmed with the developer: replace the old
system entirely (dropping the scripted Offensive/Defensive openers, which have no
roster equivalent), build the structured dropdown/slider editor in this same pass,
and persist the ID history to disk across restarts. This is native-GUI-only --
`ranking.cpp` unconditionally includes `<windows.h>` (`GetProcessTimes`), so it cannot
link into the Emscripten web build; `build_web.bat`/the web GUI are left unchanged.

## Approach

### 1. Expose a single-agent model loader from `ranking.cpp`

`ranking.cpp` already has `static string slotFile(int slot)` and
`static bool loadModelSlots(const std::vector<const RankAgent*>&, string&)` (used by
every `rank.exe` subcommand that plays games). Add, right after `loadModelSlots`:

```cpp
bool rankLoadAgentModels(const AgentSpec& spec, string& err) {
    RankAgent tmp; tmp.spec = spec;
    std::vector<const RankAgent*> v; v.push_back(&tmp);
    return loadModelSlots(v, err);
}
```

Declare it in `src/ranking.h` next to the other subcommand-adjacent helpers. This is
the one piece of ranking-side logic the GUI needs and doesn't have: loading whichever
model slot(s) a `LearnedValue`/`LearnedPolicy` agent needs into `g_mlModels` via the
already-public `mlLoadSlot` (`src/ml_eval.h`), reusing the exact slot-file convention
`rank.exe` uses instead of duplicating it.

### 2. Link the agent/ranking sources into the native GUI only

`build_gui.bat`: add `src\agents.cpp src\explorers.cpp src\choosers.cpp
src\ranking.cpp` to the `cl` source list (all four are currently unlinked into the
GUI). `build_web.bat`: unchanged. Update the "Build notes" paragraph in
`gui/CLAUDE.md` to record the new native-only dependency and why (the `windows.h`
constraint).

### 3. Replace `PlayerConfig` and the AI dispatch path in `gui/main_gui.cpp`

New includes: `agents.h`, `explorers.h`, `choosers.h`, `ranking.h` (already includes
`ai_random.h` transitively for `g_openers`, or include it directly).

**Struct.** Replace the current `PlayerConfig` (PlayerEnum/evaluator/furthest/opener
fields) with:

```cpp
struct PlayerConfig {
    bool      isHuman = false;
    AgentSpec spec;      // meaningful only when !isHuman
    string    id;        // canonical id cache, kept in sync with spec (rankAgentId)
};
```

Default White = Human (unchanged). Default Black = today's effective default,
expressed as an agent: `agentMakeSearch("gui", <AlphaBeta index>, <Classic index>, 8,
0)` -- same depth-8 Classic MiniMax the GUI ships today, just spelled as an
`AgentSpec`. Whenever a side's `spec` changes (Apply in the editor, or a history pick),
recompute `id = rankAgentId(spec)` and call `rankLoadAgentModels(spec, err)`,
surfacing `err` in the status line on failure (mirrors `rank.exe`'s own
`loadModelSlots` failure path) rather than silently proceeding with a stale model.

**Dispatch.** `LaunchAIMove`/`AiWorker` currently call `moveWhite/moveBlack(cfg.type,
SearchArg(cfg), cfg.evaluator, cfg.evalParams, cfg.opener)` and discard the return
value the same way the ranking pool discards it once `CheckWinner()`'s board scan is
available. Replace with `agentChooseMove(spec, side)`, called only when
`!cfg.isHuman`. Delete `SearchArg()` (no longer meaningful; `AgentSpec` carries depth
directly).

**Opener wiring (the part that makes `.opener(...)` agents actually play their
opener, not just parse).** Mirror `playOneGame`'s pattern in `src/ranking.cpp`
(`h`/`h/2` dispatch, `src/ranking.cpp:2484-2489`): add a shared half-move counter
`static int g_halfMove = 0;`, reset to 0 in `StartGame()`, incremented once per
applied move (human or AI) inside `AfterMove()`. Snapshot it into a new
`g_aiHalfMove` alongside the existing `g_aiCfg`/`g_aiSide` snapshot in `LaunchAIMove`
(same race-safety reason those are snapshotted). In `AiWorker`, before calling
`agentChooseMove`:

```cpp
bool playedByOpener = false;
if (spec.openerKind >= 0 && spec.openerKind < g_openerCount)
    playedByOpener = g_openers[spec.openerKind].fn(
        side, g_aiHalfMove / 2, g_aiHalfMove, spec.openerArg, spec.openerArg2, victor);
if (!playedByOpener) agentChooseMove(spec, side);
```

Store `playedByOpener` (e.g. `static bool g_aiPlayedByOpener`, main-thread-safe to
read after the worker joins, same as every other `g_ai*` field) so `FinalizeAIMove`
can gate the `pred` eval readout correctly (below).

**Eval readouts.** `immediateEvalForDisplay(isMiniMax, evaluator, params)` and the
`pred`/`g_downEvalWhite/Black` readout need an `AgentSpec`-shaped notion of "this side
has a meaningful evaluator" and "this side's last move came from a real alpha-beta
search this turn". Add two small local helpers next to `PlayerName`:

```cpp
static bool IsAlphaBetaExplorer(int idx) {
    return idx >= 0 && idx < g_explorerCount && string(g_explorers[idx].name) == "AlphaBeta";
}
```

`hasEval = !cfg.isHuman && cfg.spec.brain == BRAIN_SEARCH` (replaces `cfg.type ==
MiniMax` at the two `immediateEvalForDisplay` call sites). `hasDown = hasEval &&
IsAlphaBetaExplorer(cfg.spec.explorer) && !g_aiPlayedByOpener` (replaces `isMM` in
`FinalizeAIMove`'s downstream-eval block) -- a Greedy explorer or an opener-played move
never populated `g_downEvalWhite/Black` this turn, so showing it would be stale.

**Pacing.** `ClassifyMatchup()`: `wH/bH` become `cfg.isHuman`; `aiSlow` becomes
`!ai.isHuman && ai.spec.brain == BRAIN_SEARCH && IsAlphaBetaExplorer(ai.spec.explorer)
&& ai.spec.depth > 5` (same depth>5 heuristic as today, now also gated on actually
being an alpha-beta search rather than e.g. Greedy).

**Settings-changed detection.** `SettingsSnapshot`/`SnapMatches` collapse to
comparing `isHuman` + the canonical `id` string per side (plus the board file) --
since `id` is a faithful serialization of everything that affects play, this is
simpler than and equivalent to the old per-field struct comparison.

**Delete:** `SearchArg`, `SeedEvalParams` (folded into the editor, see below), the
`TYPES`/`OPENERS` dropdown strings and the `g_editWhiteType/Opener/Eval` family of
flags, `PlayerName` (replace call sites with a short summary string, e.g. `"Human"` or
the first segment of `id` -- see UI below).

### 4. Panel: Human/Agent toggle + summary + "Edit Agent..." button

The 210px side panel has no room for a full canonical ID (the `learned(...)` ones run
well past 100 characters). Keep the panel minimal per side: a `GuiToggleGroup`
("Human"/"Agent"), and when Agent -- a one-line elided summary (e.g. first ~28 chars of
`id` + "...") and an "Edit Agent..." `GuiButton` that opens the popup editor for that side.
This replaces the current `DrawPlayerConfig` body (type dropdown, opener dropdown,
furthest/depth/eval steppers all move into the popup).

### 5. "Edit Agent..." popup: text ID + validated history + structured editor

One popup at a time (`static int g_editorSide = -1;` -1 closed, 0/1 = White/Black),
drawn via `GuiWindowBox` (raygui, already vendored, unused elsewhere in this file --
returns nonzero when its close icon is clicked) sized to fit comfortably (e.g.
620x600, clamped/centered against `GetScreenWidth/Height` the same way `ComputeLayout`
already clamps board geometry) with a working copy `AgentSpec working` edited in
place, only committed to `g_white`/`g_black` on Apply.

Layout, top to bottom:

1. **ID text box** (`GuiTextBox`, full popup width). While it has edit focus, leave
   its text alone (don't stomp on typing). On losing edit focus (the existing
   `GuiTextBox` "toggle edit" return-value pattern used elsewhere in this file),
   call `rankAgentFromId(text, out, err)`: on success, `working = out.spec`, clear the
   error, and re-render the box from `rankAgentId(working)` (normalizes spelling); on
   failure, show `err` in red-ish text below the box and leave `working` untouched.
2. **Recent** (`GuiListView` or a small manually-drawn button list): the shared
   validated-history list (see step 6). Clicking an entry loads it into the ID box
   and re-validates exactly like a manual edit.
3. **Structured fields**, hoisted into two groups because open `GuiDropdownBox` lists
   must not be clipped by a scissor region:
   - **Fixed (non-scrolling) row**: Brain (`Search`/`Policy`), then either an
     Explorer dropdown (`Greedy`/`AlphaBeta`, from `g_explorers`) + Evaluator dropdown
     (from `g_evaluators`, already generic -- reuse today's dropdown-string-building
     pattern) for Search, or a Chooser dropdown (from `g_choosers`) for Policy, and an
     Opener dropdown (`None` + one entry per `g_openers[]`, using each `OpenerDef`'s
     `idName`/`desc`). These are the only multi-item dropdowns, so they stay outside
     any scissored area (mirrors this file's existing "draw dropdowns last, on top,
     one open at a time" convention -- reuse the same `DropSpec`/`openIdx` pattern
     already in `DrawPanel`).
   - **Scrollable body** (`GuiScrollPanel` + `BeginScissorMode`, same pattern already
     used for the Move Log): every numeric field, each one `StepperRow` -- reusing the
     existing modular widget as-is, no new stepper styles needed:
     - Search + AlphaBeta: Depth (`STEP_NUMBAR`, uncapped bar like today), feature
       toggles as `GuiCheckBox`en on one row (noAB/TT/ord/QS/part), Aspiration Window,
       Node Budget (typeable, wide range -- store as `int` in the editor, roster values
       top out in the low millions), Time Budget (ms), Depth Cap.
     - Search: Evaluator's weight rows -- the existing per-`EvalDef` loop from
       `DrawPlayerConfig`, unchanged, now writing into `working.evalParams`. Reseed
       `working.evalParams` from the registry defaults when the evaluator dropdown
       changes, same `seededFor`-style guard as today's `SeedEvalParams` (now keyed
       off `working.evaluator`, folded into this function rather than kept as a
       separate `PlayerConfig` method).
     - Search with `LearnedValue` evaluator selected, or Policy with `LearnedPolicy`
       chooser: Model Slot (`STEP_NUMBAR`, range `[0, ML_SLOTS)`) and, for
       `LearnedValue` only, Risk (evalParams[1], can be negative -- StepperRow already
       supports negative `lo`).
     - Policy with `SmartRandom` chooser: chooserParam ("Forward", same `STEP_SEGMENTS`
       row style as today's SmartRandom furthest-N).
     - Dilution: `GuiCheckBox` "Dilute" gating a Prob% stepper (`randomMoveProb`,
       stored/edited as an integer percent 0-100, converted to/from the `double`
       field) and a Dilution Depth stepper (`dilDepth`, search brain only).
     - Opener (when the Opener dropdown selects one): Arg stepper labeled from
       `OpenerDef::argLabel` (only when `hasArg`), Arg2 ("ply cap") stepper (only
       when `hasArg2`).
   - After any structured-field change, recompute `rankAgentId(working)` and push it
     into the ID box (unless the box currently has edit focus -- see point 1), so the
     text view and the structured view never disagree.
4. **Apply** / **Cancel** buttons. Apply re-validates the box's current text one more
   time (covers a field edited then never blurred); on success, commits `working` to
   `g_white`/`g_black`, calls `rankLoadAgentModels`, pushes the canonical id to
   history (step 6), and closes the popup. Cancel discards `working` and closes.
   Apply is a no-op (stays open, shows the error) while the current text is invalid.

### 6. Validated, persisted history

One shared history (an agent ID isn't side-specific), new root-level file
`gui_agent_history.txt` (plain text, one canonical ID per line, most-recent-first),
following the project's existing plain-text local-config convention
(`minimax_params.txt`). Add it to `.gitignore` next to that convention (small new
comment block -- it's local MRU state, not project history). Load once in `main()`:
read each line, `rankAgentFromId` it, keep only the ones that still parse (silently
drop e.g. a line naming a since-deleted model file -- same tolerance `rankLoadRoster`
already has for stale entries). Cap at 30 entries. On a successful Apply, move/insert
the new canonical id to the front (dedup), rewrite the file. A small
`LoadAgentHistory()`/`SaveAgentHistory()`/`RememberAgentId(const string&)` trio next
to the other top-level helpers is enough; no new abstraction needed beyond that.

## Files touched

- `gui/main_gui.cpp` -- the bulk of the change (sections above).
- `src/ranking.h` / `src/ranking.cpp` -- add `rankLoadAgentModels`.
- `build_gui.bat` -- add the four new source files to the native GUI link line.
- `gui/CLAUDE.md` -- rewrite the `main_gui.cpp` row: new agent-selection
  architecture, opener wiring, model-slot loading, popup editor, the native-only
  `windows.h` dependency note.
- `.gitignore` -- add `gui_agent_history.txt`.
- `README.md` -- rewrite "Using the GUI" (currently describes the dropdown/type
  system) to describe Human/Agent toggle, the ID box + history, and the structured
  editor; update the numbered console section's cross-references if it points at
  GUI behavior that changed.
- `tests/test_ranking.cpp` -- one or two small cases for `rankLoadAgentModels` (a
  `learned()`/`linpol()` agent's slot loads; a spec naming a missing slot file
  returns `false` with `err` set; a non-model agent is a no-op success).

## Verification

1. `.\tools\run_tests.ps1 -Build` -- confirms `ranking.cpp`'s new function and
   everything downstream of the `AgentSpec`/dispatch changes still compiles and
   passes (the suite already links `ranking.cpp`).
2. `.\build_gui.bat` -- confirms the four new links resolve cleanly (native only).
3. `.\tools\smoke_test_gui.ps1 -Build` -- build + launch + screenshot sanity.
4. Manual interactive pass (`-KeepOpen` per `TESTING.md`'s GUI playbook), covering
   the risk surface identified above:
   - Set Black to a plain roster id (`rand@1`), a search id
     (`ab(deep=4)@1.classic(chip=100)@2`), a `.dil(...)` id, a `.opener(rand,...)` id
     (confirm it visibly plays randomly only for its own opening plies, not the whole
     game), and a `.learned(...)` id from the active roster (confirm no load error
     and the model actually influences play, not a silent TieredRandom fallback).
   - Confirm an invalid/garbled ID shows the parser's error text and does not apply.
   - Confirm `now`/`pred` eval readouts behave correctly for Greedy vs AlphaBeta vs an
     opener-played move (no stale `pred`).
   - Restart the GUI and confirm the history list still shows the previously-applied
     ids.
   - Confirm the "Settings changed." / New Game flow still triggers correctly when a
     side's agent id changes mid-game.
5. `.\build_web.bat` -- confirms the web build is genuinely untouched and still
   builds (regression check on the explicit native-only scope decision).
