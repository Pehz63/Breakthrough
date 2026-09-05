# Ranking workflow: adding agents and reading rankings

How to get a new agent into `ranking/roster.txt` and get a trustworthy number
out. Read this before any ranking run. Companion docs: `Docs/benchmarking.md`
(measurement defects), `ranking/CHAMPION.md` (who holds which title and the
certification rules), `Docs/terminology.md` (head, core, loadout, lift).

## The two questions, and why they need different instruments

These are NOT the same question, and using the wrong instrument has produced
wrong conclusions in this project before:

| Question | Instrument | Can it change the champion? |
|---|---|---|
| "How strong is this new agent, on the scale I already know?" | **Pinned fit** (`rate --pin`) | **No, never** |
| "What is the true ranking of everything now?" | **Unpinned refit** (`rate`) | Yes, this is the only thing that can |

The reason they differ: an unpinned Bradley-Terry refit re-solves *every* rating
simultaneously. Adding agents therefore moves the whole table, so numbers from
the previous fit stop being comparable (`Docs/benchmarking.md`, "Elo scale drift
across fits"). That is correct behaviour for certification and a nuisance during
a study, where you want a stationary reference to measure against.

## Pinning: what it is and what was added

`rankFitBTPinned` (`src/ranking.cpp`) is the same MM fit as `rankFitBT` with a
**subset of ratings held fixed**. Mechanically:

- Each pinned agent enters the fit at `g = exp(elo / ELO_PER_NAT)` and is
  **never updated** by the MM sweep.
- The per-sweep **geometric-mean renormalization is dropped**, because the pins
  now define the scale. Renormalizing would drag them, which is exactly what
  pinning exists to prevent.
- Free agents are solved normally against the pins **and against each other**.
- A free agent with no game path to any pinned agent is unidentified on the
  pinned scale, so it is flagged `provisional` and centered on its own
  component mean of 1000.
- A pinned rating is an **input**, not an estimate, so it is reported with
  **SE = 0**.

Two CLI flags expose it:

- **`rank.exe rate --pin <ratings.tsv | standings.tsv>`** - every agent listed
  in that file is pinned at its listed Elo. Only agents actually present are
  frozen, so `--pin ranking/standings.tsv` pins the **active roster** and lets
  retired rows float, while `--pin ranking/ratings.tsv` pins **everything ever
  rated**. Writes its own `ranking/*_pinned.*` family and leaves the canonical
  files untouched.
- **`rank.exe play --cohort <id list>`** - schedule only pairs with at least one
  side in that list. Without it, a `--games 32` pass also tops up every existing
  roster pair: measured 2026-07-29, the store held 27,265 pairs at a median of 6
  games each, so that is about 702,000 games with nothing to do with your new
  agents.

Verified on the real 158-agent store: max pinned drift **0.000000000**, and the
canonical `ratings.tsv` / `standings.tsv` were byte-identical (md5) afterwards.

### Why pinning beats running N gauntlets

`rank.exe gauntlet` already rates one candidate against a frozen pool
(`rankFitSingle`). Pinning is strictly more informative for a group, because it
also uses **cohort-vs-cohort** games, so it resolves the new agents' ordering
*among themselves*. A gauntlet never plays candidates against each other, so two
candidates that score identically against the roster come out tied even when one
beats the other every time. A test covers exactly that case.

### Default gauntlet pool

When a gauntlet (or any other single-candidate screening check) is the right
instrument, `rank.exe gauntlet` needs a pool of opponents to play. Use
`ranking/roster_screening_pool.txt` as the default pool rather than hand-picking
opponents each time: 26 agents plus the `rand@1` anchor, spanning the 5
opener categories active when it was built (openless/4-book/8-book/4-random/
8-random) and a deliberate Elo spread from 1000+ down to ~100, built
2026-08-17 for exactly this purpose. It has not been rebuilt against the
2026-08-24 6-category restructure (`ranking/CHAMPION.md`), so its composition
still describes the pre-restructure taxonomy rather than the current one. Its
own header comment documents composition and how to extend it. This is
distinct from `ranking/roster_top.txt`, which exists to boost the openless
top-of-table to certification fill (Workflow B) rather than to give a new,
possibly-weak candidate an interpretable point on the scale.

### The hard limit

**A pinned fit can never dethrone a champion.** The champions' ratings are
inputs to it. If a cohort agent rates above a pinned champion in a pinned fit,
that is a *screening signal to go certify*, not a result. Certification is the
unpinned refit (`ranking/CHAMPION.md` rule 1).

## Cost model: what a cohort run actually plays

Neither workflow below tells you what a run will cost, and the number surprises
people, so it is written out here. **A cohort agent plays EVERY active agent on
the working roster.** `--cohort` restricts which agents get SCHEDULED, not which
opponents they are given. There is no opponent-pool flag for a `play` run, so the
pool is the roster, and the only lever on cost is how many agents are `on`.

```
games ~= (cohort size) x (active roster size) x (games per pair)
         + C(cohort size, 2) x (games per pair)     # cohort-vs-cohort
```

**Games per pair is not the `--games` number for most pairs.**
`pairGameTarget` (`src/ranking.cpp`) returns **2** when BOTH agents are
deterministic, and `--games N` otherwise:

```cpp
static int pairGameTarget(const RankAgent& a, const RankAgent& b, int gamesPerPair) {
    if (rankAgentIsDeterministic(a.spec) && rankAgentIsDeterministic(b.spec)) return 2;
    return gamesPerPair;
}
```

Two is both floor and ceiling there: fewer leaves a colour unmeasured, more just
stores copies of one trajectory and understates the error bar (`Docs/benchmarking.md`,
defect 3). Since most of the roster is deterministic, this roughly halves a run
against a naive reading of `--games 8`.

**A worked example, the 2026-09-05 `retain` study**, one completed cell
(`ab(deep=12,tt,ord,rem=70,retain,nodes=100k)@2.classic(chip=100)@2`) measured
against the store:

| quantity | value |
|---|---|
| rows for this one agent | 1,338 |
| distinct opponents | 306 |
| opponents at 2 games (deterministic pair) | 185 |
| opponents at 8 games (`--games 8`) | 121 |
| distinct trajectories per row | 0.95 |
| rows against other study cells | 58 (4%) |

40 cells at that size is about 52,000 games. The 4% cohort-vs-cohort share is
small but is the part a gauntlet cannot produce, and it is what resolves the
cells' order among themselves.

### How much of that carries rating information

For a Bradley-Terry fit one game contributes `p(1-p)` of Fisher information,
where `p` is the expected score, so a pairing decided 8-0 contributes almost
nothing. Measured on three cells of the same study, using each opponent's
Jeffreys-smoothed score rate:

| cell | games | share against opponents at >90% or <10% | effective 50/50 games |
|---|---|---|---|
| `retain,nodes=100k` chip counter | 1,338 | 37.7% | 721 |
| same, tdleaf_self lin `model=169` | 1,338 | 47.8% | 628 |
| same, pool_games lin `model=97` | 1,338 | 25.7% | 866 |

**So roughly a quarter to a half of a cohort run confirms what the pool already
knew.** That is the price of putting every cell on one scale with no prior about
where it will land, and it is not free to avoid: the low end of the roster exists
to spread the ladder, and a new agent has to touch enough of it to connect to the
`rand@1` anchor.

### Sizing a run, in practice

- **Count the cells before launching, and multiply.** The roster size is printed
  by `rank.exe check`. Decide whether the answer is worth that many games and say
  the number to the developer if it is large.
- **Cut rungs, not games per pair.** Dropping a budget rung removes whole cells,
  which is a linear saving. Dropping `--games` below 8 mostly does nothing, since
  the deterministic pairs are already at 2, and it weakens the pairs that carry
  the most information.
- **A stopped run keeps its work.** Shards write to `<store>.<n>` and are appended
  at the end, so killing a run mid-flight loses only the unmerged tail. Merge the
  shard files by hand, validating each line, then rerun: the scheduler counts
  stored games and only tops up what is missing. Do NOT blind-append a shard file
  that a process was writing when it died. On 2026-09-05 a hand merge of 10 shards
  recovered 23,397 games with zero torn lines, and the same store already carried
  3 torn rows from an ordinary end-of-run merge that did not validate.
- **Bench, do not delete, the cells you abandon.** Their partial games stay in the
  store. Setting them `off` keeps a low-game-count row from reaching a fit and
  being quoted (`PINNED AT LOW GAME COUNT`, `Docs/corrections.md`).

### Known gap

There is no way to give a cohort a narrower opponent pool. An `--opp-roster`
option, or an Elo-window filter around the cohort's expected rating, would cut a
screening run by roughly a third at no cost to the error bars that matter, since
the games it would drop are the ones already contributing near-zero information.
Not implemented, and the tradeoff to check first is anchor connectivity: the fit
needs the cohort connected to `rand@1`, so a window would have to keep some
spread rather than only near-equal opponents.

### Scheduling a study: ladder, do not guess `--games`

`--games N` is a target, not an increment: the scheduler counts stored games per
pair and plays only the deficit. So running `--games 2`, then `4`, then `8`, then
`16`, fitting between each, plays exactly the same games as one `--games 16`
launch, and costs only the extra process startups plus one `rate` per rung. That
buys a preliminary answer early and lets the data choose the final `N`.

The stopping rule must be stated on a within-fit contrast (treatment minus control
at matched core and budget), never on absolute Elo, which is not comparable across
fits. Full procedure, the three stopping conditions, and the proposed `--ladder`
and `--stop-when` flags:
`plans/ranking-run-scheduling-plan-1-tidal-lantern.md`.

## Workflow A: add agents and screen them (does not disturb anything)

Use during a study, when you will add and discard many candidates.

1. **Train models into slots.** A slot number is part of the agent's canonical
   identity, so each rated agent needs its own slot. Current allocation is in
   `src/CLAUDE.md` under `slotFile()`; claim a free range and record it there.

2. **Get each slot's content hash.**
   ```
   rank.exe check
   ```
   prints `models/sweep/slotN.txt = <hash8> (slot N)` for every slot.

3. **Build a working roster and a cohort list.** Copy `ranking/roster.txt`, then
   append one `on <id>` line per new agent. A learned agent's ID is
   `<head>.learned(s<slot>,<hash8>,<arch>)@1`, e.g.
   ```
   on ab(deep=6,tt,ord,nodes=200k)@1.learned(model=128,7d73ec01,value,lin,129-1,con100)@1
   ```
   Keep **one head** for everything you intend to compare (`CHAMPION.md` rule
   6). Write the same IDs, one per line, to a cohort list file.

4. **Validate before spending any compute.**
   ```
   rank.exe check --roster <working roster>
   ```
   Non-canonical IDs and stale `@N` versions fail here, with the corrected form
   printed. A learned agent whose slot file no longer matches its hash is a hard
   error.

5. **Play only the new pairs.**
   ```
   tools\run_rank.ps1 -Workers 12 play --roster <working roster> --cohort <cohort list> --games 8
   ```
   Sanity check: pending games should equal
   `(cohort x other_active + cohort_internal_pairs) x games_per_pair`. If it is
   far larger, `--cohort` did not take effect and you are refilling the roster.

6. **Screen on the frozen scale.**
   ```
   rank.exe rate --roster <working roster> --pin ranking/standings.tsv
   ```
   Read `ranking/standings_pinned.tsv`. Pinned rows come back byte-identical to
   their input, which is the check that the instrument behaved.

7. **Check the game count of every pinned row you intend to compare against**
   (see `PINNED AT LOW GAME COUNT`, `Docs/corrections.md`). A pinned agent's Elo
   is frozen at whatever the pin file recorded, so new games for it change
   nothing, and a row pinned on a handful of games hands back that handful's
   noise looking like a settled rating. Read the `games` column in the PIN FILE,
   not in the pinned output. When a comparison depends on such an agent, delete
   its row from a copy of the pin file and refit against the copy: it then
   absorbs the new games while the rest of the scale stays frozen. Dropping 2 of
   223 rows this way on 2026-09-04 left the other 221 byte-identical.

Nothing in this workflow can alter `ranking/ratings.tsv`, `standings.tsv`, or
any champion. It does append real games to `ranking/matches.jsonl`, which is
correct and permanent: the store is append-only and never regenerated.

**Where the store lives.** `ranking/matches.jsonl` is the live *tail*, not the
whole store. The rest sits in parts listed by `ranking/matches.index.txt`
(`rank.exe split`), and everything appends to the tail as before. Two
consequence for this workflow: the rating outputs are gitignored, so run a
`rank.exe rate` before reading `standings.tsv` in a fresh clone. Confirm every
index-listed part is present before running Workflow B, since a missing part is
skipped silently by design.

**Screening cohorts do not play into this store.** They play into
`ranking/matches_screen.jsonl` (`tdleaf_study.ps1 -ScreenStore`), and only the
games of agents you actually promote get merged back in, via `rank.exe split`
over the screening store. This exists because the TD-Leaf Pass-2 cohort put
457,611 rows of games against never-promoted candidates into the permanent
ladder -- 71% of the store, with some rostered agents taking 60% of their games
against agents that were then discarded. Those rows were dropped from the fit on
2026-08-01 (lines removed from `matches.index.txt`) and the files themselves
DELETED 2026-08-02, which **re-certified the openless champion**. That
population is gone for good -- it was never committed, so the excluded games
cannot be reconstructed and the pre-drop fit cannot be reproduced. Note that
dropping games is never free: Bradley-Terry fits jointly, so a game against a
retired agent is evidence about the rostered agent that played it, and this
change moved 153 of 170 rostered agents. Details:
`plans/store-sharding-results-1-tidy-albatross.md`.

## Workflow B: certify (this is what can dethrone a champion)

Run deliberately, once, after choosing which agents to keep.

1. **Decide what stays.** Keep a spread rather than only the single best: the
   roster's value is a well-connected, diverse pool. Retire near-duplicates by
   flipping them to `off` rather than deleting them; their games stay as history.

2. **Edit `ranking/roster.txt` itself**, adding the keepers as `on` lines.

3. **Fill the contenders to >= 32 games/pair** before any top-of-table claim
   (`CHAMPION.md` rule 2). `ranking/roster_top.txt` exists for this; keep its
   list current. **Never conclude from an 8-games/pair fill** - it has inverted
   the top of the table three times in this project's history.

4. **Refit with no `--pin`.**
   ```
   tools\run_rank.ps1 -Workers 12 -- run --games 32
   ```

5. **Read `ranking/standings.tsv`, never `ratings.tsv`.** `ratings.tsv` is the
   full historical fit and includes retired (`gone`) rows; quoting one as
   current strength has already produced a 91-Elo phantom gap.

6. **If the top may have changed, re-certify in the same session:** update
   `ranking/CHAMPION.md` and `todo.md`'s Agent Track goal paragraph
   (`CHAMPION.md` rule 7).

## Reading any result

- **Compare order and error bands within ONE fit.** Never compare absolute Elo
  across fits; the Bradley-Terry prior compresses the scale as the pool grows.
- **One search head per comparison.** `ab(deep=6,tt,ord,nodes=200k)@1` and
  `ab(deep=6,ord,nodes=200k)@1` are different agents. A table mixing heads is not an
  evaluator comparison.
- **Match loadouts.** Never compare a bare core against an equipped one and call
  it an evaluator result.
- **Count distinct games, not stored rows.** A pair with no dilution, no random
  opener and no random-move agent has an inert seed and replays one game per
  colour however many rows the store holds. Median across the fixed-start store
  is 0.438 distinct games per row, so printed `pm` is understated by roughly
  1.5x. Full explanation: `Docs/benchmarking.md`, defect 3.
- **Check margins against the seed-noise band.** 50-150 Elo between seed
  replicas of one training recipe, so a single seed is never a conclusion.
- **Checkpoints of one training run are not independent replicates.** They share
  a trajectory and differ only in how long it ran. Only distinct seeds count
  toward the seed band.
