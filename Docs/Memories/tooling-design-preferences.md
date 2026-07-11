---
name: tooling-design-preferences
description: "Zeph's confirmed preferences for project tooling design (deterministic fits, hand-editable text configs, compact functional IDs)"
metadata:
  type: user
---

When offered design choices for the rank.exe Elo system (2026-07-03), Zeph picked, in each case, the recommended option: a deterministic order-independent Bradley-Terry refit over stateful/incremental rating updates (he wants ratings usable as ground-truth ML labels), a compact functional agent-ID grammar like `ab(d6,tt,ord).classic(t2,c10,w3,l2).v1` over verbose key=value, and a plain-text hand-editable roster with on/off toggle words over JSONL or a tool-rewritten Markdown table.

**Why:** he values reproducibility for training signals, low-friction hand editing, and disliked the old tournament's negative Elos, arbitrary rosters, and poor progress printing.

**How to apply:** for future tooling in this project, prefer deterministic recomputation from durable stores over mutable state, plain text configs a human edits directly (with the tool validating rather than rewriting them), live per-item progress output, and anchored/absolute scales over mean-centered ones.
