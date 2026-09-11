"""Stage 1 analysis for the cross-technique replication study.

Plan: plans/replication-study-plan-1-brass-lectern.md. Each arm differs from
the baseline B0 in one training switch, every run is rated at every rung
against a frozen panel, and this script turns those ratings into the plan's
outcome measures. Run from the repo root. numpy is the only dependency.

Subcommands:

  analyze      --in <runs.tsv> [--baseline B0] [--pair A3:A2 ...]
               [--margin 30] [--alpha 0.05] [--boot 4000] [--seed 1]
      Input columns (tab separated, '#' lines ignored):
        arm, seed, rung (integer, the matched-compute rung index), cpu_s
        (realized training CPU seconds at that rung), elo, se, and optionally
        elo_a, elo_b (the same agent rated on each half of its panel games,
        for the split-half peak).
      Prints, in this order: Elo levels per arm per rung (mean over seeds with
      a 95% bootstrap interval, and every seed's value), then contrasts
      against the baseline at the final rung with Dunnett-adjusted intervals
      and p-values, bootstrap intervals, and the equivalence verdict (two
      one-sided tests at +/- margin), then the pre-registered arm-vs-arm
      pairs, then area under the learning curve, compute multiplier,
      split-half peak, plateau, and the speed-vs-ceiling curve fit.

  export       --manifest <tsv> --standings <pinned standings tsv>
               [--half-a <tsv> --half-b <tsv>] --out <runs.tsv>
      Builds analyze's input. Manifest columns: id, arm, seed, rung, model
      (the checkpoint file). Elo and SE come from the pinned fit (its `pm`
      column is one standard error), cpu_s from the checkpoint's provenance
      stamp (`cpu=`, written by src/train_budget.cpp).

  verify-store --store <jsonl> --cohort <ids.txt>
      Reads a study match store back and checks the evaluation design: every
      cohort-vs-panel couple is two games with the colours swapped on one seed
      (paired openings), and every cohort agent met each panel opponent on the
      same seeds (common openings, `rank.exe play --common-openings`).

  selftest
      Generates synthetic runs with known effects and checks that analyze
      recovers them. Exits non-zero on any miss.

Definitions, fixed by the plan:
  * The primary endpoint is Elo at the final rung, which every arm reaches at
    matched compute because rungs are calibrated game counts.
  * Dunnett's procedure compares k arms to one control. Its critical value is
    computed here by simulation from the design's own seed counts and pooled
    degrees of freedom, so no table lookup is involved.
  * Area under the learning curve (AULC) is the mean Elo over the log2(cpu_s)
    range, trapezoidal, per seed.
  * The compute multiplier is the compute an arm needs to reach the baseline's
    final-rung Elo, interpolated linearly in (log2 cpu_s, Elo) on the arm's
    mean curve, divided by the baseline's final-rung compute. Censored as
    "> max" when the arm never gets there.
  * The curve model is Elo = L_arm - B_arm * exp(-k * log2(cpu_s)) with one k
    shared by all arms. Speed gain is the horizontal shift from the baseline in
    doublings of compute, ln(B_0 / B_arm) / k, and ceiling gain is L_arm - L_0.
"""
import argparse
import csv
import json
import math
import re
import sys

import numpy as np


# ------------------------------------------------------------------ loading
def read_tsv(path):
    with open(path, encoding="utf-8") as f:
        lines = [l for l in f if l.strip() and not l.startswith("#")]
    return list(csv.DictReader(lines, delimiter="\t"))


def load_runs(path):
    rows = []
    for r in read_tsv(path):
        row = {"arm": r["arm"], "seed": r["seed"], "rung": int(r["rung"]),
               "cpu_s": float(r["cpu_s"]), "elo": float(r["elo"]), "se": float(r["se"])}
        for k in ("elo_a", "elo_b"):
            if r.get(k, "") not in ("", None):
                row[k] = float(r[k])
        rows.append(row)
    return rows


def by_arm_seed(rows):
    """arm -> seed -> {rung: row}"""
    out = {}
    for r in rows:
        out.setdefault(r["arm"], {}).setdefault(r["seed"], {})[r["rung"]] = r
    return out


# ------------------------------------------------------------------ statistics
def boot_mean_ci(vals, rng, nboot, level=0.95):
    vals = np.asarray(vals, float)
    if len(vals) < 2:
        return float("nan"), float("nan")
    idx = rng.integers(0, len(vals), size=(nboot, len(vals)))
    means = vals[idx].mean(axis=1)
    lo, hi = np.quantile(means, [(1 - level) / 2, 1 - (1 - level) / 2])
    return float(lo), float(hi)


def dunnett_null(ns_arm, n0, df, sims, rng):
    """Simulated max |T| over k arm-vs-control contrasts under the null, with a
    pooled variance on df degrees of freedom."""
    k = len(ns_arm)
    z0 = rng.standard_normal(sims) / math.sqrt(n0)
    s = np.sqrt(rng.chisquare(df, sims) / df)
    mx = np.zeros(sims)
    for n in ns_arm:
        zi = rng.standard_normal(sims) / math.sqrt(n)
        t = np.abs(zi - z0) / math.sqrt(1.0 / n + 1.0 / n0) / s
        mx = np.maximum(mx, t)
    return mx


def t_quantile(p, df, rng, sims=400000):
    """Student t quantile by simulation (numpy has no t inverse CDF)."""
    t = rng.standard_normal(sims) / np.sqrt(rng.chisquare(df, sims) / df)
    return float(np.quantile(t, p))


def final_values(ras, rung):
    return [s[rung]["elo"] for s in ras.values() if rung in s]


def aulc(seedrows):
    rungs = sorted(seedrows)
    if len(rungs) < 2:
        return float("nan")
    x = np.log2([seedrows[r]["cpu_s"] for r in rungs])
    y = np.array([seedrows[r]["elo"] for r in rungs])
    span = x[-1] - x[0]
    if span <= 0:
        return float(y.mean())
    return float(np.trapezoid(y, x) / span) if hasattr(np, "trapezoid") else float(np.trapz(y, x) / span)


def mean_curve(ras, rungs):
    xs, ys = [], []
    for r in rungs:
        pts = [s[r] for s in ras.values() if r in s]
        if not pts:
            continue
        xs.append(float(np.mean(np.log2([p["cpu_s"] for p in pts]))))
        ys.append(float(np.mean([p["elo"] for p in pts])))
    return xs, ys


def crossing(xs, ys, target):
    """First x where the piecewise-linear curve reaches target, or None."""
    if not xs:
        return None
    if ys[0] >= target:
        return xs[0]
    for i in range(1, len(xs)):
        if ys[i] >= target > ys[i - 1]:
            f = (target - ys[i - 1]) / (ys[i] - ys[i - 1])
            return xs[i - 1] + f * (xs[i] - xs[i - 1])
    return None


def multiplier(ras_arm, ras_base, rungs, tmax):
    bx, by = mean_curve(ras_base, rungs)
    target = by[-1]
    ax, ay = mean_curve(ras_arm, rungs)
    xc = crossing(ax, ay, target)
    if xc is None:
        return None
    return 2.0 ** (xc - bx[-1])


def split_half_peak(seedrows):
    rungs = sorted(r for r in seedrows if "elo_a" in seedrows[r] and "elo_b" in seedrows[r])
    if not rungs:
        return float("nan")
    pa = max(rungs, key=lambda r: seedrows[r]["elo_a"])
    pb = max(rungs, key=lambda r: seedrows[r]["elo_b"])
    return 0.5 * (seedrows[pa]["elo_b"] + seedrows[pb]["elo_a"])


# ------------------------------------------------------------------ curve model
def fit_curves(data, arms, rungs, kgrid=None):
    """Least squares for Elo = L_arm - B_arm * exp(-k x), x = log2 cpu_s, one
    shared k found by grid search, (L, B) per arm solved linearly."""
    if kgrid is None:
        kgrid = np.exp(np.linspace(math.log(0.03), math.log(4.0), 120))
    pts = {a: [(math.log2(s[r]["cpu_s"]), s[r]["elo"])
               for s in data[a].values() for r in rungs if r in s] for a in arms}
    x0 = min(x for a in arms for x, _ in pts[a])
    best = None
    for k in kgrid:
        sse, params = 0.0, {}
        for a in arms:
            x = np.array([p[0] for p in pts[a]]) - x0
            y = np.array([p[1] for p in pts[a]])
            X = np.column_stack([np.ones_like(x), -np.exp(-k * x)])
            coef, *_ = np.linalg.lstsq(X, y, rcond=None)
            sse += float(((X @ coef - y) ** 2).sum())
            params[a] = (float(coef[0]), float(coef[1]))
        if best is None or sse < best[0]:
            best = (sse, float(k), params)
    return best[1], best[2]


def speed_ceiling(k, params, arm, base):
    L0, B0 = params[base]
    L, B = params[arm]
    speed = math.log(B0 / B) / k if (B > 0 and B0 > 0) else float("nan")
    return speed, L - L0


# ------------------------------------------------------------------ analyze
def analyze(rows, baseline="B0", pairs=(), margin=30.0, alpha=0.05, nboot=4000, seed=1,
            out=print):
    rng = np.random.default_rng(seed)
    data = by_arm_seed(rows)
    if baseline not in data:
        raise SystemExit("baseline arm %r not in the input" % baseline)
    arms = [baseline] + sorted(a for a in data if a != baseline)
    others = arms[1:]
    rungs = sorted({r["rung"] for r in rows})
    tmax = max(r for r in rungs if all(any(r in s for s in data[a].values()) for a in arms))
    res = {"tmax": tmax, "contrast": {}, "pair": {}, "aulc": {}, "mult": {}, "curve": {}}

    out("== Elo levels by arm and rung (mean over seeds [95% bootstrap interval]; n seeds)")
    head = "arm".ljust(14) + "".join(("rung %d" % r).rjust(26) for r in rungs)
    out(head)
    for a in arms:
        line = a.ljust(14)
        for r in rungs:
            v = final_values(data[a], r)
            if not v:
                line += "-".rjust(26)
                continue
            lo, hi = boot_mean_ci(v, rng, nboot)
            line += ("%.0f [%.0f, %.0f] n=%d" % (np.mean(v), lo, hi, len(v))).rjust(26)
        out(line)
    out("")
    out("== Per-seed Elo at the final rung %d" % tmax)
    for a in arms:
        v = sorted(final_values(data[a], tmax))
        out("%-14s %s" % (a, " ".join("%.0f" % x for x in v)))
    out("")
    out("== Mean realized training CPU seconds per rung")
    for a in arms:
        xs = []
        for r in rungs:
            c = [s[r]["cpu_s"] for s in data[a].values() if r in s]
            xs.append("%.0f" % np.mean(c) if c else "-")
        out("%-14s %s" % (a, "  ".join(xs)))
    out("")

    # ---- contrasts against the baseline, Dunnett over the arms
    v0 = np.array(final_values(data[baseline], tmax))
    finals = {a: np.array(final_values(data[a], tmax)) for a in others}
    n_all = len(v0) + sum(len(v) for v in finals.values())
    df = n_all - len(arms)
    ss = ((v0 - v0.mean()) ** 2).sum() + sum(((v - v.mean()) ** 2).sum() for v in finals.values())
    sp = math.sqrt(ss / df) if df > 0 else float("nan")
    null = dunnett_null([len(finals[a]) for a in others], len(v0), max(df, 1), 200000, rng)
    crit = float(np.quantile(null, 1 - alpha))
    t90 = t_quantile(0.95, max(df, 1), rng)
    out("== Contrasts at rung %d against %s (Dunnett over %d arms, pooled SD %.1f on %d df,"
        " critical |t| %.3f)" % (tmax, baseline, len(others), sp, df, crit))
    out("%-14s %8s %7s %22s %9s %22s %14s" % ("arm", "diff", "SE", "Dunnett CI", "adj p",
                                             "bootstrap 95% CI", "+/-%.0f TOST" % margin))
    for a in others:
        va = finals[a]
        d = float(va.mean() - v0.mean())
        se = sp * math.sqrt(1.0 / len(va) + 1.0 / len(v0))
        t = abs(d) / se if se > 0 else float("inf")
        p = float((null >= t).mean())
        lo, hi = d - crit * se, d + crit * se
        idx_a = rng.integers(0, len(va), size=(nboot, len(va)))
        idx_0 = rng.integers(0, len(v0), size=(nboot, len(v0)))
        bd = va[idx_a].mean(axis=1) - v0[idx_0].mean(axis=1)
        blo, bhi = np.quantile(bd, [0.025, 0.975])
        e_lo, e_hi = d - t90 * se, d + t90 * se
        if e_lo > -margin and e_hi < margin:
            verdict = "equivalent"
        elif lo > 0 or hi < 0:
            verdict = "different"
        else:
            verdict = "inconclusive"
        res["contrast"][a] = {"diff": d, "se": se, "lo": lo, "hi": hi, "p": p,
                              "blo": float(blo), "bhi": float(bhi), "verdict": verdict}
        out("%-14s %8.1f %7.1f %22s %9.4f %22s %14s" % (
            a, d, se, "[%.1f, %.1f]" % (lo, hi), p, "[%.1f, %.1f]" % (blo, bhi), verdict))
    out("")

    # ---- pre-registered arm-vs-arm pairs (not part of the Dunnett family)
    if pairs:
        out("== Pre-registered pairs at rung %d (Welch, unadjusted)" % tmax)
        for spec in pairs:
            x, y = spec.split(":")
            vx = np.array(final_values(data[x], tmax))
            vy = np.array(final_values(data[y], tmax))
            d = float(vx.mean() - vy.mean())
            se = math.sqrt(vx.var(ddof=1) / len(vx) + vy.var(ddof=1) / len(vy))
            res["pair"][spec] = (d, se)
            out("%-14s diff %.1f  SE %.1f  95%% CI [%.1f, %.1f]" % (
                spec, d, se, d - 1.96 * se, d + 1.96 * se))
        out("")

    # ---- area under the learning curve
    out("== Area under the learning curve (mean Elo over the log2 CPU range, per seed)")
    au = {a: np.array([aulc(s) for s in data[a].values()]) for a in arms}
    for a in arms:
        lo, hi = boot_mean_ci(au[a], rng, nboot)
        line = "%-14s %.0f [%.0f, %.0f]" % (a, au[a].mean(), lo, hi)
        if a != baseline:
            d = au[a].mean() - au[baseline].mean()
            line += "   vs %s %+.0f" % (baseline, d)
            res["aulc"][a] = float(d)
        out(line)
    out("")

    # ---- compute multiplier
    out("== Compute multiplier: compute to reach %s's rung-%d Elo, over %s's rung-%d compute"
        % (baseline, tmax, baseline, tmax))
    seeds_b = list(data[baseline].values())
    for a in others:
        m = multiplier(data[a], data[baseline], rungs, tmax)
        seeds_a = list(data[a].values())
        boots, cens = [], 0
        for _ in range(min(nboot, 1000)):
            ra = {i: seeds_a[j] for i, j in enumerate(rng.integers(0, len(seeds_a), len(seeds_a)))}
            rb = {i: seeds_b[j] for i, j in enumerate(rng.integers(0, len(seeds_b), len(seeds_b)))}
            mb = multiplier(ra, rb, rungs, tmax)
            if mb is None:
                cens += 1
            else:
                boots.append(mb)
        res["mult"][a] = m
        ms = "> max" if m is None else "%.3f" % m
        ci = ("[%.3f, %.3f]" % tuple(np.quantile(boots, [0.025, 0.975]))) if len(boots) > 10 else "-"
        out("%-14s %8s   bootstrap %s, censored in %d of %d resamples" % (
            a, ms, ci, cens, min(nboot, 1000)))
    out("")

    # ---- split-half peak and plateau
    have_halves = any("elo_a" in r for r in rows)
    if have_halves:
        out("== Split-half peak (peak rung picked on one half of the games, read on the other)")
        for a in arms:
            v = np.array([split_half_peak(s) for s in data[a].values()])
            v = v[~np.isnan(v)]
            if len(v):
                lo, hi = boot_mean_ci(v, rng, nboot)
                out("%-14s %.0f [%.0f, %.0f]" % (a, v.mean(), lo, hi))
        out("")
    out("== Plateau: last two rungs differ by less than one combined SE of the mean")
    for a in arms:
        r1, r2 = rungs[-2], rungs[-1]
        v1, v2 = np.array(final_values(data[a], r1)), np.array(final_values(data[a], r2))
        if len(v1) < 2 or len(v2) < 2:
            out("%-14s -" % a)
            continue
        cse = math.sqrt(v1.var(ddof=1) / len(v1) + v2.var(ddof=1) / len(v2))
        diff = v2.mean() - v1.mean()
        out("%-14s rung %d - rung %d = %+.0f (combined SE %.0f): %s" % (
            a, r2, r1, diff, cse, "plateau %.0f" % v2.mean() if abs(diff) < cse else "not converged"))
    out("")

    # ---- speed vs ceiling
    k, params = fit_curves(data, arms, rungs)
    out("== Curve fit Elo = L - B exp(-k log2 cpu), shared k = %.3f" % k)
    out("%-14s %8s %9s %24s %24s %10s" % ("arm", "L", "B", "speed (doublings) CI", "ceiling CI", "kind"))
    boot_sc = {a: [] for a in others}
    for _ in range(min(nboot, 300)):
        bdata = {}
        for a in arms:
            ss_ = list(data[a].values())
            bdata[a] = {i: ss_[j] for i, j in enumerate(rng.integers(0, len(ss_), len(ss_)))}
        kb, pb = fit_curves(bdata, arms, rungs, kgrid=np.exp(np.linspace(math.log(0.03), math.log(4.0), 40)))
        for a in others:
            boot_sc[a].append(speed_ceiling(kb, pb, a, baseline))
    out("%-14s %8.0f %9.1f" % (baseline, params[baseline][0], params[baseline][1]))
    for a in others:
        sp_, ce = speed_ceiling(k, params, a, baseline)
        b = np.array(boot_sc[a], float)
        b = b[~np.isnan(b).any(axis=1)]
        if len(b) > 10:
            slo, shi = np.quantile(b[:, 0], [0.025, 0.975])
            clo, chi = np.quantile(b[:, 1], [0.025, 0.975])
            s_sig = slo > 0 or shi < 0
            c_sig = clo > 0 or chi < 0
            kind = {(True, False): "speed", (False, True): "ceiling",
                    (True, True): "both", (False, False): "neither"}[(s_sig, c_sig)]
            sci, cci = "%+.2f [%+.2f, %+.2f]" % (sp_, slo, shi), "%+.0f [%+.0f, %+.0f]" % (ce, clo, chi)
        else:
            kind, sci, cci = "n/a", "%+.2f" % sp_, "%+.0f" % ce
        res["curve"][a] = {"speed": sp_, "ceiling": ce, "kind": kind}
        out("%-14s %8.0f %9.1f %24s %24s %10s" % (a, params[a][0], params[a][1], sci, cci, kind))
    return res


# ------------------------------------------------------------------ export
def teacher_of(path):
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.startswith("teacher="):
                return line[len("teacher="):].strip()
    return ""


def stamp_number(teacher, key):
    m = re.search(r"(?<![A-Za-z0-9_])" + re.escape(key) + r"=([0-9.eE+-]+)", teacher)
    return float(m.group(1)) if m else None


def export(manifest, standings, out_path, half_a=None, half_b=None):
    st = {r["id"]: r for r in read_tsv(standings)}
    ha = {r["id"]: r for r in read_tsv(half_a)} if half_a else {}
    hb = {r["id"]: r for r in read_tsv(half_b)} if half_b else {}
    cols = ["arm", "seed", "rung", "games", "cpu_s", "elo", "se"] + (["elo_a", "elo_b"] if ha else [])
    missing = []
    with open(out_path, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(cols)
        for r in read_tsv(manifest):
            s = st.get(r["id"])
            t = teacher_of(r["model"])
            cpu = stamp_number(t, "cpu")
            if s is None or cpu is None:
                missing.append(r["id"])
                continue
            row = [r["arm"], r["seed"], r["rung"], int(stamp_number(t, "games") or 0), cpu,
                   s["elo"], s["pm"]]
            if ha:
                row += [ha[r["id"]]["elo"], hb[r["id"]]["elo"]]
            w.writerow(row)
    if missing:
        print("WARNING: %d manifest rows had no rating or no cpu= stamp: %s"
              % (len(missing), ", ".join(missing[:5])))
    return missing


# ------------------------------------------------------------------ verify-store
def store_files(store):
    """The files rank.exe reads for a store: the parts its <stem>.index.txt
    lists (relative to the store's directory), then the live tail."""
    import os
    stem = store[:-len(".jsonl")] if store.endswith(".jsonl") else store
    files = []
    if os.path.exists(stem + ".index.txt"):
        d = os.path.dirname(store)
        with open(stem + ".index.txt", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    files.append(os.path.join(d, line))
    files.append(store)
    return [p for p in files if os.path.exists(p)]


def verify_store(store, cohort_ids):
    cohort = set(cohort_ids)
    games = {}   # (cohort agent, panel agent) -> list of (seed, cohort agent is white)
    for path in store_files(store):
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                g = json.loads(line)
                if g.get("t", "g") != "g":
                    continue
                w, b = g["w"], g["b"]
                if (w in cohort) == (b in cohort):
                    continue
                x, p = (w, b) if w in cohort else (b, w)
                games.setdefault((x, p), []).append((g["seed"], w == x))
    bad_couples, couples = 0, 0
    per_panel = {}
    for (x, p), lst in games.items():
        seeds = {}
        for sd, xw in lst:
            seeds.setdefault(sd, []).append(xw)
        for sd, cols in seeds.items():
            couples += 1
            if sorted(cols) != [False, True]:
                bad_couples += 1
        per_panel.setdefault(p, {})[x] = set(seeds)
    crn_bad = 0
    for p, m in per_panel.items():
        sets = list(m.values())
        common = set.intersection(*sets)
        for s in sets:
            if s != common:
                crn_bad += 1
    print("pairs %d, couples %d, couples not exactly one game per colour %d"
          % (len(games), couples, bad_couples))
    print("panel opponents %d, cohort agents whose seed set differs from the others' %d"
          % (len(per_panel), crn_bad))
    return bad_couples == 0 and crn_bad == 0 and couples > 0


# ------------------------------------------------------------------ selftest
TRUE = {  # arm -> (ceiling shift, speed shift in doublings, constant offset)
    "B0": (0.0, 0.0, 0.0),
    "NULL": (0.0, 0.0, 0.0),
    "CEIL": (80.0, 0.0, 0.0),
    "SPEED": (0.0, 2.0, 0.0),
    "NEG": (0.0, 0.0, -60.0),
}


def true_elo(arm, x):
    c, s, o = TRUE[arm]
    return 1000.0 + c + o - 600.0 * math.exp(-0.5 * (x + s - 10.0))


def synth(seed=7, n_arm=20, n_base=40, sd_seed=30.0, sd_meas=15.0):
    rng = np.random.default_rng(seed)
    rows = []
    for arm in TRUE:
        n = n_base if arm == "B0" else n_arm
        for sd in range(n):
            u = rng.normal(0, sd_seed)
            for rung, x in enumerate(range(10, 16)):
                xr = x + rng.normal(0, 0.03)
                t = true_elo(arm, xr)
                rows.append({"arm": arm, "seed": "%s_%d" % (arm, sd), "rung": rung,
                             "cpu_s": 2.0 ** xr, "elo": t + u + rng.normal(0, sd_meas),
                             "se": sd_meas,
                             "elo_a": t + u + rng.normal(0, sd_meas * math.sqrt(2)),
                             "elo_b": t + u + rng.normal(0, sd_meas * math.sqrt(2))})
    return rows


def selftest():
    rows = synth()
    lines = []
    res = analyze(rows, baseline="B0", pairs=["CEIL:SPEED"], seed=3, nboot=2000,
                  out=lines.append)
    print("\n".join(lines))
    fails = []

    def check(ok, what):
        print(("PASS " if ok else "FAIL ") + what)
        if not ok:
            fails.append(what)

    c = res["contrast"]
    xt = 15.0
    for arm in ("NULL", "CEIL", "SPEED", "NEG"):
        truth = true_elo(arm, xt) - true_elo("B0", xt)
        check(c[arm]["lo"] <= truth <= c[arm]["hi"],
              "%s: Dunnett CI [%.1f, %.1f] covers the true final-rung effect %.1f"
              % (arm, c[arm]["lo"], c[arm]["hi"], truth))
    check(c["NULL"]["verdict"] == "equivalent", "NULL is reported equivalent (+/-30)")
    check(c["CEIL"]["verdict"] == "different" and c["CEIL"]["diff"] > 0, "CEIL is a positive difference")
    check(c["NEG"]["verdict"] == "different" and c["NEG"]["diff"] < 0, "NEG is a negative difference")
    check(abs(res["aulc"]["CEIL"] - 80.0) < 20.0, "CEIL AULC gain %.1f is near 80" % res["aulc"]["CEIL"])
    m = res["mult"]["SPEED"]
    check(m is not None and 0.18 <= m <= 0.35, "SPEED compute multiplier %s is near 0.25" % m)
    check(res["mult"]["NEG"] is None, "NEG never reaches the baseline: multiplier censored")
    cv = res["curve"]
    check(cv["SPEED"]["kind"] == "speed", "SPEED classified as %s" % cv["SPEED"]["kind"])
    check(abs(cv["SPEED"]["speed"] - 2.0) < 0.5, "SPEED shift %.2f doublings is near 2" % cv["SPEED"]["speed"])
    check(cv["CEIL"]["kind"] == "ceiling", "CEIL classified as %s" % cv["CEIL"]["kind"])
    check(abs(cv["CEIL"]["ceiling"] - 80.0) < 25.0, "CEIL ceiling gain %.1f is near 80" % cv["CEIL"]["ceiling"])
    check(cv["NULL"]["kind"] == "neither", "NULL classified as %s" % cv["NULL"]["kind"])

    # Type I error of the Dunnett family over null replicates: with every arm
    # equal to the baseline, the family should reject at most about alpha.
    rej = 0
    reps = 60
    for r in range(reps):
        saved = dict(TRUE)
        for a in ("CEIL", "SPEED", "NEG"):
            TRUE[a] = (0.0, 0.0, 0.0)
        rr = synth(seed=100 + r)
        TRUE.update(saved)
        q = []
        res0 = analyze(rr, baseline="B0", seed=r, nboot=200, out=q.append)
        if any(v["p"] < 0.05 for v in res0["contrast"].values()):
            rej += 1
    check(rej / reps <= 0.12, "family-wise false positive rate %d/%d under the null" % (rej, reps))

    # verify-store on a synthetic store with paired common openings, then a broken one.
    import os
    import tempfile
    d = tempfile.mkdtemp()
    good, bad = os.path.join(d, "good.jsonl"), os.path.join(d, "bad.jsonl")
    with open(good, "w") as f, open(bad, "w") as g:
        for p in ("P1", "P2"):
            for x in ("X1", "X2"):
                for k in range(3):
                    sd = hash((p, k)) & 0xffffffff
                    for xw in (True, False):
                        row = {"t": "g", "w": x if xw else p, "b": p if xw else x, "r": "W", "seed": sd}
                        f.write(json.dumps(row) + "\n")
                        if not (x == "X2" and k == 2):
                            g.write(json.dumps(row) + "\n")
                        elif xw:
                            row["seed"] = sd + 1
                            g.write(json.dumps(row) + "\n")
    check(verify_store(good, ["X1", "X2"]) is True, "verify-store accepts paired common openings")
    check(verify_store(bad, ["X1", "X2"]) is False, "verify-store rejects a broken couple")

    print("\nselftest: %d failure(s)" % len(fails))
    return 1 if fails else 0


# ------------------------------------------------------------------ main
def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("analyze")
    a.add_argument("--in", dest="inp", required=True)
    a.add_argument("--baseline", default="B0")
    a.add_argument("--pair", action="append", default=[])
    a.add_argument("--margin", type=float, default=30.0)
    a.add_argument("--alpha", type=float, default=0.05)
    a.add_argument("--boot", type=int, default=4000)
    a.add_argument("--seed", type=int, default=1)
    e = sub.add_parser("export")
    e.add_argument("--manifest", required=True)
    e.add_argument("--standings", required=True)
    e.add_argument("--half-a")
    e.add_argument("--half-b")
    e.add_argument("--out", required=True)
    v = sub.add_parser("verify-store")
    v.add_argument("--store", required=True)
    v.add_argument("--cohort", required=True)
    sub.add_parser("selftest")
    args = ap.parse_args(argv)
    if args.cmd == "analyze":
        analyze(load_runs(args.inp), args.baseline, args.pair, args.margin, args.alpha,
                args.boot, args.seed)
        return 0
    if args.cmd == "export":
        return 1 if export(args.manifest, args.standings, args.out, args.half_a, args.half_b) else 0
    if args.cmd == "verify-store":
        ids = [l.split()[-1] for l in open(args.cohort, encoding="utf-8")
               if l.strip() and not l.startswith("#")]
        return 0 if verify_store(args.store, ids) else 1
    return selftest()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
