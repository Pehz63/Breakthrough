"""Build the roster + cohort for the wall-clock ladder study.

The question: what is a reasonable ms bound for the time-normalized track?
The criterion the developer chose is the KNEE of Elo against realized ms, the
cheapest bound past which more time stops buying strength.

Design (settled 2026-09-06):
  4 cores x {25, 50, 100, 200, 400ms} x {plain, retain} = 40 cells.

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
RUNGS = [25, 50, 100, 200, 400]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--abver", type=int, default=2,
                    help="ab explorer code version (3 after the time= enforcement bump)")
    ap.add_argument("--base", default="ranking/roster.txt")
    ap.add_argument("--outdir", default="ranking/q7")
    a = ap.parse_args()

    want = []
    for ms in RUNGS:
        for _, core in CORES:
            for retain in (False, True):
                head = "ab(deep=12,tt,ord%s,time=%dms)@%d" % (
                    ",retain" if retain else "", ms, a.abver)
                want.append(head + "." + core)

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
        "# 4 cores x {%s} x {plain, retain}." % ", ".join("%dms" % m for m in RUNGS),
        "# tdleaf_self lin model=349 is dropped as a near-duplicate of model=169.",
        "# retain on a time head banks unspent MILLISECONDS (g_timeCarry, theory 68),",
        "# which changes realized ms per move and so changes the answer to the ms",
        "# question rather than being a separate one.",
        "#",
        "# These pairs are NOT capped at 2 games: rankAgentIsDeterministic treats any",
        "# time= head as stochastic, because a wall-clock search stops wherever machine",
        "# load puts it.",
    ]
    fresh = [i for i in want if i not in present]
    for i in fresh:
        hdr.append("on      %s" % i)

    if not os.path.isdir(a.outdir):
        os.makedirs(a.outdir)
    rp = os.path.join(a.outdir, "roster_timeladder.txt")
    cp = os.path.join(a.outdir, "cohort_timeladder.txt")
    open(rp, "w", encoding="utf-8").write("\n".join(out + hdr) + "\n")
    open(cp, "w", encoding="utf-8").write("\n".join(want) + "\n")
    print("%d cells (%d already rostered, %d new) -> %s" % (len(want), len(present), len(fresh), rp))
    print("cohort -> %s" % cp)


if __name__ == "__main__":
    main()
