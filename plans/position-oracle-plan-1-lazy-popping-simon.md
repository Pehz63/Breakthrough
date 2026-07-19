# Position-strength oracle: a distributional (mean + SD) position model trained on designed playout labels

## Context

The developer's idea: rank any board position by the Elo advantage it confers,
predicted as a distribution (mean + standard deviation) by a machine learning
model. Decisions locked in conversation: the SD represents volatility (how
unreliably the advantage converts), labels come from DESIGNED NEW DATA rather
than the historical corpus (the corpus has pseudo-replication and no gap
design), the d8/nb2m oracle agent itself joins the labeling ladder for premium
data, compute is not a constraint (days-scale generation and training are
fine), and the model should become an oracle that predicts position strength
better than the current d8/nb2m reference agent. Retraining later, when the
agent pool grows and ratings improve, is expected, so raw outcomes are stored
keyed by agent identity and can be re-fit under any future ratings snapshot.

## What this will do, in plain terms

**The target.** For a given position, the model estimates the Elo handicap at
which the game from here becomes a coin flip. A mean of +200 says White can be
200 Elo weaker than Black and still expect a 50% score. Flipped, it reads:
how much stronger White has to play than Black to be expected to tie. The SD
says how reliably that advantage converts: low SD = quiet position, the
favorite converts predictably. High SD = sharp position, upsets keep
happening regardless of the strength gap.

**How a position gets its label: measured, not assumed.** For each position we
play a batch of FRESH games from it between rated agents whose Elo gap we
control: even pairings, White much stronger, Black much stronger, at several
gap levels in both directions, with premium games from the d8/nb2m oracle
rung. Plot "did White win" against the gap and fit the standard Elo win curve:
the curve's horizontal slide is the position's mean advantage, its flatness is
the SD. Every position gets this hard, designed measurement. No reliance on
lucky repetition in old data, no better-agent-or-better-position ambiguity
(the gap is chosen by us and known exactly).

**What the model is for.** Measuring one position costs games. The model
(inputs: the existing 129 binary piece-square features, one per color and
square plus side to move, material left as a learned derivation per theory 24)
learns to reproduce the measurements so any position gets an instant estimate.
Trained on millions of raw playout outcomes, it amortizes the measurement the
way a value network amortizes search.

**Success criterion (locked).** On held-out positions with dense measured
labels, the model must beat the calibrated d8/nb2m oracle's own judgments
(its root search score mapped to Elo by a 1-parameter calibration) on both
outcome likelihood and mean-advantage error. That is the claim "stronger than
the current oracle at predicting position strength". Its Elo as a playing
evaluator is measured too (standing rule) but is a secondary metric, and
theory 27 warns it may not track prediction quality.

## The math (validated, gradients pre-derived, used in two places)

Internal units are logits. ELO_PER_LOGIT = 400/ln(10) = 173.7177928.
Per playout game from position P between players with Elo gap
d = (eloW - eloB)/ELO_PER_LOGIT:

```
mu, s = position's mean advantage (logits) and log-sigma, s clamped [-4, 3]
sigma2 = exp(2s)
kappa  = 1/sqrt(1 + (pi/8)*(sigma2 + v))   (v = optional rating-SE variance)
p      = sigmoid(kappa*(mu + d))
L      = BCE(p, y)                          (y = 1 White won, 0 Black won)
```

Gradients (hand-derived, verified, constant is pi/8 NOT pi^2/8): with
u = kappa*(mu+d): gMu = (p-y)*kappa, gS = -(p-y)*u*(pi/8)*sigma2*kappa^2,
gS = 0 when s is clamped. The same math serves (1) the per-position direct
fit, 2 parameters per position, its measured label, and (2) the amortized
model, whose mu and s heads are trained on raw rows by these gradients.

## Implementation stages

Study home: `data/labels/`. Model slots: verify 76..80 are free sweep slots
before claiming (grep ranking/roster.txt and models/sweep/), note the claim in
the slotFile() comment (src/ranking.cpp ~line 115).

### Stage A: shared math + position decoder (code only)
Files: src/ml_model.h/.cpp, src/datastore.h/.cpp
- `probitPoint(mu, s, d, v, y, ProbitGrad& out)` in ml_model: the loss +
  gradients above, double precision. Linked into every target.
- `decodePositionEnc(const string& enc, int& sideToMove)` in datastore, next
  to encodeBoard (datastore.cpp:6-16): ~15 lines, board[x][y] = enc[y*8+x],
  side from char 65, then rebuild g_whiteCount/g_blackCount/g_chipDiff/
  g_whiteAtEnd/g_blackAtEnd exactly like restoreBoardSnapshot
  (ranking.cpp:1996) and reloadBoard (board_io.cpp:53-92).
- `Model::gradStep(x, n, gOut, lr, l2)` virtual: apply an arbitrary upstream
  output-logit gradient. LinearModel: direct. MLPModel: refactor trainStep
  (ml_model.cpp:131) into computeForward + private backprop(gOut, lr, l2),
  trainStep becomes forward + backprop(p - target), byte-identical behavior
  (existing tests guard it).

### Stage B: strong stochastic ladder rungs (prep compute, hours)
The rated stochastic ladder covers Elo 0..910 (dilution family on the d6
head, all on-roster) but is deterministic above. Add 4 bounded-jitter
Advanced agents (tie-only jitter, theory 20, ~0-80 Elo cost), champion
weights, two seeds each at two budgets:
```
on ab(d6,tt,ord,nb200k)@1.adv(t1,c4,w0,l0,f0,d0,e0,m0,h0,b0,o0,r0,x0,n-8,s101,g0)@1
on ab(d6,tt,ord,nb200k)@1.adv(...same...,n-8,s202,g0)@1
on ab(d8,tt,ord,nb2m)@1.adv(...same...,n-8,s303,g0)@1
on ab(d8,tt,ord,nb2m)@1.adv(...same...,n-8,s404,g0)@1
```
(n < 0 selects the bounded tie-only jitter of magnitude 8, s is the seed.
Run rank.exe check first, it prints canonical fixes.) Append to roster,
`.\tools\run_rank.ps1 -Workers 12`, expect ~650 games each (pm ~16-18,
acceptable), jittered d6 ~1060-1140 and jittered d8 ~1100-1160. Then FREEZE
the study's rating basis: copy ranking/ratings.tsv to
data/labels/ratings_snapshot.tsv. All downstream gap math reads the snapshot.

### Stage C: position pool builder, `rank.exe posgen`
Files: src/ranking.cpp/.h, tools/rank_main.cpp (dispatch pattern :125-149)
```cpp
int rankPosGen(const string& storeFile, const string& board,
               const string& outTrain, const string& outEval,
               int targetTrain, int targetEval, int perGameCap,
               int minPly, int maxPly, unsigned seed);
```
Replays a deterministic sample of matches.jsonl (rankExtract's loop +
determinism-drift guard), emits DISTINCT positions as JSONL rows
`{"enc","h","ply","stm","md","seen"}` with: enc-set dedup (cross-game),
stratification by ply band {6-10, 11-16, 17-24, 25-34, 35-44} and |material
diff| bucket {0,1,2,>=3} caps, skip nearWinCheck hits, side-to-move from
replay parity, train/eval tiers disjoint by position hash (hash % 17 == 0
-> eval candidates). Defaults: perGameCap 4, minPly 6, maxPly 44. v1
targets: 24,000 train, 1,500 eval. Optional strong-play harvest: a small
top-agents-only roster generates fresh matches, second posgen pass merges.

### Stage D: playout labeler, `rank.exe label` (the multi-day engine)
Files: src/ranking.cpp/.h, tools/rank_main.cpp
- Ladder spec `data/labels/ladder.txt`, hand-editable: `rung <i> <canonical
  id>` lines (ids copied VERBATIM from ratings.tsv) + `pair <wi> <bi> <games>
  [mod <k> <r>]` lines (mod restricts a pairing to positions with
  hash%k==r, the d8 premium ration knob). Separate denser ladder_eval.txt
  for the eval tier.
- `rankLoadLadder(...)` parser + validation: every rung parses via
  rankAgentFromId, model slots load, REJECT rungs containing `.opener(`
  (opener ply counters assume ply-0 starts) and pairings where both rungs
  are deterministic (identical replays).
```cpp
int rankLabel(const string& poolFile, const string& ladderFile,
              const string& outFile, unsigned runSeed, int shard, int ofK,
              bool resume, const string& doneFile, int maxPositions);
```
- Per position (index % ofK == shard): decodePositionEnc, then per pairing
  per game: srand(gameSeed(wId, bId, pairingIdx*1000+g, runSeed ^
  low32(posHash))), playToConclusion(wa, ba, startHalf = stm==White ? 0 : 1)
  (ranking.cpp:2629, plays from current board, no openers). Append raw rows
  `{"h","wi","bi","g","seed","y","p"}` (~65 bytes each). Meta sidecar
  (pairgen's writer pattern, ranking.cpp:2447) freezes the ladder ID array
  (authoritative rung index -> agent ID mapping), design, pool, ratings
  snapshot path, tallies. 400-half-cap draws dropped and tallied.
- Sharding by POSITION index so each position's seed stream lives in one
  shard and any worker count reproduces identical games. `--resume --done`
  parses existing stores, counts rows per (h, wi, bi), tops up partials
  exactly (deterministic seeds). `--max-positions` chunks multi-day runs.

v1 experiment design:
- Train tier: 24k positions x 8 pairings x 8 games (gaps ~0, +-150, +-300,
  +-530, +190 via the rung table) + d8 premium pair 2x2 games on hash%4==0.
  ~1.63M games, ~0.35 wall-days on 16 workers.
- Eval tier: 1.5k positions x dense grid (24 pairings x 40 games, gaps
  +-{0..650} both color orders, d8 pairs 2x12 on ALL eval positions).
  ~1.48M games, ~0.35 wall-days. Label SE(mu) ~25-30 Elo per eval position.
- Cost basis (measured): d6-family game ~0.2 cpu-s, d8-vs-d6 ~3 cpu-s.

### Stage E: per-position direct fit + QC, `rank.exe labelfit`
Files: src/ranking.cpp/.h, tools/rank_main.cpp
```cpp
int rankFitMuSigma(const vector<double>& d, const vector<double>& v,
                   const vector<double>& y, double& mu, double& s,
                   double& seMu, double& seS);   // 2-param MLE + Fisher SEs
int rankLabelFit(const string& storeFile, const string& poolFile,
                 const string& ratingsFile, const string& outFile,
                 int minRows, bool useRatingSe);
```
Joins raw rows -> rung IDs (meta sidecar) -> Elo + pm (ratings snapshot,
reuse readRatingsTsv, ranking.cpp ~1830), fits (mu, sigma) per position,
emits labels JSONL `{"enc","h","mu_elo","sd_elo","se_mu","se_sd","n","nll",
"flags":"ok|allwin|allloss|clamped|thin"}` + QC diagnostics (per-pairing
empirical vs fitted win-rate table, flag counts). This is also the free
re-fit path after any future ratings refit: same raw rows, new ratings file.

### Stage F: the `dist` model type
Files: src/ml_model.h/.cpp, src/ml_eval.cpp
```cpp
struct DistModel : public Model {   // type=dist
    Model* muHead;  Model* sHead;   // owned, linear or mlp each, same features
    float forward(x,n) override;    // = muHead->forward (evaluator-compatible)
    void  forwardDist(x, n, float& muLogit, float& sigmaLogit) const;
    float trainStepRow(x, n, y, d, v, lr, l2);      // probitPoint + gradStep both heads
    float trainStepGauss(x, n, muLab, sdLab, wMu, wSd, lr, l2);  // secondary mode
};
```
Save format: type=dist + mu_type=/s_type= + inline weight blocks (mirror
ResidualModel's inner serialization). loadModel case reusing
buildLinearFromKV/buildMLPFromKV per head. g_modelTypes row (auto-docs).
Incremental search: in mlIncrementalBegin, unwrap DistModel to its muHead
before the existing ResidualModel unwrap (3 lines) so a linear mu head keeps
the fast g_mlAcc leaf. MLP mu head falls back to full scan (correct, fine at
node-budget heads).

### Stage G: training regime, `train.exe dist-value`
Files: src/ml_train.cpp/.h, tools/train_main.cpp
```cpp
int trainDistValue(outPath, rawStore, poolFile, ratingsFile, epochs, lr, l2,
                   seed, muType, muHidden, sType, sHidden, valSplit,
                   earlyStop, labelsFile = "");
```
- PRIMARY mode: raw rows. Exact generative likelihood, uses every game
  including all-win positions, no double-counted label noise, revalidates
  under future ratings. d and v recomputed at load from the ratings file +
  meta ladder IDs. Features built once per position (v2 extractor after
  decodePositionEnc), rows index into them. Secondary mode (labelsFile):
  SE-weighted Gaussian NLL on fitted labels.
- Split by POSITION hash (never by row). Per-epoch report: train/val BCE,
  mean predicted sigma in Elo, calibration by |d| bucket, and two nulls
  printed once: intercept-only (constant mu and sigma) and gap-only (mu=0).
- g_regimes row + CLI dispatch + usage.
- v1 configs (parallel processes, ~1.6M rows, 40-80 epochs, lr 0.02, l2
  1e-6, val 0.10, early stop): dist-lin (linear/linear, slot 76, baseline +
  incremental variant), dist-mlp seeds 1001/2002 (mlp 128,64 mu + mlp 32 s,
  slots 77/78, primary oracle candidates), dist-mlp-wide (256,128 + 64,
  slot 79, capacity probe).

### Stage H: evaluation vs the current oracle, `train.exe dist-eval`
Files: src/ml_train.cpp/.h, tools/train_main.cpp
```cpp
int distEval(modelPath, labelsEval, rawEval, poolEval, ratingsFile,
             labelsTrain, rawTrain);
```
Baselines, each mapped raw-score -> Elo by a 1-param scale fitted on TRAIN
labels only, each given one global sigma MLE-fit on train rows so outcome
NLL is defined: (a) d8/nb2m oracle root search score (build the AgentSpec
directly, agentChooseMove from the decoded position, read the mover's
g_downEvalWhite/Black, re-decode enc to undo the move), (b) pst_value slot
2, (c) Classic static eval. Metrics on eval tier: MAE + RMSE vs measured
mu_elo (flags==ok only), per-row outcome NLL under predicted (mu, sigma),
Spearman rank correlation, SD validity (correlation of predicted sigma vs
measured sd_elo + SD calibration by bucket). SUCCESS = beat the calibrated
d8 baseline on outcome NLL AND mu MAE.

### Stage I: ship surfaces
- `mlValueScoreDist(turnColor, slot, double& muElo, double& sdElo)` in
  ml_eval (nearWinCheck first, false unless a dist model is slotted).
- `train.exe score --model <path> --pos <encs-file | board.txt> [--stm w|b]`:
  prints positions ranked by mu with mean +- SD in Elo.
- Roster wiring (standing rule, secondary metric): slot files + roster lines
  `on ab(d6,tt,ord,nb200k)@1.learned(s76,<hash8>)@V` etc. for dist-lin and
  both mlp seeds, rank.exe check for hashes, run_rank refit, report pooled
  Elo within one fit.

### Stage J: orchestration, `tools/label_study.ps1`
Patterns: Start-Process shard loop + Wait-Process + exit-code check + merge
(run_tournament.ps1:50-73), CSV ledger with early-return on done cells +
phase banners (train_scaling.ps1), -DryRun (train_vs_champion.ps1).
Params: -Workers 12, -Phases prep,posgen,label-train,label-eval,fit,train,
eval,rate, -DryRun, -Seed, -ChunkPositions 2000, -Csv data/labels/study.csv.
label phases loop chunks: launch worker shards with --resume --done, merge,
ledger, so an interruption loses at most one chunk (~30-60 min). prep never
overwrites an existing ratings_snapshot.tsv (frozen basis). -DryRun runs
everything on a 20-position pool with a d2/d4 ladder in minutes.

### Compute budget (v1, 16 workers)
prep ~1 wall-h. posgen minutes. label train ~0.35 days. label eval ~0.35
days. labelfit minutes. train 4 configs ~2-6 h parallel. dist-eval ~4 min
(1500 d8 searches). roster Elo ~3-6 h. Total ~2 wall-days. Disk: ~220 MB raw
stores (gitignored), pools + labels + meta ~10 MB (committed).
Scale-up path: double train positions, then an adaptive second pass (pair
selection centered on -mu_hat per position from v1 labels), then new top
rungs as the pool improves followed by free labelfit refit + retrain.

## Ordered execution sequence
1. Stage A + tests, build gate. 2. Stage F + tests. 3. Stages C, D, E +
tests. 4. Stage J script, -DryRun end-to-end green. 5. Stage B prep compute,
freeze snapshot, write ladder files. 6. Data campaign (posgen, label x2,
fit). 7. Stage G training, Stage H evaluation, iterate if val BCE still
falling. 8. Stage I ship + roster Elo. 9. Docs + results write-up.

## Tests (gate: .\tools\run_tests.ps1 -Build before every commit)
probitPoint vs finite differences (clamped and not). gradStep == trainStep
(linear, mlp). decodePositionEnc roundtrip vs positionKey along random games
with counter verification. rankFitMuSigma synthetic recovery over a
(mu, sigma) grid + determinism. Ladder parse good/bad + opener and
deterministic-pair rejection. Labeler determinism, shard-split invariance,
resume top-up (d2/d4 micro ladder). Store row + meta parse roundtrips.
dist(linear) and dist(mlp) save/load roundtrip + trainStepRow finite-diff
through both heads + clamped-s zero gradient. Incremental equivalence for
dist with linear mu head. Deterministic labelfit.

## Verification before the big run
label_study.ps1 -DryRun green end to end. Sanity anchors: standard start
labels ~ +30-60 Elo for White (cross-check turn-swing), a +2-material
midgame position strongly positive, labelfit per-pairing QC table shows no
systematic misfit. Determinism spot check: rerun one chunk to scratch, diff.

## Risks and mitigations
- Ratings-scale drift across refits: raw rows keyed by rung IDs frozen in
  meta, gaps recomputed at load, study trains against the frozen snapshot,
  future refits rerun labelfit + retrain on the same raw store (documented
  retraining path).
- Ladder style bias: labels = advantage under this pool's play. Mixed rung
  families (random-dilution, depth-dilution, tie-jitter), stated as scope.
- Sigma floor from dilution noise: every gap level uses the strongest pair
  achieving it, near-zero and premium gaps use tie-jitter rungs
  (deterministic off-tie), labelfit's per-pairing residual table detects
  rung-dependent inflation.
- d8 determinism: premium rungs are the NEW jittered adv agents, labeler
  rejects deterministic-vs-deterministic pairings.
- Leakage: train/eval tiers split by position hash at posgen, training val
  split also by position hash, enc dedup.
- MLE degeneracies: allwin/allloss positions stay in raw training (BCE
  handles them), flagged labels excluded from mu-error metrics, retained in
  outcome-NLL.
- Theory-27 divergence: the evaluator Elo may lag prediction quality. Ship
  the measurement either way, the success criterion is prediction.

## Docs and process
Archive as plans/position-oracle-plan-1-lazy-popping-simon.md + companion
results doc (same name, results). ML.md: hand-written pipeline section
(posgen -> label -> labelfit -> dist-value -> dist-eval, formats, the pi/8
math) + train.exe docs regeneration. Docs/theories.md: two new entries at
the next free numbers (verify): the oracle-prediction theory and the sigma
volatility-identification theory, with the theory-27 caution carried.
todo.md updates (mark substrate items, add adaptive-pass and refit
follow-ups). .gitignore: data/labels/raw_*.jsonl* and gen_matches.jsonl
(commit pools, labels, meta, ledger). src/CLAUDE.md + tools/CLAUDE.md file
tables, root CLAUDE.md pointer to data/labels/ and the frozen-snapshot
rule. Memory mirror per the standing instruction. Style: no semicolons, no
em dashes, no special Unicode.
