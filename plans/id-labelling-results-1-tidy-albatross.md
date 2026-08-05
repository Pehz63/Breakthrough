# Universal ID labelling convention -- results

Companion to `plans/store-sharding-results-1-tidy-albatross.md`, which came out
of the same session. That one covers the match-store restructuring; this one
covers the agent-ID grammar change and the migration that carried 381 identities
onto it.

There was no separate plan document. The convention was designed conversationally
over about twenty rounds of developer feedback, and the decisions are recorded in
"Design decisions and who made them" below rather than in a prior plan file.

## The problem

`4-random` and `4-book` name two of the five champion categories in
`ranking/CHAMPION.md`. The developer asked what the `4` counts. In `4-random` it
is plies. In `4-book` it is also plies, but the `2` in `.opener(book,2)@1` is a
SLOT number, and the `6` in `d6` is a depth, and the `4` in `c4` is a weight.
Four different quantities, all bare integers, several of them nominally "in
plies". The request was not to fix those two labels but to adopt a convention
that could not produce the ambiguity again.

## The convention

**Every number carries a label saying what it is, joined to its value by `=`.**

Not what UNIT it is in. Many quantities here are measured in plies, so `ply`
alone conflates a search depth with a dilution depth with an opening cutoff.
`ply=` survives only where the number genuinely is a position on the shared game
clock (the book opener's cutoff).

`_` is reserved for joining words inside a name (`mu_shape`, `tdleaf_self`).
Keeping the two jobs on different characters is what makes
`mu_shape=129-512-8-1` readable.

| Before | After |
|---|---|
| `ab(d6,tt,ord,nb200k)@1` | `ab(deep=6,tt,ord,nodes=200k)@1` |
| `ab(d3,noab,part,asp50,tb250ms,cap2)@1` | `ab(deep=3,noab,part,margin=50,time=250ms,maxdeep=2)@1` |
| `classic(t1,c4,w0,l0)@2` | `classic(chip=100)@2` |
| `classic(t2,c10,w3,l2)@2` | `classic(chip=10,wall=3,column=2)@2` |
| `smart(4)@1` | `smart(pieces=4)@1` |
| `dil(r30,d3)@1` | `dil(prob=30,deep=3)@1` |
| `opener(rand,6)@1` | `opener(rand,moves=6)@1` |
| `opener(book,2)@1` | `opener(book,book=2)@1` |
| `learned(s169,4975683c,value,lin,129-1,con100)@1` | `learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1` |
| `learned(s110,1466db6c,dist,mlp,129-512-8-1,sig129-64-1,con100)@1` | `learned(model=110,1466db6c,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1` |
| `linpol(s1,9f3e21aa)` | `linpol(model=1,9f3e21aa)` |

Every superseded spelling still PARSES, so the store's existing rows keep
resolving to the agents the roster names. Only the current spelling is ever
EMITTED, and `rankUpgradeId()` rewrites a stored id into it on read.

### Weights became a subset, with four suppression rules

Previously an evaluator segment listed ALL of its registry parameters. Now it
lists only the ones that say something about the agent, and an omitted weight
takes its registry default. Four rules, each with a reason the developer gave:

1. **A weight that is ZERO when its DEFAULT is zero** is not emitted: an
   optional term that was simply never switched on.
2. **A non-default zero IS emitted.** `racewin=0` (default 1) survives. The rule
   is deliberately not "hide zeros", which would silently re-enable it. This
   correction came from the developer directly: "if the default value is not 0
   then hiding a 0 violates the spirit of what I said."
3. **The TURN weight is elided unless the head has `qs` or `part`.** At fixed
   depth every leaf shares one ply parity, so the turn term shifts them all by
   the same constant and reorders nothing. It goes live only at mixed leaf
   parity. This one was already recognised in `standings.tsv`'s `eff_evaluator`
   column; the id now agrees with it.
4. **A Classic whose only ACTIVE weight is chip is written `chip=100`,**
   whatever its stored value. A lone weight has no ratio to any other, so its
   magnitude is meaningless, and scaling the sole live term is a monotonic
   rescale of every leaf that cannot reorder a single move. 100 matches the hill
   climber's sum-to-100 convention. This collapses `c4` / `c10` / `c100` onto the
   one identity they have always played as.

Rule 4 was checked for collisions before adopting: 54 roster agents and 95 store
agents are chip-only-active, and the rescale introduced **0** collisions.

### `@N` is hidden from printed output only

`rankDisplayId()` drops each segment's `@N` when N is already that module's
current version. A retired identity pinned at an older version keeps its `@N`,
which is exactly the case where the number is load-bearing: `classic(...)@1`
beside a live `classic(...)@2` is a different player.

Console tables only. Every FILE keeps the full canonical id, because the match
store is keyed by it and `standings.tsv`/`ratings.tsv` are read back by tooling.
`rankAgentId` is untouched, so nothing functional ever sees the short form.

```
 rank    Elo    +/-   games  ...  id
    3   1103       9    2416  ...  ab(deep=8,tt,ord,nodes=2m).classic(chip=100)
    4   1099      23     504  ...  ab(deep=6,ord,nodes=200k).classic(chip=100)@1 (retired)
```

## The migration, and how it was verified

The store holds 192,639 rows keyed by the old spellings; the roster now carries
only the new ones. `rankUpgradeId()` bridges them on read.

**Verified against a build of HEAD, not against memory.** The pre-migration
sources were checked out (`git stash`), `rank.exe` rebuilt, and `rate` run to
produce a baseline; the working tree was then restored, rebuilt, and rated again.

| | Pre-migration build | Post-migration build |
|---|---|---|
| Rated agents (`ratings.tsv`) | 381 | 381 |
| Active agents (`standings.tsv`) | 170 | 170 |
| Total games | 192,639 | 192,639 |
| Elo multiset | -- | **identical** |
| Games multiset | -- | **identical** |
| Rank-ordered Elo sequence | -- | **identical** |

Aggregate identity is necessary but not sufficient: a swap of two agents'
ratings would preserve every multiset above. So each of the 381 pre-migration ids
was pushed through `rank.exe canon` and joined to the new fit by the resulting
string:

- 381 old ids -> **381 distinct** new ids (no merges, no splits)
- 288 changed spelling, 93 were already canonical
- **0** unmatched, **0** elo/games mismatches

### The bug that verification caught

The first migration attempt reported **315,664** games where the baseline had
**296,650** -- **+19,014 phantom games**. (Both counts are from the pre-TD-Leaf-drop
population; the current store is 192,639 rows.)

Cause: `rankAgentId` re-emits each module's CURRENT version, so re-emitting a
stored `classic(...)@1` produced `classic(...)@2`. That merged **49 retired
identities into their live successors**, combining game histories across agents
that are deliberately distinct -- module versioning exists precisely so a
behaviour change re-identifies the affected agents.

Fix: `rankUpgradeId` preserves every segment's `@N` verbatim, rebuilding the
canonical form and then restoring each original version token. Re-verified at
296,650 -> 296,650 with 0 merges.

This is the whole argument for the "validate the instrument before quoting the
reading" rule. The aggregate Elo table looked entirely plausible with 49 agents
silently merged into other agents.

## A real defect found while fixing the tests

`canonicalizeLearnedIds()` read the slot number at the wrong offset. It scanned
for `.learned(` and set `open = pos + 9`, then took the payload from `open + 1`
-- correct only for the old `.learned(s` form, where index `pos+9` is the `s`.
Under the new `model=` label it consumed the `m`, so `atoi("odel=169")` returned
0, the hash check against slot 0 failed, and the function silently did nothing.

It had three callers:

1. **The strict-parse legacy alias.** Broke the legacy `learned(sN,hash8)` form
   that lets a hand-written roster line name a learned agent by slot and hash
   without spelling out an architecture only the model file knows.
2. **`rankSplitStore`.** This is the serious one. Split buckets rows by whether
   both agents are rostered. With the roster on new labels and the store on old
   ones, an id-comparison that does not modernise labels files **every live
   agent's games under `retired_other`**. Running `split --apply` in that state
   would have regrouped the whole store wrongly. Nothing would have been lost
   (split verifies row counts before deleting), but the index would have been
   badly wrong.
3. **`loadIdList`**, the `--cohort` file reader, where a mismatch silently
   schedules zero games -- a failure mode this project has already hit once.

Fixed by replacing the helper entirely: callers 2 and 3 now use `rankUpgradeId`
(a superset), and caller 1 uses a new `learnedSpellingOnly(id, canon)` predicate
that accepts an id differing from its canonical form ONLY inside learned()'s
parentheses, with the (slot, hash) identity pair still agreeing. That covers all
three generations of superseded descriptor -- legacy two-arg, stale regime
vocabulary, old labels -- and rejects everything else, so a stale `@N` or a
legacy weight spelling is still reported with the current form to paste.

## Test suite

22 failing cases at the start of this segment, then 13, now **0**: the full suite
is green at **127 test cases / 3064 assertions**.

The failures fell into three groups, and the third is the interesting one:

- **Stale expectations.** `evalParams[1] == 4` where the chip rescale now yields
  100; `rankAgentRegime` expecting the pre-2026-08-01 token.
- **Rejection tests whose inputs became legal.** A mechanical spelling conversion
  had rewritten the INPUT of `parseErr("smart(04)@1")` into `smart(pieces=4)@1`,
  which parses fine, so the test asserted that a valid id is invalid. These were
  rewritten against genuinely non-canonical inputs rather than relabelled. Same
  for `classic(t1,c4,w0)` (once "missing weight", now a legal subset), replaced
  with an unknown weight key.
- **Fixtures that failed for the wrong reason.** Two `rankLabel` ladders and a
  `rankSplitStore` `--group` string still held old spellings, so those tests
  returned the right value via the wrong path -- an id-spelling error rather than
  the deterministic-pairing rejection and bucket grouping they exist to check.
  A third, the resume test, was failing on a `.meta.json` sidecar left behind by
  an EARLIER build, so it was asserting against a previous run's ladder.

New coverage added: the weight-subset rules (omitted weight takes its default,
non-default zero survives, turn elided without `qs` but carried with it, chip-only
rescale), and `rankDisplayId` dropping only current versions while returning
unparseable input verbatim.

## Documentation migration

269 embedded ids across 12 live documents were rewritten. Not by hand and not by
a bespoke regex: the mapping is the SAME verified old -> new pairing the store
migration was checked against, pushed through `rank.exe canon`, so a doc cannot
drift from what the parser accepts.

Two passes were needed. The first handles whole ids and their positionally-aligned
segments; the second handles bare segments quoted without their `@N`
(`ab(d6,tt,ord,nb200k)` on its own), which is how prose usually cites one, plus a
small family of label rewrites for deliberately elided forms (`learned(s98,...)`).

Verified three ways:

- Both passes re-run as **no-ops** (0 replacements across all files).
- All **36** complete ids quoted in docs come back from `rank.exe canon`
  unchanged, so every pasteable id in the documentation is canonical.
- All **7** roster files parse strictly under `rank.exe check`.

`plans/` was deliberately left alone: those are archived records of what was
written at the time.

## Design decisions and who made them

Every one of these is the developer's, recorded because the reasoning does not
survive in the code:

- **Labels, not units.** "It's unclear whether '4-random' refers to 4 plies or 4
  moves. Come up with a solution that not only works for this case, but a
  convention that can be adopted universally."
- **Number after the label**, matching the grammar's existing shape.
- **Descriptive over terse.** `deep6` over `6ply` or `dply`: "for 'deep' it can
  be assumed that it's ply. Deep is descriptive enough... 'dply' is just awkward."
  And `deep6` over the familiar `d6`: "d6 is only fine because it's so common,
  but deep6 is just better."
- **`margin` for the aspiration window**, chosen after asking for an explanation
  of what the parameter does rather than accepting the jargon term.
- **A separator between label and number.** "'model111' is hard to read because
  the l and 1 are so similar."
- **`=` for label-to-value, `_` for words-within-a-name**, the final refinement:
  `mu_shape=129-512-8-1` needs both jobs and they must not collide.
- **Spell out `mu_shape`/`sigma_shape` rather than implying them by position.**
  "Don't leave it implied by the dashes, that's a very common/useful symbol to
  reserve arbitrarily like this."
- **Hide obvious defaults and disabled features, not zeros** (see rule 2 above).
- **`conn=100` and the current `@N` are obvious defaults.** `conn` is hidden
  everywhere; `@N` was then narrowed to printed output only: "just hide it from
  ephemeral printed outputs. Keep the version in permanent documents and anywhere
  functional that's needed."
- **`chip=100` for a bare chip counter**, for consistency with the hill climber's
  sum-100 constraint.
- **Confirm before acting on a multi-part change.** "Before you just do stuff,
  confirm what you're doing with me... this might as well be one pass instead of
  a bunch of passes which increases risk of miscommunication or getting in a bad
  state."

## Environment gotchas

- **Builds must be launched from the PowerShell tool, not the Bash tool.**
  `vcvars64.bat` does not survive the MSYS environment: the build hangs
  indefinitely with `cmd` processes at 0.0% CPU and no `cl.exe` ever spawning.
  Roughly 14 hours were lost to this before it was diagnosed. From PowerShell the
  same build takes about 17 seconds.
- **`build_tests.bat`'s `vswhere` lookup fails inside its `for /f`,** though
  `vswhere.exe` runs fine when invoked directly. Use the explicit
  `vcvars64.bat` path from the root `CLAUDE.md` as the documented fallback.
- **Never run two builds or two `tests.exe` concurrently.** `tests.exe` holds a
  file lock that produces `LNK1104`, and `rankPosGen` APPENDS rather than
  truncating, so concurrent runs doubled `build/pool_tr.jsonl` and produced a
  false "globally unique" failure.
- **`Out-File` wraps at the console width.** Writing a list of long ids to a file
  with it silently split them across lines, desynchronising a 65-element mapping
  into 490 lines. Use `[System.IO.File]::WriteAllLines`.
- **The test suite still overwrites `models/manifest.{json,md}`.** Restore them
  before committing. Tracked in `todo.md` under Data + Infrastructure.

## Future Work

- **The chip rescale is unverified for `exp` and `adv`.** Rule 4 fires whenever
  chip is the only active weight, for ANY evaluator with a Chip parameter, but
  the 0-collision check was run over the current roster and store, where the
  chip-only agents happen to all be Classic. An `exp`/`adv` agent with every
  other weight at zero would rescale too. That is correct by the same argument
  (a monotonic rescale of the sole live term), but it has not been observed in
  practice. A test constructing such an agent for each evaluator would settle it.
- **`learnedSpellingOnly` accepts any descriptor difference inside the
  parentheses, checking only (slot, hash).** That is deliberate -- the
  architecture fields are descriptive and re-derived -- but it means a
  hand-corrupted descriptor is silently replaced rather than reported. Whether
  that should warn depends on whether anyone ever hand-writes a rich learned
  form, which so far nobody has.
- **`rankDisplayId` is applied to three console surfaces** (the ranked table and
  the two per-game progress lines) and deliberately NOT to `rank.exe check`,
  whose output mirrors a roster file the user edits and pastes from. Other
  console surfaces that print ids -- `gauntlet`, `matchup`, `opener-bias`,
  `opener-swap` -- were left alone. Whether those count as ephemeral is a
  judgement call that has not been put to the developer.
- **The `ab(default)` nickname was proposed and never resolved.** The developer
  floated it ("Hide things like the default ab configuration because it's reused
  so much. Or maybe just give it a nickname... make it be 'ab(default)'"), then
  the conversation moved to other refinements and never returned. The migration
  ran without it and the developer has approved the resulting roster, so the
  current reading is that it was superseded by the `@N` elision. It would need a
  fresh decision, and it is a bigger change than `@N` was: `deep=6` vs `deep=8`
  is a real strength difference, so a nickname hiding the whole head would hide
  something load-bearing in a way a current-version marker does not.

## Ideas This Inspired

- **A `rank.exe lint` subcommand** over documentation, not just rosters. The doc
  verification here (extract every backtick-quoted complete id, push through
  `canon`, assert unchanged) is a three-line check that would have caught 269
  stale ids the moment they went stale. It generalises to any convention change.
- **Version the grammar itself.** Segment `@N` versions the MODULE's behaviour.
  Nothing versions the spelling, so a reader cannot tell whether a doc predates
  the label convention without recognising the old form by eye.
- **Emit the elided turn weight into `report.md` as a diagnostic column.** The id
  now hides it and `eff_evaluator` elides it, which is right for comparison but
  means a hill-climbed agent's stored turn weight is no longer visible anywhere
  a reader looks. It still matters the moment `qs` is switched on.
- **The developer's iteration pattern is worth capturing as a workflow.** Twenty
  rounds of single-question refinement produced a materially better convention
  than the first proposal, and the explicit instruction mid-way ("this might as
  well be one pass instead of a bunch of passes") marks the point where
  batching became more valuable than iterating. That transition -- diverge while
  the design is open, batch once it is converging -- is a reusable rule.
