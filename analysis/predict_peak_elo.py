#!/usr/bin/env python3
"""Fit an interpretable peak-Elo predictor over a cohort's swept config axes.

Given a long-format cohort export (one row per checkpoint, e.g.
plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv from
tools/export_cohort_results.ps1), collapses each training run's checkpoint
ladder down to its peak Elo, then fits:

  - a shallow decision tree (depth chosen by cross-validation) for a
    human-readable rule set that captures interactions and the
    non-monotonic rung effects this project has repeatedly observed,
  - a random forest for a more stable, less variance-prone feature
    importance ranking (impurity-based, plus an out-of-fold permutation
    importance),
  - a linear regression on standardized features, as a baseline contrast --
    a linear R2 well below the tree/RF R2 is itself informative: it means
    the axes' effects are not additive or monotonic,
  - univariate bucket means per feature: the same question with no model at
    all, to sanity-check the model-based importances against raw data.

Usage:
    python analysis/predict_peak_elo.py \
        --in plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv \
        --group-by block --target elo --rung-col rung \
        --features sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m \
        --out-report plans/gumbel-mcts-peak-elo-predictor-5-violet-harbor.md \
        --out-tree-image plans/gumbel-mcts-peak-elo-tree-5-violet-harbor.png

All arguments except --in have the defaults shown above baked in as the
common case for a rung-ladder cohort study; override any of them for a
differently-shaped cohort (e.g. no rung ladder: pass --rung-col "" and
--target directly at the per-run Elo).
"""
import argparse
import sys

import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestRegressor
from sklearn.inspection import permutation_importance
from sklearn.linear_model import LinearRegression
from sklearn.model_selection import KFold, cross_val_score
from sklearn.preprocessing import StandardScaler
from sklearn.tree import DecisionTreeRegressor

DEFAULT_FEATURES = "sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m"


def _fmt_threshold(v):
    """Adaptive-precision number format: fixed decimals (like sklearn's
    export_text) breaks on this cohort's mixed-scale axes -- l2's grid
    (0, 0.0003, 0.001) rounds to indistinguishable 0.0 at 1 decimal place,
    while replaycap's grid (500..8000) doesn't need decimals at all."""
    return f"{v:.6g}"


def render_tree_text(tree, feature_names):
    t = tree.tree_
    lines = []

    def recurse(node, depth):
        indent = "|   " * depth
        if t.feature[node] != -2:  # not a leaf (sklearn TREE_UNDEFINED == -2)
            name = feature_names[t.feature[node]]
            thr = _fmt_threshold(t.threshold[node])
            lines.append(f"{indent}|--- {name} <= {thr}")
            recurse(t.children_left[node], depth + 1)
            lines.append(f"{indent}|--- {name} >  {thr}")
            recurse(t.children_right[node], depth + 1)
        else:
            val = t.value[node][0][0]
            lines.append(f"{indent}|--- value: [{val:.1f}]")

    recurse(0, 0)
    return "\n".join(lines)


def load_peaks(path, group_by, target, rung_col, features):
    df = pd.read_csv(path, sep="\t", comment="#")
    rows = []
    for key, g in df.groupby(group_by):
        peak_idx = g[target].idxmax()
        row = {group_by: key, "peak_" + target: g.loc[peak_idx, target]}
        if rung_col and rung_col in g.columns:
            row["peak_" + rung_col] = g.loc[peak_idx, rung_col]
        for f in features:
            vals = g[f].unique()
            if len(vals) != 1:
                sys.exit(f"feature '{f}' is not constant within group {key}: {vals}")
            row[f] = vals[0]
        rows.append(row)
    return pd.DataFrame(rows)


def cv_r2(model, X, y, kf):
    scores = cross_val_score(model, X, y, cv=kf, scoring="r2")
    return scores.mean(), scores.std()


def pick_tree_depth(X, y, kf, depths, min_samples_leaf):
    results = []
    for d in depths:
        m = DecisionTreeRegressor(
            max_depth=d, min_samples_leaf=min_samples_leaf, random_state=0
        )
        mean, std = cv_r2(m, X, y, kf)
        results.append((d, mean, std))
    best = max(results, key=lambda r: r[1])
    return best[0], results


def oof_permutation_importance(X, y, kf, feature_names, n_estimators, seed):
    importances = np.zeros((kf.get_n_splits(), len(feature_names)))
    for i, (train_idx, test_idx) in enumerate(kf.split(X)):
        rf = RandomForestRegressor(n_estimators=n_estimators, random_state=seed)
        rf.fit(X.iloc[train_idx], y.iloc[train_idx])
        pi = permutation_importance(
            rf, X.iloc[test_idx], y.iloc[test_idx], n_repeats=30, random_state=seed
        )
        importances[i] = pi.importances_mean
    return importances.mean(axis=0), importances.std(axis=0)


def find_coupled_features(peaks, features, threshold=0.99):
    """Axes that were not actually drawn independently (|Spearman rho| >=
    threshold) -- reading their importances as two separate effects double-
    counts one underlying choice."""
    coupled = []
    for i, a in enumerate(features):
        for b in features[i + 1 :]:
            rho = peaks[a].corr(peaks[b], method="spearman")
            if abs(rho) >= threshold:
                coupled.append((a, b, rho))
    return coupled


def univariate_tables(peaks, features, target_col):
    tables = {}
    for f in features:
        g = peaks.groupby(f)[target_col].agg(["mean", "median", "std", "count"])
        g = g.sort_index()
        tables[f] = g
    return tables


def render_report(
    peaks, features, target_col, rung_col, kf, tree_depth, depth_results,
    tree, rf, rf_perm_mean, rf_perm_std, lin, lin_r2, tree_r2, rf_r2,
    uni_tables, source_path, out_tree_image, coupled,
):
    n = len(peaks)
    lines = []
    lines.append("# Peak-Elo predictor: decision tree / feature importance")
    lines.append("")
    lines.append(
        f"Source: `{source_path}`, {n} draws (one row per `{peaks.columns[0]}`, "
        f"collapsing each draw's checkpoint ladder to its peak `{target_col}`)."
    )
    lines.append(
        "Purpose: identify, per swept config axis, which values tend to produce "
        "the highest peak Elo, to bias the next (MLP) round's sweep ranges rather "
        "than searching from scratch. This is descriptive/exploratory, not a "
        "certification -- see Caveats."
    )
    lines.append("")
    lines.append("## Method")
    lines.append("")
    lines.append(
        "Three models fit on the same 10 swept axes -> peak Elo, compared by "
        f"5-fold cross-validated R2 (`sklearn.model_selection.KFold`, "
        "shuffle, seed 0):"
    )
    lines.append("")
    lines.append("| Model | CV R2 (mean +/- std) | Why fit it |")
    lines.append("|---|---|---|")
    lines.append(
        f"| Decision tree (depth={tree_depth}) | {tree_r2[0]:.3f} +/- {tree_r2[1]:.3f} "
        "| Human-readable rules; captures interactions and non-monotonic effects "
        "without assuming a functional form |"
    )
    lines.append(
        f"| Random forest (500 trees) | {rf_r2[0]:.3f} +/- {rf_r2[1]:.3f} "
        "| Averages many trees for a more stable importance ranking than any "
        "single tree's splits |"
    )
    lines.append(
        f"| Linear (standardized) | {lin_r2[0]:.3f} +/- {lin_r2[1]:.3f} "
        "| Baseline: if this is much worse than the tree/RF, the axes' effects "
        "are not additive/monotonic |"
    )
    lines.append("")
    lines.append(
        "Tree depth chosen by CV R2 over depths "
        + ", ".join(f"{d}={m:.3f}" for d, m, _ in depth_results)
        + " (min_samples_leaf=8 throughout, to keep leaves large enough to "
        "average out per-draw seed noise)."
    )
    lines.append("")
    lines.append("## Decision tree rules")
    lines.append("")
    lines.append(
        "Fit on all "
        + str(n)
        + " draws (the CV split above is for model comparison only; this tree "
        "is refit on the full dataset for interpretation):"
    )
    lines.append("")
    lines.append("```")
    lines.append(render_tree_text(tree, features))
    lines.append("```")
    if out_tree_image:
        lines.append("")
        lines.append(f"Rendered diagram: `{out_tree_image}`.")
    lines.append("")
    lines.append("## Feature importance")
    lines.append("")
    if coupled:
        for a, b, rho in coupled:
            lines.append(
                f"**Note:** `{a}` and `{b}` were not drawn independently in this "
                f"sweep (Spearman rho={rho:.2f} across all {n} draws -- every "
                f"draw's value of one determines the other). Their importances "
                "below describe one underlying axis counted twice, not two "
                "independent effects."
            )
        lines.append("")
    lines.append(
        "RF impurity importance (in-sample, from the full-data fit) vs. "
        "out-of-fold permutation importance (mean R2 drop when a feature is "
        "shuffled in a held-out fold, averaged over the 5 folds -- the more "
        "trustworthy of the two since it is evaluated out-of-sample):"
    )
    lines.append("")
    lines.append("| Feature | RF impurity importance | OOF permutation importance (mean +/- std) |")
    lines.append("|---|---|---|")
    rf_imp = dict(zip(features, rf.feature_importances_))
    order = sorted(
        range(len(features)), key=lambda i: rf_perm_mean[i], reverse=True
    )
    for i in order:
        f = features[i]
        lines.append(
            f"| {f} | {rf_imp[f]:.3f} | {rf_perm_mean[i]:.3f} +/- {rf_perm_std[i]:.3f} |"
        )
    lines.append("")
    lines.append("## Linear regression (standardized coefficients)")
    lines.append("")
    lines.append(
        "Elo change per +1 standard deviation of the feature, holding the "
        f"others fixed (R2={lin_r2[0]:.3f}, see caution above about how much "
        "this linear fit actually explains):"
    )
    lines.append("")
    lines.append("| Feature | Std. coefficient (Elo / 1 sd) |")
    lines.append("|---|---|")
    coef_order = sorted(
        zip(features, lin.coef_), key=lambda kv: abs(kv[1]), reverse=True
    )
    for f, c in coef_order:
        lines.append(f"| {f} | {c:+.1f} |")
    lines.append("")
    lines.append("## Univariate bucket means (no model, raw data)")
    lines.append("")
    lines.append(
        "Every swept axis here is a small discrete grid, so the plain "
        f"per-value mean peak `{target_col}` is itself readable, with no model "
        "in the loop:"
    )
    lines.append("")
    for f in features:
        t = uni_tables[f]
        lines.append(f"**{f}**")
        lines.append("")
        lines.append("| value | mean | median | std | n |")
        lines.append("|---|---|---|---|---|")
        for idx, row in t.iterrows():
            lines.append(
                f"| {idx} | {row['mean']:.0f} | {row['median']:.0f} | "
                f"{row['std']:.0f} | {int(row['count'])} |"
            )
        lines.append("")
    if rung_col and ("peak_" + rung_col) in peaks.columns:
        lines.append(f"**peak_{rung_col} distribution** (which rung the peak landed on)")
        lines.append("")
        vc = peaks["peak_" + rung_col].value_counts().sort_index()
        lines.append("| rung | count |")
        lines.append("|---|---|")
        for idx, c in vc.items():
            lines.append(f"| {idx} | {c} |")
        lines.append("")
    lines.append("## Recommendation for the next round")
    lines.append("")
    lines.append(
        "Auto-generated from the tables above: for each axis, ranked by "
        "out-of-fold permutation importance, the univariate best-performing "
        "level and a read on whether the effect clears the noise floor. "
        "Coupled axes (see note above) are merged into one entry."
    )
    lines.append("")
    coupled_partner = {a: b for a, b, _ in coupled}
    coupled_partner.update({b: a for a, b, _ in coupled})
    already_emitted = set()
    lines.append("| Axis | Best level (highest mean peak Elo) | OOF perm. importance | Read |")
    lines.append("|---|---|---|---|")
    for i in order:
        f = features[i]
        if f in already_emitted:
            continue
        label = f
        if f in coupled_partner and coupled_partner[f] in features:
            label = f + " (= " + coupled_partner[f] + ")"
            already_emitted.add(coupled_partner[f])
        already_emitted.add(f)
        t = uni_tables[f]
        best_level = t["mean"].idxmax()
        best_mean = t.loc[best_level, "mean"]
        imp = rf_perm_mean[i]
        if imp > 0.03:
            read = "clear effect -- fix at the best level"
        elif imp > 0.01:
            read = "weak/marginal effect"
        else:
            read = "no detectable effect at this sample size -- fine to fix at any convenient/cheap value"
        lines.append(f"| {label} | {f}={best_level} (mean {best_mean:.0f}) | {imp:.3f} | {read} |")
    lines.append("")
    lines.append("## Caveats")
    lines.append("")
    lines.append(
        "- **n=" + str(n) + ", single seed per draw.** Per-draw Elo sits inside "
        "this project's documented 50-150 Elo seed-noise band, so the target "
        "variable itself is noisy; the CV R2 above is the honest ceiling on "
        "how much of that noise these models can actually explain, not just "
        "the training-set fit."
    )
    coupled_note = (
        " Exception: " + "; ".join(f"`{a}`/`{b}`" for a, b, _ in coupled) + " are perfectly coupled in this sweep (see the Feature importance note), so they are not independent of each other."
        if coupled else ""
    )
    lines.append(
        "- **Not a controlled factorial.** Most axes were drawn "
        "independently per draw, which keeps them roughly uncorrelated in "
        "expectation (unlike a hand-picked grid), but 101 draws over this "
        "many axes still leaves each importance estimate wide. Treat "
        "rankings as directional, not precise." + coupled_note
    )
    lines.append(
        "- **Predicts peak Elo, not final-rung Elo.** The target is the max "
        "over each draw's 4-rung ladder, so `peak_" + rung_col + "` is an "
        "output of the search, not a controlled input -- do not read a "
        "feature's importance here as telling you which rung to train to."
    )
    lines.append(
        "- **Architecture/init scope.** Fit only on linear, from-scratch "
        "checkpoints (this cohort's only architecture). Extrapolating these "
        "axis preferences to an MLP or a warm-started init is untested."
    )
    lines.append(
        "- **Correlational.** As with every observation in the results doc "
        "this predictor is built from, nothing here isolates a single axis's "
        "causal effect. Use it to prioritize where a controlled follow-up "
        "sweep should look, not as a substitute for one."
    )
    lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="in_path", required=True, help="long-format cohort TSV")
    ap.add_argument("--group-by", default="block", help="column identifying one training run/draw")
    ap.add_argument("--target", default="elo", help="per-checkpoint result column to take the peak of")
    ap.add_argument("--rung-col", default="rung", help="checkpoint-ladder column, empty string to disable")
    ap.add_argument("--features", default=DEFAULT_FEATURES, help="comma-separated swept config columns")
    ap.add_argument("--out-report", required=True, help="markdown report path")
    ap.add_argument("--out-tree-image", default="", help="optional PNG path for a rendered tree diagram")
    ap.add_argument("--depths", default="1,2,3,4", help="candidate decision-tree max_depth values")
    ap.add_argument("--min-samples-leaf", type=int, default=8)
    ap.add_argument("--rf-trees", type=int, default=500)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    features = [f.strip() for f in args.features.split(",") if f.strip()]
    peaks = load_peaks(args.in_path, args.group_by, args.target, args.rung_col, features)
    target_col = "peak_" + args.target
    X = peaks[features]
    y = peaks[target_col]

    kf = KFold(n_splits=5, shuffle=True, random_state=args.seed)

    depths = [int(d) for d in args.depths.split(",")]
    tree_depth, depth_results = pick_tree_depth(X, y, kf, depths, args.min_samples_leaf)
    tree = DecisionTreeRegressor(
        max_depth=tree_depth, min_samples_leaf=args.min_samples_leaf, random_state=args.seed
    ).fit(X, y)
    tree_r2 = cv_r2(
        DecisionTreeRegressor(max_depth=tree_depth, min_samples_leaf=args.min_samples_leaf, random_state=args.seed),
        X, y, kf,
    )

    rf_r2 = cv_r2(RandomForestRegressor(n_estimators=args.rf_trees, random_state=args.seed), X, y, kf)
    rf = RandomForestRegressor(n_estimators=args.rf_trees, random_state=args.seed).fit(X, y)
    rf_perm_mean, rf_perm_std = oof_permutation_importance(X, y, kf, features, args.rf_trees, args.seed)

    scaler = StandardScaler().fit(X)
    Xs = pd.DataFrame(scaler.transform(X), columns=features)
    lin_r2 = cv_r2(LinearRegression(), Xs, y, kf)
    lin = LinearRegression().fit(Xs, y)

    uni_tables = univariate_tables(peaks, features, target_col)
    coupled = find_coupled_features(peaks, features)

    report = render_report(
        peaks, features, target_col, args.rung_col, kf, tree_depth, depth_results,
        tree, rf, rf_perm_mean, rf_perm_std, lin, lin_r2, tree_r2, rf_r2,
        uni_tables, args.in_path, args.out_tree_image, coupled,
    )
    with open(args.out_report, "w", encoding="utf-8") as f:
        f.write(report)
    print(f"wrote report to {args.out_report}")

    if args.out_tree_image:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from sklearn.tree import plot_tree

        fig, ax = plt.subplots(figsize=(16, 9))
        plot_tree(
            tree, feature_names=features, filled=True, rounded=True,
            precision=4, fontsize=9, ax=ax,
        )
        fig.tight_layout()
        fig.savefig(args.out_tree_image, dpi=150)
        print(f"wrote tree diagram to {args.out_tree_image}")


if __name__ == "__main__":
    main()
