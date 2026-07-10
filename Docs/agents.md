# Agent Glossary

A plain-English description of every NAMED, trained agent referenced across the
project's results docs (`plans/vs-champion-training-results-1-cozy-forest.md`,
`plans/opener-bias-results-1-synchronous-stearns.md`, and others), so a reader
doesn't have to reconstruct "what is champdil, exactly" from scattered script
arguments. This complements, and doesn't replace:

- `Docs/terminology.md` -- general concepts (agent, evaluator, dilution, canonical
  ID grammar) with one-line definitions.
- `Docs/theories.md` -- the open questions these agents were built to test.
- `agents/registry.md` -- an auto-generated, mechanically-complete list of every
  agent ever rated in a full tournament (a different Elo scale from the ranking
  pool below; see the note at the bottom).
- `ranking/roster.txt` / `ranking/ratings.tsv` -- the live, authoritative source of
  which agents are active and their current rating.

Each entry states its recipe precisely: what data it trained on, and critically,
whether a random opener was used **during training data generation** versus only
**during evaluation** afterward -- conflating the two is an easy and important
mistake (see the Champdil entry below for the case that prompted this document).

## Foundational agents (not trained; building blocks the others are measured against or built from)

**Champion** -- `ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`. A depth-6 alpha-beta
search using the Classic evaluator with weights turn=1, chip=4, **wall=0,
column=0**. That last part matters: the champion's own evaluation is completely
blind to positional structure (piece placement, walls, columns) -- it only ever
weighs material count and whose turn it is. Anything resembling "the champion has
positional habits" is an emergent side effect of its depth-6 search interacting
with move ordering, not something its evaluator deliberately optimizes for or
against. The reigning best agent; everything below is measured against it.
Ranking-pool Elo ~1140 (a different, incompatible scale from `agents/champion.txt`'s
2244, which comes from the separate full-tournament rating system -- see the note
at the bottom).

**Oracle (the teacher, not a trained model)** -- `ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2`.
A depth-8 alpha-beta search (same Classic weights as the champion, just deeper),
used purely to *generate* training data by playing against the champion -- it is
never itself trained on anything. "Oracle" in this project always refers to this
deep-search teacher agent; "oracle-vs-champ" (below) is the trained *model* whose
labels came from its games.

**PstD2** -- `ab(d2,tt,ord)@1.learned(s2,<hash>)@1`. Whatever model currently sits
in `models/pst_value.txt` (slot 2), wrapped in a depth-2 search. Used as a cheap
"current best learner" generator in some training arms (below).

**ClassicD2** -- `ab(d2)@1.classic(t1,c4,w0,l0)@2`. A shallow depth-2 Classic
search, used as a weak, non-learned baseline generator.

## The vs-champion training study's promoted models

All from `tools/train_vs_champion.ps1`; results and Elo numbers in
`plans/vs-champion-training-results-1-cozy-forest.md`. Each is a linear
piece-square-table (PST) value model (feature v2), trained via
`train.exe selfplay-supervised --from-data <recipe file>`, then wrapped as
`ab(d6,tt,ord,nb200k)@1.learned(s<slot>,<hash>)@1` for rating. Ranking-pool d6 Elo
after the full re-fit is given for each; screening Elo (the cheaper d4-wrapper
gauntlet used to pick the best of several training seeds before the d6 confirm) is
in the results doc if you need it.

- **theory1 (slot 94, d6 Elo ~987)** -- best of three arms testing "does a vs-champ
  agent avoid the diverse-opponent weakness Theory 1 worried about": (a) `PstD2`
  (the current best learner) diluted 30%->5%, playing the clean Champion; (b)
  `ClassicD2` (a weak depth-2 searcher) diluted the same way, playing the clean
  Champion; (c) a 50/50 mix of half champion games with half replay-extracted data.
  No random opener in any of these three recipes.

- **bootstrap (slot 95, d6 Elo ~889)** -- a self-improvement attempt: took the best
  champion-sourced model *from this same study* and used IT (wrapped at depth 2) as
  the data-generating agent against the clean Champion, gated to only run if the
  champion-sourced arms were already competitive with the replay baseline. Failed
  badly (this cell scored far below its parent), which is why the theory log
  (theory 7) asks whether an iterative, multi-generation curriculum would succeed
  where this one-shot attempt didn't.

- **dilution / champdil (slot 96, d6 Elo ~1153 in the ranking-pool refit)** -- **the
  Champion playing against a diluted copy of itself**, nothing else. Both sides in
  training were literally the Champion's own ID; one side ("a") had its move
  chosen randomly with a probability that decays linearly from 30% at the start of
  the game to a 5% floor by move 30 (`--dil-apply a --dil-start 0.3 --dil-floor 0.05
  --dil-decay-plies 30`), then holds at that floor for the rest of the game. **No
  oracle involved, and no fixed random opener during training** -- the closest
  correct one-line description is "learned from Champion-vs-(Champion with a
  decaying chance of a random move) self-play," not anything involving the oracle
  or a fixed opener window.
  The 6-ply-random-opener detail that's easy to conflate with this belongs to a
  *different* step: when champdil was later evaluated head-to-head against the
  clean Champion (measuring its win rate, not training it), that evaluation used
  `--open-plies 6` (both sides forced random for 6 plies) so a deterministic pair
  wouldn't just replay the same 2 games over and over. That gave champdil a 62.5%
  apparent head-to-head win rate, which is what "Theory 2: dilution can't beat the
  champion" was originally marked Refuted on. The opener-bias study
  (`plans/opener-bias-results-1-synchronous-stearns.md`) later re-ran that same
  head-to-head with the Champion allowed to play its own real opening instead of a
  forced-random one, and champdil's win rate collapsed to 40% -- so that 62.5%
  number was mostly an artifact of the *evaluation* opener, not evidence about what
  champdil actually learned in training. Theory 2 is reopened as a result.
  A follow-up color-swap test (`rank.exe opener-swap`, theory 15) found a real,
  separate signal though: from 20 identical random-opener snapshots played out
  twice with colors swapped, 65% of outcomes were explained by which color won
  (a Breakthrough-wide White advantage, not agent skill), but in the remaining 35%
  where the SAME agent won regardless of color, it was champdil every time (7/7),
  never the Champion (0/7) -- suggestive (n=20) that champdil genuinely recovers
  from a bad/random start better than the Champion does, independent of color.

- **branch (slot 97, d6 Elo ~683)** -- same Champion-vs-diluted-Champion self-play
  as champdil, but games are FIRST filtered to keep only the diluted side's wins,
  then each kept win is "branch-mined": rewound to a random ply where the winner
  was to move, a different legal move is substituted, and the resulting line is
  kept only if the winner wins again from there. Intended to enrich the dataset
  with alternative winning lines. Scored worst of the promoted family.

- **oracle-vs-champ / "oracle" (slot 98, d6 Elo ~1137)** -- **the deep d8 Oracle
  teacher playing against the clean Champion**, with `--open-plies 6` (both sides
  forced random for the first 6 plies) used *during training-data generation*
  itself, unlike every other arm above. This is the one case where "random turn 6"
  genuinely is part of the description: the accurate one-liner is "learned from
  (depth-8 Oracle)-vs-Champion games starting from a 6-ply random opening." This is
  the model that ties the Champion's own ranking-pool rating (~1140) at d6 -- the
  headline result of the whole study. The opener-bias study's Layer 1 re-evaluated
  this head-to-head with the Champion playing normally and the result held up
  (58.8% -> 66.2%), so unlike champdil, this one is not an opener artifact. Layer 3
  retrained a fresh oracle-family model from an asymmetric-opener version of this
  same recipe (below) as a further check.

- **replay-baseline (slot 99, d6 Elo ~934)** -- NOT a champion-vs-anything recipe at
  all. Built from `rank.exe extract`, which replays a deterministic sample of
  *already-stored* games from the whole rated agent pool's match history
  (`ranking/matches.jsonl`) into training positions -- reusing existing play instead
  of generating anything fresh. Included as the established strong baseline (per
  theory 12) that every champion-specific recipe above is trying to beat.

- **champloss-only** (mentioned in the results doc's addendum, not one of the six
  promoted slots above) -- Champion-vs-diluted-Champion self-play like champdil,
  but filtered to keep ONLY the games where the diluted side won, i.e. only the
  *clean* Champion's losses. Diagnosed as producing a systematically miscalibrated
  model (0-280 head-to-head, never fixed by adding more data) because one-sided
  win/loss labels teach a degenerate value function -- see theory 3 (disproven:
  more data doesn't fix it) in `Docs/theories.md`.

## The opener-bias study's model

- **oracle-asym (screening slots 81-83, d6-confirmed as slot 93, d6 Elo 832, NOT
  rostered)** -- from `tools/opener_bias_retrain.ps1`. The same oracle-vs-champ
  recipe as slot 98 above, but generated with `--open-side a` instead of the
  symmetric opener: only the Oracle teacher plays the 6-ply random opener; the
  Champion plays its own real opening throughout, including during those first 6
  plies. Scored substantially lower than slot 98 (832 vs 1137 at d6), but this drop
  is confounded by training-label skew (the resulting dataset's oracle win:loss
  ratio is 4.46:1 versus the symmetric recipe's 2.55:1 -- the same kind of
  imbalanced-label problem that broke champloss-only above) and should not be read
  as a clean confirmation that slot 98's number was inflated. See
  `plans/opener-bias-results-1-synchronous-stearns.md`'s Layer 3 section and its
  Future Work #2 for the unresolved follow-up (retrain with the label ratio
  controlled for). Archived at `models/sweep/vsc_oracle-asym_<seed>.txt`, never
  added to `ranking/roster.txt`.

## General opener-sensitivity measurement

The champdil/oracle-specific study above answers "was THIS PARTICULAR head-to-head
result an opener artifact." For a general, reusable version -- "how much does ANY
agent's rating depend on being allowed a random opener" -- every agent's canonical
ID can carry an `.opener(<kind>[,<arg>])@1` segment (`AgentSpec::openerKind`, the
`g_openers[]` registry, `Docs/terminology.md`). One opener kind is registered so
far, `rand`: `.opener(rand,6)@1` makes the agent play its first 6 own moves as
uniform-random, then hand off to its normal brain (the registry is where a future
opening-book or scripted opener would slot in). Gauntletting the same agent with and
without this segment gives a direct Elo gap, e.g.:

| Agent | clean ranking-pool Elo | with `.opener(rand,6)@1` (gauntlet) | drop |
|---|---|---|---|
| Champion | 1140 | 923 | ~217 |
| champdil (s96) | 1153 | 962 | ~191 |

Note this measures something different from the champdil/oracle study above: it's
each agent's own Elo drop against the WHOLE POOL from playing a random opener
itself, not a head-to-head win-rate change against one specific opponent under
different combinations of who's handicapped. Both numbers here are non-trivial and
similar in magnitude -- on this measure the champion and champdil are not wildly
different in raw opener sensitivity, which is a useful independent cross-check on
the champdil/oracle asymmetric-opener findings above.

## A note on Elo scales

Two separate, non-comparable rating systems appear in this project:

1. **The ranking pool** (`rank.exe`, `ranking/ratings.tsv`), anchored at
   `rand@1` (UniformRandom) = Elo 0. All the numbers above are on this scale; the
   Champion sits at roughly 1140 here.
2. **The full tournament** (`train.exe tournament-rate`, `agents/champion.txt`,
   `agents/registry.md`), which uses a different agent pool and anchor and reports
   the Champion at Elo 2244.

Never compare a number from one system to the other directly; only compare within
the same system (all the d6 Elo figures above are ranking-pool numbers, safe to
compare with each other and with 1140).
