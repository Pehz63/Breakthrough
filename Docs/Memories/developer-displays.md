---
name: developer-displays
description: Developer's monitors are 144 Hz 4K and 300 Hz 1080p, so web GUI frame-rate tests should cover those, not just 60 Hz
metadata:
  type: user
---

The developer uses a 144 Hz 4K monitor and a 300 Hz 1080p monitor (stated 2026-10-06).

**Why:** the web GUI's analysis benchmarks (`tools/web_ana_bench.ps1`) defaulted to a 60 fps cap as "a typical display". On the developer's own hardware the browser redraws 2.4x to 5x as often, and at 4K each frame covers more pixels.

**How to apply:** when measuring or reasoning about web GUI performance, treat 144 and 300 fps as the developer's real conditions, and do not present 60 fps results as representative of their setup. Any browser run still needs permission first under the high-CPU rule in `CLAUDE.md`.
