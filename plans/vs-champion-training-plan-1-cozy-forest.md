# Vs-Champion Training Study: pairgen infrastructure + data-source matrix

## Context

The session goal is an agent stronger than the current active #1 in `ranking/ratings.tsv`,
`ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2` at Elo 1077 (a depth-6 classic chip-counter),
without searching deeper than 6 in the final agent. Deeper searches are allowed only as
oracles/teachers, and an oracle may live in the rankings without being the target.

The chosen idea (developer's): train a value model not by self-play but from games involving
the champion, testing every plausible way to source that data. Two developer theories to
record now and answer in the results doc:

- **Theory 1:** a model trained only against the champion still does reasonably well overall
  (most of the pool is chip-count-like), but goes out of distribution and loses as new,
  differently-built bots enter the pool.
- **Theory 2:** "you'll never beat the champion by playing against a random dilution of the
  champion." Dilution-sourced data will not produce a champion-beater; oracle games and
  branch-mined winning lines are the more promising counter-data.

Known measured facts (do not re-derive): training-seed noise is 50-150 Elo and dominates
every hyperparameter axis. Replay-extraction training beat single-teacher self-play by
~250 Elo (best linear v2 PST = d6 Elo 920, `models/pst_value.txt`). The linear class is
near its ceiling, so no linear arm is expected to outrate the champion outright. The
deliverables are the theory verdicts, pool diversity, and pair-play generation
infrastructure that future regimes (MLP, pool-pair generation) reuse.

Verified code facts:
- `playGame` (src/ml_train.cpp:139) already plays two different AgentSpecs, but its dilution
  decay (lines 148-152) mutates BOTH sides, and `trainSupervisedValue` (line 221) builds one
  generator spec for both colors.
- `rank.exe` already links everything needed: `rankAgentFromId` (ranking.cpp:432),
  `loadModelSlots` (ranking.cpp:1128), and `playOneGameCapture` (ranking.cpp:1931), which
  captures every half-move from both sides and whose caller `rankExtract` (ranking.cpp:1950)
  emits the exact JSONL rows (`{"ver":2,"stm":...,"label":...,"idx":[...]}`) that
  `loadReplayDataset` (ml_train.cpp:645) reads via `train.exe --from-data`. White-centric
  labels + stm feature mean asymmetric play needs no labeling change.
- `train.exe` does NOT link ranking.cpp (build_train.bat), so generation goes in rank.exe.
  `teacher=replay:<file>` provenance for `--from-data` already exists; only the self-play
  dilution-decay params are missing from provenance.
- Study harness pattern to copy: `tools/train_scaling.ps1` (resumable CSV, train cell ->
  `rank.exe check` hash scrape -> `rank.exe gauntlet` screen -> d6 confirm).

## Stage 1: `rank.exe pairgen` (new subcommand)

New generation entry point in src/ranking.cpp + src/ranking.h, dispatch in
tools/rank_main.cpp (next to `extract`, reuse `getOpt`/`getInt`, add a `getDbl` helper
copied from train_main.cpp). Update `usage()`.

```
rank.exe pairgen --a <id> --b <id> --games N --out data/<file>.jsonl
    [--board boards/board1.txt] [--feature-version 2] [--seed 1]
    [--dil-apply a|b|both|none]        default none
    [--dil-start 0.3 --dil-floor 0.05 --dil-decay-plies 30]
    [--open-plies K]                   uniform-random first K half-moves for BOTH sides
    [--filter winner=a|b|any]          default any; emit only games won by that agent
    [--branch-tries T]                 see Stage 2, default 0
    [--shard i --of k]                 caller varies --out per shard
```

Implementation details:
- **`rankDilutedProb(start, floor, decayPlies, ply)`**: new pure helper declared in
  ranking.h (testable). Linear decay from start at ply 0 to floor at decayPlies, held
  after, `decayPlies <= 0` returns start.
- **`RankDilOverride { int apply; double start, floor; int decayPlies; }`** passed as an
  optional trailing `const RankDilOverride*` to `playOneGameCapture` (existing
  `rankExtract` call untouched). Inside, copy each spec to a local and mutate ONLY the
  designated side's `randomMoveProb` per half-move. The override replaces that side's own
  `randomMoveProb`, `dilDepth` stays 0 (diluted move = fully random).
- **`--open-plies K`**: for half-moves `h < K`, both sides play a uniform random legal move
  (via `generateMoves` + `rand()`), positions still captured. Needed because deterministic
  pairs (oracle vs champion, champion vs itself undiluted) otherwise produce only 2 unique
  games. This also ships the "random-first-K-plies opening diversity" idea.
- **Color alternation:** game g has agent A as White when `g % 2 == 0`. `--dil-apply a`
  follows agent A to whichever color it holds.
- **Seeding:** per-game seed from the existing FNV `gameSeed(whiteId, blackId, ordinal,
  runSeed)` pattern used by the scheduler/gauntlet, then `srand(seed)`. Deterministic and
  shard-invariant.
- **`--filter winner=a`**: play the game, discard all its positions unless agent A won
  (draws discarded when a filter is set). Print kept/discarded tally.
- **Output:** truncate `--out`, append v1/v2 rows exactly like `rankExtract` lines
  1997-2014. Final summary line: games played/kept, A-perspective W-L-D (label-skew
  report), positions written.
- **Provenance sidecar:** write `<out>.meta.json` with one JSON object: both IDs, games,
  dilution fields, open-plies, filter, branch-tries, seed, board, feature version. Add
  `data/*.meta.json` to .gitignore next to `data/*.jsonl`.

Small provenance fix in src/ml_train.cpp (~line 267): in the self-play branch, when
`genRandomDecayPlies > 0`, append `dil(<start>-><floor>/<plies>p)` to the `teach` string.
Closes the remaining half of the `[Now]` provenance-gap todo item.

## Stage 2: branch-from-win mode (`--branch-tries T`)

Implements the developer's "find a line that beats the champion, then branch from it" idea
inside pairgen (no separate binary):

- During each base game, additionally snapshot the board (`char[SIZE][SIZE]` memcpy) every
  half-move.
- If the base game qualifies (passes `--filter`, e.g. agent A beat the champion), make T
  branch attempts: pick a uniform random half-move index t where A was to move (not the
  final move), restore the snapshot at t (memcpy back + recount `g_whiteCount`/
  `g_blackCount`/`g_chipDiff`/`g_whiteAtEnd`/`g_blackAtEnd` in a small
  `restoreBoardSnapshot` helper mirroring `reloadBoard`'s count init), pick a different
  legal move for A (identify the originally played move by comparing a simulated result to
  the snapshot at t+1, redraw if equal), then play out clean (no dilution) A vs B from
  there.
- Keep the branch game's positions only if A wins again. Per-branch seed = FNV(base seed,
  branch index). Tally branches kept/discarded in the summary and sidecar.

This is the most complex piece. It is severable: if it fights back, ship Stages 1 and 3
first and add the branch arm after.

## Stage 3: study script `tools/train_vs_champion.ps1`

Modeled on tools/train_scaling.ps1 (resumable CSV keyed by arm+seed, RunCell = train ->
hash -> gauntlet -> append). Not a sweep_pst_v2 group: no roster mutation during screening,
small matrix, per-cell gauntlets.

Constants:
- `$Champion = "ab(d6,ord,nb200k)@1.classic(t1,c4,w0,l0)@2"` (pinned, Elo 1077).
- `$Oracle = "ab(d8,tt,ord,nb2m)@1.classic(t1,c4,w0,l0)@2"` (deeper + 10x node budget;
  allowed as oracle. Verify the ID parses with `rank.exe check` before the run; adjust
  budget token to the grammar if needed).
- Best-PST generator: `ab(d2,tt,ord)@1.learned(s2,<hash>)@1` (hash scraped from
  `rank.exe check`).
- Dilution recipe where used: start 0.3, floor 0.05, decay 30 plies (sweep-validated).
- Slots 81..96 cycle for training cells, 97..99 reserved for promoted models (clear of
  sweep 3..80 and scaling 100..125). Screening wrapper `ab(d4,tt,ord,nb200k)@1`, d6 confirm
  wrapper `ab(d6,tt,ord,nb200k)@1`. Seeds `1001,2002,3003` (3 for headline arms, 2 for
  secondary). CSV `models/sweep/vs_champ.csv`: arm, source-file, games, seed, slot, hash,
  elo, pm, id.

Generation phase (sharded via -Workers, files merged by concatenation, which
`loadReplayDataset` handles):

| Dataset | Recipe |
|---|---|
| `pg_pstd2_champ_4k` | best-PST d2 (dil a) vs champion, 4000 games |
| `pg_classicd2_champ_4k` | classic d2 teacher (dil a) vs champion, 4000 games |
| `pg_champdil_champ_4k` | champion copy (dil a) vs clean champion, 4000 games |
| `pg_oracle_champ_2k` | oracle vs champion, clean both, `--open-plies 6`, 2000 games |
| `pg_champloss_*` | reuse `pg_champdil` generation with `--filter winner=a` until ~1200 kept champion-loss games (top up games count as needed) |
| `pg_branch_*` | champion-loss bases + `--branch-tries 4`, clean playouts, keep re-wins |
| `replay_4k` | `rank.exe extract --sample 4000 --seed 777` (fresh re-anchor of the baseline) |
| `selfplay control` | `train.exe selfplay-supervised` classic d2 diluted, 4000 games (no champion involvement, the sweep recipe) |

Training arms (all `train.exe selfplay-supervised --from-data <file> --feature-version 2
--epochs 6 --lr 0.05 --seed <s> --out models/sweep/slot<N>`, except the self-play control
which generates internally):

| Arm | Data | Seeds | Tests |
|---|---|---|---|
| replay-4k | replay_4k | 3 | baseline re-anchor |
| selfplay-4k | control, no champion | 2 | control |
| pstd2-vs-champ | pg_pstd2_champ_4k | 3 | Theory 1 headline |
| classicd2-vs-champ | pg_classicd2_champ_4k | 2 | recipe comparison |
| champdil-vs-champ | pg_champdil_champ_4k | 3 | Theory 2 (dilution arm) |
| oracle-vs-champ | pg_oracle_champ_2k | 3 | Theory 2 (oracle arm), "mimic an oracle beating the champ" |
| champloss-only | pg_champloss | 2 | cherry-picked counter-data (all games are champion defeats; note the label-skew caveat in results) |
| branch-wins | pg_branch | 2 | Theory 2 (mined winning lines) |
| mix-50-50 | 2k pstd2-vs-champ + 2k replay concatenated | 2 | does champion data add to the best general recipe |
| bootstrap | best vschamp model becomes generator at d2, regenerate 4k vs champion, retrain | 1 | gated: run only if best vschamp screen is within ~50 Elo of replay-4k |

After screening: d6-confirm the best cell overall AND the best cell of each of the three
theory-critical families (dilution-sourced, oracle-sourced, branch-sourced) even if weaker,
since the theory verdicts need their full-pool records.

Promotion (per the roster philosophy below): copy the promoted models to slots 97-99 /
stable names (`models/vschamp_best.txt` etc.) so slot cycling never invalidates a rostered
hash, append `on <d6 wrapper>.learned(s<slot>,<hash>)@1` lines idempotently, also roster the
oracle itself (`on $Oracle`, flagged in notes as oracle, not the goal), then one
`.\tools\run_rank.ps1 -Workers 8 --games 8` full run.

## Stage 4: analysis + theory verdicts

Small PowerShell analysis block at the end of the study script (parses
`ranking/games.tsv` + `ranking/ratings.tsv`, no new C++):

1. Bucket each promoted agent's opponents: `champion` (exact ID), `classic-like` (any
   `.classic(`/`.exp(` including diluted ladder), `diverse` (learned/linpol/greedy/policy/
   rand/tiered/smart heads).
2. Per bucket: actual score, Elo-expected score (logistic on ratings), residual = actual -
   expected. Compare each vs-champ-family agent against the replay-4k baseline agent.
3. **Theory 1 holds if** the vs-champ model's residuals in champion + classic-like buckets
   are >= the baseline's (within noise) AND its diverse-bucket residual is lower by a clear
   margin (>= ~0.08 expected-score per game, ~55 Elo, outside the seed-noise band).
   Refuted if the profile is flat relative to baseline or uniformly worse.
4. **Theory 2 verdict:** compare head-to-head-vs-champion scores of the dilution-sourced
   arm vs the oracle-sourced and branch-sourced arms. Theory 2 holds if the dilution arm
   shows no head-to-head edge on the champion while oracle/branch arms show a measurably
   better one. Also report each arm's raw record vs the champion next to its overall Elo.
5. Longitudinal half of Theory 1: standing note in todo.md + results doc to re-run the
   bucket split after the next batch of diverse agents joins the pool (the promoted agents
   stay `on` so data accrues free).

## Roster curation philosophy (record in todo.md + results doc)

Interim policy, per the developer's request for a durable strategy:
- Keep `on`: the anchor, the dilution ladder, the reigning champion family, one oracle,
  the best agent per distinct data-source family (replay, self-play, vs-champion,
  oracle-mimic, branch-mined), and any agent with a distinctive bucket profile (e.g. a
  counter-agent that beats the champ but loses broadly). Retire (`off`) near-duplicates
  whose head-to-head profiles match an existing agent, since their games remain in
  `matches.jsonl` forever.
- Add to todo.md as `[Later]`: **agent behavioral classifier** — characterize agents by
  how they play, not just Elo: position-distribution overlap between agents (shared
  `positionKey` histograms from stored games, so two agents reaching the same positions
  99% of the time rate as similar), a left-right symmetry measure, responses against a
  fixed discriminator agent as a feature vector, and k-means-style clustering to pick
  which agents are interesting enough to keep active.

## todo.md edits

- Strike "Pool-pair game generation" `[Next]` (shipped as `rank.exe pairgen`, annotate).
- Strike the provenance-gap `[Now]` item (decay params now in teacher=, from-data already
  recorded, pairgen recipe in the `.meta.json` sidecar).
- New Training Regimes line for the vs-champion regime + the longitudinal re-check note.
- New `[Later]` agent-classifier item (above).
- Opener ideas under "Add variety in openers and moves":
  1. Elo-rate the existing openers as ID modules: optional `op(o|d)@1` ID segment, roster
     the champion build under each opener, let the BT fit price openers directly `[Next]`
  2. Mine `matches.jsonl` for an opening book: tabulate first 8-10 plies of >= 900 Elo
     games by `positionKey` with win rates, emit a book file, add a book-follower opener
     `[Next]`
  3. ~~Random-first-K-plies opening diversity for data generation~~ (ships with pairgen
     `--open-plies`); extend to tournament/self-play generation `[Next]`
  4. Learned opener: policy head trained only on plies < 10 of high-Elo replay games, used
     as an opener module that hands off to the main brain `[Later]`
  5. Offline refutation book against the champion: deep budgeted searches (d8-d10, nb2m)
     on the champion's preferred opening lines (deterministic, so minable from games.tsv),
     best replies keyed by `positionKey`; a book+search d6 agent then attempts the dethrone
     with LESS live computation by construction `[Next]` — flag to the developer as the
     most promising follow-up for the actual dethroning goal

## Tests (tests/test_ranking.cpp)

- `rankDilutedProb`: values at ply 0, midpoint, decayPlies, beyond, and decayPlies=0.
- pairgen determinism: two identical tiny runs (2 games, d1/d2 classic agents, out under
  `build/`) produce byte-identical files with > 0 rows.
- Fixed side untouched: `--dil-apply none` output byte-identical to an override present
  with start=floor=0; caller-visible RankAgent specs unmodified after the call.
- Output format: emitted lines contain `"ver":2`, `"stm":`, `"label":`, `"idx":[`.
- Branch mode (if Stage 2 lands): snapshot restore round-trip (restore then recount equals
  original counts), and a branch run is deterministic across two invocations.

## Docs

- README.md: `pairgen` one-liner + example in the rank.exe section.
- CLAUDE.md: ranking.cpp/rank_main.cpp row updates, new `tools/train_vs_champion.ps1` row,
  headline result appended after the study.
- ML.md: pairgen prose next to the extract description (manual edit, AUTODOC unaffected).
- todo.md per above.

## Verification

1. `cmd /c .\build_rank.bat`, `cmd /c .\build_train.bat`, `.\tools\run_tests.ps1 -Build`
   (all existing assertions + new ones).
2. Smoke pairgen: 4 games best-PST-d2 vs champion with dilution, eyeball JSONL + sidecar,
   run twice, confirm identical. Repeat once with `--filter winner=a` and once with
   `--open-plies 6` on a deterministic pair (confirm games differ).
3. Smoke consume: `train.exe selfplay-supervised --from-data <smoke file> --feature-version
   2 --epochs 2 --out models/sweep/slot81`, confirm `teacher=replay:<file>` in the model.
4. Study dry run: `-DryRun` (tiny arm sizes, 1 seed, 2-game gauntlets), kill and re-run
   mid-way once to validate CSV resume.
5. Full run (generation ~1-2 h CPU sharded, ~23 gauntlet screens at 10-20 min each =
   roughly one long overnight or two nights, resumable), then d6 confirms, promotion, full
   rank run, analysis block.

## Archiving

- Plan: `plans/vs-champion-training-plan-1-<suffix>.md`.
- Results: `plans/vs-champion-training-results-1-<suffix>.md`. Must contain: both theory
  verdicts per the Stage 4 criteria, per-arm screening/confirm Elos with seeds, each arm's
  head-to-head vs the champion next to overall Elo, generation label-skew and
  branch-keep-rate tallies, the roster-curation policy adopted, and the longitudinal
  re-check note.
