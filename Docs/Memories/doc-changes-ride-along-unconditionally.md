---
name: doc-changes-ride-along-unconditionally
description: Bundle any modified doc-type file (CLAUDE.md, Docs/, manifest.md/json) into the next commit even if you didn't make the edit
metadata:
  type: feedback
---

CLAUDE.md's own "doc changes ride along with the next commit" rule is
unconditional on who made the edit, not just uncommitted edits from the
current session.

**Why:** 2026-08-27, a session found 5 pre-existing modified doc/manifest
files it hadn't touched and couldn't explain, and left them uncommitted
across two commits out of caution. The developer corrected this: "They
should have joined your commit, as they are just doc changes." Caution about
unexplained changes is right for CODE (could be someone's in-progress work)
but not for doc-type files, which is exactly what the ride-along rule exists
for.

**How to apply:** at commit time, fold any modified `CLAUDE.md`, `Docs/**`,
`todo.md`, `plans/**`, or `models/manifest.{json,md}` into the current
commit regardless of origin. Still hold back genuine CODE changes you don't
recognize (`src/`, `tests/`, build scripts).
