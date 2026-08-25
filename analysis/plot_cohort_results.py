#!/usr/bin/env python3
"""Generate standard exploratory charts from a cohort study's results export
(tools/export_cohort_results.ps1's long-format TSV, one row per checkpoint).

Every study driver in this project (gumbelzero_*_study.ps1, tdleaf_study.ps1,
...) ends in a hand-typed markdown table in a plans/*-results-*.md doc; this
is the one step SIERRA's "generate deliverables" pipeline stage has that this
project didn't, so a study's rung ladders and swept axes can be looked at
instead of read off a table. Three chart types, written as PNGs next to
--out-dir:

  - <name>-elo-vs-<feature>.png, one per --features column: each run's peak
    target value against that swept axis, with error bars from the target's
    "<target>_pm" column when present. Numeric axes get a scatter+errorbar;
    categorical axes get a box+strip plot per category.
  - <name>-learning-curves.png: target vs --rung-col, one line per run, so a
    rung ladder's shape (still rising vs plateaued vs regressing) is visible
    at a glance. Only the top --legend-top-n runs by final-rung value get a
    legend entry; the rest draw thin and grey so the plot stays readable.
  - <name>-seed-spread.png: peak target per run, grouped by the non-seed part
    of --group-by, with individual seed points overlaid. Only written when
    "seed" is one of --group-by's columns and takes more than one value, and
    reports the median within-config spread next to the project's documented
    50-150 Elo seed-noise band (CLAUDE.md's "seed-noise band" vocabulary
    entry) so a spread can be read against the known noise floor instead of
    eyeballed.

Usage:
    python analysis/plot_cohort_results.py \\
        --in plans/gumbel-mcts-joint-sweep-agents-5-violet-harbor.tsv \\
        --group-by block --target elo --rung-col rung \\
        --features sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m \\
        --out-dir plans/

--group-by, --target, --rung-col and --features share defaults and meaning
with analysis/predict_peak_elo.py so the two tools can point at the same
export unchanged. Unlike that script, --group-by here accepts a
comma-separated list of columns (e.g. "block,seed"), needed whenever a study
seed-replicates within a block -- a single "block" column would then collapse
each seed's own peak together and understate the seed-noise band.
"""
import argparse
import os
import sys
import textwrap

import numpy as np
import pandas as pd

DEFAULT_FEATURES = "sims,lr,l2,replaycap,replaywarm,batch,open,cvisit,cscale,m"
SEED_NOISE_BAND_ELO = (50, 150)  # CLAUDE.md vocabulary: seed-noise band


def collapse_to_peak(df, group_cols, target, rung_col, features, minimize):
    """One row per run (a --group-by key): its peak (or, with minimize, its
    min) target value plus that row's error-bar and rung, and every swept
    --features value that is constant within the run. A feature that is NOT
    constant within a run means --group-by does not actually identify one
    run, so this is a hard error rather than a silently wrong plot."""
    rows = []
    for key, g in df.groupby(group_cols):
        idx = g[target].idxmin() if minimize else g[target].idxmax()
        key_tuple = key if isinstance(key, tuple) else (key,)
        row = dict(zip(group_cols, key_tuple))
        row[target] = g.loc[idx, target]
        pm_col = f"{target}_pm"
        if pm_col in g.columns:
            row[pm_col] = g.loc[idx, pm_col]
        if rung_col and rung_col in g.columns:
            row[rung_col] = g.loc[idx, rung_col]
        for f in features:
            if f not in g.columns:
                continue
            vals = g[f].unique()
            if len(vals) != 1:
                sys.exit(
                    f"feature '{f}' is not constant within group {key}: {vals} "
                    f"-- --group-by does not fully identify one run"
                )
            row[f] = vals[0]
        rows.append(row)
    return pd.DataFrame(rows)


def stem(path):
    base = os.path.basename(path)
    return base[:-4] if base.lower().endswith(".tsv") else base


def _fmt_value(v):
    """Adaptive-precision label for a discrete axis value: a mixed-scale grid
    like l2's {0, 0.0003, 0.001} rounds to indistinguishable 0.0 at a fixed
    decimal count, so format each value at its own natural precision instead."""
    return f"{v:.6g}" if isinstance(v, float) else str(v)


def plot_elo_vs_features(peaks, features, target, out_dir, name, max_discrete):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    pm_col = f"{target}_pm"
    has_pm = pm_col in peaks.columns
    written = []
    for feat in features:
        if feat not in peaks.columns:
            print(f"skip elo-vs-{feat}: column not in export", file=sys.stderr)
            continue
        fig, ax = plt.subplots(figsize=(7, 5))
        is_numeric = pd.api.types.is_numeric_dtype(peaks[feat])
        n_unique = peaks[feat].nunique(dropna=False)
        # A sweep axis is drawn from a small discrete grid far more often than
        # it is genuinely continuous (this project's hyperparameter sweeps are
        # all randomly sampled from a fixed short list per axis, e.g. l2 in
        # {0, 0.0003, 0.001}), so group by value whenever there are few enough
        # distinct values for that to be readable, numeric or not -- only a
        # numeric axis with many distinct values falls back to a scatter.
        grouped = not is_numeric or n_unique <= max_discrete
        if grouped:
            # dropna=False groupby buckets NaN (e.g. an axis that only applies
            # to some rows, like conv's FC-head width) into its own group --
            # plain `peaks[feat] == np.nan` is always False and would silently
            # drop that whole bucket instead of grouping it.
            by_value = peaks.groupby(feat, dropna=False)[target]
            real = [c for c in by_value.groups if pd.notna(c)]
            real = sorted(real) if is_numeric else sorted(real, key=str)
            categories = real + [c for c in by_value.groups if pd.isna(c)]
            labels = [_fmt_value(c) if pd.notna(c) else "(none)" for c in categories]
            data = [by_value.get_group(c).values for c in categories]
            ax.boxplot(data, tick_labels=labels, showmeans=True)
            for i, c in enumerate(categories, start=1):
                ys = by_value.get_group(c)
                xs = np.random.normal(i, 0.04, size=len(ys))
                ax.scatter(xs, ys, alpha=0.5, s=15, color="black")
        elif has_pm:
            ax.errorbar(
                peaks[feat], peaks[target], yerr=peaks[pm_col],
                fmt="o", capsize=3, alpha=0.7,
            )
        else:
            ax.scatter(peaks[feat], peaks[target], alpha=0.7)
        ax.set_xlabel(feat)
        ax.set_ylabel(target)
        ax.set_title(f"{target} vs {feat} ({len(peaks)} runs, {n_unique} distinct values)")
        fig.tight_layout()
        out_path = os.path.join(out_dir, f"{name}-elo-vs-{feat}.png")
        fig.savefig(out_path, dpi=150)
        plt.close(fig)
        written.append(out_path)
    return written


def plot_learning_curves(df, group_cols, rung_col, target, out_dir, name, legend_top_n):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    if not rung_col or rung_col not in df.columns:
        print("skip learning-curves: no rung column", file=sys.stderr)
        return None

    fig, ax = plt.subplots(figsize=(9, 6))
    runs = []
    for key, g in df.groupby(group_cols):
        g = g.sort_values(rung_col)
        label = key if isinstance(key, str) else "/".join(str(k) for k in key)
        runs.append((label, g[target].iloc[-1], g))
    runs.sort(key=lambda t: t[1], reverse=True)

    top_labels = {label for label, _, _ in runs[:legend_top_n]}
    for label, _, g in runs:
        if label in top_labels:
            ax.plot(g[rung_col], g[target], marker="o", label=label)
        else:
            ax.plot(g[rung_col], g[target], color="grey", alpha=0.25, linewidth=1)
    ax.set_xlabel(rung_col)
    ax.set_ylabel(target)
    ax.set_title(
        f"{target} per checkpoint rung, {len(runs)} runs "
        f"(top {min(legend_top_n, len(runs))} by final rung labelled)"
    )
    ax.legend(fontsize=7, loc="best", ncol=2)
    fig.tight_layout()
    out_path = os.path.join(out_dir, f"{name}-learning-curves.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    return out_path


def plot_seed_spread(peaks, group_cols, target, out_dir, name):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    if "seed" not in group_cols or peaks["seed"].nunique() <= 1:
        print("skip seed-spread: no varying 'seed' column in --group-by", file=sys.stderr)
        return None
    config_cols = [c for c in group_cols if c != "seed"]
    config_key = (
        peaks[config_cols].astype(str).agg("/".join, axis=1)
        if config_cols else pd.Series("all", index=peaks.index)
    )
    configs = sorted(config_key.unique())
    fig, ax = plt.subplots(figsize=(max(6, len(configs) * 0.6), 5))
    data = [peaks.loc[config_key == c, target].values for c in configs]
    spreads = [d.max() - d.min() for d in data if len(d) > 1]
    ax.boxplot(data, tick_labels=configs, showmeans=True)
    for i, c in enumerate(configs, start=1):
        ys = peaks.loc[config_key == c, target]
        xs = np.random.normal(i, 0.04, size=len(ys))
        ax.scatter(xs, ys, alpha=0.6, s=20, color="black")
    lo, hi = SEED_NOISE_BAND_ELO
    ax.set_xlabel("config")
    ax.set_ylabel(target)
    title = f"{target} across seeds per config"
    if spreads:
        title += f" (median within-config spread {np.median(spreads):.0f}, documented seed-noise band {lo}-{hi})"
    ax.set_title("\n".join(textwrap.wrap(title, width=max(40, len(configs) * 9))), fontsize=9)
    plt.setp(ax.get_xticklabels(), rotation=45, ha="right")
    fig.tight_layout()
    out_path = os.path.join(out_dir, f"{name}-seed-spread.png")
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    return out_path


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--in", dest="in_path", required=True, help="long-format cohort TSV")
    ap.add_argument("--group-by", default="block", help="comma-separated column(s) identifying one run, e.g. block,seed")
    ap.add_argument("--target", default="elo")
    ap.add_argument("--rung-col", default="rung", help="checkpoint-ladder column, empty string to disable learning-curves")
    ap.add_argument("--features", default=DEFAULT_FEATURES)
    ap.add_argument("--out-dir", default="", help="defaults to --in's directory")
    ap.add_argument("--legend-top-n", type=int, default=15)
    ap.add_argument(
        "--max-discrete-values", type=int, default=20,
        help="numeric axes with at most this many distinct values are grouped (box+strip) like a "
        "categorical axis instead of scattered -- this project's swept axes are almost always drawn "
        "from a small fixed grid, not genuinely continuous",
    )
    ap.add_argument(
        "--minimize", action="store_true",
        help="select each run's MIN target value instead of its max (e.g. for a cost metric like cpu_ms_move)",
    )
    args = ap.parse_args()

    df = pd.read_csv(args.in_path, sep="\t", comment="#")
    total = len(df)
    df = df[df[args.target].notna()]
    if len(df) < total:
        print(f"dropped {total - len(df)} of {total} rows with no {args.target} value (unrated checkpoints)", file=sys.stderr)

    group_cols = [c.strip() for c in args.group_by.split(",") if c.strip()]
    features = [f.strip() for f in args.features.split(",") if f.strip()]
    out_dir = args.out_dir or os.path.dirname(os.path.abspath(args.in_path))
    name = stem(args.in_path)

    peaks = collapse_to_peak(df, group_cols, args.target, args.rung_col, features, args.minimize)

    written = []
    written += plot_elo_vs_features(peaks, features, args.target, out_dir, name, args.max_discrete_values)
    lc = plot_learning_curves(df, group_cols, args.rung_col, args.target, out_dir, name, args.legend_top_n)
    if lc:
        written.append(lc)
    ss = plot_seed_spread(peaks, group_cols, args.target, out_dir, name)
    if ss:
        written.append(ss)

    for w in written:
        print(f"wrote {w}")


if __name__ == "__main__":
    main()
