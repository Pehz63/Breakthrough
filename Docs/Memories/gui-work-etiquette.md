---
name: gui-work-etiquette
description: GUI sessions: never pop a visible window (the developer may be in a game), use hidden --capture; decide taste calls provisionally and ask at the end
metadata:
  type: feedback
---

When working on the GUI (`gui/`), two standing preferences from the developer
(2026-09-10):

1. **Never open a visible GUI window without checking.** The developer may be
   playing a full-screen game. The GUI's `--capture` mode renders in a hidden
   window and never takes focus, and `tools/smoke_test_gui.ps1` and
   `tools/gui_shot.ps1` use it by default. `smoke_test_gui.ps1 -Visible` refuses
   while a Steam or Epic game runs.
2. **Matters of taste are the developer's call, but do not block on them.** Pick a
   reasonable default, build it, show screenshots, then ask the taste questions
   together at the end (colors, layout, defaults, what is included). When the
   developer says they are busy, do not ask mid-work at all.

**How to apply:** verify GUI changes with hidden captures only. Keep a running
list of provisional taste decisions and present them with screenshots in one
AskUserQuestion batch at the end.

Related: [[no-unsolicited-time-estimates]].
