# TD-Leaf(lambda) self-play bootstrap - results 1

Companion to `tdleaf-plan-1-amber-pangolin.md`. Work done 2026-07-29.

> **READ THIS FIRST: every Elo below is from a PINNED SCREENING FIT at 8
> games/pair.** The 158 existing roster agents were held at their current Elo
> (`rank.exe rate --pin ranking/standings.tsv`) and only the cohort was solved.
> A pinned fit **cannot dethrone a champion** - their ratings are its inputs.
> And 8 games/pair is below this project's 32-games/pair certification standard
> (`ranking/CHAMPION.md` rule 2), a fill that has inverted the top of the table
> three times here. Nothing below is a champion claim. The canonical
> `ranking/standings.tsv` was verified byte-identical (md5) before and after.

## What shipped

`src/ml_tdleaf.cpp` / `.h`, `train.exe tdleaf` - the project's first **online,
bootstrapped** value regime. Every other value regime is supervised and offline
(a fixed label computed once, fit in a batch). Here a position's target is the
model's own evaluation of a later position backed up through the search, applied
at the principal-variation leaf, with weights moving during play.

Supporting infrastructure, written because the study needed it:

- `rankFitBTPinned` + `rate --pin` - Bradley-Terry with a subset of ratings held
  fixed, so a cohort is measured on a stationary scale. Verified on the real
  158-agent store: max pinned drift **0.000000000**.
- `play --cohort` - schedule only pairs touching a listed agent. Without it a
  32/pair pass would have added ~702,000 roster-vs-roster fill games.
- `run_rank.ps1 -NoRate` - see "Gotchas".
- `--ckpt-at` game-count ladder, so game count is an output.
- `ML_SLOTS` 128 -> 256; the study owns slots 128..165.
- `Docs/ranking-workflow.md`, `Docs/corrections.md`.

## Cohort

38 agents from 13 training runs, one axis at a time around a base
(`init=models/pst_value.txt`, lambda 0.7, lr 0.01, d6/nb200k, 4 random opener
plies per side). All at ONE head, `ab(d6,tt,ord,nb200k)@1`. 19.5% of the
resulting 196-agent pool. 53,656 games played, plus 1,568 for the init control.

## Instrument checks (run BEFORE reading the numbers)

| Check | Result |
|---|---|
| Distinct games, not rows (defect 3) | **53,551 distinct of 55,224 rows, median ratio 1.000**, 6,256 of 6,903 pairs fully independent. Only 4.5% of pairs at <= 0.5, worst being deterministic-vs-deterministic (`ab(d1)@1.classic` at 2/8). Error bars are essentially honest, unlike the fixed-start pool's 0.438 median. |
| Does the PV walk work? | Mean PV depth **5.57 of 6** at d6 (13.2% truncated), 3.80 of 4 at d4 (8.3%). Reported per run, not assumed. |
| Do the knobs do anything? | lambda and seed both change ~100 of 129 weights vs the init; lambda variants differ from each other in 106-107 weights. |
| Roster frozen? | 158 pinned agents returned with max drift 0.000000000; canonical files md5-identical. |
| Draws | 0 across every run. |

## The init control, and why it nearly got missed

`models/pst_value.txt` - the model every champ-init cell starts from - was
**commented out of the roster** (`# off`), so the first screening pass had no
reference and the plan's own success criterion ("beats its own initialisation")
was unmeasurable. Added as an explicit control agent and played in (1,568 games).

**Init control: Elo 791 +/- 10 (1,568 games), freely fitted in the pinned fit.**

Do not compare that to the 920 quoted for this model in `tools/CLAUDE.md`: that
came from a different fit, and absolute Elo is not comparable across fits
(`ranking/CHAMPION.md` rule 3). 791 is the correct in-fit reference here.

## Result 1: the game count has an interior optimum, and it is not at the end

Block A, `init=champ, lambda 0.7, lr 0.01, d6/nb200k`, 4 seeds:

| games | s1001 | s2002 | s3003 | s4004 | mean | spread | vs init (791) |
|---|---|---|---|---|---|---|---|
| 100 | 870 | 862 | 864 | 881 | 869 | 19 | +78 |
| 250 | 930 | 920 | 954 | 973 | 944 | 53 | +153 |
| 500 | 991 | 1015 | 999 | 994 | 1000 | 24 | +209 |
| **1000** | 1056 | 1040 | 1056 | 1053 | **1051** | 16 | **+260** |
| 2000 | 1033 | 1007 | 995 | 1027 | 1016 | 38 | +224 |

**Peak at 1000 games, +260 Elo over the initialisation.** Then it *declines*:
paired per seed, 1000 -> 2000 gives -23, -33, -61, -26 (mean **-35.8**, all four
seeds the same sign). A paired test matters here because the seeds share an init.

This is the study's methodological payoff. A fixed game-count guess would very
likely have landed on the wrong side of that peak, and the withdrawn "500 games"
figure (see `Docs/corrections.md`) would have stopped 51 Elo short of it. The
count had to be measured.

**The spreads (16-53) are NOT the theory-8 seed band.** All four seeds start from
the same init, so the seed varies only openings and play, not initialisation.
These are not from-scratch replicates and must not be read as one.

## Result 2: the bootstrap does essentially all the work

Compared against the block-A base at the same rung:

| arm | g500 | vs base | g2000 | vs base |
|---|---|---|---|---|
| lambda = 0.0 (pure one-step TD) | 944 (n=2) | -56 | 1044 (n=2) | +28 |
| **lambda = 1.0 (== supervised on PV leaves)** | **713 (n=2)** | **-287** | **762 (n=2)** | **-254** |
| init = scratch | 778 (n=2) | -222 | 912 (n=2) | -103 |
| lr = 0.003 | 870 (n=1) | -130 | 1018 (n=1) | +2 |
| lr = 0.03 | 976 (n=1) | -24 | 886 (n=1) | -130 |
| generator d4 (vs d6) | 987 (n=1) | -13 | 1017 (n=1) | +2 |

`lambda = 1` is not an arbitrary setting: the implementation makes it *provably
identical* to outcome-supervised training on PV-leaf positions (`gOut_t = p_t - z`
by telescoping, unit-tested). At g500 it scores **713, BELOW the init's 791** -
worse than not training at all - and it is 254-287 Elo behind lambda=0.7.

So the +260 gain is **not** "training on PV leaves instead of played positions".
It is specifically the temporal-difference bootstrap. That is the cleanest result
here, and it comes from an arm that exists only because the closed form was
derived while implementing the gradient core.

Meanwhile lambda=0 is close to lambda=0.7 (worse at 500, +28 at 2000), so the
*trace length* matters far less than having a bootstrap at all.

## Result 3: from-scratch works, lags, and has not converged

778 at g500, 912 at g2000: **+134 while champ-init was already declining by -36.**
The gap to champ-init narrowed from -222 to -103. Scratch may keep improving past
2000; this study does not know, because 2000 was the top rung.

## Result 4: generator depth looks irrelevant (n=1, not settled)

d4 vs d6 came out -13 at g500 and +2 at g2000 - indistinguishable. This does not
support theory 44 (which predicted depth WOULD matter for a bootstrapped target,
unlike the supervised case) and is consistent with the repo's existing
"teacher depth is irrelevant" finding. **One seed per rung, so this is suggestive
only.**

## Result 5: colour skew is seed variance, not a systematic bias

Flagged mid-study as a possible defect in the scratch arm (776 W / 1224 B), then
withdrawn: the second scratch seed skewed the *opposite* way.

| block | White % of self-play wins |
|---|---|
| A-base (4 seeds) | 50-52% |
| B-lam0 | 56-57% |
| B-lam1 | **62%** |
| C-scratch | **39%** and **57%** (opposite directions) |
| E-d4 | 46% |

Note without claiming it: the best-rated block is the most balanced and the
worst-rated (lambda=1) is the most skewed. That association is **untested** - no
statistic was computed for it - and is recorded as an idea, not a finding.

## Gotchas discovered

1. **`run_rank.ps1` absorbs a leading `play` token and ALWAYS finishes with an
   unpinned `rate`.** With a study roster passed through, that silently
   overwrote canonical `ranking/ratings.tsv` and `standings.tsv` with a fit
   including the cohort - exactly the drift `--pin` exists to prevent. Caught by
   `git diff`, repaired by restoring the four *generated* files from git while
   keeping the 53,656 new rows in the append-only store. Fixed properly by adding
   **`-NoRate`**, now required by `tdleaf_study.ps1` and documented in both.
2. **A model's own agent may be `off` in the roster**, so "compare to its
   initialisation" can silently have no reference. Check the control is rated
   *before* running the cohort, not after.
3. `mlSetModel` takes ownership; pairing it with `delete` is a double free.
4. Test suite clobbers tracked `models/manifest.{json,md}` (pre-existing, via
   `trainDistValue` -> `writeManifest`). Restored rather than committed each time.
5. Writing PowerShell/C++ string literals through a heredoc or a non-raw Python
   string mangles `\t` and `\r` in Windows paths. Two comment blocks were
   corrupted this way and had to be rewritten.

## Future Work

Each tethered to a specific conclusion above.

- **Certify the peak.** Best single agent is s1001 at g1000, **1056** in this
  screening fit, against the pinned openless champion's 1012. That is a signal
  to certify, NOT a result: pinned fits cannot dethrone, and 8 games/pair is half
  the standard. Refutes or confirms: pick the top few rungs, append to
  `ranking/roster.txt`, fill contenders to 32 games/pair, run an unpinned refit
  (`Docs/ranking-workflow.md` Workflow B).
- **Bracket the 1000-game peak.** The ladder jumps 500 -> 1000 -> 2000, so the
  optimum is located only to within a factor of 2, and the -36 decline past it is
  unexplained (overfitting to self-play distribution? learning-rate decay
  needed?). Add rungs at 700/1400 and a decayed-lr arm.
- **Does scratch overtake champ-init?** Result 3's curves were converging when the
  ladder ran out. Extend the scratch arm to 4000/8000 games.
- **lambda and lr arms are n=1 or n=2.** Result 2's lambda=1 collapse is far
  outside any noise band and is safe; the lr ordering and the d4/d6 equivalence
  are not. Re-run those at 4+ seeds before quoting them.
- **Result 4 leaves theory 44 untested, not refuted.** One seed cannot separate
  "depth is irrelevant" from "this seed was lucky".
- **The batched update path (`--batch`) was implemented but never run.** The
  online-vs-batched question the developer asked about is still open.
- **MLP arm never run.** The whole cohort is the 130-parameter linear model.
  Theory 39/40 predict MLPs generalise worse, and TD-Leaf may or may not change
  that.

## Ideas This Inspired

- A pinned fit makes a *continuous* ladder affordable: rate a checkpoint every N
  games during one long run and watch the Elo curve directly, rather than
  discretising into rungs.
- The lambda=1 collapse suggests a diagnostic: for any new regime, implement the
  degenerate setting that reduces to an existing regime and rate it. A free
  control that validates both the implementation and the novelty claim.
- Colour balance during self-play might be a cheap online health signal, usable
  to stop or restart a run without any rating at all (see Result 5's untested
  association).
- If the post-peak decline is overfitting to the self-play distribution, mixing
  in ranked-pool replay positions during TD-Leaf might remove the peak entirely.
- `Docs/corrections.md` could carry a machine-checkable phrase list, so a script
  fails when a known-defective claim is restated without its defect name.
