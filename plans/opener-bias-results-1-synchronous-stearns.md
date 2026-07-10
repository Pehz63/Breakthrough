# Results: Theory 6 -- do symmetric random openers inflate "beats the champion" results?

Companion to `plans/opener-bias-plan-1-synchronous-stearns.md`. Tests theory 6 in
`Docs/theories.md`. Verdict: **partially confirmed** -- the symmetric random opener
inflated the dilution (champdil) result that refuted Theory 2, but NOT the oracle
headline result.

## The question

Every "beats the champion" number in the vs-champion study
(`plans/vs-champion-training-results-1-cozy-forest.md`) was measured with
`--open-plies 6` on BOTH sides, forcing the deterministic champion
(`ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2`) to play 6 random opening moves it
would never choose. If that handicaps the champion, the challengers' wins could be
"beats a champion given a bad random start," not "beats the champion's real play."
The confound is one-directional -- it can only inflate a "beats the champion"
result, never a "fails to beat" one.

## What was built

- **`pairgen --open-side a|b|both`** (`src/ranking.cpp` `playoutCapture` /
  `rankPairGen`, `ranking.h`, `tools/rank_main.cpp`). A color bitmask, mapped a/b ->
  color per game like `--dil-apply`, of which side plays the random opener; the
  unmasked side plays its own policy inside the opener window. Default `both` keeps
  the historical symmetric behavior byte-identical (guarded by a back-compat test).
  Recorded as `open_side` in the `.meta.json` sidecar.
- **`rank.exe opener-bias`** (`rankOpenerBias`), a read-only mechanism measure (see
  Layer 2), with a `--judge` agent that scores positions.
- **`tools/opener_bias_study.ps1`** (Layers 1+2) and **`tools/opener_bias_retrain.ps1`**
  (Layer 3).
- Tests in `tests/test_ranking.cpp` (open-side divergence, byte-identical default,
  meta field, determinism, opener-bias smoke). Suite: 501 -> 532 assertions.

## Layer 1 -- head-to-head sensitivity sweep (80 games/config, d6, seed 220)

Challenger win% vs the champion under three opener configs:
- **S** symmetric (`--open-side both`) -- the original study's setup
- **C** challenger random (`--open-side a`) -- challenger handicapped, champion true policy
- **P** champion random (`--open-side b`) -- champion handicapped, challenger true policy

| Model | S | C | P | C as White | C as Black |
|---|---|---|---|---|---|
| champdil-s96 (Theory 2 basis) | 65.0% | **40.0%** | 73.8% | 13/40 (32%) | 19/40 (48%) |
| oracle-s98 (headline tie) | 58.8% | **66.2%** | 65.0% | 29/40 (72%) | 24/40 (60%) |

Reading:
- **champdil COLLAPSES** from 65% (S) to 40% (C). When the champion plays its own
  opening instead of forced-random moves, the dilution model does NOT beat it
  (loses 40-60). Its S=65% and P=73.8% both came from a handicapped champion. The
  C-P spread is huge (40% -> 74%), i.e. its result is entirely opener-dependent.
- **oracle SURVIVES**: 58.8% (S) -> 66.2% (C), and its C (66.2%) and P (65.0%) are
  within noise of each other. The oracle model beats the champion whether or not the
  champion is handicapped -- it is opener-insensitive, so the headline is real.

The C-P spread is the key discriminator: ~34 points for champdil (fragile) vs ~1
point for the oracle (robust). n=80 gives a binomial SE of ~5.5%, so champdil's
25-point S->C drop and the oracle's opener-insensitivity are both well outside
noise.

Methodology caveat: C and P *bracket* the truth rather than isolate it. The true
"both play their own best from the standard start" is only 2 distinct games for
deterministic agents, so it is unmeasurable at scale; C handicaps the challenger, P
handicaps the champion, and the real win rate sits between them. For champdil that
bracket is [40%, 74%] -- wide, and it includes <50%, so we cannot claim it beats the
champion. For the oracle it is [65%, 66%] -- tight and clearly >50%.

## Layer 2 -- mechanism measurement (n=60, judge = s98 learned PST)

Does the random opener actually leave the champion worse off? `rank.exe opener-bias`
replays the both-random opener and, at each champion ply, scores the position after
its forced-random move against the position after its own move (both with the
opponent to move, so the eval's turn term cancels), using a judge agent's search.

The champion's own Classic eval (`w0, l0`) is positionally blind: with it as judge,
mean delta is exactly 0.00 -- it cannot distinguish opener positions at all (this is
itself why the `--judge` override was added, a deviation from the plan's "champion's
own static eval"). With a positionally-aware learned judge (s98):

| Champion color | plies | mean delta (judge units) | % hurt | % helped | games hurt |
|---|---|---|---|---|---|
| White | 90 | +46.3 | 65.6% | 28.9% | 30/30 |
| Black | 90 | +61.0 | 63.3% | 32.2% | 30/30 |
| both | 180 | +53.7 | 64.4% | 30.6% | 60/60 |

The random opener objectively degrades the champion's position on ~64% of its opener
plies, in every one of the 60 games (positive mean delta). The mechanism theory 6
posits is real. What differs between champdil and the oracle (Layer 1) is not
whether the champion is handicapped -- it always is -- but whether the challenger is
strong enough to beat a champion that plays its own game anyway.

## Layer 3 -- asymmetric-opener retrain

Regenerated the oracle training set with `--open-side a` (only the oracle plays the
random opener; the champion plays its own opening in every training game),
`data/pg_oracle_champ_asym.jsonl`, 2000 games, seed 104. Retrained the 3-seed oracle
cell (`--from-data`, 6 epochs, lr 0.05, feature v2), gauntlet-screened at the d4
wrapper, and d6-confirmed the best (seed 2002, screen Elo 642).

| | symmetric (baseline) | asymmetric (this run) |
|---|---|---|
| screen (d4), 3 seeds | 792 / 738 / 825 (mean 785) | 575 / 642 / 612 (mean 610) |
| d6 confirm | 1137 | 832 |
| training data | 2000 games, oracle 1437-563 (71.8%), 98999 positions | 2000 games, oracle 1634-366 (81.7%), 88970 positions |

The raw d6 delta is **-305 Elo**, a large drop that at face value looks like a clean
confirmation of theory 6 for the oracle arm. **It is not** -- the drop is confounded
and should not be read that way, for two reasons the numbers themselves reveal:

1. **The champion is not "protected" by playing its true policy here -- it does
   worse.** Removing its random-opener handicap raised the oracle's training-data win
   rate from 71.8% to 81.7%. This matches Layer 1's own finding (the challenger's win
   rate went UP, not down, once the champion played normally: 58.8% -> 66.2%), so it
   is a real and consistent pattern, not noise: the champion's fixed, deterministic
   opening is apparently more exploitable by a deep searcher than a random one is. If
   theory 6's "handicapped champion inflates results" mechanism were the dominant
   effect here, letting the champion play normally should have made the TRAINING data
   harder for the oracle, not easier -- the opposite of what happened.
2. **The asymmetric dataset's labels are far more skewed** -- a 4.46:1 win:loss ratio
   vs 2.55:1 in the symmetric set, from removing 6 plies' worth of the champion's own
   randomness (which used to generate more losses for the oracle to learn from). This
   project already has a documented failure mode for exactly this: the champloss
   study (`vs-champion-training-results-1-cozy-forest.md`, addendum) found that a
   more one-sided outcome distribution degrades a linear value model into a
   miscalibrated one, independent of data volume (theory 3, disproven: "more data
   fixes it" -- it doesn't; it's a label-distribution problem). The asymmetric set's
   88970 positions (~10% fewer) plus its more skewed labels are enough on their own to
   explain a large Elo drop in a linear model, with no opener-fairness story needed.

**Conclusion:** Layer 3's -305 Elo drop is real but is most likely explained by
training-data label skew, not by "the original 1137 number was measuring a
handicapped champion." Layer 1 is the cleaner test of that specific question (same
fixed model, only the evaluation opener changes) and it already answered it: the
oracle's real head-to-head strength holds up. Layer 3 is retained as a data point --
and as a reminder that "regenerate training data slightly differently" changes more
than one variable at a time -- but does not move the theory 6 verdict.

## Addendum: general opener-Elo and the color-swap recovery test (Theory 15)

Two follow-on tools, both raised in the same conversation that reviewed the three
layers above, extending the investigation beyond Theory 6 itself.

**General per-agent opener-Elo (opener registry `g_openers[]`, ID segment
`.opener(<kind>[,<arg>])@1`).** Every measurement above was a bespoke pairwise study
built for this one theory. To get a general, reusable "how opener-sensitive is any
given agent" number, an agent's canonical ID can now carry an opener from a pluggable
registry -- one kind registered so far, `rand`, so `.opener(rand,N)@1` makes the agent
play its first N own plies uniform-random then hand off to its brain (honored by
`rank.exe run`/`play`/`gauntlet` via `playOneGame` and by `pairgen` via
`playoutCapture`, composing via OR with `--open-plies`/`--open-side`). The registry
is the extension point for future openers (an opening book, the scripted
offensive/defensive openers). Gauntletting the champion and champdil with and without
`.opener(rand,6)@1`:

| Agent | clean ranking-pool Elo | with `.opener(rand,6)@1` | drop |
|---|---|---|---|
| Champion | 1140 | 923 | ~217 |
| champdil (s96) | 1153 | 962 | ~191 |

Both agents lose a substantial and broadly similar amount of Elo from a 6-ply
random opener on this general measure -- champdil is not dramatically more
resilient than the champion in raw Elo terms, which is a useful cross-check
against the next finding (they aren't in tension: an agent can be similarly
Elo-sensitive to a random opener overall while still recovering better than a
specific rival from any one given bad position -- see below).

**Color-swap recovery test (`rank.exe opener-swap`, Theory 15).** The developer
proposed a cleaner design than either Layer 2's static-judge score or a plain
head-to-head win rate: take the SAME random-opener snapshot and play it out to
conclusion twice, swapping which agent is White and which is Black. If whoever is
White wins both times, the position itself favors White (a color/board-position
effect, unrelated to either agent's skill). If the same AGENT wins both times
regardless of color, that agent genuinely recovers from that specific position
better -- a real skill signal, cleanly separated from color.

Run: champdil (s96) vs the champion, 20 random-opener snapshots, 6-ply opener,
seed 42 (`rank.exe opener-swap --a <champdil> --b <champion> --games 20
--open-plies 6 --seed 42`):

| Outcome | count | % of classified |
|---|---|---|
| White won both (color effect) | 11 | 55% |
| Black won both (color effect) | 2 | 10% |
| **champdil won both (agent effect)** | **7** | **35%** |
| champion won both (agent effect) | 0 | 0% |

Two-thirds of outcomes (65%) are explained by color alone -- consistent with the
champion's own historical White/Black split (96.5% vs 87.9%), Breakthrough evidently
favors White substantially regardless of who's playing. But in the remaining
agent-effect third, **champdil won every single one; the champion won none.** This
is a real, causally cleaner signal than anything in Layers 1-3 that champdil
recovers from a bad/random starting position better than the champion does,
independent of color -- filed as Theory 15 (Promising / unproven, n=20).

*Caveat:* n=20 is a small sample for a 0/7-vs-7/7 split within the agent-effect
bucket; while suggestive (0/20 for the champion isn't just "the split went the
other way," it's a clean zero), a larger run (e.g. 100+ snapshots) is needed before
calling this settled -- added to Future Work below.

## How to test

1. Build: `.\build_rank.bat` and `cmd /c ".\build_tests.bat"`, then `.\tests.exe`
   (568 assertions pass; the byte-identical back-compat test proves the default
   opener behavior is unchanged).
2. Flag: `rank.exe pairgen --a <oracle> --b <champion> --games 4 --open-plies 6
   --open-side a --out data/opener_smoke.jsonl` -> `.meta.json` shows `"open_side":"a"`.
3. Mechanism: `rank.exe opener-bias --a <champion> --b <s98> --judge <s98>
   --games 60 --open-plies 6`.
4. Layers 1+2: `.\tools\opener_bias_study.ps1 -Games 80 -Workers 12`.
5. Layer 3: `.\tools\opener_bias_retrain.ps1 -DryRun` (pipeline), then without
   `-DryRun` for the real comparison.
6. General opener-Elo: `rank.exe gauntlet --id "<any id>.opener(rand,6)@1" --games 4`.
7. Color-swap: `rank.exe opener-swap --a <champdil> --b <champion> --games 20
   --open-plies 6 --seed 42`.

## Candidate commit messages

- `Add asymmetric pairgen openers + opener-bias measure; test Theory 6 (symmetric openers inflated the dilution result, not the oracle)`
- `Test Theory 6: symmetric random openers inflate champdil's champion-beating result but not the oracle's`
- `Add pairgen --open-side + rank.exe opener-bias; three-layer opener-inflation study`

Top recommendation: the first -- it names the new capability and the headline verdict.

## Differences from the plan

- **Added `--judge` to `opener-bias`** (not in the plan). The plan said to score
  positions with the champion's own static eval, but the champion's Classic eval is
  positionally blind (w0/l0) and returned a flat 0.00 delta. Separating "who plays"
  (champion) from "who judges" (configurable, default champion) was necessary to get
  a discerning read; a learned PST judge shows the real +54 mean delta.
- **Removed the fixed "one chip = 4" materiality threshold** in favor of sign-based
  fractions (% hurt / % helped past a tiny epsilon) plus the mean delta, because a
  learned judge's eval scale (tanh*out_scale) is not in chip units, so an absolute
  threshold would be judge-dependent and meaningless.
- **Fixed a tempo artifact discovered mid-implementation.** The first version scored
  the own-move line with the champion to move and the random line with the opponent
  to move, so the eval's turn term contributed a constant offset (a spurious flat
  +2.00 delta). Both positions are now scored with the opponent to move so the turn
  term cancels.

## Future Work

Each tethered to a specific conclusion above.

1. **The oracle's opener-insensitivity (Layer 1, C approx P) rests on n=80.** *Tied
   to:* the "oracle result is real" verdict. *The hole:* 80 games gives SE ~5.5%, so
   C=66.2% vs P=65.0% being "within noise" is suggestive, not proven; a 34-point
   champdil spread is unambiguous but the oracle's ~1-point spread could hide a
   smaller real effect. *Test:* rerun the oracle S/C/P configs at 400+ games each.
2. **Layer 3's drop is confounded by training-label skew, not isolated to the
   opener question.** *Tied to:* the -305 Elo d6 delta. *The hole:* the asymmetric
   recipe changed two things at once -- the opener AND the resulting win/loss ratio
   in the generated data (2.55:1 -> 4.46:1) -- so the drop cannot be attributed to
   the opener alone. *Test:* regenerate an asymmetric set with a winner filter or
   resampling that matches the symmetric set's 71.8% win rate, retrain, and see if
   the drop persists once label skew is controlled for. If it does, that would be
   real evidence for theory 6's training-side mechanism; if the drop disappears, the
   whole -305 Elo was skew, not opener fairness.
3. **Layer 3 uses one data seed (104) for generation.** *Tied to:* the Layer 3
   retrain comparison. *The hole:* the asymmetric and symmetric oracle sets differ in
   both opener AND their specific random draws; a single generation seed confounds
   the two. *Test:* generate the asymmetric set at 2-3 seeds and compare the screen
   distribution, not a single mean.
4. **Only champdil and oracle were swept (Layer 1).** *Tied to:* generalizing the
   "marginal challengers are opener-inflated, strong ones are not" reading. *The
   hole:* the other promoted families (theory1 s94, bootstrap s95, branch s97,
   replay s99) were not run under S/C/P. *Test:* extend `opener_bias_study.ps1`'s
   model list to all of slots 94..99 and see whether the C-P spread predicts an
   agent's true strength.
5. **The mechanism judge is a single learned model (s98).** *Tied to:* the Layer 2
   "+54 mean delta" magnitude. *The hole:* s98 is itself oracle-trained, so it may
   over- or under-weight the exact positions the oracle exploits. *Test:* rerun
   `opener-bias` with a structurally different judge (e.g. the replay-trained s99 or
   a deeper classic search) and check the delta sign/magnitude is stable.
6. **`--open-plies 6` length was never swept (inherited from the original study).**
   *Tied to:* every number here uses 6. *The hole:* a shorter or longer opener could
   change the handicap magnitude and thus the champdil collapse point. *Test:* the
   standing `todo.md` opener-length sweep (0/2/4/6/8/12), now cheap to combine with
   `--open-side` since the primitive exists. This is a 2-D sweep (length x side).
7. **Theory 15's color-swap result rests on n=20, all from one seed.** *Tied to:*
   the "champdil recovers better" finding (7/20 agent-effect snapshots, all
   champdil, 0 champion). *The hole:* a 7-vs-0 split within a 20-snapshot sample is
   suggestive but not statistically decisive, and a single seed (42) picks one
   specific set of 20 random positions. *Test:* rerun `opener-swap` at 100+
   snapshots across 2-3 seeds; if the champion ever wins both continuations at a
   nontrivial rate, the "champdil always wins the agent-effect bucket" reading
   softens. Also worth running the SAME test for oracle (s98) vs the champion, to
   see whether "recovers from bad positions better" is a champdil-specific trait
   or shared by other champion-beating models.

## Ideas This Inspired

Lighter, not tethered to a specific conclusion.

- **Opener-sensitivity as a strength metric.** The C-P win-rate spread (huge for the
  fragile champdil, ~0 for the robust oracle) looks like a general "how much does
  this agent depend on the opponent's opening mistakes" score. Could be logged
  routinely for any agent vs the champion as a robustness stat alongside Elo.
- **A no-opener paired evaluator with real opening diversity.** Instead of random
  openers (which handicap deterministic agents), seed each game from a distinct
  position drawn from the mined opening book (`todo.md`), so both agents play their
  true policy from a realistic-but-varied start. That would measure head-to-heads
  without any handicap confound at all.
- **Color-conditioned openers.** Layer 2 shows the opener hurts the champion more as
  Black (+61) than White (+46), and champdil's collapse is worse as White (32% in C)
  than Black (48%). A per-color opener length or dilution might be a fairer eval knob
  -- and connects to theory 5 (color-specific evaluator weights).
- **Use `opener-bias` as a data-quality filter.** During training-data generation,
  drop opener plies where the judge delta exceeds a threshold (positions the random
  opener made pathologically bad), keeping only openings that resemble real play.
