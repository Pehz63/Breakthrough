# TT contamination fix: impact measurement and `ab` version bump

Results-only document (no separate plan file: this was reactive follow-up work
within a single session, not a pre-planned task). Continues the investigation
in `plans/refutation-oracle-results-1-quiet-lodestone.md` and
`plans/refutation-oracle-results-2-quiet-lodestone.md`, which found and fixed
the `TT CROSS-AGENT CONTAMINATION` defect but explicitly left its Elo
consequence unmeasured.

## What was asked

Rerun the ranking after the TT fix (committed in another session,
`06e4738`), and report any major Elo swings or new champions. A naive rerun
(`rank.exe rate` against unchanged historical data) is a no-op: it only
re-fits the same stored games, and the fix changes future search behavior,
not past stored outcomes. The developer chose to measure the fix's impact
before deciding whether to act on it, and separately pre-authorized bumping
the `ab` explorer's code version "if you notice any impact."

## Measurement

**Instrument.** `rank.exe extract` replays a sample of stored games from
their exact recorded seed and has a built-in determinism-drift guard: it
skips (and counts) any replayed game whose outcome no longer matches the
stored result. Repurposed here as an impact-of-the-fix measurement rather
than its usual purpose (dumping replay-derived training data), since no
purpose-built "replay and compare" tool exists yet.

**Isolating the affected population.** Contamination requires both players of
a game to carry `tt` (a `tt` agent's opponent never reads the table if it
doesn't use one). Filtered the full match store (3 parts, 269,533 rows) with
an `awk` pass on the `"w"`/`"b"` fields into `tt`-vs-`tt` (161,938 rows,
59.06% of the store) and its complement (107,595 rows) as an unaffected
control. 161,938 / 269,533 is close to the `(169/217)^2` approx 60.6% sanity
check from the fix's own results doc (169 of 217 active agents carry `tt`).

**Sample sizes.** tt-vs-tt: 3000 games, seed 42 (about 2 hours wall time).
Control: sized down to 250 games (about 10 minutes) after the developer asked
for a faster check once the full-size control run's cost became clear
mid-run; both samples use the same seed for comparability.

**Results.**

| Sample | Sampled | Unparseable/stale ID | Replayable | Mismatches | Rate |
|---|---|---|---|---|---|
| tt-vs-tt | 3000 | 716 | 2284 | 705 | 30.9% |
| control (non-tt-vs-tt) | 250 | 72 | 178 | 24 | 13.5% |

The control rate is not zero. The store spans a long history, and any code
change since a game was recorded (not only this fix) can make its replay
diverge, so the two rates are not directly subtractable into a clean
"TT-caused fraction." The gap between them (17.4 points, two-proportion
z approx 6.4) is far too large to be sampling noise given the sample sizes,
so the tt-vs-tt rate is not simply reproducing the same baseline drift at a
larger N.

**Conclusion.** The defect changed a substantial share of `tt`-vs-`tt` game
outcomes, well beyond ordinary reproducibility drift. This is a
reproducibility measurement, not yet a direct Elo delta (no re-fit of the
existing pool under a fixed binary was performed; the version bump makes that
question moot for the `ab` head going forward, since Elo history restarts at
zero games instead).

## Action taken: `ab` explorer version bump

Judged the measured gap as "impact" under the developer's standing
pre-authorization and proceeded to bump the `ab` explorer's code version,
`1 -> 2`, in `src/ranking.cpp`'s `g_rkExplorers` table. Per the project's
module-versioning scheme (documented at that table), this re-identifies
*every* `ab(...)` agent, `tt` and non-`tt` alike -- the scheme has no finer
grain than per-module, so agents whose behavior did not change were swept up
too. This is the same tradeoff the developer was told about before
authorizing it.

**Steps performed:**

1. Bumped `g_rkExplorers`'s `{ "AlphaBeta", "ab", 1 }` to version 2
   (`src/ranking.cpp`).
2. Rebuilt all four linked binaries (`breakthrough.exe`, `tests.exe`,
   `rank.exe`, `train.exe`) -- each target links a different source subset
   (see `src/CLAUDE.md`'s "Engine link set"; `tests.exe`/`rank.exe`/
   `train.exe` additionally need `explorers.cpp`/`choosers.cpp`/`agents.cpp`
   and the gumbel/tdleaf sources that `breakthrough.exe` does not).
3. Bumped `ranking/roster.txt`'s 216 active `ab(...)@1` head segments to
   `@2` (verified: exactly one `ab(` occurrence per active line, all already
   in canonical `deep=`/`nodes=`/`time=` spelling, no collisions with the 8
   commented-out dead lines that also mention `ab(...)@1` -- those were left
   untouched since they are historical, not live).
4. `rank.exe check` passes: 218 active agents, all canonical.
5. Ran the test suite (`tests.exe`, 3700+ assertions) to confirm the rebuild
   didn't break anything -- see "Test results" below.
6. Re-ran `rank.exe rate`. `ranking/standings.tsv` for the `ab` head is now
   empty (216 agents, zero games under `@2`); the pre-fix history sits under
   the frozen `@1` identity, `gone` in the full-historical `ratings.tsv`.

**Test results:** the first run surfaced 14 real failures, all `tests/test_ranking.cpp`
fixtures hardcoding `ab(...)@1` ids used to actually build/run/schedule agents
(`parseOk` round-trip checks, `mkActive`-built agents, `rankPairGen`/
`rankOpenerBias`/`rankOpenerSwap`/`rankLabel` fixture ids, and the
`rankDisplayId` current-vs-retired pair). Fixed by bumping every WORKING
`ab(...)@1` fixture in that file to `@2` (69 lines changed), while leaving the
~13 deliberately-invalid error-path fixtures (unknown flag, missing version,
bad dil depth, trailing dot, etc.) untouched, since none of them depend on
which version `ab` carries -- their errors fire on a different check entirely,
before the version-mismatch path is ever reached. See the correctness-gotchas
section below for how this was scoped safely. Full suite result after the fix: **all tests passed, 4301 assertions in 184
test cases.**

## Documentation updated

- `Docs/corrections.md`: added the measured mismatch numbers and the version
  bump consequence to the existing `TT CROSS-AGENT CONTAMINATION` entry
  (previously flagged "the Elo consequence has not been measured").
- `ranking/CHAMPION.md`: added a dated banner (`IDENTITIES RETIRED
  2026-08-28`) at the top, since all 6 category champions are declared on the
  `ab(...)` head and are now `gone` identities with zero games under `@2`.
  The banner follows this file's own existing convention for "the ground
  truth moved, previous numbers are historical" events (matching the
  `RE-CERTIFIED 2026-08-01` and `RESTRUCTURED 2026-08-24` banners already
  there), not the `Docs/corrections.md` defect-banner convention, since this
  is a legitimate state change rather than a writing defect.
- `todo.md`: struck through the "decide what the TT fix means" item as
  resolved, and added a new open item for re-certifying all 6 categories
  under the `@2` identities (explicitly left as a developer scope/sequencing
  decision, not started).

## What changed vs. what was planned

Nothing diverged from the plan the developer approved (measure first, bump
if impact is real). The one adjustment: the control replay was cut from 3000
games (sized to match the tt-vs-tt sample) down to 250 after the developer
asked for a faster check mid-run, sized from the tt-vs-tt run's measured
overall rate (3000 games in 119 minutes) to land around 10 minutes.

## Correctness gotchas (test fixtures)

- **`tests/test_ranking.cpp` hardcodes 82 occurrences of `ab(...)@1` across
  2513 lines, and a blind find-and-replace across all of them would have been
  wrong.** About a dozen are DELIBERATE error-path fixtures (an unknown `ab()`
  flag, a missing module version, an out-of-range dilution depth, a trailing
  dot) or legacy-spelling round-trip tests, where the assertion under test
  fires on a check that happens before the final canonical-version comparison,
  so their `@1` doesn't need to move -- confirmed by first running the OLD
  test binary and checking that exactly those lines were NOT among the actual
  failures, rather than guessing from reading the code alone. Two more needed
  individually-reasoned fixes rather than a blind version bump: the
  `rankDisplayId` "current vs. retired" pair (line ~1015-1021) needed its
  INPUT versions changed so the pair still demonstrates one current/stripped
  segment beside one genuinely-retired/visible one, and the legacy-spelling
  suggestion test (line ~480-481) turned out not to need special-casing after
  all -- bumping both its input and its expected-suggestion string to `@2`
  left the test still correctly exercising the label-spelling upgrade (`d6`
  -> `deep=6` etc.), independent of which version number rides along.

## Correctness gotchas

- **Backreferences in `awk` gsub don't work the way `sed`'s do.** A first
  attempt to bump the roster's `ab(...)@1` segments via `awk gsub(/ab\(([^()]*)\)@1/,
  "ab(\\1)@2")` silently inserted a literal control character instead of the
  captured group (POSIX awk's ERE has no backreference support in
  replacement text; that's a `sed`/PCRE feature). Switched to GNU `sed -E`
  with `\1`, which worked correctly. Also lost the roster file's CRLF line
  endings in the same attempt (the awk pass silently normalized them to LF,
  which would have shown as a full-file diff on every line); redid the whole
  substitution and restored CRLF explicitly before touching the real file.
- **Two dead/commented roster lines share the live lines' exact canonical
  text.** `ab(deep=4,tt,ord,nodes=200k)@1` and `ab(deep=6,tt,ord,nodes=200k)@1`
  each appear a few more times in commented-out `# off` documentation lines
  than in active lines. A blind global substitution across the whole file
  would have silently rewritten those historical comments too. Fixed by
  restricting the substitution to lines not starting with `#`.
- **Hand-rolling the `cl` command for `tests.exe`/`rank.exe`/`train.exe`
  misses source files that `breakthrough.exe`'s command doesn't need.**
  `src/explorers.cpp`, `src/choosers.cpp`, `src/agents.cpp`, `src/ai_gumbel.cpp`,
  `src/ml_gumbelzero.cpp`, and `src/ml_tdleaf.cpp` are all required by
  `tests.exe`/`rank.exe`/`train.exe` but absent from the root `CLAUDE.md`'s
  documented `breakthrough.exe` build line. Root `CLAUDE.md`'s "Engine link
  set" note covers this in principle ("a header change is not built until
  every binary that links it is rebuilt") but doesn't spell out the full
  per-target source lists; `build_rank.bat`/`build_train.bat` are the
  authoritative source lists and were used directly for those two targets.
  Worth adding the full per-target list to `src/CLAUDE.md`'s "Engine link
  set" note as a follow-up, so this isn't rediscovered by hand-rolling again.
- **Stale locked `.exe` files from earlier-in-session processes blocked
  relinking twice** (`tests.exe` held by a leftover `tests.exe` process,
  `rank.exe` held by a leftover `rank.exe` extract process). `LINK : fatal
  error LNK1104: cannot open file` was the symptom both times; fixed by
  `taskkill //F //PID <pid>` on the process holding the handle before
  retrying the link.
- **`cmd /c '"<path>" && cl ...'` behaves differently from the Bash tool vs.
  the PowerShell tool**, beyond the already-documented MSYS path-mangling
  issue. From the Bash tool (even with `MSYS_NO_PATHCONV=1` and an absolute
  Windows path), the invocation failed immediately with a garbled leading
  `\"` in the reported command and "not recognized," a different symptom
  from the previously-documented hang. Root `CLAUDE.md`'s example command is
  written for the PowerShell tool ("From PowerShell, wrap the build in...");
  switching to the PowerShell tool resolved it immediately with no further
  quoting changes.

## Future Work

- **Re-certify all 6 category champions under `@2`.** Open in `todo.md`,
  scope/sequencing not yet decided by the developer. This is a large
  re-play commitment (216 active agents, 23,653 pairs at the last `rank.exe
  check`), not a quick top-up.
- **A direct Elo-delta measurement was never taken**, only the reproducibility
  proxy above. If the pre-fix `@1` identities are ever refit again for any
  reason (they remain in `ratings.tsv` as `gone` rows), that fit's numbers
  would still describe the contaminated state and should not be compared to
  any future `@2` fit as if they were the same players before/after a clean
  intervention -- they are different identities under this project's own
  versioning rules, not a controlled before/after pair.
- **The control sample (250 games) is much smaller than the tt-vs-tt sample
  (3000 games)**, per the developer's own request to keep the check to about
  10 minutes. The z approx 6.4 gap is robust to that asymmetry, but a larger
  control sample would tighten the baseline-rate estimate itself, which
  currently has a wide confidence interval (24/178).
- **Per-target source lists for `tests.exe`/`rank.exe`/`train.exe` are not
  written down anywhere in `src/CLAUDE.md`**, only in the three `build_*.bat`
  scripts. Worth cross-referencing so a future hand-rolled build (e.g. inside
  a shell where the `vswhere`-based wrapper scripts don't resolve, per the
  existing root `CLAUDE.md` gotcha) doesn't need to rediscover the missing
  files by trial and error again.

## Ideas This Inspired

- A purpose-built "replay N stored games and report the mismatch rate,
  broken down by search head / loadout" subcommand would generalize what was
  hand-assembled here from `rank.exe extract`'s determinism-drift guard, and
  would be reusable for any future engine-behavior-changing fix, not just
  this one.
- The module-versioning scheme's "no finer grain than per-module" tradeoff
  (a `tt`-free `ab` agent's history was retired for a fix that didn't touch
  its behavior) suggests a possible future refinement: version the `tt`/`ord`
  /`qs` toggles as their own sub-segments with independent versions, so a fix
  scoped to one loadout item doesn't force a blanket re-identification of
  agents that don't wear it. Not attempted here; the existing scheme was used
  as-is per the developer's explicit go-ahead.
