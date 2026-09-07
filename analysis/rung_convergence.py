"""Evaluate the pre-registered convergence test between two ladder rungs.

The rule it checks is written down in plans/track-migration-plan-1-slate-kestrel.md
BEFORE any rung was read. This script only applies it, so that the decision to
stop doubling is a comparison against a fixed number rather than a judgment made
while looking at the answer.

Usage, after each rung's pinned fit:

    cp ranking/cores_pinned.tsv ranking/rungs/cores_r<N>.tsv
    python analysis/rung_convergence.py ranking/rungs/cores_r8.tsv \
                                        ranking/rungs/cores_r16.tsv

Prints the three criteria and a STOP / DOUBLE AGAIN verdict. A rung below 32
games/pair can never print STOP: conclusions on this project have twice inverted
between 8 and 32 games/pair, so 32 is a floor rather than something the
diagnostics may override.
"""
import csv
import math
import sys

FLOOR_GAMES_PER_PAIR = 32


def load(path):
    """core -> {cell: (elo, pm, games)}, plus the mean-elo column."""
    rows = [l for l in open(path, encoding="utf-8") if not l.startswith("#")]
    r = csv.DictReader(rows, delimiter="\t")
    cells = [c[:-4] for c in r.fieldnames if c.endswith("_elo")]
    out, mean = {}, {}
    for row in r:
        core = row["core"]
        mean[core] = float(row["mean_elo"])
        out[core] = {}
        for c in cells:
            if row.get(c + "_elo"):
                out[core][c] = (float(row[c + "_elo"]),
                                float(row[c + "_pm"]),
                                int(row[c + "_games"]))
    return out, mean, cells


def spearman(a, b):
    """Rank correlation over the keys both dicts share."""
    keys = sorted(set(a) & set(b))
    ra = {k: i for i, k in enumerate(sorted(keys, key=lambda k: -a[k]))}
    rb = {k: i for i, k in enumerate(sorted(keys, key=lambda k: -b[k]))}
    n = len(keys)
    if n < 3:
        return 1.0, 0
    d2 = sum((ra[k] - rb[k]) ** 2 for k in keys)
    rho = 1.0 - 6.0 * d2 / (n * (n * n - 1))
    worst = max(abs(ra[k] - rb[k]) for k in keys)
    return rho, worst


def champion(tab, cell):
    best, bestElo = None, -1e18
    for core, cells in tab.items():
        if cell in cells and cells[cell][0] > bestElo:
            best, bestElo = core, cells[cell][0]
    return best, bestElo


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    prev, prevMean, cells = load(sys.argv[1])
    cur, curMean, _ = load(sys.argv[2])

    # --- 1. order stability -------------------------------------------------
    rho, worst = spearman(prevMean, curMean)
    ok1 = rho >= 0.99 and worst <= 3
    print("1. order stable        rho=%.4f (need >= 0.9900), worst rank move=%d (need <= 3)  %s"
          % (rho, worst, "PASS" if ok1 else "FAIL"))

    # --- 2. champion stability ---------------------------------------------
    ok2 = True
    print("2. champions stable")
    for c in cells:
        pc, pe = champion(prev, c)
        cc, ce = champion(cur, c)
        if pc is None or cc is None:
            print("   %-20s no champion in one rung" % c)
            ok2 = False
            continue
        se = math.hypot(prev[pc][c][1], cur[cc][c][1])
        same = (pc == cc)
        moved = abs(ce - pe)
        cellOk = same and moved < se
        ok2 = ok2 and cellOk
        print("   %-20s %-8s d=%5.1f vs 1 SE %5.1f  %s%s"
              % (c, "same" if same else "CHANGED", moved, se,
                 "PASS" if cellOk else "FAIL",
                 "" if same else ("  (%s -> %s)" % (pc, cc))))

    # --- 3. error bars falling like independent samples ---------------------
    def medianPm(tab):
        v = sorted(x[1] for cells_ in tab.values() for x in cells_.values())
        return v[len(v) // 2] if v else float("nan")

    mp, mc = medianPm(prev), medianPm(cur)
    ratio = mp / mc if mc else float("nan")
    ok3 = 1.30 <= ratio <= 1.55
    print("3. pm falling like sqrt(2)   median pm %.1f -> %.1f, ratio %.2f "
          "(need 1.30-1.55)  %s" % (mp, mc, ratio, "PASS" if ok3 else "FAIL"))
    if not ok3 and ratio < 1.30:
        print("   A shallow fall means the extra games are largely replays, not new")
        print("   information (Docs/benchmarking.md defect 3). Investigate rather than")
        print("   doubling again: more of the same replay does not fix it.")

    # --- floor --------------------------------------------------------------
    gs = sorted(x[2] for cells_ in cur.values() for x in cells_.values())
    minGames = gs[0] if gs else 0
    print("\nminimum games on any cell: %d" % minGames)

    if ok1 and ok2 and ok3:
        print("VERDICT: criteria met.")
        print("         (the floor is a rung of >= %d games/pair, which this script"
              % FLOOR_GAMES_PER_PAIR)
        print("          cannot read from cores.tsv -- confirm the rung before STOP)")
    else:
        print("VERDICT: DOUBLE AGAIN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
