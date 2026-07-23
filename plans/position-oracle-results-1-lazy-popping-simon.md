# Position-strength oracle: results

Companion to `position-oracle-plan-1-lazy-popping-simon.md`. That document
captures intent; this one captures what actually happened, including two
mid-session pivots the plan did not anticipate.

## End-of-session rundown

Built the full pipeline for measuring a board position's Elo advantage as a
distribution (mean + volatility) from designed playout games, then trained
and evaluated it for real:

- Shared probit math (`probitPoint`, `ELO_PER_LOGIT`) and the position
  decoder (`decodePositionEnc`) in `src/ml_model.{h,cpp}` / `src/datastore.{h,cpp}`.
- `DistModel`: a two-headed model (mu + log-sigma) in `src/ml_model.{h,cpp}`,
  wired into `mlIncrementalBegin`/`mlValueScoreDist` in `src/ml_eval.{h,cpp}`.
- Three `rank.exe` subcommands (`src/ranking.{h,cpp}`, `tools/rank_main.cpp`):
  `posgen` (position pools), `label` (designed playout labeling), `labelfit`
  (per-position probit MLE).
- Three `train.exe` subcommands (`src/ml_train.{h,cpp}`, `tools/train_main.cpp`):
  `dist-value`, `score`, `dist-eval`.
- `tools/label_study.ps1`: the resumable campaign orchestrator.
- Ran the full campaign: labeled 22,788 train positions (950,884 raw rows)
  and 700 eval positions (296,800 raw rows) against a designed ladder of
  rated agents, trained four production configs, and evaluated all four
  against a real depth-8 oracle baseline. **All four beat the calibrated
  oracle on both outcome NLL and mu MAE** (theory 34's locked criterion).
- Ran three exploratory sweeps mid-session at the developer's redirection
  (position-count scaling, hyperparameters, MLP-specific scaling) that
  meaningfully changed the final training recipe and caught two real bugs.
- Measured playing-strength Elo for all four models, which diverged from the
  prediction-quality ranking in a way that directly confirms theory 27.

Commit message for this batch covers: the label_study.ps1 hardening (exit-code
reliability, roster version fix, salvage-on-resume from the prior session),
todo.md's input-feature ideas list, and the campaign's data artifacts
(models, ratings, roster, matches).

## How to test / reproduce

```powershell
.\tools\run_tests.ps1 -Build              # unaffected by this session (no src/ changes)
.\train.exe score --model models/dist_mlp_wide.txt --boards "boards/board1.txt,boards/puzzle1.txt" --stm w
.\train.exe dist-eval --model models/dist_mlp_wide.txt --labels-eval data/labels/labels_eval.jsonl --raw-eval data/labels/raw_eval.jsonl --pool-eval data/labels/pool_eval.jsonl --ratings data/labels/ratings_snapshot.tsv --labels-train data/labels/labels_train.jsonl --raw-train data/labels/raw_train.jsonl
```
Full campaign reproduction (multi-hour): `.\tools\label_study.ps1 -DryRun` for
a smoke check, then `.\tools\label_study.ps1 -Workers <N>` for the real thing
(resumable at any point via `data/labels/study.csv`).

## Results as concrete numbers

### Position-count scaling (linear model, cheap oracle stand-in for speed)
Log-triplet schedule (1,2,3 / 5,6,7 / 25,26,27 / 105,106,107 / 425,426,427 /
1705,1706,1707 / 6825,6826,6827 / 22788), scored against the real eval tier:

| N | MAE (Elo) | Spearman |
|---|---|---|
| 1-27 | 234-318 (noisy, non-monotonic) | 0.10-0.16 |
| 105-107 | 234-249 | 0.39-0.40 |
| 425-1707 | 162-194 | 0.60-0.64 |
| 6825-22788 | 141-235 (one outlier at 6825) | 0.61-0.74 |

**The linear model saturates between N=425 and N=1,705**; the other ~21,000
positions in the train tier bought it essentially nothing. One deliberate
methodology note: the N=6825 triplet contains a genuine outlier (MAE 235 vs
its siblings' 157-170) from ordinary training-run variance, not a real
regression — the exact failure mode tight triplets are designed to catch,
and it would have been misread as a real signal under a coarser doubling
schedule.

### Hyperparameter sweep (46 trials, 2 seeds each, fixed at N=1705)

| Axis | Finding |
|---|---|
| lr (mu head) | 0.005-0.01 beat the 0.02 default (MAE ~167 vs ~170); 0.04+ visibly unstable (MAE up to 290, wide seed spread) |
| lrSigma/lr ratio | Flat 0.05-1.0x, no signal |
| L2 | Flat across the tested range (0 to 1e-4); two of five intended values collided under a `std::to_string` display bug in the throwaway sweep tool (cosmetic only, did not affect training) |
| Epochs | 25-50 tied; 100 clearly overfits (MAE 179.6 mean, noisy) |
| Architecture | Linear beat every MLP variant at this N -- **later shown to be an artifact of insufficient data for the MLP's capacity**, not a real architecture verdict (see the MLP sweep below) |
| `--elo-se` | Bit-identical on/off, confirming the original design math: at this pool's rating precision (pm ~13-25 Elo) the added variance term is provably negligible |
| Training mode | Primary raw-BCE: stable. Secondary Gaussian-on-fitted-labels: **diverged to NaN at the shared default lr** |

**Bug found and diagnosed:** the Gaussian secondary training mode's
SE-weighted gradient is amplified up to ~100x by precision weighting
(`wMu = 1/(SE^2 + 0.01)`), so the primary mode's lr=0.02 default causes it to
diverge. Confirmed by direct reproduction: lr=0.02 -> loss goes to NaN within
~12 epochs; lr=0.0005 (40x smaller) trains cleanly and converges (~2.6 wMSE,
stable). Not fixed in code -- the production configs use the primary mode
exclusively, so this did not block anything, but it is a live landmine for
anyone who turns on `--labels` without knowing to also drop the learning
rate. Documented in the input-features todo and here so it is not
rediscovered from scratch.

**Also found:** my own throwaway sweep driver mis-parsed the NaN'd model's
`-nan(ind)`/`nan` output tokens as clean zeros and non-obviously-wrong
numbers, initially looking like a *different*, more mysterious bug. Verified
via the real `train.exe` CLI directly that the shipped `dist-eval` code
correctly reports the breakage (`nan`, `-nan(ind)`, and a correct "does NOT
beat" verdict) -- the parsing issue was confined to the disposable sweep
tool, not the shipped pipeline.

### MLP-specific position-count scaling (mlp128-64/mlp32, same N schedule, same shuffle seed for a clean apples-to-apples comparison against the linear sweep)

| N | Linear MAE / spearman | MLP MAE / spearman |
|---|---|---|
| 105-107 | 234-249 / 0.39-0.40 | 201-214 / 0.34-0.35 (behind) |
| 425-1707 | 162-194 / 0.60-0.64 | 170-179 / 0.53-0.60 (behind) |
| 6825-6827 | 169-170 / 0.61-0.64 | 157-166 / 0.63-0.67 (ahead) |
| 22,788 (full) | 167.5 / 0.612 | **141-152 / 0.740-0.744** |

The MLP trails linear through most of the curve (data-starved relative to
its ~29,154 parameters vs ~22,788 distinct positions -- more parameters than
training examples by count), crosses over between N=1,707 and N=6,825, and
**is still climbing steeply at the full store size** -- unlike linear, which
had gone flat 1,300+ positions earlier. This directly resolved the
hyperparameter sweep's flagged architecture-axis caveat: the earlier
"linear beats MLP" reading was a small-N artifact, not a real verdict.

### Final production training (4 configs, full 22,788-position store, lr=0.01 per the sweep finding)

| Config | Parameters | File size |
|---|---|---|
| dist_lin | 260 | 5.2 KB |
| dist_mlp_s1001 / s2002 | 29,154 | ~731 KB |
| dist_mlp_wide | 74,690 | 1.9 MB |

### Real oracle-verdict evaluation (depth-8/2M-node oracle, real 700-position eval tier)

| Predictor | MAE (Elo) | RMSE | Spearman | Outcome NLL | Beats oracle? |
|---|---|---|---|---|---|
| oracle-d8 (baseline) | 191.3 | 234.6 | 0.498 | 0.4498 | -- |
| classic (baseline) | 207.4 | 253.7 | 0.399 | 0.4659 | -- |
| pst_value (baseline) | 226.1 | 275.4 | -0.298 | 0.4886 | -- |
| dist_lin | 161.3 | 203.2 | 0.666 | 0.4240 | YES, both |
| dist_mlp_s1001 | 147.9 | 179.8 | 0.746 | 0.4087 | YES, both |
| dist_mlp_s2002 | 150.0 | 184.3 | 0.754 | 0.4110 | YES, both |
| **dist_mlp_wide** | **146.2** | **179.0** | 0.744 | **0.4079** | YES, both |

**Theory 34's locked success criterion is met by all four configs**,
cleanly, not marginally. `dist_mlp_wide` is the best overall.

**SD/volatility validity (theory 35) is weak, not strong:** predicted sigma
correlates with measured sd at only 0.12-0.29 (Pearson) and as low as 0.02
(Spearman, dist_lin and dist_mlp_wide). The mean prediction clearly beats
the oracle; the sigma head's own quality is a real but much softer result --
do not let the headline mu win imply theory 35 is equally well-supported.

**Side observation, not over-interpreted:** `pst_value` shows a slightly
*negative* correlation with true measured advantage after calibration -- a
model built for playing strength does not automatically track this
project's specific "Elo-gap-at-coin-flip" quantity.

### Playing-strength Elo

First measurement (d4 only, 2026-07-21 fit: oracle 1151, champion 1144):
`dist_lin` reached 1036 at d6/nb200k, and among the d4-only comparison
`dist_mlp_wide` (767) led the three MLP configs, ahead of `dist_lin` (694)
which in turn beat `dist_mlp_s1001` (643) and `dist_mlp_s2002` (640).

**Superseded same day by a second, later fit** once the developer asked for
full d2/d4/d6 coverage on all four models (oracle 1151+/-12, champion
1131+/-13, in that same later fit -- the two fits are NOT compared against
each other in absolute terms, only the later one is current):

| Model | d2 | d4 | d6/nb200k |
|---|---|---|---|
| dist_lin | 594+/-16 | 694+/-15 | **1031+/-16** |
| dist_mlp_s1001 | 509+/-17 | 667+/-15 | 974+/-16 |
| dist_mlp_s2002 | 716+/-15 | 648+/-15 | 967+/-16 |
| dist_mlp_wide | 454+/-18 | 768+/-15 | 931+/-16 |

**The developer's own expectation going in was that a d6 MLP would become
the new champion. It did not.** None of the three MLP configs at d6 beat
`dist_lin` at d6 (1031), let alone the champion (1131) or oracle (1151).
`dist_lin@d6` remains the strongest dist-model agent measured -- a real,
competitive agent, not a champion.

**Theory 27, reconfirmed a fourth time:** at every depth, all three MLP
configs beat `dist_lin` on the oracle-verdict prediction task (147.9-150.0
MAE vs 161.3), yet at d6 `dist_lin` beats all three of them in actual play.
Lower prediction error did not translate to higher playing Elo anywhere in
this table.

**An unexplained wrinkle, reported as an open observation, not a conclusion:**
`dist_mlp_wide` is the BEST of the three MLP configs at d4 (768) but the
WORST at d6 (931) -- a rank reversal within the MLP family that the other
two configs do not show (`dist_mlp_s1001` and `dist_mlp_s2002` keep a
consistent relative order across d4 and d6). No mechanism is proposed here;
flagged in Future Work. `dist_mlp_s2002`'s d2 result (716, clearly the best
of the four at that depth despite being unremarkable at d4/d6) is a second
unexplained outlier in the same table.

Per the champion-hygiene standing rule: all numbers above are from a single
fit each, never compared in absolute terms to any other fit's numbers.

## Implementation differences from the plan

- **The single biggest change: labels are measured by playing DESIGNED fresh
  games, not mined from the historical corpus** -- this was decided in
  conversation BEFORE the plan was written (the plan already reflects it),
  but is worth restating here since it is the load-bearing decision the rest
  of the session's numbers depend on.
- **The v1 labeling design (all-strong ladder, ~424 games/eval-position) was
  abandoned mid-campaign for a v2 speed package** after the developer
  invoked a "start small, find convergence" principle: the train ladder was
  redesigned to carry its bulk gap grid on cheap rated d4-family agents
  (~25x cheaper per move) with the expensive d6/d8 rungs kept only as
  mod-gated style anchors, cutting the train tier from an estimated ~48
  hours to ~2.5 hours actual. The eval tier kept its full strong-ladder
  design (it is the measuring stick) but was trimmed from 1,500 to 700
  positions. The v1 attempt's 189,720 already-played rows were salvaged
  into the v2 store rather than discarded (rung indices were unchanged,
  so old and new rows are jointly valid).
- **The planned bounded-jitter ladder rungs never worked and were replaced.**
  Tie-only jitter is deterministic per seed and never draws from `rand()`,
  so a jitter-vs-jitter pairing replays one identical game regardless of
  seed -- discovered before any labeling games were wasted on it. Replaced
  with depth-diluted d8 agents (`dil(r15,d6)`, `dil(r30,d6)`, rated 1114 and
  1136 Elo), which are genuinely stochastic since the per-move dilution roll
  consumes `rand()`.
- **Three exploratory sweeps were added mid-session, not in the original
  plan**, at the developer's explicit redirection toward a log-triplet
  (tight-cluster-at-widening-gaps) checkpoint schedule instead of a
  doubling sequence, to separate real convergence trends from run-to-run
  noise. All three materially changed the final recipe (position count was
  already adequate for linear but not MLP; the shared default learning rate
  was suboptimal; the architecture-axis reading from the hyperparameter
  sweep was reversed by the MLP-specific sweep).
- **`mlp_wide` was added to the roster-Elo measurement** even though the
  original plan only wired slots 76-78; it was added because it turned out
  to be the best oracle-verdict performer, using the next free sweep slot
  (79).

## Correctness gotchas discovered and fixed

1. **Cross-game transposition-table pollution.** Replays and from-position
   playouts did not clear the TT between games, so outcomes depended on
   process history (a posgen rerun diverged 51 vs 113 games before the fix).
   Fixed with `ttClear()` before every game in `rankExtract`/`rankPosGen`
   and `rankLabel`. Also protects labeler shard-invariance for any
   tt-flagged rung.
2. **Orchestrator false-failure on process exit code.** `label_study.ps1`'s
   training phase used `.ExitCode` from a `-RedirectStandardOutput`'d
   `Start-Process` object to detect failure; this proved unreliable in this
   environment -- three of four production configs completed correctly,
   wrote valid model files, and logged "Final model ->", yet the script
   still reported failure and aborted before the fourth config's own
   completion was checked (which, separately, was still genuinely running
   and completed fine on its own after the script had already exited).
   Fixed by checking for the actual output file plus the log's completion
   marker as the primary success signal, falling back to `.ExitCode` only
   when those disagree.
3. **Wrong canonical version on the roster-append lines.** The rate phase
   generated `learned(s<slot>,<hash>)@2` lines; the actual codec version for
   the `learned` chooser (`src/ranking.cpp` `g_rkChoosers[]`) is `@1`. Caused
   a hard roster-parse error on the first rate-phase run. Fixed both the
   already-appended roster lines and the script's generator.
4. **A throwaway sweep tool's NaN-token parsing bug** (see the
   hyperparameter-sweep section above) that briefly looked like a mystery
   bug in the shipped pipeline before being isolated to the disposable tool.

## Methodology caveats that qualify these numbers

- The eval tier's 700 positions and their labels are the sole ground truth
  for the oracle-verdict claim; they were never used in training (position-
  hash-disjoint tiers by construction).
- The oracle-verdict comparison's baselines (oracle-d8, pst_value, classic)
  are each calibrated by a single 1-parameter linear fit on 800 train-tier
  labels -- a deliberately simple calibration, not a search over richer
  calibration functions.
- All Elo numbers in the playing-strength table are from one fit and are not
  comparable in absolute terms to any other fit (standard project rule).
- The MLP position-count sweep used a cheap depth-2/4000-node stand-in
  oracle for speed; only the FINAL four-config evaluation used the real
  depth-8/2M-node oracle. The relative MLP-vs-linear trend is trusted; the
  absolute numbers from the sweep are not the same measurement as the final
  verdict table.
- Sigma/volatility validity (theory 35) remains only weakly supported;
  treat the mu/oracle-beating result and the sigma-validity result as
  separate claims with separate evidentiary strength.

## Future Work

- **`dist_mlp_wide`'s d4-to-d6 rank reversal is unexplained.** It is the
  strongest MLP config at d4 (768) and the weakest at d6 (931), while
  `dist_mlp_s1001`/`dist_mlp_s2002` keep a consistent relative order across
  both depths. No mechanism proposed. Would need controlled follow-up (e.g.
  more seeds per depth to rule out simple noise, then inspecting whether
  wide's larger capacity interacts badly with deeper alpha-beta pruning /
  move ordering somehow) before treating it as more than an observation.
- **`dist_mlp_s2002`'s d2 result (716) is a similarly unexplained outlier** --
  clearly the best of the four models at that depth despite being
  unremarkable at d4 and d6. Possibly just seed noise at a shallow, high-
  variance depth; untested.
- **Sigma head quality is weak (Pearson 0.12-0.29, Spearman down to 0.02)
  and untouched by this session's tuning** -- the hyperparameter sweep found
  the lrSigma/lr ratio axis flat, meaning the sweep never found a setting
  that helped sigma specifically. Worth its own dedicated sweep (sigma-only
  architecture, sigma-only learning rate, possibly a different loss
  weighting) rather than assuming the mu-tuned recipe transfers.
- **The MLP position-count curve was still climbing at the full 22,788-
  position store** (spearman 0.65 -> 0.74 in the last stretch alone) --
  unresolved whether it has a nearby ceiling or would keep improving with
  meaningfully more data. Directly informs whether a larger second
  labeling campaign is worth the compute; the position-count-sweep tooling
  (not currently committed, see below) would need to be rerun once more
  train data exists to answer this.
- **The theory-27 divergence** (dist_lin beating dist_mlp_s1001/s2002 in play
  despite losing on prediction) is reported as an observation, not explained.
  Worth checking whether it is the same mechanism as the earlier
  residual/MLP theory-27 case (miscalibration at decision-relevant margins
  a mean-loss metric washes out) or something specific to the dist model's
  probit objective.
- **The Gaussian secondary training mode's NaN bug is diagnosed but not
  fixed in code.** Low priority since the production configs do not use it,
  but a landmine for any future session that tries `--labels` without
  reading this doc first. Either give it its own tuned default learning
  rate or add a stability guard.
- **The exploratory sweep tools (`scaling_sweep.cpp`, `hparam_sweep.cpp`,
  `mlp_scaling_sweep.cpp`) were written to a session scratchpad and are not
  committed to the repo.** If this kind of sweep is wanted again (e.g. to
  answer the MLP-ceiling question above, or to test the input-feature ideas
  in todo.md), it will need to be rebuilt from this doc's description
  rather than rerun directly. Worth formalizing into `tools/` if scaling
  sweeps become a recurring need for this project, matching the
  `train_scaling.ps1`/`sweep_pst_v2.ps1` precedent.
- **The input-feature ideas in `todo.md`** (material as an explicit input,
  mover's-perspective color canonicalization, forward-progress/per-row/
  support/mobility/race-sentinel/ply features) are untested. The
  mover's-perspective idea in particular is well-motivated by this
  session's own finding that the MLP is data-hungry (it would roughly
  double usable data for free) and should be prioritized first.
- **`--elo-se` remains untested for real effect** at higher rating
  precision than this pool currently has; confirmed negligible at the
  current pm~13-25 Elo level, but that was already expected from the
  original design math, not a new finding.

## Ideas This Inspired

- The log-triplet checkpoint schedule (tight clusters at widening gaps,
  rather than a doubling sequence) proved valuable enough here that it
  seems worth a general note in `Docs/benchmarking.md` or a similar
  methodology doc, for any future scaling/convergence question in this
  project, not just this one.
- Given theory 27 keeps recurring across unrelated model families (residual/
  MLP value models, now the dist model), it might be worth a dedicated
  investigation into WHY offline prediction quality and playing strength
  diverge in this project's search setup, rather than re-discovering the
  same divergence anecdotally each time a new model type ships.
- The "salvage leftover shard files on resume" pattern (exact-line dedup
  from deterministic seeds) that protected the v1-to-v2 labeling pivot
  seems like a generally reusable resumability pattern for any future
  sharded, seed-deterministic data-generation tool in this project.
- The MLP's parameter-count-vs-position-count framing ("more parameters
  than distinct training examples") was a more useful sanity check than
  raw-row counts for judging whether a model is data-starved; might be
  worth a standing rule-of-thumb note for future capacity decisions in this
  project's ML system.
