# Split the single champion into 5 opener-based categories

## Context

The project currently declares ONE reigning champion (`ranking/CHAMPION.md`): the
highest-Elo target-class agent in the full-roster anchored Bradley-Terry refit at
the standard head `ab(d6,tt,ord,nb200k)@1`. The current champion wears an opening
book (`.opener(book,11)@1`, 16-ply), and `todo.md` already flags an open, unresolved
question: should book-augmented agents even be champion-eligible, or does the throne
need a separate track? (Six of the last top eight target-class agents wear a book;
theory 38 shows a book's Elo lift is a memorized-line artifact that collapses once
the opponent stops reproducing the replies it made when the book was mined.)

The developer's resolution: stop picking a side, split the throne into 5 parallel
category-champions instead, one per opener loadout:

1. **openless** -- no `.opener(...)` ID segment at all.
2. **4-book** -- wears `.opener(book,N)` where book N was mined at `--plies 4`.
3. **8-book** -- same, mined at `--plies 8`.
4. **4-random** -- wears `.opener(rand,4)@1` (uniform-random for the agent's own
   first 4 plies, then hands off).
5. **8-random** -- same, `.opener(rand,8)@1`.

"4"/"8" mean opener PLIES (confirmed), not book slot numbers or ensemble sizes.

**Critically: this stays ONE roster, ONE match store, ONE Bradley-Terry fit.**
The developer explicitly rejected splitting into a second incompatible-scale pool
(the way `ranking/roster_open.txt`/`matches_open.jsonl` already does for random
openers today) AND rejected using a second boosting file
(`ranking/roster_top.txt`) to raise precision on just the new agents. The goal
is deliberately broader than "efficiently resolve 5 close calls": add a good-sized
batch of new opener-wearing identities directly to `ranking/roster.txt`, play them
into the SAME store as everyone else, and read each category's champion straight
off the resulting `ranking/standings.tsv`, filtered by which `.opener(...)` segment
an ID carries. More agents wearing more openers is treated as a feature (Elo
diversity, richer late-game variety from the same core under different openers),
not overhead to be minimized. If a top-of-category call comes out too close to
separate confidently at the standard screening game count, that gets reported
honestly (the same way the current champion is reported "statistically tied with
the runner-up") rather than solved with extra machinery in this pass.

**Confirmed consequence, developer sign-off obtained:** under this exact taxonomy,
the current champion (`book11`, 16-ply) does not fall into either book category. It
retires with no title in any of the 5 tracks -- stays in `ranking/roster.txt` and in
`CHAMPION.md`'s historical lineage table as a data point, but is not a category
champion. No replacement rung is being added to preserve its former slot.

## Category eligibility rule (state once in `CHAMPION.md`, reuse everywhere)

Within the standard head `ab(d6,tt,ord,nb200k)@1`, excluding reference-class agents
(currently just the d8/nb2m oracle, per existing `CHAMPION.md` convention), a
target-class agent's canonical ID places it in exactly one of the 5 categories:
- no `.opener(...)` segment -> **openless**
- `.opener(book,N)@1` where `models/bookN.txt`'s mining depth was 4 -> **4-book**
- `.opener(book,N)@1` where depth was 8 -> **8-book**
- `.opener(rand,4)@1` -> **4-random**
- `.opener(rand,8)@1` -> **8-random**
- any other opener depth (6/16/30/60-ply books, `rand,6`, etc.) is not a member of
  any of the 5 categories -- it's ladder/study data, not a title contender.

Champion = highest Elo within a category's membership, from `ranking/standings.tsv`
(never `ratings.tsv`), same fit, same head, for all 5 categories at once.

## Step 1 -- Mine 4 new book files (free: replays already-stored games, no new play)

Next free book slot is 13 (book1-12 exist in `models/`). Mine 4-ply and 8-ply own
books for the two cores whose existing 6/16/30/60-ply ladders already established
the exact `--a`/`--b` pair to replay (`ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`
vs `ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1`, the pair
behind book7/8/9 and book10/11/12):

```powershell
.\rank.exe bookgen --a "ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2" `
                    --b "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1" `
                    --plies 4 --out models\book13.txt
.\rank.exe bookgen --a "ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2" `
                    --b "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1" `
                    --plies 8 --out models\book14.txt
.\rank.exe bookgen --a "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1" `
                    --b "ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2" `
                    --plies 4 --out models\book15.txt
.\rank.exe bookgen --a "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1" `
                    --b "ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2" `
                    --plies 8 --out models\book16.txt
```

**Ordering hazard (must run before Step 2):** `bookForSlot()` (`src/ai_random.cpp`)
loads `models/book<N>.txt` lazily and SILENTLY -- a missing file makes the `book`
opener permanently return false for that process with no warning anywhere. If any
roster line below is played before its book file exists, those games get recorded
as "book-equipped" while the agent actually played bare, and `matches.jsonl` is
append-only/never regenerated, so that bad data is permanent. Mine first, verify
each file is non-empty, then edit the roster.

Sanity check on entry counts against the existing sibling rungs: book13 (4-ply,
classic-own) should have <= book7's count (6-ply, 13 entries); book14 (8-ply)
should sit between book7 (13) and book8 (16-ply, 38); book15 (4-ply, s98-own)
should be <= book10's count (6-ply, 41); book16 (8-ply) should sit between book10
(41) and book11 (16-ply, 134). A violation signals swapped `--a`/`--b`.

## Step 2 -- Add new agent identities to `ranking/roster.txt`

Append one new block after the existing "Book-loadout cohort" section (after line
241). All new lines `on`, all at the standard head, all bare target-class cores
that already have an established Elo in `ranking/standings.tsv`.

**4-book / 8-book (4 new lines):**
```
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,13)@1        # classic-own, 4-ply
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(book,14)@1        # classic-own, 8-ply
on      ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,15)@1   # s98-own, 4-ply
on      ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(book,16)@1   # s98-own, 8-ply
```

**4-random / 8-random (20 new lines):** every bare target-class core currently
>= ~1000 Elo in the standard-head standings, plus the universal `classic` control,
each wearing `.opener(rand,4)@1` and `.opener(rand,8)@1`:

```
on      ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s76,ef183148,dist,lin,129-1,sig129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99,value,lin,129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s6,eac8ab99,value,lin,129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s3,68364898,value,lin,129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s3,68364898,value,lin,129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e,value,lin,129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974,dist,mlp,129-512-8-1,sig129-64-1,con100)@1.opener(rand,4)@1   # 370ms/move core, cost flag
on      ab(d6,tt,ord,nb200k)@1.learned(s111,78ef6974,dist,mlp,129-512-8-1,sig129-64-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s7,c7f7ce61,value,lin,129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s7,c7f7ce61,value,lin,129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s96,990e39e7,value,lin,129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s8,6f1a4264,value,lin,129-1,con100)@1.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s8,6f1a4264,value,lin,129-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e,dist,mlp,129-512-8-1,sig129-64-1,con100)@1.opener(rand,4)@1   # NNUE-wide core, cost flag
on      ab(d6,tt,ord,nb200k)@1.learned(s113,e3cc8b4e,dist,mlp,129-512-8-1,sig129-64-1,con100)@1.opener(rand,8)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,4)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2.opener(rand,8)@1
```

24 new lines total, 0 removed/modified -- purely additive, no existing agent's
canonical ID or history changes. Note these `.opener(rand,4)@1` IDs share exact ID
text with entries already in `ranking/roster_open.txt`; that's fine and intentional,
since `matches.jsonl` and `matches_open.jsonl` are separate stores -- this roster
addition builds independent history for the same identity in the merged pool,
answering a different question (single-scale category ranking) than the
opener-bias-controlled pool answers.

The `adv(t20,c77,...)` core is deliberately excluded from the random-opener
expansion: its existing book variants live at a different search head
(`ab(d6,ord,nb200k)@1`, no `tt`), and mixing heads within one category violates
the project's own "fix ONE search head per comparison" rule. It can be added in a
follow-up if a same-head `adv` variant is ever built.

## Step 3 -- Play and rate (one command, one roster, one store)

```powershell
.\tools\run_rank.ps1 -Workers 8 run --games 8 --paired-openings
```

- Default `--roster ranking/roster.txt`, default `--in`/`--out ranking/matches.jsonl`
  -- no overrides, this is the one and only pool.
- `--games 8` matches this roster's existing convention; already-played pairs need
  0 new games (the scheduler only plays what's missing), so this call plays exactly
  the new pairs to 8/pair.
- `--paired-openings`: inert for the ~116 pre-existing deterministic pairs, and
  gives every pair touching a new `.opener(rand,N)` agent a colour-swapped couple
  that shares one seed (same random opening, colours reversed) instead of pure
  luck-of-the-draw variance -- strictly beneficial to add now that rand-opener
  agents join this store for the first time.
- No `-Build` needed (no `src/` changes).

Cost estimate: 24 new IDs against ~116 existing active agents, plus new-vs-new
pairs among the 24 = 24*116 + C(24,2) = 2784 + 276 = 3060 new pairs x 8 games =
**~24,480 new games**. This is the plan's entire compute cost -- no second boosting
phase.

## Step 4 -- Read standings, declare the 5 champions

Read `ranking/standings.tsv`, filter to `head == ab(d6,tt,ord,nb200k)@1` and
`active == on`, exclude the d8/nb2m oracle, and bucket every row by its
`.opener(...)` segment per the eligibility rule above. The highest-Elo row in each
bucket is that category's champion. Re-quote **openless** from this SAME
post-expansion fit too (not the pre-expansion 1077 +/- 9 number) for consistency --
adding 24 agents to an anchored Bradley-Terry fit shifts every existing agent's Elo
slightly, and absolute Elo is never compared across fits.

If any category's top two are within a few combined SE (as already happened with
the pre-split champion vs its book10 rung), report it exactly that way -- "X and Y
are statistically tied" -- rather than manufacturing more precision. Note both
nominal and distinct-game counts for any deterministic pair per the project's own
hygiene rule (only the `rand,4`/`rand,8` pairs get genuinely independent trajectories
from `--paired-openings`; book-opener pairs and bare-vs-bare pairs stay subject to
the existing cross-game-TT-state caveat).

## Step 5 -- Rewrite `ranking/CHAMPION.md`

Restructure from one champion to 5 parallel sections:

- **Shared intro**: explain the split (why -- resolves the book-eligibility
  question from `todo.md` by splitting instead of picking a side), state the 5
  category definitions and the eligibility rule verbatim, restate the shared
  certification rules once (full-roster anchored refit is the instrument; read
  `standings.tsv` not `ratings.tsv`; fix the one standard head; the d8/nb2m oracle
  is reference-class in every category; never compare absolute Elo across fits).
- **Summary table**, 5 rows, columns: `Category | Champion ID | Elo +/- SE | Fit
  date | Nearest rival (gap)`.
- **Five per-category sections**, each shaped like the current single-champion
  section: ID, certified date+fit, Elo+SE, a short "what the agent is" paragraph,
  a "nearest rivals" table (same columns as today's), a category-scoped Lineage
  table, a category-scoped Defended-challenges table (starts empty/"None yet").
- **Pre-split history**: keep the existing single-champion Lineage and
  Defended-challenges tables under a renamed heading ("Pre-split lineage,
  single-champion era, until 2026-07-28") as a permanent historical record rather
  than retrofitting them into 5 copies. Note book11's retirement explicitly there
  (highest-Elo book agent at the point of the split, but not a member of either
  new book category, no title carried forward).

## Step 6 -- Update the surrounding docs

- **`todo.md`**: strikethrough (not delete) the "Decide whether book agents are
  champion-eligible" item, with a resolution note pointing at `ranking/CHAMPION.md`.
  Rewrite the Agent Track "Goal:" paragraph to reference the 5-category system.
  Add a `[Next]` item for growing the category pools further (more cores wearing
  4/8-ply openers, e.g. `s7`/`s96`/`s8` bookwise, once a stable bookgen `--b`
  target convention is picked for cores whose historical books were mined against
  a moving "champion" target rather than a fixed pair).
- **`tools/CLAUDE.md`**: 4 new rows in the "Mined books" table for book13-16
  (line owner / target / depth / entries / kept-replays -- entries/kept-replays
  filled in from Step 1's actual output).
- **Root `CLAUDE.md`**: the "Champion declaration and ranking-claim hygiene"
  Standing Instruction describes a single champion today. **Do not rewrite it
  without a specific confirmation first** (per this project's own meta-rule for
  discretionary CLAUDE.md changes) -- ask via a multiple-choice question at
  execution time whether to (a) generalize "the champion" -> "each category
  champion" pointing at `CHAMPION.md`'s summary table, or (b) name the 5
  categories inline in the bullet itself.
- **`plans/`**: archive this plan (`<topic>-plan-N-<suffix>.md`) plus a companion
  results doc (`<topic>-results-N-<suffix>.md`) with the actual Elo numbers,
  distinct-game-count caveats, what was harder than expected, Future Work, and
  Ideas This Inspired, per the project's standing "after every functional change"
  workflow.
- No `src/`/`tests/` changes anywhere in this work, so `run_tests.ps1 -Build` is a
  no-op for this commit per the project's own exception rule -- skip it.

## Verification

1. **Book mining**: each `bookgen` call prints a kept-replay count and entry count;
   cross-check against the sibling-rung sanity bounds in Step 1.
2. **Roster parse**: `.\rank.exe check` after the Step 2 edit -- confirm 140 active
   agents, no parse errors on the 24 new lines, and `git diff ranking/roster.txt`
   shows a pure addition (0 changed/removed lines).
3. **Play/rate**: after Step 3, `ranking/standings.tsv` contains all 24 new IDs at
   the standard head with plausible game counts; spot-check a couple of book13-16
   games actually used the book (non-bare opening moves) rather than having
   silently played bare (the Step 1 ordering hazard).
4. **Declaration**: the 5 champions named in `ranking/CHAMPION.md` are each the
   verified highest-Elo `standings.tsv` row in their category bucket, and every
   number in the doc is tagged with the fit date per existing convention.

### Critical files
- `ranking/roster.txt` -- 24 new lines
- `models/book13.txt`..`book16.txt` -- new, mined
- `ranking/CHAMPION.md` -- restructured, 1 champion -> 5
- `todo.md` -- resolved item struck through, goal paragraph updated
- `tools/CLAUDE.md` -- Mined-books table +4 rows
- `CLAUDE.md` (root) -- gated rewrite, confirm wording at execution time
- `plans/` -- new plan + results doc pair
