"""Locate the source of a sparse design's mean shift against the full fit.

Refits every cohort pair using only one Round 4 rung's rows, and using the
first / last / random 2 rows per pair, then prints the mean and RMS Elo
difference from the rung-16 pinned reference per category cell. If first,
last and random rows all shift the same way, the shift comes from games per
pair (the fit's per-pair prior), not from when the games were played.

    python analysis/sparse_prior_diag.py <work dir of a sparse_schedule_sim run>

The work dir must already hold that run's base.jsonl (the non-cohort rows).
Results: plans/sparse-schedule-results-1-cedar-lynx.md.
"""
import json, math, os, random, sys
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, "analysis")
import sparse_schedule_sim as ss

W = sys.argv[1]
cohort = ss.read_ids("ranking/q8/cohort_round4b.txt")
cset = set(cohort)
pairs = {}
for path in ss.store_files("ranking/matches.jsonl"):
    for line in open(path, encoding="utf-8"):
        if '"t":"g"' not in line:
            continue
        g = json.loads(line)
        if g["w"] in cset or g["b"] in cset:
            pairs.setdefault(frozenset((g["w"], g["b"])), []).append((g.get("run", ""), line.strip() + "\n"))

def grp(run):
    if run < "20260907T153940Z": return "pre"
    if run == "20260907T153940Z": return "r2"
    if run == "20260908T022321Z": return "r4"
    if run == "20260908T120449Z": return "r8"
    return "r16"

designs = {}
for gname in ("pre", "r2", "r4", "r8", "r16"):
    designs["dg_" + gname] = [l for v in pairs.values() for r, l in v if grp(r) == gname]
designs["dg_first2"] = [l for v in pairs.values() for r, l in v[:2]]
designs["dg_last2"] = [l for v in pairs.values() for r, l in v[-2:]]
for s in (1, 2):
    rng = random.Random(s)
    designs["dg_rand2_s%d" % s] = [l for v in pairs.values() for r, l in rng.sample(v, min(2, len(v)))]
designs["dg_nopre"] = [l for v in pairs.values() for r, l in v if grp(r) != "pre"]

ref = ss.read_standings("ranking/rungs/standings_r16.tsv")
rank_exe = os.path.abspath("rank.exe")

def one(tag):
    got = ss.fit(tag, os.path.join(W, "base.jsonl"), designs[tag], W, "ranking/roster.txt",
                 "ranking/rungs/pin_source_standings_20260906.tsv", rank_exe)
    return tag, ss.read_standings(got["standings"])

with ThreadPoolExecutor(max_workers=8) as ex:
    res = dict(ex.map(one, list(designs)))

def track(i): return "time" if "time=" in i.split(".")[0] else "node"
cells = [(d, t) for d in ("openless", "opener8", "dil20") for t in ("node", "time")]
hdr = "%-14s %7s %6s" % ("design", "rows", "medpm") + "".join(" %15s" % ("%s_%s" % c) for c in cells) + " %15s" % "all"
print("mean diff / rms diff vs rung-16 pinned reference, per cell (Elo)")
print(hdr)
for tag in designs:
    st = res[tag]
    pm = sorted(float(st[i]["pm"]) for i in cohort if i in st)
    out = "%-14s %7d %6.0f" % (tag, len(designs[tag]), pm[len(pm) // 2] if pm else float("nan"))
    for c in cells + [None]:
        ids = [i for i in cohort if i in st and i in ref and (c is None or (ss.division(i), track(i)) == c)]
        d = [float(st[i]["elo"]) - float(ref[i]["elo"]) for i in ids]
        out += " %7.1f/%6.1f" % (sum(d) / len(d), math.sqrt(sum(x * x for x in d) / len(d))) if d else " %15s" % "-"
    print(out)
