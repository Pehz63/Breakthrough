# tests/ Reference

The Catch2 test suite. Loaded when working on files in `tests/`. Run it with
`.\tools\run_tests.ps1 -Build` (the `/run-tests` skill). The raw build command
is in the root `CLAUDE.md`, and console-driving instructions are in
`TESTING.md`.

## File details

| File | Purpose |
|---|---|
| `catch.hpp` | Catch2 v2 single-header test framework (no external dependency) |
| `helpers.h` | `setupBoard()`, `clearBoard()`, `runGame()` -- shared test utilities |
| `test_main.cpp` | Catch2 entry point (defines `CATCH_CONFIG_MAIN`) |
| `test_move_validation.cpp` | Unit tests for `tryMoveWhite/Black` and `tryMoveQuickWhite/Black` |
| `test_win_detection.cpp` | Unit tests for `canWinWhite/Black` and `findWinWhite/Black` |
| `test_eval.cpp` | Unit tests for `evaluateBoard`, the Classic/Experimental equivalence check, and the incremental-eval walk asserting `g_evalPos` matches `evalPosFull` over make/unmake |
| `test_ai_integration.cpp` | MiniMax forced-win scenarios on hand-crafted positions; search-value invariance under tt/ord (with and without quiescence); quiescence horizon-fix value test (a defended-capture exchange resolves to equal material at depth 1 + qs) |
| `test_game_outcomes.cpp` | Full-game outcome tests using puzzle boards (Black/White MiniMax vs TieredRandom) |
| `test_ml.cpp` | ML unit tests: feature determinism, `LinearModel` forward/save/load + SGD loss drop, `mlValueScore` bounds, Greedy winning move, LearnedPolicy legal move, Elo monotonicity |
| `test_ranking.cpp` | Ranking unit tests: ID canonical round trips + rejection battery (incl. stale/missing `@N` module versions, and legacy label spellings being reported with their modern form rather than accepted), the weight-subset rules (an omitted weight taking its registry default, a non-default zero surviving, the turn weight elided without `qs`/`part` but carried with it, and a chip-only Classic rescaling to `chip=100`), `rankDisplayId` dropping only CURRENT module versions and returning unparseable input verbatim, learned-model hash checks, codec completeness vs `g_evaluators`, roster parsing, match-row round trip (new cpu/pc/ed fields + old-row sentinels) + board filter, regime classification off the canonical id (incl. a loadout not changing it, and the legacy learned() form reporting no regime rather than guessing), matchup residual reading ~0 when Elo explains the results, regime-balanced fit moving an over-represented bloc's ratings while reproducing the plain fit exactly when regimes are equal-sized, match-store sharding (shard-path naming incl. not colliding with `run_rank.ps1`'s per-worker temps, sealed shards loading ahead of the live tail, a gap ending the chain, sealing preserving every row while bounding part size, re-sealing being a no-op, appends still landing in the tail), the part index (a dropped line dropping those games from the fit, a listed-but-missing part being skipped not fatal), store splitting by roster membership (dry run writes nothing, the tagged bucket winning over `retired_other`, and the parts still loading as the original row set), scheduler color balance / incremental top-up / determinism, BT fit accuracy + anchoring + disconnected components, gauntlet 1-D fit, pairgen (dilution schedule values, byte-identical determinism, zero-override equivalence, winner-filter tally honesty, open-plies divergence, asymmetric open-side divergence + default byte-identical back-compat + meta field, branch-mode determinism), opener-bias runs deterministically, identity-level `.opener(rand,N)@1` randomizes independent of pairgen's own opener flags + rejects unknown/argless opener kinds, opener-swap classifies deterministically |
