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

**`ranking/roster_open.txt`**, 14 agents each wearing `.opener(rand,4)@1` (4 of the
agent's OWN half-moves, so 8 plies of random play), plus `rand@1` as anchor, which
needs no opener because it already plays randomly.

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

## Standings, diversified pool, 32 games/pair

One fit. These Elo values are NOT comparable to the fixed-start pool's: different
instrument, different agent set, different Bradley-Terry prior.

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

## Did learned models collapse under diversified openings?

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
