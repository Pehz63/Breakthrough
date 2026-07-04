# Ranking v2: per-module ID versions, color splits, CPU-based efficiency, richer metrics, bigger roster

## Context

The rank.exe system is committed (6e7c6a0) and working. The user gave six pieces of feedback:
1. Versions should be per module, not per agent: changing the ab search code should re-identify ab agents only, leaving classic evaluator identities alone. The new grammar is already spec'd in the edited src/ranking.h header: each module segment carries `@V`, versions are code constants in the codec tables, and a stale `@N` fails the canonical check printing the current form. Example: `ab(d6,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r10)@1`. `linpol(s1,<hash>)` carries no `@V` (the model hash is its version); `learned(s0,<hash>)@1` keeps one for the LearnedValue evaluator code.
2. Draws are impossible in Breakthrough, so remove W-L-D from displays (keep 'D' tolerance in the schema and fit silently).
3. Split results by color in report.md: W-L as White and W-L as Black per agent.
4. Record and surface richer per-game metrics: plies, end piece counts, and other worthwhile ones.
5. Efficiency metric from CPU time (not node counts): user chose Elo per compute doubling, `eff = Elo / log2(1 + cpu_us/move)`, plus a pareto-front marker over (Elo, cpu/move).
6. Add more agents (all four proposed groups PLUS aggressive-pruning aspiration variants and d6 dilution), run them, and sample compute resource usage at a few moments during the run.

Decisions confirmed with the user: migrate the existing 264-game store by rewriting the 12 old IDs in place (nothing replays). Per-game detail = aggregate columns in report.md plus a machine-readable `ranking/games.tsv` written on each rate (`history` keeps per-game rows).

Current state: only src/ranking.h is modified (the grammar spec). src/ranking.cpp, tests, roster, README, CLAUDE.md all still implement the old trailing `.vN` grammar. Store: ranking/matches.jsonl, 264 rows (132 par=1, 132 par=4), exactly 12 distinct old-style IDs.

## 1. ID codec v2 (src/ranking.cpp, finalize src/ranking.h)

- Codec tables gain a version constant: `RankNameCodec {regName, idName, version}` for choosers (rand, tiered, smart, policy) and explorers (greedy, ab), `RankEvalCodec {regName, idName, letters, version}` for evaluators (classic, exp, learned), and `static const int RK_DIL_VERSION = 1` for the dil module. All start at 1. Bumping a constant re-identifies every agent using that module.
- Emit `rankAgentId(const AgentSpec&)` (version parameter removed): head token + `@V` (chooser row's V for policy brains, explorer row's V for search brains), evalseg + `@V`, `linpol(...)` with no `@`, `dil(...)` + `@V`. No trailing version segment.
- Parse: tokenizer handles `word@N` and `word(args)@N` (extend `splitTok` to strip a trailing `@<int>` before paren handling, N >= 1 lenient). Segment missing its `@` errors with "needs a module version like @1". A wrong N parses structurally, then the canonical re-emit check rejects it and prints the current canonical form (existing machinery, matches the header spec).
- `RankAgent` drops the now-meaningless `int version` field (fix the stale struct comment in ranking.h). `rankAgentFromId` signature unchanged otherwise.
- Update the ranking.h header comment where stale (struct comment, "user-bumped version" wording).

## 2. Store schema v2 (RankMatchRow, format/parse, playOneGame)

New per-game fields, all backward tolerant (absent in old rows -> sentinel -1, aggregates skip):
- `wpc`, `bpc` (int): end piece counts from `g_whiteCount` / `g_blackCount`.
- `wcpu`, `bcpu` (double ms): per-side process CPU time, GetProcessTimes delta around each `agentChooseMove`, summed per side (Windows only, `#ifdef _WIN32`, else -1). CPU time ignores core contention, so these stay honest in `-Workers` runs, unlike wall ms.
- `wed`, `bed` (double) and `wsn`, `bsn` (int): summed `g_lastEffDepth` and count over that side's search moves (guard `brain == BRAIN_SEARCH && g_lastNodes > 1`), for the aspiration/budget analysis.

`rankParseMatchRow` defaults the new keys to -1 when missing. Round-trip test extended.

## 3. Report v2 (rate outputs)

- All displays drop the draw column: W-L everywhere ('D' still scores 0.5 internally if ever present).
- Main table (report.md + console): rank, Elo, +/-, games, W-L as White, W-L as Black, avg plies, cpu/move, eff, state, id. Wall ms/move and nodes/move stay in report.md (wall keeps the serial-only rule with `*` fallback; cpu/move uses all rows that carry cpu fields).
- Efficiency: `eff = Elo / log2(1 + cpu_us/move)`, shown as `-` when cpu_us/move < 1 or Elo <= 0. New "Compute efficiency" section: table sorted by cpu/move ascending with Elo, eff, and a `*` pareto marker (on the frontier iff no other agent has both cpu/move <= its and Elo > its). This is the hill-climber's target surface.
- Per-agent sections: per-opponent tables gain avg plies; keep score/expected/delta.
- New aggregate: avg end-piece margin (own minus opponent pieces at game end, rows with pc fields only).
- `ranking/games.tsv` written on each rate: one row per game with every field (ts, run, board, w, b, r, plies, wpc, bpc, wms, bms, wcpu, bcpu, wmv, bmv, wnod, bnod, wed, bed, wsn, bsn, seed, par).
- `ranking/ratings.tsv` gains columns: white_wins, white_losses, black_wins, black_losses, avg_plies, cpu_ms_move, eff (id stays the LAST column so existing consumers by-last-column survive).

## 4. Store + roster migration (one-time, before the new code's first rate)

Rewrite the 12 known IDs inside ranking/matches.jsonl with a small PowerShell script (explicit old->new pairs, string replacement inside the JSON strings), e.g.
`ab(d6,tt,ord,nb200k).classic(t1,c4,w0,l0).v1` -> `ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1`
`ab(d4).classic(t1,c4,w0,l0).dil(r10).v1` -> `ab(d4)@1.classic(t1,c4,w0,l0)@1.dil(r10)@1`
`rand.v1` -> `rand@1`, etc. Verify: line count unchanged, zero remaining `.v1"` occurrences, `rank.exe rate` rates all 12 with anchor `rand@1` = 0. Delete stale ranking/gauntlet.jsonl scratch.

## 5. Roster v2 (ranking/roster.txt, 32 active agents)

Rewrite in the new grammar. Keep the existing 12 (migrated IDs) and add, one axis at a time:
- Depth ladder: `ab(d1)` and `ab(d5)` classic defaults.
- Pruning: `ab(d4,noab)` classic (alpha-beta off baseline).
- Feature ablations at d6 (all nb200k, classic defaults): `tt` only, `ord` only (base and tt+ord exist).
- Aggressive-pruning aspiration ladder: `ab(d6,tt,ord,asp25,nb200k)`, `asp50`, `asp100`.
- Eval weights at d4: classic structure-weighted `(t2,c10,w3,l2)`, exp forward ladder `f1`, `f4`, `f8` (f2 exists).
- Dilution ladder on ab(d4): `dil(r5)`, `dil(r25)`, `dil(r50)` (r10 exists).
- Dilution on d6: `ab(d6,nb200k)...dil(r10)`, `dil(r25)`, `dil(r50)`.
- Random family: `smart(2)`, `smart(6)`.

32 active -> 496 pairs at --games 4 = 1984 target games, ~1720 pending after the migrated 264.

## 6. Tests (tests/test_ranking.cpp)

- Migrate every ID literal to the new grammar (49 old-style references).
- New cases: stale module version `rand@2` rejected with `rand@1` in the message, missing `@` rejected, `linpol` with an `@` rejected, `learned(...)@1` round trips (conditional on model file).
- Match-row round trip covers the new fields, plus an old-style row (no new keys) parsing with -1 sentinels.
- Fit/scheduler tests unchanged except ID literals.

## 7. Docs

- README.md "Agent Elo ranking" section: new grammar + examples, module-version semantics (bump the constant in the codec table), new report columns, eff metric definition, games.tsv.
- CLAUDE.md: ranking.h/ranking.cpp row updates (grammar, module versions, new store fields, games.tsv, eff/pareto), test-count update.
- ranking/roster.txt header comment: "bump the module's version constant in src/ranking.cpp" replaces "bump the trailing version".

## 8. Run + resource documentation (after everything verifies)

1. `.\tools\run_tests.ps1 -Build` green.
2. Migrate store (step 4), rebuild rank.exe, `rank.exe check`, `rank.exe rate` sanity on migrated store.
3. Big run: `.\tools\run_rank.ps1 -Workers 8 --games 4` in the background. While it runs, take 3-4 `Get-Process rank | Select CPU, WorkingSet64` snapshots at random moments plus overall memory, and include the samples as a small table in the final summary to the user.
4. After the parallel run, `rank.exe run --games 4` (serial, should find ~0 pending) then inspect report.md: color splits present, depth ladder monotone, dilution ladders falling, asp ladder ordering, eff column and pareto stars sensible, avg plies populated, games.tsv row count = store line count.
5. Quick `gauntlet --id "ab(d5,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1" --games 2` to confirm gauntlet under the new grammar.
6. Suggest commit messages covering the uncommitted work (per CLAUDE.md standing instructions, no committing).

## Files touched

`src/ranking.h` (finalize spec comments, struct/API change), `src/ranking.cpp` (codec, schema, reports, games.tsv), `tests/test_ranking.cpp`, `ranking/roster.txt`, `ranking/matches.jsonl` (migration), README.md, CLAUDE.md. No build-script changes needed.
