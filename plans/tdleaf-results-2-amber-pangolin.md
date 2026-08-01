# TD-Leaf Pass 2 -- 25-draw random search, screening results and analysis

Companion to `tdleaf-plan-1-amber-pangolin.md` (Pass 1) and
`tdleaf-results-1-amber-pangolin.md`. This document covers Pass 2 of the
three-pass process in `Docs/model-training-playbook.md`: the broad sweep.

Status when written (2026-07-31): the single-seed screening round and the
top-12 seed-replication round are complete. The learning-curve round (top-3
draws x all seeds x all 6 rungs) is still outstanding, so every conclusion here
is at rung 4000 only, and the interior-optimum question (theory 46) is NOT
re-tested in this document.

> **[UPDATE 2026-08-01]** The learning-curve round has since COMPLETED, and it
> changes how the rung-4000 tables below should be read. All three top configs
> peak in the interior and decline by rung 4000, so **every number in this
> document is a post-peak measurement**:
>
> | rung | REF (n=6) | R23 (n=3) | R9 (n=3) |
> |---|---|---|---|
> | 250 | 932 | 845 | 956 |
> | 500 | 991 | 904 | 979 |
> | 1000 | 1022 | 968 | 980 |
> | 1500 | **1030** | 991 | **986** |
> | 2500 | 987 | **1001** | 976 |
> | 4000 | 955 | 953 | 959 |
>
> Theory 46 therefore reproduces independently at three configs, not one. The
> rung-4000 "dead heat" reported below (955/953/959) is an artifact of measuring
> all three after they had declined; at their own peaks they separate
> (1030/1001/986). The shortlist in this document was selected at rung 4000, so
> it was chosen on a post-peak criterion -- worth knowing before reusing it.
>
> Two later changes also postdate this document:
> * **Roster selection.** 16 agents were added to `ranking/roster.txt` (one seed
>   each: the three peaks, the same three at 4000, plus R14/R10/R19 champion-init
>   and R11/R15/R8/R5/R12/R22/R4 scratch-init for style spread). An earlier
>   33-agent, all-seeds version was replaced by this one.
> * **Agent-id regimes.** The id's first descriptor field now names the TRAINING
>   REGIME (`tdleaf_self`, `pool_games`, `position_elo`, ...) rather than the
>   model type (`value`/`dist`), so ids quoted in this document appear in the
>   superseded form. See `src/ranking.h` and `Docs/benchmarking.md`.

---

## 1. What was run

Random search over the joint hyperparameter space (Bergstra & Bengio 2012, see
`Docs/works-cited.md`), not one-axis-at-a-time. 24 random draws plus 1 fixed
reference recipe (`REF`, the previous best-known TD-Leaf settings). Sampler:
`tools/tdleaf_sample_pass2.ps1`. Ledger: `models/sweep/tdleaf_pass2_draws.csv`.

Sampled sets:

| Axis | Values |
|---|---|
| Lambda | 0.0 .. 1.0 in steps of 0.1 |
| Lr | 0.001, 0.003, 0.01, 0.03, 0.1 |
| LrSched | on, off |
| L2 | 0, 0.0003, 0.001, 0.003 |
| Explore | 0, 0.01, 0.02, 0.05 |
| ExploreSched | on, off |
| Init | champion, scratch |
| Depth | d4, d6 |
| OpenPlies | 2, 4, 8 |
| Arch | linear, mlp16, mlp32 |
| FeatVer | 1, 2 |

Two rounds of measurement, both on a **pinned** Bradley-Terry fit (roster held
fixed, screening only, cannot dethrone -- see `Docs/ranking-workflow.md`
Workflow A):

- **Round A (screening).** 25 draws, 1 seed each, rung 4000, 8 games/pair.
- **Round B (seed replication).** Top 12 draws by Round A, all seeds, rung 4000.

---

## 2. The confound baked into the sampler

`tools/tdleaf_sample_pass2.ps1` lines 60-61:

```powershell
$arch    = if ($init -eq "scratch") { Pick $ArchSet }    else { "linear" }
$featver = if ($init -eq "scratch") { Pick $FeatVerSet } else { 2 }
```

Architecture and feature version are properties of the model FILE, so they
cannot be chosen freely when the run starts from an existing champion model.
The constraint is correct as engineering and wrong as experimental design: it
makes `Init` non-orthogonal to `Arch` and `FeatVer`.

Consequence: **every champion row is linear+v2**, while 8 of 12 scratch rows are
MLPs. Any raw champion-vs-scratch comparison is also a linear-vs-MLP
comparison. Section 4 separates the two.

The scratch+linear+v2 cell -- the cell the whole initialization conclusion rests
on -- has **n = 1** (draw R15). That is the highest-value design fix for Pass 3.

---

## 3. Round A results (1 seed, rung 4000)

Ordered by screening Elo. `Arch` shown as the sampler's own names.

| Draw | Elo | +/-SE | Lambda | Lr | LrDec | L2 | Expl | ExpDec | Init | Depth | Open | Arch | FV |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| REF | 975 | 11 | 0.7 | 0.01 | off | 0 | 0 | off | champion | d6 | 4 | linear | 2 |
| R23 | 967 | 11 | 0.8 | 0.003 | off | 0.001 | 0 | off | champion | d6 | 4 | linear | 2 |
| R9 | 960 | 11 | 0.4 | 0.03 | on | 0.0003 | 0 | on | champion | d6 | 4 | linear | 2 |
| R2 | 941 | 11 | 0.5 | 0.003 | on | 0.0003 | 0.02 | on | champion | d6 | 4 | linear | 2 |
| R24 | 923 | 10 | 1.0 | 0.001 | on | 0 | 0.01 | on | champion | d4 | 2 | linear | 2 |
| R14 | 911 | 10 | 0.1 | 0.003 | on | 0.003 | 0.05 | on | champion | d4 | 8 | linear | 2 |
| R21 | 911 | 10 | 0.2 | 0.1 | off | 0 | 0.01 | off | champion | d6 | 8 | linear | 2 |
| R13 | 909 | 10 | 0.2 | 0.003 | on | 0 | 0.02 | on | champion | d4 | 8 | linear | 2 |
| R7 | 904 | 10 | 0.2 | 0.001 | off | 0 | 0.01 | on | champion | d6 | 4 | linear | 2 |
| R3 | 898 | 10 | 0.1 | 0.001 | off | 0.003 | 0.02 | on | champion | d6 | 2 | linear | 2 |
| R12 | 893 | 10 | 0.7 | 0.003 | off | 0.003 | 0.02 | on | scratch | d4 | 8 | mlp32 | 2 |
| R11 | 890 | 10 | 0.4 | 0.003 | off | 0 | 0 | on | scratch | d6 | 4 | linear | 1 |
| R6 | 867 | 10 | 0.6 | 0.01 | on | 0.0003 | 0.02 | off | scratch | d4 | 2 | linear | 1 |
| R1 | 857 | 10 | 0.5 | 0.001 | on | 0.001 | 0.02 | on | scratch | d6 | 2 | linear | 1 |
| R15 | 841 | 10 | 0.9 | 0.003 | off | 0.001 | 0.02 | on | scratch | d4 | 2 | linear | 2 |
| R8 | 790 | 10 | 0.9 | 0.001 | on | 0 | 0.02 | on | scratch | d6 | 8 | mlp16 | 1 |
| R10 | 757 | 10 | 0.3 | 0.1 | off | 0.001 | 0 | off | champion | d6 | 2 | linear | 2 |
| R17 | 706 | 10 | 0.1 | 0.01 | on | 0.003 | 0.01 | on | scratch | d6 | 4 | mlp16 | 2 |
| R20 | 706 | 10 | 0.3 | 0.003 | off | 0.003 | 0 | off | scratch | d6 | 2 | mlp32 | 2 |
| R22 | 670 | 10 | 0.9 | 0.1 | on | 0.001 | 0 | off | scratch | d6 | 8 | mlp32 | 1 |
| R16 | 658 | 11 | 0.2 | 0.1 | off | 0.003 | 0 | off | champion | d4 | 2 | linear | 2 |
| R5 | 650 | 11 | 0.0 | 0.001 | off | 0.001 | 0 | on | scratch | d6 | 2 | mlp16 | 1 |
| R18 | 641 | 11 | 0.1 | 0.001 | on | 0 | 0.05 | off | scratch | d4 | 4 | mlp32 | 2 |
| R19 | 629 | 11 | 0.1 | 0.1 | off | 0.003 | 0 | on | champion | d6 | 4 | linear | 2 |
| R4 | 621 | 11 | 0.2 | 0.001 | on | 0.001 | 0.05 | on | scratch | d6 | 8 | mlp32 | 2 |

`+/-SE` is the Bradley-Terry standard error on that agent's rating given its
game record. It is NOT seed error, and it is the smaller of the two. Round B
below supplies the seed error, which is 2-5x larger.

---

## 4. Round B results (seed replication, rung 4000)

| Draw | Mean Elo | n | SEM | sd | Spread |
|---|---|---|---|---|---|
| R23 | 958.3 | 3 | 6.7 | 11.6 | 21 |
| REF | 956.7 | 6 | 11.1 | 27.2 | 73 |
| R9 | 955.3 | 3 | 0.9 | 1.5 | 3 |
| R2 | 939.0 | 3 | 8.7 | 15.1 | 29 |
| R24 | 935.3 | 3 | 3.8 | 6.7 | 13 |
| R7 | 908.0 | 3 | 1.5 | 2.6 | 5 |
| R13 | 903 | 3 | -- | -- | 17 |
| R14 | 901 | 3 | -- | -- | 17 |
| R3 | 900 | 3 | -- | -- | 6 |
| R21 | 900 | 3 | -- | -- | 24 |
| R11 | 859 | 3 | 9.6 | 16.6 | 33 |
| R12 | 848 | 3 | 29.3 | 50.7 | 97 |

The top three are statistically tied: R23 958.3 +/- 6.7, REF 956.7 +/- 11.1,
R9 955.3 +/- 0.9, a 3.0 Elo range against SEMs of 0.9-11.1.

REF's #1 finish in Round A (975) was substantially luck. Its own seed spread is
73 and its 6-seed mean is 956.7, i.e. 18 Elo below its screening number. This
is a concrete instance of why single-seed screening order is a shortlist and not
a ranking.

R12 is the only MLP that reached the top 12 and it has the widest seed spread
in the table (97, sd 50.7), reproducing theory 40.

---

## 5. Effect analysis

All figures below are computed from the Round A table (25 rows). Group means,
not model fits. Where a group is contaminated by the Lr=0.1 cliff (section 5.3)
the contaminated and decontaminated versions are both given.

### 5.1 Architecture is the largest single effect

Holding `Init = scratch` fixed, so initialization cannot contribute:

| Group | n | Mean Elo |
|---|---|---|
| scratch + linear (R11, R6, R1, R15) | 4 | 863.8 |
| scratch + mlp16/mlp32 (R12, R8, R17, R20, R22, R5, R18, R4) | 8 | 709.6 |

Gap **154 Elo**, sd 20.5 vs 90.9, difference SE ~45. Consistent with theory 37
(the narrow first layer is the efficient shape) and theory 40 (MLP dist models
are seed-fragile).

### 5.2 The initialization effect is roughly 66 Elo, not 110-160

| Comparison | Champion | Scratch | Gap |
|---|---|---|---|
| Raw, all rows (mean) | 872.5 (n=13) | 761.0 (n=12) | +111.5 |
| Raw, all rows (median) | 911 | 748 | +163 |
| Architecture controlled (linear only) | 872.5 (n=13) | 863.8 (n=4) | +8.8 |
| Architecture controlled AND Lr=0.1 dropped | 929.9 (n=10) | 863.8 (n=4) | **+66.2** |

The middle row overcorrects. All three champion catastrophes (R19 629, R16 658,
R10 757) are Lr=0.1 draws, and no scratch-linear draw happened to sample
Lr=0.1, so the middle row penalises the champion group for a knob failure the
comparison group never faced. The bottom row removes the cliff from both sides
and is the better estimate: **+66.2 Elo, SE ~13.7** (group sds 28.5 and 20.5,
n=10 and n=4).

Decomposition of the raw gap: roughly 100 Elo is architecture, roughly 66 Elo is
initialization.

**The effect is nonetheless real.** The top 10 of 25 being all champion has
probability C(13,10)/C(25,10) = 8.7e-5 under a no-effect null, and it survives
restriction to the 17 linear rows: the top 10 of those are also all champion,
C(13,10)/C(17,10) = 0.015.

**Caveats.** n=4 on the scratch-linear side. FeatVer is still uncontrolled
within it (R11, R6, R1 are v1; only R15 is v2). And +66 Elo is one seed-noise
band wide (the project's measured band is 50-150), so this supports "champion
init is better" and does not support "champion init dominates".

### 5.3 Learning rate: a cliff at 0.1, flat below it

Champion rows only (so architecture is constant):

| Lr | n | Mean Elo |
|---|---|---|
| 0.001 | 3 | 908.3 |
| 0.003 | 4 | 932.0 |
| 0.01 | 1 | 975 |
| 0.03 | 1 | 960 |
| 0.1 | 4 | 738.8 |

The 0.001-0.03 range spans 67 Elo, inside the noise band. The 0.1 drop is
170-235 Elo. One unexplained exception: R21 (Lr=0.1) scored 911; nothing else in
its row distinguishes it from R10/R16/R19.

### 5.4 Lambda has an interior optimum near 0.6-0.8

| Lambda bin | All 25 (n, mean) | Champion only (n, mean) |
|---|---|---|
| 0.0-0.2 | 11, 767.1 | 7, 831.4 |
| 0.3-0.5 | 6, 851.8 | 3, 886.0 |
| 0.6-0.8 | 4, 925.5 | 2, 971.0 |
| 0.9-1.0 | 4, 806.0 | 1, 923.0 |

Both binnings peak in 0.6-0.8. Consistent with Pass 1, where the lambda=1
control scored 713, below its own initialization. Supports theory 43.

Weak point: the champion 0.6-0.8 bin is n=2 and those two draws ARE the top two,
so that slice is partly circular. The all-25 binning is the less circular
version and shows the same peak, which is why both are given.

### 5.5 Four knobs show nothing at this resolution

| Knob | Split | Means |
|---|---|---|
| Depth | d4 (n=8) vs d6 (n=17) | 830.4 vs 813.6; champion-only medians 910 vs 911 |
| Lr decay | on (n=12) vs off (n=13) | 816.3 vs 821.5 |
| Explore decay | on (n=17) vs off (n=8) | 821.4 vs 813.9 |
| L2 | 0 / 0.0003 / 0.001 / 0.003 | 867.9 / 922.7 / 766.1 / 771.6, no monotone trend |

The depth result is worth acting on: d4 self-play generation is cheaper than d6
and costs nothing measurable in the resulting model's strength. Note this is
about the GENERATOR depth during training, not the evaluation head.

The L2 bins for 0.001 and 0.003 each contain two Lr=0.1 rows, which is enough to
explain their position without any L2 effect.

### 5.6 The apparent exploration effect is an artifact

| Explore | n | Mean (raw) | Mean (Lr=0.1 rows removed) |
|---|---|---|---|
| 0 | 10 | 786.2 | 858.0 (n=6) |
| 0.01 | 4 | 861.0 | 861.0 |
| 0.02 | 8 | 874.5 | 874.5 |
| 0.05 | 3 | 724.3 | 724.3 |

Four of the five Lr=0.1 draws landed in the Explore=0 bin. Removing them makes
Explore=0 indistinguishable from 0.01 and 0.02. The top three draws (R23, REF,
R9) all have Explore=0. There is no measurable benefit to exploration noise in
this data, and the raw table's suggestion of one is a sampling accident.

### 5.7 A prediction that failed

Prediction made before computing: the scratch penalty should grow with opening
randomness, because a scratch model cannot evaluate off-book positions and its
TD targets there are noise.

Champion minus scratch gap by OpenPlies (champion side excludes Lr=0.1 rows):
126 (Open=2), 204 (Open=4), 167 (Open=8). Not monotone. **Not supported.**
Recorded because a prediction that failed is as much of a result as one that
held.

---

## 6. Hypotheses about the initialization effect

The developer proposed two mechanisms for champion-init's advantage:

- **H1 (convergence).** Scratch is on the same learning curve, just further
  back; more games would close the gap.
- **H2 (pool homogeneity).** Much of the roster plays like the champion
  (primarily a chip counter with a left-file bias), so starting near the
  champion buys a good score against a narrow opponent distribution rather than
  absolute strength.

### 6.1 A correction to H2's stated mechanism

TD-Leaf here trains by **self-play**. The roster is never in the training loop,
so "self-play causes you to learn how to play well against the roster agents"
cannot be a training-side mechanism. Pool homogeneity can only act on the
**measurement** side.

Restated, H2 becomes: *an agent whose evaluation function stays near the
champion's scores well against a pool of champion-like agents, whether or not it
is stronger in absolute terms.* That is a claim about the rating instrument, and
in that form it is testable and worth testing.

### 6.2 The decisive test needs zero new games

Measure the champion-init vs scratch-init gap **head to head**, not through the
pool. Cohort-vs-cohort games were scheduled in both rounds, so the direct record
between champion-init draws and scratch-init draws is already in
`ranking/matches.jsonl`.

- If the head-to-head gap is close to the +66 Elo pooled estimate, H2 is dead:
  the gap is not an artifact of who is in the pool.
- If it is much smaller, H2 is live.

This is strictly better than the diversity experiment as a FIRST move, because
adding agents also changes the Elo scale (the Bradley-Terry prior compresses as
the pool grows, `Docs/benchmarking.md`). A gap that shrinks after adding
diversity is confounded with that scale change and cannot be read directly.
Head-to-head has no pool at all.

Apply the distinct-games check (`CLAUDE.md` champion-hygiene rule 7) before
quoting any record from this extraction.

### 6.3 The mirror test, specified

The developer's "flip the inputs and outputs" idea is sound and cheap. For v2
(129 sparse piece-square features) a left-right file mirror is an **index
permutation** on the feature vector. Applying it to a trained model's weights
yields an agent that is exactly as strong under a mirrored board but plays
differently against the fixed pool. No retraining required.

Scope limit: this tests the left-file-bias half of H2 (theories 23 and 32). It
does not test "the pool is mostly chip counters".

### 6.4 Additional theories

- **T-A, warm-started PV rather than warm-started policy.** TD-Leaf applies its
  gradient at the principal-variation leaf. With scratch weights the PV is
  effectively arbitrary for hundreds of games, so eligibility traces back noise
  into noise. Distinct from H1: H1 says scratch is on the same curve later, T-A
  says scratch's early games are actively uninformative and may corrupt weights.
  Test: a hybrid arm with scratch weights but PVs extracted from a fixed strong
  search head for the first 250 games. If that closes most of the gap, T-A beats
  H1.
- **T-B, no special role for the champion specifically.** Initialize from a
  different strong roster linear v2 agent, and separately from a deliberately
  mediocre one. If a strong non-champion matches the champion, "competent
  starting point" is the operative property, not "the champion". This also bites
  H2: if a strong NON-champion init rates just as high, closeness to the
  champion is not what the pool is rewarding.
- **T-C, self-play needs an asymmetry to bootstrap out of.** With scratch init
  both players are equally weak, outcomes approach coin flips, and the terminal
  signal z carries little information. Prediction: scratch learning curves show
  a flat or dipping region before rising, while champion curves rise from the
  first rung. The learning-curve round cannot answer this, because all three of
  its draws are champion-init. This is the largest hole in the current evidence.
- **T-D, the fit favours champion-init independent of pool composition.** In a
  Bradley-Terry fit an agent with an intransitive profile (beats some opponents
  badly, loses to others) is compressed toward the middle, while a near-copy of
  a strong pinned agent has a clean transitive profile and rates high. So the
  advantage may live in the rating MODEL rather than in the pool's membership.
  Test with zero new games: compare each cohort agent's raw score percentage
  against the pinned roster to its fitted Elo, and inspect Bradley-Terry
  residuals. Systematically smaller residuals for champion-init agents would
  make T-D live. T-D and H2 are separable: H2 is cured by adding diverse agents,
  T-D is not.
- **T-E, the null against H2: the champion start is objectively good, not
  relatively good.** Chip counting is close to correct in Breakthrough, so the
  champion's weights may simply be a genuinely good point in weight space that
  TD-Leaf can only refine locally. Under T-E, adding pool diversity would NOT
  shrink the gap. T-E is the hypothesis the diversity experiment must be capable
  of rejecting, and it is why the diversity-free head-to-head measurement should
  come first.
- **T-F, the left bias is a move-generator tie-break artifact rather than
  something learned.** Theory 23 already records this pool's shared left-file
  tie-break bias. If a SCRATCH-init TD-Leaf agent also skews left, the bias comes
  from move-generation order and champion init cannot be inheriting it from the
  champion's weights. Test: measure the file distribution of chosen moves for
  the champion, a champion-init TD-Leaf agent, and a scratch-init TD-Leaf agent.
  Cheap, and needs no ranking run.

### 6.5 Suggested order

1. Head-to-head champion-init vs scratch-init extraction from existing
   `ranking/matches.jsonl`. Free. Can settle H2 outright.
2. Bradley-Terry residual check for T-D. Also free, same data.
3. Scratch learning curves (separates H1 from T-C). The biggest gap in current
   evidence.
4. Mirror test (6.3), for the left-bias half of H2.
5. T-B alternate-initialization arm.

---

## 7. Design changes for Pass 3

- **Make Init orthogonal to Arch and FeatVer.** The engineering constraint is
  real (architecture is a property of the model file), but the sampler can
  stratify instead of forcing: sample Arch and FeatVer first, then sample Init
  only among the values compatible with that model shape, and require a minimum
  cell count for scratch+linear+v2. That cell currently has n=1.
- **Drop Lr=0.1 from the set, or stratify it.** It consumed 5 of 25 draws
  (20% of the budget) confirming a cliff, and its uneven landing across Init and
  Explore contaminated three separate group comparisons in section 5.
- **Keep depth at d4 for generation** unless a specific test needs d6. Section
  5.5 finds no strength difference and d4 is cheaper.
- **Narrow lambda to 0.4-0.9** and spend the freed budget on seeds.
- **Add a scratch learning-curve arm** so H1 and T-C are answerable.

---

## 8. Measurement caveats on everything above

- All Round A numbers are **1 seed**. Round B shows per-draw seed spreads of
  3-97 Elo, so Round A order is a shortlist, not a ranking.
- All fits are **pinned** (`Docs/ranking-workflow.md` Workflow A). The roster is
  held fixed and none of these ratings can dethrone anything, nor are they
  comparable in absolute terms to any other fit (`CLAUDE.md` rule 3: compare
  order and error bands within one fit, never absolute Elo across fits).
- Group means in section 5 are unweighted means over draws, not a regression.
  With 25 draws and 11 axes there is not enough data for a joint fit, which is
  why every comparison is stated with its n and its contaminating confound
  named.
- **Distinct-games check not yet applied.** These are 8 games/pair nominal.
  Per `CLAUDE.md` rule 7 and `Docs/benchmarking.md` defect 3, deterministic
  pairs replay, so the effective sample is smaller than the nominal one and the
  printed SEs are understated. The Explore=0 draws are the ones most at risk,
  which includes all three top finishers. This check should be run before any of
  these numbers is quoted outside this document.

---

## 9. Future work

- **Scratch learning curves.** Tethered to section 6.4 T-C and to H1. Without
  them, neither "scratch converges later" nor "scratch's early games are
  uninformative" can be distinguished, and both are live explanations of the
  section 5.2 gap.
- **Head-to-head extraction.** Tethered to section 5.2's +66 Elo. If the gap is
  a pool artifact then the +66 figure describes the instrument and not the
  models, and section 7's design changes are aimed at the wrong thing.
- **Distinct-games audit of Round A and Round B.** Tethered to every number in
  sections 3, 4, and 5. If the effective sample is much below nominal, the
  section 5.4 lambda peak (champion bin n=2) is the first casualty.
- **Rung sensitivity of the architecture result.** Section 5.1 is measured at
  rung 4000 only. MLPs may simply need more games; the same interior-optimum
  question that produced theory 46 applies here and is untested for MLPs.
- **Seed variance as a function of regularization.** R9 (Lr decay + L2) has
  sd 1.5 across 3 seeds against REF's sd 27.2 across 6. This is a lead, not a
  finding: sd from n=3 is very weak, and the two draws differ on lambda and Lr
  as well. A direct test would fix everything but the regularization and run 6+
  seeds.

---

## 10. Ideas this inspired

- A "sampler lint" step that reports which axes are non-orthogonal in a drawn
  design before the runs launch, so a constraint like section 2's is visible at
  design time rather than discovered during analysis.
- Reporting raw score percentage next to fitted Elo in the pinned-fit output, so
  intransitivity (T-D) is visible without a separate analysis.
- A symmetry-augmentation option in training: present each position and its
  left-right mirror, which would remove the file bias from any learned evaluator
  by construction rather than by measurement.
- Ranking a deliberately constructed "diverse foil" set (agents chosen to be
  maximally unlike the champion) as a permanent second axis of the roster, so
  pool-homogeneity effects are detectable on every future study rather than
  investigated ad hoc.
