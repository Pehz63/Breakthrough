"""Build the roster + cohort for the wall-clock ladder study.

The question: what is a reasonable ms bound for the time-normalized track?
The criterion the developer chose is the KNEE of Elo against realized ms, the
cheapest bound past which more time stops buying strength.

Design (settled 2026-09-06, revised the same day):
  4 cores x {25, 50, 100, 200, 400, 800, 1600ms}, RETAIN ONLY = 28 cells.

RETAIN ONLY, on developer instruction after the first pass measured it. `retain`
raises a `time=` head's realized spend from 0.432 of its flag (sd 0.053) to 0.854
(sd 0.028), so a plain head under-delivers its own budget by more than half and
does so less predictably. A track whose flag does not describe the spend is not a
wall-clock instrument, so the plain condition is no longer carried. Theory 70.

The first pass's plain cells are NOT deleted from the store. They are benched
`off` in the roster, so their games remain on record and can be re-rated, but no
fit places them beside the retain cells as if the comparison were still open.

The ladder is extended to 1600ms because the first pass did not reach the knee:
`position_elo mlp model=113` still gained +41 Elo in the 200 -> 400ms doubling
under `retain`, and `pool_games lin model=97` had not settled either. Without
those rungs the study's own criterion is unmet and no bound is defensible.

Four cores, not the node grid's five: `tdleaf_self lin model=349` is dropped as
a near-duplicate of `model=169` (same regime, same architecture, and the two
tracked each other across all 40 cells of the node study).

Known limitation, accepted when the ladder was chosen: `position_elo mlp
model=113` costs 252-1431 ms/move in the node grid, so it may still be climbing
at the top rung and show no knee. If it does not, say so rather than fitting one.

Usage:
    python tools/make_time_ladder_roster.py [--abver 3] [--base ranking/roster.txt]

Writes ranking/q7/roster_timeladder.txt and ranking/q7/cohort_timeladder.txt.
Pass --abver 3 after the ab explorer's @2 -> @3 bump; until then the ids must
match whatever version the live roster uses or `rank.exe check` rejects them.
"""
import argparse, os

CORES = [
    ("chip counter",         "classic(chip=100)@2"),
    ("tdleaf_self lin 169",  "learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1"),
    ("position_elo mlp 113", "learned(model=113,e3cc8b4e,position_elo,mlp,"
                             "mu_shape=129-512-8-1,sigma_shape=129-64-1)@1"),
    ("pool_games lin 97",    "learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1"),
]
RUNGS = [25, 50, 100, 200, 400, 800, 1600]
# Rungs the superseded plain condition actually played, first pass 2026-09-06.
# Only these get an `off` bench line: a plain twin at a rung that never ran has
# no games to retire and listing it would imply otherwise.
PLAYED_PLAIN = [25, 50, 100, 200, 400]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--abver", type=int, default=2,
                    help="ab explorer code version (3 after the time= enforcement bump)")
    ap.add_argument("--base", default="ranking/roster.txt")
    ap.add_argument("--outdir", default="ranking/q7")
    a = ap.parse_args()

    want, bench = [], []
    for ms in RUNGS:
        for _, core in CORES:
            want.append("ab(deep=12,tt,ord,retain,time=%dms)@%d.%s" % (ms, a.abver, core))
            # The superseded plain twin, benched rather than dropped so its games
            # stay on record without entering a fit alongside the retain cells.
            if ms in PLAYED_PLAIN:
                bench.append("ab(deep=12,tt,ord,time=%dms)@%d.%s" % (ms, a.abver, core))
    benchset = set(bench)

    base = open(a.base, encoding="utf-8").read().rstrip("\n").split("\n")
    present = set()
    out = []
    for ln in base:
        body = ln.split("#")[0].strip()
        if not body:
            out.append(ln)
            continue
        st, _, rid = body.partition(" ")
        rid = rid.strip()
        if rid in want:
            present.add(rid)
            out.append("on      " + rid if st == "off" else ln)
        elif rid in benchset:
            out.append("off     " + rid)          # superseded plain twin
        else:
            out.append(ln)

    hdr = [
        "",
        "# --- wall-clock ladder study (2026-09-06) -----------------------------------",
        "# What ms is a reasonable bound for the time-normalized track? Criterion: the",
        "# KNEE of Elo against REALIZED ms, not against the flag. A time-budgeted search",
        "# finishes at roughly 40%% of its allowance because nextIterationFits declines",
        "# the iteration it predicts will not fit, so the flag is not the spend.",
        "#",
        "# 4 cores x {%s}, RETAIN ONLY." % ", ".join("%dms" % m for m in RUNGS),
        "# tdleaf_self lin model=349 is dropped as a near-duplicate of model=169.",
        "# The plain twins are benched off, not deleted: retain raises realized spend",
        "# from 0.432 of the flag (sd 0.053) to 0.854 (sd 0.028), so a plain time= head",
        "# under-delivers its own budget by more than half. Theory 70.",
        "#",
        "# These pairs are NOT capped at 2 games: rankAgentIsDeterministic treats any",
        "# time= head as stochastic, because a wall-clock search stops wherever machine",
        "# load puts it.",
    ]
    fresh = [i for i in want if i not in present]
    for i in fresh:
        hdr.append("on      %s" % i)

    # Superseded plain twins that the base roster does not already carry. Listed
    # explicitly as `off` rather than omitted, so the roster records that they
    # were played and then retired from the comparison, not that they never ran.
    hdr.append("#")
    hdr.append("# Superseded plain twins, retired from the comparison (theory 70).")
    for i in bench:
        if i not in present:
            hdr.append("off     %s" % i)

    if not os.path.isdir(a.outdir):
        os.makedirs(a.outdir)
    rp = os.path.join(a.outdir, "roster_timeladder.txt")
    cp = os.path.join(a.outdir, "cohort_timeladder.txt")
    open(rp, "w", encoding="utf-8").write("\n".join(out + hdr) + "\n")
    open(cp, "w", encoding="utf-8").write("\n".join(want) + "\n")
    print("%d retain cells (%d already rostered, %d new), %d plain twins benched -> %s"
          % (len(want), len(present), len(fresh), len(bench), rp))
    print("cohort -> %s" % cp)


if __name__ == "__main__":
    main()
