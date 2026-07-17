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
| `test_ranking.cpp` | Ranking unit tests: ID canonical round trips + rejection battery (incl. stale/missing `@N` module versions), learned-model hash checks, codec completeness vs `g_evaluators`, roster parsing, match-row round trip (new cpu/pc/ed fields + old-row sentinels) + board filter, scheduler color balance / incremental top-up / determinism, BT fit accuracy + anchoring + disconnected components, gauntlet 1-D fit, pairgen (dilution schedule values, byte-identical determinism, zero-override equivalence, winner-filter tally honesty, open-plies divergence, asymmetric open-side divergence + default byte-identical back-compat + meta field, branch-mode determinism), opener-bias runs deterministically, identity-level `.opener(rand,N)@1` randomizes independent of pairgen's own opener flags + rejects unknown/argless opener kinds, opener-swap classifies deterministically |
