# Results: GUI full agent-ID selection

Companion to `gui-agent-selection-plan-1-sharded-swimming-petal.md`. Work done
2026-08-08.

## Summary of changes

- **`gui/main_gui.cpp`** rewritten to drive AI moves through `agentChooseMove`
  (`src/agents.h`), the same composition the ranked agent pool uses, instead of
  the old `moveWhite`/`moveBlack(playerType, w1, evaluator, params, opener)` path.
  `PlayerConfig` is now `{ bool isHuman; AgentSpec spec; string id; }`. Either side
  can be set to any canonical agent ID the project's grammar (`src/ranking.h`)
  expresses -- the same IDs `ranking/roster.txt` and `rank.exe` use.
- **Opener wiring:** `AgentSpec::openerKind` (the `.opener(rand,...)`/
  `.opener(book,...)` id segment) is otherwise inert outside
  `src/ranking.cpp`'s `playOneGame`/`playoutCapture`. `AiWorker` now replicates
  that dispatch with a shared half-move clock (`g_halfMove`, mirroring
  `playOneGame`'s `h`/`h/2`), so an agent wearing an opener actually plays it in
  the GUI instead of the id merely parsing. Measured against the live roster:
  62/170 active agents carry an opener, so this was not optional coverage.
- **Model loading:** a new `rankLoadAgentModels(spec, err)` (`src/ranking.h`/
  `.cpp`) wraps the existing internal `loadModelSlots` for a single spec, reused
  by the GUI whenever an agent is applied, so a `LearnedValue`/`LearnedPolicy`
  agent's model is actually loaded (with a surfaced error on failure) rather than
  playing on stale or absent weights. 106/170 active roster agents use a learned
  model.
- **Agent editor popup** (`DrawAgentEditor`, `GuiWindowBox` modal): a canonical-ID
  textbox with live parse validation (`rankAgentFromId`) and inline error text,
  a "Recent" history list, and structured dropdowns/sliders (reusing the existing
  `StepperRow` widget) covering every field the grammar exposes -- brain,
  explorer/chooser, evaluator + weights, depth, search feature toggles, node/time
  budgets, depth cap, model slot + Risk, dilution, opener + its arguments. Editing
  either the text or the structured fields keeps the other in sync.
- **Agent ID history:** `gui_agent_history.txt` (repo root, gitignored), one
  canonical id per line, newest first, capped at 30, re-validated on load so a
  stale entry (e.g. a deleted model file) is silently dropped. Persists across
  restarts.
- **Panel simplified:** the old fixed player-type dropdown, its evaluator
  dropdown, and the three scripted Offensive/Defensive/Standard openers
  (`OpenerEnum`, unrelated to the roster's `rand`/`book` opener axis) are gone,
  replaced by a Human/Agent toggle + elided id summary + "Edit Agent..." button
  per side.
- **Web build kept working, at partial feature parity (option 2 of the 3 laid
  out below, per the developer's choice).** `agents.cpp`/`explorers.cpp`/
  `choosers.cpp` are portable (no `<windows.h>`), so `build_web.bat` now links
  them too: the web GUI gets the full `AgentSpec`-driven dispatch and the same
  structured dropdown/slider agent editor as native. Only the pieces that need
  `ranking.cpp`'s codec -- the free-text canonical-ID box and its persisted
  history -- are native-only, guarded by `#if !defined(PLATFORM_WEB)` throughout
  `main_gui.cpp`. Model-slot loading is unified behind one
  `GuiLoadAgentModels(spec, err)` name: native calls `rankLoadAgentModels`
  (`src/ranking.h`); web gets a small portable reimplementation of the same
  slot-file naming convention plus a direct, forward-declared call to the
  otherwise-portable `mlLoadSlot` (`src/ml_eval.h`). Where native needs a
  canonical id string (`MakeDefaultBlackAgent`, Apply), web substitutes
  `agentDescribe()` (`src/agents.h`, portable, a readable label rather than a
  re-parseable id).
- Tests: `tests/test_ranking.cpp` gained a `rankLoadAgentModels` case (no-op for
  a non-learned agent, success loading a real sweep-slot model, failure with
  `err` set for a missing slot file).
- Docs updated: `gui/CLAUDE.md` (Build notes + the `main_gui.cpp` row rewritten
  for the native/web split), `README.md` ("Using the GUI"), `TESTING.md` (two
  new gotchas, see below).

## How to test

```powershell
.\tools\run_tests.ps1 -Build              # engine/ranking suite, incl. the new rankLoadAgentModels case
.\build_gui.bat                           # native; links agents/explorers/choosers/ranking.cpp
.\tools\smoke_test_gui.ps1 -Build          # build + launch + screenshot sanity
.\build_web.bat                           # web; links agents/explorers/choosers, NOT ranking.cpp --
                                           # not run this session (no emsdk), see "Web build" below
```

Then interactively (`-KeepOpen` per `TESTING.md`'s GUI playbook): set Black to a
plain roster id (`rand@1`), a search id (`ab(deep=4)@1.classic(chip=100)@2`), a
`.dil(...)` id, a `.opener(rand,...)` id (confirm it visibly plays randomly only
for its own opening plies), and a `.learned(...)` id from the active roster
(confirm no load error and the model actually influences play). Confirm an
invalid/garbled id shows the parser's error text and does not apply. Confirm
`now`/`pred` eval readouts behave correctly for Greedy vs AlphaBeta vs an
opener-played move. Restart the GUI and confirm the history list persisted.
Confirm "Settings changed." / New Game still triggers when an agent id changes
mid-game.

## Verification status this session

- **Compiles clean:** native GUI build (`cl` direct invocation, `build_gui.bat`'s
  own source list) produces `breakthrough_gui.exe` with zero errors/warnings,
  both before and after the `GuiToggleGroup` fix below.
- **Catch2 suite: all tests passed, 3116 assertions in 131 test cases** (up
  from the prior baseline of 3109/130 recorded in
  `risk-weight-results-1-dusty-kestrel.md` by exactly the one new
  `rankLoadAgentModels` test case). The full engine/ranking/ML suite, which
  exercises `AgentSpec`, `agents.cpp`, and `ranking.cpp` extensively, still
  passes completely with the new `rankLoadAgentModels` function and the
  `src/ranking.h` declaration in place.
- **Visual spot-check (screenshots via `smoke_test_gui.ps1` +
  `gui_capture.ps1`):** the default panel (Human/Agent toggles, summary, Edit
  Agent button, top-bar summary) renders correctly for both sides. The agent
  editor popup (id textbox, Recent list, Brain/Opener/Search/Eval dropdowns,
  Depth/Flags/budget/weight rows, Apply/Cancel) renders correctly. This first
  pass is what caught the `GuiToggleGroup` bug below; after the fix, both
  affected toggle groups (the panel's Human/Agent, the popup's Search/Policy)
  were rebuilt and re-screenshotted and now show both items correctly (previously
  only the active item was visible, the other pushed off-panel).
- **Not yet done, on hold at the developer's request earlier in the session**
  ("stop spawning new games... I'm busy", later "Ok smoke test now"): the
  interactive manual-play checklist above (opener agents actually randomizing
  only their own opening plies, a learned agent's model measurably affecting
  play, `now`/`pred` gating across brain/explorer combinations, history
  surviving a real restart, mid-game "Settings changed." behavior) has not yet
  been exercised by actually playing a game through the new UI -- structural/
  visual correctness of the panel and the popup are confirmed, but not gameplay
  behavior.

## Implementation gotchas

- **`GuiToggleGroup`'s `bounds` argument is the size of ONE item, not the whole
  group.** raygui's implementation places each subsequent item at `bounds.x +=
  bounds.width + GROUP_PADDING`. Passing the intended *total* row width (the
  natural first guess, and what the pre-existing "Sliders" `GuiComboBox` pattern
  in this file does successfully for a different widget) draws item 0 at full
  width and pushes every other item off past the edge of the panel/popup --
  invisible, not merely misaligned, and neither the process exit code nor a
  glance at a full-screen screenshot catches it unless you look at exactly that
  pixel region. Both toggle groups added this session (the panel's Human/Agent,
  the editor's Search/Policy) had this bug; both are now fixed (divide the
  intended total width by the item count). Documented in `TESTING.md`'s gotchas
  list so it isn't rediscovered.
- **`ml_model.h`'s `class Model` collides with raylib's own `struct Model`** (a
  3D model asset). Unlike the pre-existing `WHITE`/`BLACK` macro collision this
  file already works around, this is a type name, so `#undef` cannot fix it.
  `#include "ml_eval.h"` (wanted only for its `ML_SLOTS` constant) pulls in
  `ml_model.h` and fails with `C2011: 'Model': 'struct' type redefinition`.
  Fixed by not including `ml_eval.h` and instead defining a local
  `GUI_MODEL_SLOTS` constant that mirrors `ML_SLOTS`, with a comment pointing at
  the original. Documented in `TESTING.md`.
- **Build environment path mismatch (session-local, not committed):** this
  session's environment has Visual Studio installed as the **BuildTools**
  edition under `C:\Program Files (x86)\...\18\BuildTools\`, not the
  **Community** edition at the path `CLAUDE.md`/`README.md` document
  (`C:\Program Files\...\18\Community\`), and `build_gui.bat`/`build_tests.bat`'s
  own `vswhere.exe`-based auto-detection failed to resolve VS in this
  environment for an unknown reason (vswhere.exe is present at the expected
  path). Worked around locally by invoking `vcvars64.bat` (found via
  `Get-ChildItem`) directly, then `cl` with the same source/flag list the `.bat`
  files already specify, rather than going through the wrapper scripts. Left the
  documented paths and the `.bat` files themselves unchanged, since this may be
  specific to this sandboxed execution environment rather than the developer's
  actual machine -- flagging it here rather than "fixing" a path that might be
  correct on the real machine.

## Web build: found broken, then fixed (partial feature parity)

The first pass of this change made `main_gui.cpp` unconditionally call into
`agents.cpp`/`ranking.cpp` APIs with no `#ifdef PLATFORM_WEB` fallback, which
would have broken `build_web.bat`'s link step (`ranking.cpp` cannot compile
under Emscripten -- unconditional `#include <windows.h>` for
`GetProcessTimes`). Caught before committing (`git log` showed
`build_web.bat`/`docs/`/`gui/shell.html` untouched since the GUI was first
added, suggesting a dormant target, but "probably unused" isn't "safe to leave
broken"). Presented the developer three options (gate everything behind
`#if !defined(PLATFORM_WEB)` with a minimal old-system fallback; link the
portable subset into `build_web.bat` and gate only the `ranking.cpp` slice;
defer to a future session with `emcc` available); **the developer chose the
second**, implemented this session (see "Summary of changes" above).

**Verification caveat: compile-checked on native only.** This session's
environment has no `emcc`/emsdk installed, so `build_web.bat` itself was never
actually run -- there is no way to fully verify the `#if defined(PLATFORM_WEB)`
branches in `main_gui.cpp` (the `GuiSlotFile`/`GuiLoadAgentModels` web
reimplementation, the `agentDescribe`-based id substitution, the `mlLoadSlot`
forward declaration) compile under Emscripten. What was verified: the native
(`#if !defined(PLATFORM_WEB)`) path compiles and links cleanly after every
change in this section; every `#if`/`#else`/`#endif` in the file is balanced
and single-level (checked mechanically); the web-only branches were read
through carefully for symbol availability (everything they call --
`agentDescribe`, `ChooserIndexByName`, `learnedValueIndex`, `mlLoadSlot`,
`GUI_MODEL_SLOTS` -- is either already portable or locally forward-declared).
**A real `build_web.bat` run (needs emsdk, see `INSTALL.md`) is the one
verification step this session could not perform.**

## Future Work

- **Run `build_web.bat` for real** once emsdk is available, to close the
  verification gap above -- the one thing this session could not check.
- **Finish the interactive manual-play verification checklist** (see
  "Verification status" above): opener agents, learned-model agents, dilution,
  eval-readout gating, history persistence across a real restart, and the
  mid-game settings-changed notice have only been exercised structurally/via
  code review and static screenshots, not by actually playing games through the
  new UI.
- The agent editor's scrollable body uses a fixed generous virtual content
  height (950px) rather than an exact per-selection row count, so the scroll
  range is sometimes larger than the actual content (cosmetic only, not
  attempted to fix precisely -- see the "Ideas This Inspired" note below if this
  becomes annoying in practice).

## Ideas This Inspired

- If the fixed-950px scroll body ever feels imprecise in practice, the fields
  drawn in `DrawAgentEditor`'s scrollable body already share one set of boolean
  flags (`isSearch`, `isAB`, which evaluator, dilute, which opener); an exact
  row-height tally could be computed from those same flags once, without
  duplicating the branch logic, and passed as the `GuiScrollPanel` content
  height.
- The `GuiToggleGroup` per-item-width gotcha suggests it may be worth a quick
  audit of any other multi-item raygui control usage in this file for the same
  "bounds means one item, not the group" misreading, in case it recurs the next
  time a new control is added.
- Since agent IDs are now a first-class GUI input, a "copy current agent ID to
  clipboard" button next to the id textbox (or a paste-from-clipboard shortcut)
  would make round-tripping an ID between the GUI and `rank.exe`/`roster.txt`
  faster than manual retyping.

## Commit

Not yet committed -- pending the developer's read of this doc and, ideally, the
still-open interactive verification pass and the web-build regression decision.
