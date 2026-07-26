# Diversified-Opening Rating Pool -- Results

Session 2026-07-26, continuing directly from
[book-opener-audit-results-1-vivid-lantern.md](book-opener-audit-results-1-vivid-lantern.md).
That audit found defect 3: for a deterministic pair the seed is inert, so N stored
rows are about 2 distinct games and every error bar is understated. This is the
fix, built as a SECOND pool rather than by changing the existing one.

Developer framing (2026-07-26): run agents with a random opener so games are more
diverse and strength is stress-tested, and make the random openings symmetric, so
"agent A's random opening on white is reused by agent B on white". The point is to
compare agents on how well they recover from equal openings rather than on which
of them drew the luckier start.

## What was built

**`coupleSeed` + `--paired-openings`** (`src/ranking.cpp`, `rankSchedule`). The
scheduler already emitted each pair as colour-swapped couples (a-White, b-White,
a-White, b-White), so ordinals 2k and 2k+1 are one couple. The flag replaces the
per-game seed with an FNV hash over the CANONICALLY ORDERED pair plus the couple
index, so both games of a couple share one seed.

That is the whole mechanism. `.opener(rand,K)` draws its moves from `rand()`, and
inside the opener window no brain is consulted, so an identical `rand()` stream
produces an identical opening line regardless of which agent sits on which side.
The couple therefore plays the same position with the colours reversed. The flag is
inert when neither side consumes `rand()`, so it cannot disturb the fixed-start pool.

**Store-derived output paths** (`setOutSuffixFromStore`, `outPath`). The four rating
artifacts are now named after the store, so `matches_open.jsonl` writes
`ratings_open.tsv` / `standings_open.tsv` / `games_open.tsv` / `report_open.md`. The
default store keeps its historical unsuffixed names, so nothing existing moved.

**`ranking/roster_open.txt`**, initially 14 agents each wearing `.opener(rand,4)@1`
(4 of the agent's OWN half-moves, so 8 plies of random play), plus `rand@1` as anchor,
which needs no opener because it already plays randomly. Later grown to 20 rated
agents by the dist architecture ladder below, which is the final state and the fit
every number in the ladder section comes from.

## Why a separate pool, and why no book agents

Two constraints found in the code before building, both of which ruled out the
simpler designs:

1. **One opener per agent.** `AgentSpec::openerKind` is a single int and the ID
   parser rejects a "duplicate opener() segment" (`src/ranking.cpp:574`). A book
   agent therefore cannot also carry a random opener.
2. **The pool must be homogeneous in opener.** If only one side randomises, that is
   the asymmetric `--open-side a` condition, which the opener-bias study measured at
   65% -> 40% for one matchup. Mixing diversified agents into the 116-agent
   fixed-start roster would have made 116 x 14 asymmetric pairings.

Books are excluded and that costs nothing: a book is keyed on exact position hashes,
so after a random opening it never fires, which makes a book agent identical to its
bare core. Under diversification `s98.opener(book,11)` and bare `s98` are the same
player. Books are a fixed-start phenomenon and stay in the fixed-start pool, which is
where they mean something. The open question of whether a book could instead be mined
to RECOVER from a bad random opening is filed in `todo.md`.

## Verification before trusting any number

Run in that order deliberately, per the standing instruction added this session
("validate the instrument before quoting the reading"). All three passed.

| Check | Method | Result |
|---|---|---|
| Seeds actually pair | 3-agent probe, 4 games/pair, with and without the flag | without: 4 distinct seeds per pair. With: two couples of identical seeds |
| The seed drives the opening | same probe at `--seed 1` and `--seed 7` | 0 of 24 (w,b,seed) triples shared, outcome and ply multisets differ |
| A couple is two games, not a clone | compare the two rows of each couple | 12 of 12 couples differ in result, plies, or node totals |

## The payoff: defect 3 does not exist in this pool

Distinct game trajectories per stored row (colour + plies + result + both node
totals), over pairs with at least 16 games:

| Pool | Pairs | Median distinct / rows | Min |
|---|---|---|---|
| Fixed start (`matches.jsonl`) | 630 | 0.438 | 0.062 |
| Diversified (`matches_open.jsonl`) | 91 | **1.000** | **1.000** |

Every game in the diversified pool is a distinct trajectory. Corroborated
independently by the error bars, which scale as `1/sqrt(n)` exactly as independent
samples should:

| Games/pair | Store rows | Median `pm` | Predicted from n=4 |
|---|---|---|---|
| 4 | 364 | 53 | -- |
| 8 | 728 | 38 | 37 |
| 16 | 1456 | 28 | 26 |
| 32 | 2912 | 20 | 19 |

## Rank-order stability across fills

| Transition | Spearman rho | Agents changing rank |
|---|---|---|
| 4 -> 8 | 0.974 | 6 of 14 |
| 8 -> 16 | 0.969 | 6 of 14 |
| 16 -> 32 | **0.987** | **3 of 14** |

Converging, not converged. Rank churn halved on the last step. 32 games/pair is a
reasonable working point for this pool but a top-of-table claim would want more.

## Standings, diversified pool, 32 games/pair -- 14-agent stage (SUPERSEDED)

This is the fit as it stood BEFORE the dist architecture ladder was added. It is kept
because the stability curve above was measured against it, but the 20-agent fit in the
ladder section below is the current one. Adding six agents changed the fit, so these
two tables must never be read against each other, and neither is comparable to the
fixed-start pool's: different instrument, different agent set, different
Bradley-Terry prior.

| Elo | Head | Agent (all wear `.opener(rand,4)@1`) |
|---|---|---|
| 1405 +/- 24 | `ab(d8,tt,ord,nb2m)@1` | `classic(t1,c4,w0,l0)@2` (REFERENCE class) |
| 1250 +/- 20 | `ab(d6,ord,nb200k)@1` | `adv(t20,c77,...)@1` (hill-climbed) |
| 1240 +/- 20 | `ab(d6,tt,ord,qs,nb200k)@1` | `learned(s98,5801570e)@1` (quiescence) |
| 1235 +/- 20 | `ab(d6,tt,ord,nb200k)@1` | `learned(s76,ef183148)@1` |
| 1211 +/- 20 | `ab(d6,tt,ord,nb200k)@1` | `learned(s6,eac8ab99)@1` |
| 1190 +/- 20 | `ab(d6,tt,ord,nb200k)@1` | `learned(s98,5801570e)@1` |
| 1186 +/- 20 | `ab(d6,tt,ord,nb200k)@1` | `learned(s3,68364898)@1` |
| 1178 +/- 20 | `ab(d6,tt,ord,nb200k)@1` | `classic(t1,c4,w0,l0)@2` |
| 1125 +/- 20 | `ab(d6,tt,ord,nb200k)@1` | `learned(s111,78ef6974)@1` (dist) |
| 1003 +/- 21 | `ab(d6,tt,ord,nb200k)@1` | `classic@2.dil(r30,d4)@1` |
| 996 +/- 22 | `ab(d4)@1` | `classic(t1,c4,w0,l0)@2` |
| 817 +/- 28 | `ab(d2)@1` | `classic(t1,c4,w0,l0)@2` |
| 369 +/- 58 | `ab(d6,tt,ord,nb200k)@1` | `classic@2.dil(r63)@1` |
| 0 +/- 88 | `rand@1` | anchor |

Within the one head that has more than one agent (`ab(d6,tt,ord,nb200k)@1`), the
order is s76 1235, s6 1211, s98 1190, s3 1186, classic 1178, s111 1125. Only the
s111 gap clears 2 SE. The top four are inside one error bar of each other.

`adv(t20,c77,...)` at 1250 is the highest target-class agent, but it sits at a
different head (`ab(d6,ord,nb200k)`, no TT), so that is a pooled comparison and not
an evaluator result.

## Did learned models collapse under diversified openings? (14-agent stage)

Superseded in part by the ladder section below, which rates eight dist models rather
than one. The conclusion here about the replay-trained LINEAR models still stands.
The earlier pairwise finding (s111 going 62% -> 25% against the champion) suggested
learned models might be train-distribution dependent, since they learn from
standard-start games while a material counter is opening-agnostic by construction.
Tested with head-to-head WIN RATES against the chip counter at the same head, which
are fit-independent and therefore legitimately comparable across the two pools:

| Learned core | Fixed-start pool | Diversified pool | Change |
|---|---|---|---|
| `learned(s76,ef183148)@1` | 17-15 (53%) | 13-19 (41%) | -12pp |
| `learned(s6,eac8ab99)@1` | 12-20 (38%) | 19-13 (59%) | **+21pp** |
| `learned(s98,5801570e)@1` | 23-9 (72%) | 21-11 (66%) | -6pp |
| `learned(s3,68364898)@1` | 22-10 (69%) | 22-10 (69%) | 0pp |
| `learned(s111,78ef6974)@1` (dist) | 4-4 (50%, n=8) | 8-24 (25%) | **-25pp** |

**The general hypothesis is not supported.** Four of the five move by less than 1.5
SE (about 12.5pp for a difference of two n=32 proportions), and one of them moves
UP. There is no systematic collapse.

**The dist model is the exception and it is a real one.** `learned(s111,78ef6974)`
scores 25% against the chip counter over 32 diversified games, about 3 SE below even.
It is also last among the six d6 contenders at 1125, below the chip counter it
outrated in the fixed-start pool. Its fixed-start comparison was only n=8 so the
"change" column overstates the contrast, but the diversified number stands on its own
32 games. Whether this is specific to the distributional mu/sigma head or to that one
seed is untested: only one dist agent is in the pool.

## Follow-up: the dist architecture ladder (20 agents, 6080 games)

Developer question: why does `learned(s111,78ef6974)` rate last among the d6
contenders, is it overtrained, or too wide or thin in a layer? Answered by adding
the rest of the dist family so architecture and seed could be separated.

**First, `s111` is the NNUE-shaped head from theory 37**: `mu_layers = 129,512,8,1`,
a 512-wide first layer into an 8-unit bottleneck, `s_layers = 129,64,1`.

**Second, the premise was wrong.** `slot76` is ALSO a dist model (`type=dist`,
`mu_type=linear`, no hidden layer) and it is the strongest agent at that head. The
question was never "do dist models generalise badly".

**Third, `s111` is the LUCKY seed of its architecture, not a poor one.** With four
seeds rated, `129,512,8,1` spans 986 to 1128 at +/- 16 each, a 142-Elo range, and
`s111` is the top of it. Its architecture's mean is 1034. Filed as theory 40.

### Diversified pool, 32 games/pair, one fit, head `ab(d6,tt,ord,nb200k)@1`

| Elo | Architecture | Agent |
|---|---|---|
| 1215 +/- 16 | dist, linear mu | `learned(s76,ef183148)@1` |
| 1178 +/- 16 | learned linear | `learned(s6,eac8ab99)@1` |
| **1166 +/- 16** | **chip counter** | **`classic(t1,c4,w0,l0)@2`** |
| 1164 +/- 16 | learned linear | `learned(s98,5801570e)@1` |
| 1136 +/- 16 | learned linear | `learned(s3,68364898)@1` |
| 1128 +/- 16 | dist `129,512,8,1` | `learned(s111,78ef6974)@1` |
| 1096 +/- 16 | dist `129,128,64,1` | `learned(s77,ddaa5090)@1` |
| 1081 +/- 16 | dist `129,128,64,1` | `learned(s78,2fa21eda)@1` |
| 1027 +/- 16 | dist `129,512,8,1` | `learned(s113,e3cc8b4e)@1` |
| 1025 +/- 16 | dist `129,256,128,1` | `learned(s79,18f19059)@1` |
| 993 +/- 16 | dist `129,512,8,1` | `learned(s115,21d7e638)@1` |
| 986 +/- 16 | dist `129,512,8,1` | `learned(s110,1466db6c)@1` |

Outside that head: the d8 oracle 1399 (reference), the hill-climbed
`adv(t20,c77,...)` 1249 at `ab(d6,ord,nb200k)`, and `learned(s98,...)` + quiescence
1181 at `ab(d6,tt,ord,qs,nb200k)`.

### Grouped by architecture

| Architecture | Seeds | Mean Elo | Range |
|---|---|---|---|
| dist, linear mu | 1 | 1215 | -- |
| learned linear (replay-trained) | 3 | 1159 | 1136-1178 |
| chip counter (reference point) | 1 | 1166 | -- |
| dist `129,128,64,1` | 2 | 1088 | 1081-1096 |
| dist `129,512,8,1` | 4 | 1034 | 986-1128 |
| dist `129,256,128,1` | 1 | 1025 | -- |

**Every hidden-layer dist model rates below a plain material counter. Both linear
families rate at or above it.** Filed as theory 39, untested hypothesis.

It is NOT the 8-unit bottleneck specifically: `129,256,128,1` has no bottleneck and
sits at 1025, level with `129,512,8,1`'s 1034. The pattern tracks having hidden
layers at all, not their shape.

### Win rate against the chip counter, same head, fit-independent

| Model | Architecture | Fixed start | Diversified |
|---|---|---|---|
| s76 | dist linear | 53% (n=32) | 41% (n=32) |
| s6 | learned linear | 38% (n=32) | 59% (n=32) |
| s98 | learned linear | 72% (n=32) | 66% (n=32) |
| s3 | learned linear | 69% (n=32) | 69% (n=32) |
| s77 | dist 128,64 | 38% (n=8) | 34% (n=32) |
| s78 | dist 128,64 | 50% (n=8) | 31% (n=32) |
| s79 | dist 256,128 | 25% (n=8) | 25% (n=32) |
| s110 | dist 512,8 | 50% (n=8) | 31% (n=32) |
| s111 | dist 512,8 | 50% (n=8) | 25% (n=32) |
| s113 | dist 512,8 | 62% (n=8) | 19% (n=32) |
| s115 | dist 512,8 | 75% (n=8) | 31% (n=32) |

The fixed-start column for every dist MLP is an 8-game fill, which this project's
hygiene rule 2 says never to conclude from, so the apparent swings are not evidence.
What IS evidence is the diversified column on its own: at n=32 each, every dist MLP
scores 19-34% against a material counter while the linear learned models score
59-69%.

### On the overfitting hypothesis

Theory 37 measured this architecture's held-out prediction as neutral (MAE 143.5 /
NLL 0.40795 versus the wide head's 146.2 / 0.4079), so it is not overfit to the
training SAMPLE. Those held-out positions came from `posgen` sampling standard-start
games, so they cannot see distribution-level overfit, which is what theory 39
proposes. In-distribution discrimination is also intact: mu spread on the
standard-start eval pool is 86-113 SD across all seven architectures tested, with the
NNUE-shaped seeds mid-pack, so nothing is collapsed in-distribution.

The direct test, NOT run: build a position pool from the diversified games with
`rank.exe posgen --in ranking/matches_open.jsonl` and compare each model's mu spread
on that pool against its spread on the standard pool. A within-model comparison, so
it needs no labels. Started, then stopped to give the rating run full CPU, and left
undone by the scoping decision below.

### Scope note

The developer scoped this deliberately (2026-07-26): the session was after major Elo
swings and opener effects in general, not rigour on the architecture-by-opener
intersection. Two seeds of `129,512,8,1` (`s112`, `s114`) are benched in
`ranking/roster_open.txt` rather than rated, and no additional seeds were trained for
the `128,64` and `256,128` rungs, which is why those rungs have 2 and 1 seed against
theory 40's measured 142-Elo seed spread. Theory 39 is therefore filed as an
observation to explore in a later session, not as a result.

## Caveats

- 14 agents is a small pool. The Bradley-Terry prior compresses less than the
  116-agent fit, which is one more reason these Elo values cannot be read against the
  fixed-start ones.
- `.opener(rand,4)` (8 plies of random play) is one arbitrary diversification depth,
  never swept. Deeper openings would produce more unbalanced starts, shallower ones
  less diversity.
- Uniform-random opening plies are not curated. Some starts are likely already
  decided, which adds variance the paired design cancels between the two agents but
  does not remove from the pool. A mined pool of near-balanced openings would be
  better, and `rank.exe posgen` already produces exactly that shape of artifact.
- Only one dist agent and one hill-climbed agent are present, so neither family has a
  seed replicate inside this pool to bound seed noise.
- Rank order is still moving at 32 games/pair (3 of 14 agents changed rank on the
  last doubling).

## A mistake worth recording

The first run of this pool wrote its 364 games into the MAIN store and refit it.
Cause: `tools/run_rank.ps1` owns `--in`/`--out` in worker mode and takes the store via
its own `-Store` parameter, which its header comment says plainly. The pass-through
`--in`/`--out` were appended after the driver's own and lost. Detected because
`matches_open.jsonl` was 0 bytes. Recovery was exact: `git diff --numstat` showed 364
added and 0 deleted lines, all matching `opener(rand,`, so those lines were extracted
into `matches_open.jsonl`, `matches.jsonl` was restored from the commit, and the main
pool re-rated back to 123,854 games and 320 agents. Same failure class as the others
this session: the tool's contract was assumed rather than read.

## Future Work

- **Sweep the opening depth.** Tethered to: the arbitrary `.opener(rand,4)`. Rate the
  same agents at 2, 4, 8, and 12 own-plies and see where rank order stops changing.
  Too shallow gives no diversity, too deep gives decided positions.
- **Replace random openings with a mined balanced pool.** Tethered to: the "uniform
  random is not curated" caveat. Use `rank.exe posgen` to pull distinct positions at
  ply 6-12 from games that were not decided early, filter to near-equal material, and
  sample one per couple. Would keep the paired design while removing the junk-position
  variance. Needs a new opener kind that reads a position pool.
- **Add a second dist seed.** Tethered to: the s111 result, which is currently one
  agent and cannot distinguish "the distributional head generalises badly across
  openings" from "this seed is weak". Add `learned(s113,...)` or `learned(s115,...)`
  and re-rate.
- **Add seed replicates for the learned recipe.** Tethered to: the s6 +21pp / s76
  -12pp split, which is currently indistinguishable from seed noise. Three or four
  seeds of one recipe inside this pool would bound the band directly.
- **Decide whether this pool or the fixed-start pool defines the champion.** Tethered
  to: `ranking/CHAMPION.md`, whose current holder is a book agent whose advantage this
  instrument is designed to be immune to.

## Ideas This Inspired

- Paired openings are a general variance-reduction tool, not book-specific. The same
  couple-seed trick would sharpen the dilution ladder and the hill climber's fitness
  function, both of which currently pay full variance for their stochasticity.
- A **recovery book**: mine from positions that arise AFTER a bad random opening
  rather than from the standard start. That would be a book encoding "how to refute
  from a worse position", which is a transferable skill rather than the memorisation
  theory 38 found. Already filed in `todo.md`.
- Rank-order stability (Spearman rho between successive fills) is a better stopping
  rule than a fixed games/pair target, and it is cheap to compute. It could be printed
  by `rank.exe rate` whenever a previous ratings file exists.
- The two-pool split suggests a general pattern: one instrument per question, named by
  its store, rather than one roster trying to answer everything. Speed-constrained and
  time-controlled pools would slot in the same way.
