# Push size guard - results

Session date: 2026-09-12. No prior plan document. The work started from a
reported symptom (`git push` failing on large files) and the design was settled
with the developer mid-session, so this results doc stands alone. It covers the
history repair that unblocked the push and the guard that prevents a repeat.

## The problem

`git push origin main` failed because `ranking/matches.jsonl` exceeded GitHub's
100 MiB per-file limit inside the unpushed history. GitHub checks every pushed
commit, not only the tip, so sealing the store at HEAD did not unblock it.

This was the second occurrence:

| Date | Unpushed commits | Oversized files in them |
|---|---|---|
| 2026-08-01 | 15 | `ranking/matches.jsonl` up to 270.6 MB, `ranking/games.tsv` 179.2 MB (`plans/store-sharding-results-1-tidy-albatross.md`) |
| 2026-09-10 | 56 | `ranking/matches.jsonl` in 48 commits, 8 distinct versions of 240,077,671 to 581,856,418 bytes |

Cause, the same both times: the store only grows, sealing (`rank.exe seal`) was
a manual step, and nothing checked file size when a commit was made. Pushes
happen only on request, so oversized commits accumulated for weeks before a push
surfaced them.

## History repair

1. **Commit the sealed store.** The working tree's store had been sealed on
   2026-09-11 into six 90 MiB parts (`ranking/matches.0001.jsonl` to `.0006`)
   plus a 19 MB tail. Checked before committing: parts + tail hold 1,139,243
   rows (1,132,483 + the 6,760 recovered in the Round 4 entry of `todo.md`), and
   the previously committed 1,132,483-row store is an exact byte prefix of
   parts + tail concatenated (`cmp` reached EOF on the old store with no
   difference).
2. **Rewrite the unpushed commits.** `git filter-branch --index-filter` over
   `origin/main..main`: in any commit whose `ranking/matches.jsonl` blob exceeded
   100,000,000 bytes, the file was replaced by the last version under the limit
   (blob `2d10ba95`, 39,268,781 bytes, 77,710 rows, set in `51e197e`). A
   `--msg-filter` rewrote commit hashes cited in commit messages to their
   rewritten values. Tried first on a throwaway branch, and adopted only after
   these checks passed:
   - no blob over 100 MB in `origin/main..<rewritten>` (the largest are the six
     94,371,xxx-byte parts)
   - `git diff` between each old commit and its rewrite lists no path other than
     `ranking/matches.jsonl`
   - the tip trees are identical, so the working tree and the running Round 4
     job were untouched (`main` was moved with `git reset --soft`)
   - author, committer, dates and messages identical except three message lines
     whose cited hash changed
3. **Update cited hashes in files.** `d5cb15b` -> `0acfdd1` (`Docs/corrections.md`
   and 7 `ranking/refute_book*.tsv` headers), `778487e` -> `7ccaef6` (`todo.md`).
   In messages, `8b9cd1f` -> `69c6451` as well. The cited `git show d5cb15b^:<path>`
   instructions still work with the new hash, since those files are unchanged.
4. **Push.** The first attempt failed on the network (`curl 55 Send failure:
   Connection was aborted`), not on size: the whole push packed to 94,149,158
   bytes. With `http.postBuffer` set to 524,288,000 in the repo config, a
   two-step push (history up to `14aacbb`, then the tip) succeeded. GitHub
   warned that the six parts exceed its recommended 50 MB, which is a warning
   only.

What the repair cost: in the 48 rewritten commits `ranking/matches.jsonl` is the
77,710-row store, so rating one of those commits does not reproduce its
standings at the time. No row was lost at the tip. The original commits are on
the local branch `backup-pre-push-rewrite`.

## The guard

Two layers, chosen by the developer ("hook + auto-seal").

1. **`.githooks/pre-commit`** refuses a commit that stages any added or modified
   file over 95 MiB (99,614,720 bytes), and prints the seal command for the match
   store. It covers every file, so it would also have caught `games.tsv` in the
   first incident. Git does not enable hooks from a clone, so each clone runs
   `git config core.hooksPath .githooks` once (`INSTALL.md` section 4, root
   `CLAUDE.md`). `.gitattributes` keeps `.githooks/*` LF, since sh fails on the
   CRLF a `core.autocrlf=true` checkout would otherwise produce.
2. **Auto-seal at every writer of the committed store.** `rankAutoSealStore`
   (`src/ranking.cpp`) runs `rankSealStore` at `RANK_STORE_SEAL_MB` = 90, but
   only for a store that has a part index (`<stem>.index.txt`). The index marks
   the committed store, and screening and scratch stores are gitignored, so they
   stay single files. Callers: `rankPlay` when it appended to the store itself
   (not a sharded worker's `--out`), `rankGauntlet` with `--keep`, and
   `tools/run_rank.ps1` after every rung's merge (it tests for the index file
   and calls `rank.exe seal --in <store> --max-mb 90`). `rank.exe seal`'s
   default `--max-mb` is now 90, the value every doc already passes.

## Verification

| Check | Result |
|---|---|
| Hook, scratch repo: 100 MiB file named with a space + 90 MiB file staged | commit refused, exit 1, message lists only the 100 MiB file |
| Hook, scratch repo: the 90 MiB file alone | committed |
| Hook file line endings | 0 CR bytes |
| New `rank` binary (`build/rankcheck/rank_check.exe`), `play` into an indexed 186,433,154-byte store | 2 games played, then "sealed 1 shard(s)", shard 94,371,334 bytes, tail 92,062,429 bytes, index gained the shard, rows 381,564 -> 381,566 |
| Same, store without an index | 2 games played, no shard written, file 186,433,765 bytes |
| `run_rank.ps1 -Workers 2 -NoRate` on an indexed 381,564-row store | merged 2 rows, then sealed 1 shard, index updated, rows 381,566 |
| `rank.exe seal` with no `--max-mb` | "under 90 MB, nothing to seal" on a 94,371,447-byte file (90 MiB = 94,371,840) |
| New unit test "ranking match store - auto-seal acts only on an indexed store" | passes, 9 assertions |
| Full suite, `tests.exe` built from the committed source | all tests passed (5,879 assertions in 237 test cases) |

The smoke-test stores were built from `ranking/matches.0001.jsonl` with every
`tiered@1` row removed and the result doubled, so the roster pair
`rand@1` vs `tiered@1` had no stored games and `--games 2` had to play.

## How to test

```powershell
git config core.hooksPath .githooks      # once per clone
.\tools\run_tests.ps1 -Build             # includes the auto-seal unit test
```

Expected behaviour: staging a file over 95 MiB makes `git commit` fail with the
message above. After `run_rank.ps1` merges a rung into `ranking/matches.jsonl`,
a tail over 90 MiB is sealed and the script prints `sealed N shard(s)`. The new
`ranking/matches.NNNN.jsonl` files and `ranking/matches.index.txt` must be
committed together.

## Commits

- `Note two holes in the replication study's Pass 2 driver: A8's annealing horizon and best-seed tuning` (also committed the sealed store)
- `Update cited commit hashes to the history rewritten under GitHub's file limit`
- `Add a pre-commit size guard and seal the match store at every writer`

## Gotchas

- **The tail rests at up to 90 MiB, not near zero.** `rankSealStore` emits only
  whole shards and leaves the remainder in the tail (92,062,429 bytes in the
  smoke test). The hook's 95 MiB limit sits above that on purpose.
- **Round 4 is not covered.** Its shards hold `rank.exe` open, so the rebuilt
  binary could only be produced under another name, and its driver is the
  `run_rank.ps1` loaded at launch. Its merges will not seal. `todo.md` carries
  the follow-up.
- **`run_tests.ps1 -Build` could not find Visual Studio in this shell**
  (`vswhere` not on PATH, the quirk in the root `CLAUDE.md`), and it deleted
  `tests.exe` before failing. `tests.exe` was built with `cl` directly and run
  with `run_tests.ps1` without `-Build`.
- **A hook is bypassable** with `git commit --no-verify`, and a clone without
  `core.hooksPath` is unguarded. The standing instructions forbid skipping hooks.

## Future Work

- **Unconfigured clones are unguarded.** The hook protects only clones that ran
  the `core.hooksPath` command, and the auto-seal only runs from rebuilt
  binaries. A server-side check would close both gaps: a GitHub Actions job that
  fails when any tracked file exceeds 95 MiB. It cannot stop a push GitHub
  already rejects, but it would flag an oversized file the moment any other host
  or a force-added commit carries one.
- **Stores tracked without an index** (`ranking/matches_open.jsonl`, 2.7 MB) are
  not auto-sealed. The hook refuses them once they pass 95 MiB. Whether they
  should get an index is open.
- **`data/labels/raw_train.jsonl` is tracked at 72.1 MB** although `.gitignore`
  lists it. If it grows past 95 MiB the hook will refuse the commit. Untracking
  it (`git rm --cached`) would settle it.
- **Pack growth from re-committing the tail was not measured.** Each checkpoint
  commit stores a new version of a tail of up to 90 MiB. The 57-commit push
  packed to 94 MB, which suggests appends delta well, but the long-run growth of
  `.git` was not measured.

## Ideas This Inspired

- Seal at 45 MiB instead of 90 so the parts stay under GitHub's 50 MB warning.
- A pre-push hook that prints how many commits are unpushed and how large the
  pack is, so weeks of unpushed history are visible before they cause trouble.
- `rank.exe check` could report the tail size against `RANK_STORE_SEAL_MB`.
