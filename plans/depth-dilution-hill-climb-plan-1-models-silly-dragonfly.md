# Hill-climb Experimental eval weights for Elo, add diluted opponents, rank the winners

## Context

The persistent ranker (`rank.exe`) rates a fixed 32-agent roster, but the Experimental
evaluator's weight mix (chip / wall / column / forward) has only been sampled at a few
hand-picked points. We want to (a) automatically search that weight space for the
highest-Elo mix at a given depth via a simple stochastic hill climber, and (b) enrich the
roster so rankings are better resolved and the winners get fully rated.

Two problems shape the design:

1. **Scale invariance.** The evaluator is scale-invariant for move selection, so uniformly
   scaling a weight vector produces an identical player. The climb must vary the *relative*
   mix, not the scale. We fix this by normalizing every candidate to a fixed magnitude: turn
   pinned at 20, chip+wall+column+forward renormalized to sum to 80 (total 100). This pins the
   scale so scalar duplicates collapse to one canonical integer-simplex point, which dedupes
   the cache and keeps the search well-conditioned.
2. **Determinism -> ties / overfitting.** Alpha-beta agents are deterministic, so a candidate
   replays the *same* game against a deterministic opponent every time (only color alternation
   gives 2 distinct lines). Results quantize into ties and a candidate can overfit a single
   lucky line. Diluted opponents inject per-move randomness so each of N games differs, giving
   a smooth win rate. We add a dense ladder of diluted strong-d6 opponents.

The ranker supports two dilution knobs today: random-move dilution `dil(rP)@1` (play a fully
random move P% of the time, [agents.cpp:77](src/agents.cpp#L77)) and a deterministic depth cap
`ab(d6,capN)` ([agents.cpp:87](src/agents.cpp#L87)). It does **not** support *stochastic*
depth dilution ("a d6 that plays a d3 move P% of the time"), which is a better noise source
(plausible-but-weaker moves rather than blunders). We add that as a small engine feature.

Key facts (from `src/ranking.cpp`, `src/agents.cpp`, `src/ai_eval.cpp`, verified by Explore):

- `rank.exe gauntlet --id "<id>" --games N` plays one candidate against every rated active
  roster agent (N games each, alternating colors), fits an anchored Elo via `rankFitSingle`
  ([ranking.cpp:1001](src/ranking.cpp#L1001)), and prints `  Elo <n> +/- <se> (pool ratings
  held fixed)`. Without `--keep` it writes only scratch `ranking/gauntlet.jsonl` (never the
  permanent store). Opponent Elos are read from `ranking/ratings.tsv`; active agents lacking a
  rating are skipped. Same anchored scale as the main table. This is the fitness function.
- A candidate is deterministic by id: same id + roster + seed -> identical games -> identical
  Elo, so repeated normalized candidates are cached (no per-eval resampling).
- Canonical Experimental id: `ab(dD)@1.exp(t20,cC,wW,lL,fF)@1`. All weights present, registry
  order (turn,chip,wall,column,forward), integers. Any non-canonical id is rejected by gauntlet.

## Deliverables

### 1. Engine feature: stochastic depth dilution (C++)

When the dilution trigger fires, play a shallow search at a "dilution depth" instead of a
fully random move. Encoded as an optional second `dil()` argument: `dil(r20,d3)@1` = "search
at the agent's depth, but 20% of moves use a depth-3 search". `dil(r20)@1` (no depth) keeps
its exact current meaning (fully random move).

- `src/agents.h`: add `int dilDepth;` to `AgentSpec` (0 = no depth dilution) next to
  `depthCap`, with a comment.
- `src/agents.cpp`: init `a.dilDepth = 0` in both `agentMakeSearch` and `agentMakePolicy`.
  In `agentChooseMove`, compute the trigger once, then:
  `if (dilute && (a.dilDepth <= 0 || a.brain != BRAIN_SEARCH)) return pureRandomMove...;`
  (existing fully-random path), and for the search brain set the effective depth to
  `a.dilDepth` when `dilute && a.dilDepth > 0` (before the existing `depthCap` clamp). The
  per-agent budget/feature save-restore is unchanged, so the shallow move still uses the
  agent's tt/ord/nb settings. Extend `agentDescribe` with a `dil-dN` note.
- `src/ranking.cpp`: emit (near [line 293](src/ranking.cpp#L293)) append `,d<dilDepth>` inside
  `dil(...)` when `dilDepth > 0`. Parse (near [line 549](src/ranking.cpp#L549)) accept an
  optional `args[1]` of the form `d<int>`: require it start with `d`, parse a positive int,
  and validate it is a search head with `0 < dilDepth < depth` (a dilution must be shallower).
  Store into `a.dilDepth`. Keep `RK_DIL_VERSION = 1` (plain `dil(rP)@1` behavior and identity
  are unchanged, so existing diluted agents keep their match history; the new `,dN` form is a
  backward-compatible grammar extension). Update the grammar comment in `src/ranking.h`.
- `tests/test_ranking.cpp`: replace the "reserved future arg" rejection at
  [line 133](tests/test_ranking.cpp#L133) with a `dil(r30,d3)@1` round-trip (assert
  `dilDepth == 3`, `randomMoveProb == 0.30`) plus rejections for a bad second arg
  (`dil(r5,x)`, `dil(r5,d0)`, `dil(r5,d9)` where 9 >= depth) and a non-search head
  (`rand@1.dil(r5,d3)@1`).
- Rebuild both `rank.exe` (`build_rank.bat`) and `tests.exe` (they link `agents.cpp` /
  `ranking.cpp`); `breakthrough.exe`/GUI are unaffected (feature is inert unless `dilDepth` set).

### 2. Diluted strong-d6 batch appended to `ranking/roster.txt` (~20 agents)

A dense ladder over the strongest pruned d6 base
`B = ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1`, all new `on` lines:

- **12 random-move dilutions**: `B.dil(rP)@1` for P in
  `4, 7, 10, 14, 18, 22, 28, 35, 43, 52, 63, 75`.
- **8 stochastic depth dilutions** (needs deliverable 1): a d3 ladder
  `B.dil(rP,d3)@1` for P in `15, 30, 45, 60`, and a d4 ladder `B.dil(rP,d4)@1` for the same P.

This gives a fine-grained, mostly-stochastic strength ladder at the top of the table, which
resolves the current d5/d6 ties and supplies non-deterministic climb opponents. They must be
rated by a full run *before* climbing (gauntlet skips unrated actives). Because ~20 agents
pair with the whole roster, rate the expansion with `-Workers` and a modest `--games` (see
Verification).

### 3. `ranking/climb_roster.txt` (fast, stochastic opponent set)

A reduced roster so each gauntlet eval is cheap (the full roster is ~52 opponents/eval). Every
entry must already be rated (run deliverable 2 first). Exactly one anchor. Brackets from
random up through d6 but leans on the diluted batch so most opponents are stochastic:

```
anchor  rand@1
on      smart(6)@1
on      ab(d3)@1.classic(t1,c4,w0,l0)@1
on      ab(d4)@1.classic(t1,c4,w0,l0)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r10)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r22)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r43)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r63)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r30,d3)@1
on      ab(d6,tt,ord,nb200k)@1.classic(t1,c4,w0,l0)@1.dil(r30,d4)@1
```

### 4. `tools/hill_climb.ps1` (the climber)

Mirrors `tools/run_*.ps1` conventions (`[CmdletBinding(PositionalBinding=$false)]`, `-Build`
switch that shells `build_rank.bat`). Parameters / defaults:

- `-Depth 4` -> head `ab(d<Depth>)`. `-Head "<flags>"` overrides for a budget band
  (e.g. `-Head "d6,tt,ord,nb200k"`).
- `-Roster ranking/climb_roster.txt` (fast; point at `ranking/roster.txt` for full fidelity).
- `-Games 4` (per opponent), `-Iters 40`, `-Seed 1`, `-Drastic 0.3`.
- `-Sum 100`, `-Turn 20` (fixed); the four positional weights renormalize to `Sum-Turn` = 80.
- `-Start "c4,w0,l0,f1"` seed direction (renormalized to sum 80; turn never mutated).
- `-Promote` + `-PromoteTop 3` + `-PromoteGames 8`.

`Normalize-Weights`: scale `(c,w,l,f)` to sum 80, round to integers via largest-remainder so
they sum to exactly 80; all-zero input falls back to `(80,0,0,0)`. Every candidate passes
through this before its id is built, so the cache key is canonical.

Algorithm (greedy hill climb from the best, with drastic escapes):

1. Normalize `-Start`, evaluate it, set as best.
2. Each of `-Iters` iterations, mutate **from the current best** on the sum-80 simplex:
   - prob `-Drastic`: set `chip` to a random int in `[0,80]` (the drastic chip mutation) and
     distribute `80 - chip` across wall/column/forward at random.
   - else: move a delta of `{1,3,5}` units from one random component to another (source
     clamped at 0), preserving the sum.
   - Re-normalize to sum 80; resample only if it collapses to all-zero.
3. Fitness: build id `ab(<head>)@1.exp(t<Turn>,c,w,l,f)@1`; if cached, reuse; else run
   `.\rank.exe gauntlet --id "<id>" --games <Games> --roster <Roster> --seed <Seed>`, parse
   `Elo\s+(-?\d+)\s+\+/-\s+(\d+)`, cache by id.
4. Adopt as new best iff Elo strictly greater.
5. Append a row to `ranking/climb_exp_d<Depth>_<UTCstamp>.tsv`
   (`iter accepted elo se turn chip wall column forward id`); print a running best line.
6. After the loop, print the best id + top distinct candidates.

Promotion (`-Promote`): take the top `-PromoteTop` distinct ids by Elo, append any missing as
`on <id>` lines to the real `ranking/roster.txt` (dedupe by exact id), then run
`.\rank.exe run --games <PromoteGames>` for a full anchored refit that includes them, and
print the new top of `ranking/ratings.tsv`. Header note in the script: gauntlet's scratch
file is truncated per call (climber is serial); the pool is only as fresh as `ratings.tsv`.

### 5. Docs (required by CLAUDE.md standing instructions)

- `README.md`: "Hill-climbing eval weights" note (command, what it optimizes, auto-promote)
  and a mention of the diluted-d6 opponent ladder + the new `dil(rP,dN)` depth-dilution form.
- `CLAUDE.md`: add `tools/hill_climb.ps1` + `ranking/climb_roster.txt` to the root file table;
  usage blurb under "Ranker (`rank.exe`)"; document the `AgentSpec.dilDepth` field and the
  `dil(rP,dN)` codec in the `agents.*` / `ranking.*` rows.
- `todo.md`: add the task, strikethrough when done.

## Critical files

- New: `tools/hill_climb.ps1`, `ranking/climb_roster.txt`.
- Feature: `src/agents.h`, `src/agents.cpp`, `src/ranking.cpp`, `src/ranking.h`,
  `tests/test_ranking.cpp`.
- Edited: `ranking/roster.txt` (append the ~20 diluted-d6 lines now; promoted winners later).
- Reused as-is: `rank.exe gauntlet` / `rankFitSingle`, `tools/run_rank.ps1`, `build_rank.bat`.
- Docs: `README.md`, `CLAUDE.md`, `todo.md`.

## Verification

1. Build + test the feature: `.\tools\run_tests.ps1 -Build` passes (new `dil(rP,dN)`
   round-trip + rejection tests). Then `.\tools\run_rank.ps1 -Build check` builds `rank.exe`
   and validates the roster (all new ids canonical).
2. Sanity-check depth dilution end to end: `.\rank.exe check` reports the expanded roster with
   no errors, and `.\rank.exe history` or a short `run` shows the `dil(rP,dN)` agents playing
   (they should sit between the deterministic dN and the full d6 in Elo).
3. Rate the expanded roster (~52 agents): `.\tools\run_rank.ps1 -Workers 8 run --games 4`.
   Confirm `ranking/ratings.tsv` contains the 20 diluted-d6 agents and `ranking/report.md`
   shows a graded, mostly-tie-free top-end ladder.
4. Smoke the climber: `.\tools\hill_climb.ps1 -Iters 4 -Games 2 -Depth 2`. Expect 4 gauntlet
   evals, a `ranking/climb_exp_d2_*.tsv` with 5 rows, a printed best line, and no change to
   `ranking/matches.jsonl` (only scratch `ranking/gauntlet.jsonl`). Ids are canonical
   `exp(t20,...)` with positional weights summing to 80.
5. Real climb: `.\tools\hill_climb.ps1 -Iters 40 -Games 4`. Best Elo trends up, drastic chip
   swings appear, repeated normalized candidates hit the cache (no duplicate gauntlet calls).
6. Promotion: `.\tools\hill_climb.ps1 -Iters 20 -Promote -PromoteTop 2`. Up to 2 new
   `on ab(d4)@1.exp(...)@1` lines land in `ranking/roster.txt`, the full `run` completes, and
   the new mixes appear in `ranking/ratings.tsv` / `ranking/report.md`.
