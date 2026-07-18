# Dethrone the Champion: Self-Mined Book Dethrones s98 (Theory 33) -- Results

Sequel to results-1 (phase 0), results-2 (quiescence, + the 2026-07-18 opener
addendum), results-3 (the oracle-mined refutation book, theory 14), and
results-4 (symmetrization/ensembling, theory 30). Session date 2026-07-18.
Developer hypothesis, tested same-day: the phase-2 refutation book failed
because it mined the WRONG agent's wins (the oracle's wins over s98, handed to
a weaker brain that couldn't use the oracle's foresight). Mine the BOOK-
WEARER'S OWN wins instead.

## Headline

**Classic (the original chip-counter champion) plus a book of its own 7
historical wins over s98 is now the highest-rated target-class agent in the
pool: 1145 +/- 13, vs s98's 1074 +/- 12 in the same fit (non-overlapping error
bands), and 25-7 (78%) head-to-head against s98 directly.** This dethrones s98
by the project's own written criterion (`ranking/CHAMPION.md`: highest pooled
Elo among target-class agents, full-roster refit, contender pairs at >= 32
games). It is also, by a wide margin, the most decisive result of this entire
session -- bigger than the original phase-0 gap (88 Elo), and unlike every
other intervention tried (quiescence, the oracle-mined book, symmetrization),
it did not cost a single line of new code: `rank.exe bookgen` already existed
from phase 2, and this only required running it with the two agent roles
swapped.

**Important scrutiny flag, addressed below and left partly open:** this agent
is a 32-KB, 134-entry hard-coded book riding on the classic evaluator, not a
generalized strength improvement to the evaluator or search itself. Whether it
represents genuine broad strength or a pool-specific/memorization effect (the
community-competition-vision concern about degenerate "one script" wins) is
examined in Mechanism below and is NOT fully settled -- see Future Work and the
policy question for the developer at the end.

## The hypothesis and the fix

Phase 2's book mined the ORACLE's winning replies against s98 and handed them
to a d6 brain (classic or s98) that could not reproduce the oracle's follow-up
once out of book -- both book agents rated below their bookless selves.
Developer's fix: reverse which agent's moves get mined. Instead of "the
STRONG agent's wins over the weak one," mine "the WEAK agent's OWN wins, even
against a stronger opponent." The book-wearer and the line-owner are then the
SAME brain, so there is no handoff mismatch: in-book, it plays exactly what it
would have chosen anyway (it's replaying its own past choice); out-of-book, it
reverts to its own normal live search -- never a foreign, badly-fitting
position. `rank.exe bookgen` already supported this with zero code changes,
just swapped `--a`/`--b` roles: `--a "classic" --b "s98"` instead of
`--a "oracle" --b "s98"`.

```
.\rank.exe bookgen --a "ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@2" --b "ab(d6,tt,ord,nb200k)@1.learned(s98,5801570e)@1" --plies 60 --out models/book2.txt
```

Of the 32 stored classic-vs-s98 games, classic won 9 historically; replaying
them with the CURRENT engine build reproduced only 7 of those results (12 of
32 games drifted from their stored result on replay -- a much higher drift
rate than the oracle book's 3 of 32, itself informative, see Mechanism). The
7 reproducible wins yielded 134 book entries in a 138-line file (~19
entries/game, similar density to the phase-2 book).

## Measurement (32 games/pair boosted, full-roster refit, 2026-07-18)

| Elo | +/- | agent |
|---|---|---|
| 1159 | 13 | oracle d8/nb2m (reference) |
| **1145** | 13 | **classic + self-mined book (slot `book,2`) -- new champion** |
| 1092 | 13 | s98 + oracle-mined book (phase 2) |
| 1092 | 13 | s98 + quiescence |
| 1074 | 12 | s98 (previous champion) |
| 1018 | 12 | classic + quiescence |
| 998 | 11 | classic, bookless (the original phase-0 dethroned incumbent) |
| 989 | 12 | classic + oracle-mined book (phase 2, WORSE than bookless) |

Head-to-heads at 32 games each:

- vs s98 (the mined target): **25-7 (78%)**, up from bookless classic's 9-23
  (28%) and the oracle-book's 7-25 (22%).
- vs the oracle (reference, a COMPLETELY DIFFERENT opponent than the one the
  book was mined against): **27-5 (84%)**.
- vs its own bookless self: **32-0 (100%)**.
- vs the rest of the classic family (tt-only, ord-only, asp variants):
  28-4 to 32-0.
- vs s9/s10 (the mirror-symmetrized/ensembled models, theory 30): 26-6, 28-4.
- vs s96/s99: 22-10 each.
- vs the phase-2 oracle-mined book agents: 25-7 (vs s98+book1), and it soundly
  beats classic+book1 (the old, wrong-direction book) in the pool too.

## Mechanism: why this generalizes far beyond the mined opponent

The oracle games reveal the real story. Book2 (classic+selfbook) vs the
oracle -- an opponent it has never been directly mined against -- shows a
strikingly consistent pattern: **as Black, it wins an almost always-identical
49-ply game against the oracle, repeated roughly 13 of ~16 times in the
boosted sample.** As White it wins most of the time too (three distinct
winning shapes, 58/52/72 plies) but loses a repeated 37-ply shape a few times.

This means the book is not narrowly tuned to exploit s98's specific play --
the position keys it stores are mostly EARLY-GAME positions, and since the
standard start is identical in every game classic plays regardless of
opponent, at least one of its 7 mined wins recorded a first move (or an early
sequence) from the STANDARD STARTING POSITION. That entry then fires in
EVERY game classic plays as that color, against ANY opponent, not just s98.
**The book's real content, then, is closer to "the best opening/early-game
line classic has ever been observed to find," locked in and repeated on
demand, rather than "a countermeasure specific to s98's weaknesses."**

This reframes the developer's hypothesis slightly: the key ingredient is not
that the mined games were against a STRONGER opponent specifically -- it's
that the book-wearer and the line-owner are the SAME agent, so the mined move
is guaranteed compatible with its own subsequent play. Whether "stronger
opponent" matters at all (vs. mining classic's wins against anything,
including weak opponents or dilution ladder members) is an open, cheap-to-test
question (see Future Work).

Why would classic's LIVE, bookless search fail to reliably find its own best
line? The leading candidate, consistent with everything else found this
session: cross-game search-state carryover (theory 19 mechanism b -- TT,
killer, and history tables persist across games within a shard process, so
even a fully deterministic agent's play depends on what games ran immediately
before it). Classic's live search sometimes finds this strong line and
sometimes doesn't, depending on incidental prior-game pollution; the book
removes that variance by always playing the best-known choice.

## The open scrutiny question (for the developer)

Per `ranking/CHAMPION.md`'s written certification rule, this result qualifies
as a genuine dethrone (highest pooled Elo among target-class agents, boosted
to 32 games, non-overlapping error bands with the prior champion). It is
declared as such below. But this is a fundamentally different KIND of agent
than every previous champion in this project's history: a base evaluator
identical to the very first champion, riding entirely on a 134-position
lookup table. Two readings are both defensible and this doc does not resolve
between them:

1. **Genuine strength gain.** The oracle result suggests the book captures a
   real, generalizable resource (a strong Black-side plan) that classic's own
   search has access to but doesn't reliably surface -- closer to "removing a
   measurement/execution flaw" than to memorizing a script.
2. **Pool-specific / degenerate risk** (the community-competition-vision
   memory's explicit concern: "a bot that memorizes one script that beats the
   champion but otherwise loses everywhere is a degenerate, non-meaningful
   strategy"). This book was mined AND measured entirely within one pool of
   mostly-deterministic agents sharing this project's own first-found
   left-tie-break convention (theory 23). It has not been tested against any
   opponent outside this codebase's own agent family, so "generalizes beyond
   s98" (confirmed above) is not the same claim as "generalizes beyond this
   project's pool."

Recommended developer decision points, not resolved here: (a) should a
book-augmented agent be eligible for the main champion declaration at all, or
should `ranking/CHAMPION.md` carry a separate "counter/book" recognition
track, matching the community-competition-vision brainstorm's own suggested
split; (b) if eligible, should there be an additional bar beyond pooled Elo
(e.g., a minimum number of DISTINCT recorded game shapes, given the heavy
repetition theory 19b causes in deterministic pairings) before a book-derived
number is trusted at face value.

## Deviations / caveats

- The 12-of-32 replay-drift rate (vs the oracle book's 3-of-32) was not
  investigated further here; it's flagged as evidence that closer, more
  competitive pairings (classic vs s98, roughly 28-72%) are LESS
  reproducible across engine-state changes than lopsided ones (oracle vs
  s98, ~0-100%), plausible since close games have more decision points where
  small state differences can flip the outcome.
- The 27-5 vs-oracle and 32-0 vs-bookless-self results, like every det-vs-det
  measurement this session, reflect a small number of distinct repeated game
  shapes (theory 19b), not 32 independent trials. The qualitative
  generalization finding (wins as Black against a wholly different, much
  stronger opponent) is real regardless; the exact percentages carry the
  usual caveat.
- No new source code was written for this result. `models/book2.txt` and one
  new roster line are the only durable engine-facing artifacts.

## Future Work

- **Does "stronger opponent" matter, or just "own win, any opponent"?**
  (tethered to the Mechanism reframing): mine classic's wins from a different,
  easier pairing (e.g., vs a diluted or weaker roster member) and see if the
  same generalization holds. If it does equally well, the "own-brain
  consistency" explanation is confirmed as sufficient and the "stronger
  opponent" qualifier is not load-bearing.
- **Isolate the universal opening entry**: check whether book2's very first
  move (keyed to the shared standard-start position) is the SAME across every
  game regardless of opponent, and whether that single entry alone accounts
  for most of the gain (vs. deeper entries mattering too).
- **Held-out generalization** (tethered to the scrutiny question):
  measure this agent (or an equivalent book mined the same way) against an
  agent/engine entirely outside this codebase, if one becomes available, to
  distinguish "genuine strength" from "pool-specific artifact."
- **Apply the same fix to s98's own book** (the practical next dethrone-plan
  step): mine s98's own wins over the oracle (if any exist in the store) or
  over the rest of the pool, the same way, and see if it raises s98 further
  above its own bookless self -- the same construction, aimed at extending
  the CURRENT leader's lead rather than dethroning it.
- **Reset-state prerequisite** (still open, from theory 14/results-3): would
  let this comparison be re-run without the repeated-game-shape caveat.

## Ideas This Inspired

- A "book coverage/reproducibility" metric -- how many DISTINCT game shapes
  (by final ply count + piece count) appear in N games between a pair --
  would let future det-vs-det measurements self-report how much they're
  relying on repeated shapes vs genuine diversity, rather than requiring a
  manual awk scan like the one that surfaced this.
- Given how cheap and effective this was (zero new code, one bookgen
  invocation), mining a self-book from EVERY strong deterministic agent's own
  historical wins might be a fast, generic Elo lever worth applying pool-wide,
  not just to one pairing -- essentially "self-play distillation of an
  agent's own lucky/best-found lines back into itself."

## Commit

One commit: `models/book2.txt`, the roster + roster_top additions, new match
data + refit artifacts (`ranking/`), this results doc, theory 14 update +
new theory 33 (`Docs/theories.md`), `ranking/CHAMPION.md` lineage/defended-
challenges update, and `todo.md`'s goal paragraph + book todo item. No source
changes; test suite unaffected.
