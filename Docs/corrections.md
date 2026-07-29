# Corrections index

The enumerable list of **defect classes** found in this project's own past
writing. One entry per class. Analogous to a Wikipedia tracking category or the
RFC index: the affected documents carry a banner, and this file is what makes
them countable in one place instead of only discoverable by stumbling into them.

**Read this before quoting a number out of any `plans/` document.** A document
carrying a banner is not worthless, it is *unverified in a specific way*, and
each entry below says exactly which way and what to do about it.

## How the convention works

Two mechanisms, deliberately separate because they answer different questions.

**1. Document banner: "is this document sound?"** A blockquote at the very top
of the affected document:

```
> **[<DEFECT NAME> - flagged <YYYY-MM-DD>]** <what was wrong, what it means for
> this document's numbers, and what a reader should do instead.>
```

`<DEFECT NAME>` is SHORT, UPPERCASE, and reused verbatim across every affected
document so `grep -rn "<DEFECT NAME>"` finds the whole set. Register the name in
this file. Never invent a second name for a class that already has one.

**2. Point-of-citation note: "is this claim safe to quote?"** The banner marks
where a claim was *analysed*. It does not travel to where the claim is *quoted*,
and quoting sites are where the damage happens (see the `SELF-PLAY CONVERGENCE
UNSUPPORTED` entry, which is a case of exactly that). So at each place that
states the conclusion - a `CLAUDE.md` table row, a script header, a `todo.md`
summary - add a short inline note naming the defect class and pointing here:

```
(see <DEFECT NAME>, Docs/corrections.md)
```

This is the documentation equivalent of a deprecation warning firing at the use
site rather than the definition. Keep the original claim text visible in both
cases: the wrong claim is the reason the mark exists, and a later reader needs to
recognise it if they meet it quoted somewhere that was missed.

---

## `ELO HYGIENE UNVERIFIED` - flagged 2026-07-25

**38 documents.** Elo comparisons written before the ranking-claim hygiene rules
existed. Three defects, detailed in `Docs/benchmarking.md`:

1. Numbers read from `ranking/ratings.tsv`, which mixes RETIRED agents
   (`active = gone`, superseded `@N` identities frozen at old game counts) with
   live ones. Read `ranking/standings.tsv` instead. This alone produced a
   91-Elo phantom gap.
2. Agents compared across different SEARCH HEADS, which are different agents, so
   the comparison is not an evaluator result.
3. Stored ROWS counted instead of DISTINCT games. `rand()` is consumed only by
   dilution and random-move agents, so a pair with neither replays one game per
   colour however many rows exist. Median 0.438 distinct games per row in the
   fixed-start store, so printed `pm` is understated by roughly 1.5x.

**What to do:** treat any Elo number in a banner-carrying document as
provisional. Re-derive from the current `ranking/standings.tsv` before quoting.
Affected documents are found with `grep -rl "ELO HYGIENE UNVERIFIED"`.

## `SELF-PLAY CONVERGENCE UNSUPPORTED` - flagged 2026-07-29

**The claim:** "single-teacher self-play converges/plateaus at 500 games",
sourced from `models/sweep/scaling.csv` and `tools/train_scaling.ps1`.

**Why it does not hold.** The entire self-play arm is four rows:

| games | seed 1001 | seed 2002 | mean |
|---|---|---|---|
| 250 | 536 | 442 | 489 |
| 500 | 541 | 469 | 505 |

`train_scaling.ps1` stopped because the +16 mean gain fell under its
`-ConvergeElo 20` rule. But the seed spread WITHIN a size is 94 Elo (250) and 72
Elo (500), so the threshold is smaller than the noise it is thresholding and
fires at the first rung by construction. Each point is a 4-games/pair gauntlet at
+/- 29-31, understated ~1.5x by defect 3 above. **No size above 500 was ever
run**, so there is no evidence about the curve's shape past 500 in either
direction. The ladder stopped; it did not converge. Filed as theory 45.

**It also does not transfer to online regimes at all**, which is a separate point
from the resolution problem. It measures a FIXED teacher generating a FIXED
distribution, which is precisely why saturation is expected there. An online
bootstrapped learner (TD-Leaf) changes its own generator every game, so the
distribution keeps moving and the saturation argument has no purchase.

**Why this entry exists at all.** The correct reading was ALREADY recorded, in
`plans/training-sweep-results-1-luminous-snail.md` item 3 on 2026-07-24, which
said the stop "triggered on noise, not on convergence" and warned "do not trust
'self-play plateaus at 500'". It was contradicted anyway on 2026-07-29 while
planning the TD-Leaf study, because none of the places the result gets quoted
from carried the caveat. That is the failure this file and the point-of-citation
note exist to prevent, and it is why the two mechanisms are separate.

**What to do:** do not cite a self-play game-count ceiling. Before reusing
`train_scaling.ps1`, either raise `-Seeds` until the seed band sits below
`-ConvergeElo`, or drop the early stop and rate a fixed ladder end to end, which
is what `tools/tdleaf_study.ps1` does via `train.exe --ckpt-at`.

**Affected:** `plans/training-sweep-results-1-luminous-snail.md` (source),
`tools/CLAUDE.md`, `tools/train_scaling.ps1`, `todo.md` (citation sites).
