"""Sparse opponent graphs tested on games already played.

Plan: plans/sparse-schedule-plan-1-cedar-lynx.md. A round robin's game count
grows with the square of the pool. This script asks what a sparse schedule
(each cohort agent meets k opponents, g games each) would have rated, by
subsampling a store in which every cohort agent already met every opponent,
and fitting each subsample with the same pinned rank.exe fit as the full data.
No game is played. Run from the repo root.

Subcommands:

  run   --cohort <ids> --pin <standings tsv> --ref-standings <tsv> --ref-cores <tsv>
        [--store ranking/matches.jsonl] [--roster ranking/roster.txt]
        [--strata <standings tsv>] [--k 8,16,32,64,128,0] [--g 2,4,8,16]
        [--modes random,strat] [--seed 1] [--work <dir>] [--workers 1] [--dry]
      k = 0 means every opponent. Each (mode, k, g) is drawn twice with no pair
      in common (A and B) when there are enough opponents, else once. Every
      design is fitted with `rank.exe rate --pin <pin>` over the non-cohort rows
      plus the design's cohort rows, and compared (1) against the reference
      fit (the full data, same pin) and (2) A against B, which share no games,
      so their disagreement is the design's own error. --dry prints the design
      sizes without fitting. Rows go to <work>/sparse_results.tsv.

  selftest
      Checks the design builder on synthetic ids: every cohort agent reaches k
      opponents, A and B share no pair, a stratified draw spreads over bands,
      and g caps games per pair.
"""
import argparse
import csv
import json
import math
import os
import random
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import rung_convergence as rc  # noqa: E402


# ------------------------------------------------------------------ store
def store_files(store):
    """The files rank.exe reads for a store: <stem>.index.txt parts, then the tail."""
    stem = store[:-len(".jsonl")] if store.endswith(".jsonl") else store
    files = []
    if os.path.exists(stem + ".index.txt"):
        d = os.path.dirname(store)
        for line in open(stem + ".index.txt", encoding="utf-8"):
            line = line.strip()
            if line and not line.startswith("#"):
                files.append(os.path.join(d, line))
    files.append(store)
    return [p for p in files if os.path.exists(p)]


def read_ids(path):
    return [l.split()[-1] for l in open(path, encoding="utf-8")
            if l.strip() and not l.lstrip().startswith("#")]


def read_standings(path):
    rows = [l for l in open(path, encoding="utf-8") if l.strip() and not l.startswith("#")]
    return {r["id"]: r for r in csv.DictReader(rows, delimiter="\t")}


def division(agent_id):
    if ".opener(" in agent_id:
        return "opener8"
    if ".dil(" in agent_id:
        return "dil20"
    return "openless"


# ------------------------------------------------------------------ designs
def build_design(cohort, others_of, k, rng, exclude=frozenset(), bands=None):
    """Edges (frozenset pairs) giving each cohort agent >= k distinct opponents
    (all of them when k = 0 or fewer are available), avoiding `exclude`.
    With `bands` (opponent -> band label), opponents are drawn round-robin over
    the bands in random order, so each band contributes about k / #bands."""
    edges = set()
    deg = {a: 0 for a in cohort}
    order = list(cohort)
    rng.shuffle(order)
    for a in order:
        cand = [o for o in others_of[a] if frozenset((a, o)) not in exclude
                and frozenset((a, o)) not in edges]
        want = len(cand) if k == 0 else max(0, k - deg[a])
        if bands is None:
            rng.shuffle(cand)
            pick = cand[:want]
        else:
            by = {}
            for o in cand:
                by.setdefault(bands.get(o, "none"), []).append(o)
            for lst in by.values():
                rng.shuffle(lst)
            # Opponents a already has (from other cohort agents' draws) count
            # toward their band, so the top-up goes to the thinnest bands.
            have = {}
            for e in edges:
                if a in e:
                    o = next(iter(e - {a}))
                    have[bands.get(o, "none")] = have.get(bands.get(o, "none"), 0) + 1
            labels = sorted(by)
            rng.shuffle(labels)
            pick = []
            while len(pick) < want and any(by[b] for b in labels):
                b = min((x for x in labels if by[x]), key=lambda x: have.get(x, 0))
                pick.append(by[b].pop())
                have[b] = have.get(b, 0) + 1
        for o in pick:
            edges.add(frozenset((a, o)))
            deg[a] += 1
            if o in deg:
                deg[o] += 1
    return edges


def elo_bands(standings, ids, nb=4):
    """Opponent -> quartile label by Elo in `standings` (unrated -> 'none')."""
    rated = sorted((float(standings[i]["elo"]), i) for i in ids if i in standings)
    out = {}
    for j, (_, i) in enumerate(rated):
        out[i] = "q%d" % min(nb - 1, j * nb // max(1, len(rated)))
    return out


# ------------------------------------------------------------------ fitting
def fit(tag, base, part_lines, work, roster, pin, rank_exe):
    """Write <work>/matches_<tag>.index.txt over the base part and this design's
    part, run the pinned fit, and move its outputs into <work>/<tag>/."""
    part = os.path.join(work, tag + ".part.jsonl")
    with open(part, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(part_lines)
    store = os.path.join(work, "matches_" + tag + ".jsonl")
    with open(os.path.join(work, "matches_" + tag + ".index.txt"), "w", encoding="utf-8") as f:
        f.write(os.path.basename(base) + "\n" + os.path.basename(part) + "\n")
    out = subprocess.run([rank_exe, "rate", "--roster", roster, "--in", store, "--pin", pin],
                         capture_output=True, text=True)
    if out.returncode != 0:
        raise RuntimeError("rank.exe rate failed for %s:\n%s" % (tag, out.stdout[-2000:]))
    dest = os.path.join(work, tag)
    os.makedirs(dest, exist_ok=True)
    got = {}
    for stem in ("standings", "cores", "ratings", "games", "report"):
        ext = ".md" if stem == "report" else ".tsv"
        src = os.path.join("ranking", "%s_%s_pinned%s" % (stem, tag, ext))
        if os.path.exists(src):
            if stem in ("standings", "cores"):
                got[stem] = shutil.move(src, os.path.join(dest, os.path.basename(src)))
            else:
                os.remove(src)   # large and not needed
    os.remove(part)
    return got


# ------------------------------------------------------------------ metrics
def spearman(a, b):
    keys = sorted(set(a) & set(b))
    n = len(keys)
    if n < 3:
        return float("nan"), 0
    ra = {x: i for i, x in enumerate(sorted(keys, key=lambda x: -a[x]))}
    rb = {x: i for i, x in enumerate(sorted(keys, key=lambda x: -b[x]))}
    d2 = sum((ra[x] - rb[x]) ** 2 for x in keys)
    return 1.0 - 6.0 * d2 / (n * (n * n - 1)), max(abs(ra[x] - rb[x]) for x in keys)


def compare(st, ref, ids):
    """Elo differences over `ids` present in both fits. The mean shift is
    reported on its own and is included in the RMS: both fits are pinned to
    the same agents, so a common shift of the cohort is a real error."""
    e = {i: float(st[i]["elo"]) for i in ids if i in st}
    r = {i: float(ref[i]["elo"]) for i in ids if i in ref}
    keys = sorted(set(e) & set(r))
    d = [e[i] - r[i] for i in keys]
    rms = math.sqrt(sum(x * x for x in d) / len(d)) if d else float("nan")
    rho, worst = spearman(e, r)
    pm = [float(st[i]["pm"]) for i in keys]
    z = [abs(x) / p for x, p in zip(d, pm) if p > 0]
    return {"n": len(keys), "rho": rho, "worst": worst, "rms": rms,
            "max": max((abs(x) for x in d), default=float("nan")),
            "mean_shift": sum(d) / len(d) if d else float("nan"),
            "med_pm": sorted(pm)[len(pm) // 2] if pm else float("nan"),
            "over2pm": sum(1 for x in z if x > 2) / len(z) if z else float("nan")}


def core_compare(cores_path, ref_cores_path):
    cur, cur_mean, cells = rc.load(cores_path)
    ref, ref_mean, _ = rc.load(ref_cores_path)
    rho, worst = rc.spearman(ref_mean, cur_mean)
    same = sum(1 for c in cells if rc.champion(cur, c)[0] == rc.champion(ref, c)[0])
    return {"core_rho": rho, "core_worst": worst, "champs_same": same, "cells": len(cells)}


# ------------------------------------------------------------------ run
def run(a):
    rank_exe = os.path.abspath("rank.exe")
    cohort = read_ids(a.cohort)
    cset = set(cohort)
    os.makedirs(a.work, exist_ok=True)
    base = os.path.join(a.work, "base.jsonl")
    pairs = {}          # frozenset -> list of raw lines, store order
    n_base = 0
    with open(base, "w", encoding="utf-8", newline="\n") as fb:
        for path in store_files(a.store):
            for line in open(path, encoding="utf-8"):
                s = line.strip()
                if not s:
                    continue
                g = json.loads(s)
                w, b = g.get("w"), g.get("b")
                if g.get("t", "g") == "g" and (w in cset or b in cset):
                    pairs.setdefault(frozenset((w, b)), []).append(s + "\n")
                else:
                    fb.write(s + "\n")
                    n_base += 1
    others_of = {x: [] for x in cohort}
    for p in pairs:
        u, v = tuple(p)
        if u in others_of:
            others_of[u].append(v)
        if v in others_of:
            others_of[v].append(u)
    for x in others_of:
        others_of[x].sort()
    print("store: %d non-cohort rows, %d cohort pairs, %d cohort rows, opponents per agent %d..%d"
          % (n_base, len(pairs), sum(len(v) for v in pairs.values()),
             min(len(v) for v in others_of.values()), max(len(v) for v in others_of.values())))

    strata = elo_bands(read_standings(a.strata), sorted({o for v in others_of.values() for o in v})) \
        if a.strata else None
    ks = [int(x) for x in a.k.split(",")]
    gs = [int(x) for x in a.g.split(",")]
    modes = a.modes.split(",")
    max_opp = min(len(v) for v in others_of.values())

    jobs = []
    for mode in modes:
        if mode == "strat" and strata is None:
            continue
        for k in ks:
            if mode == "strat" and k == 0:
                continue   # every opponent: nothing to stratify
            rng = random.Random("%s-%d-%d" % (mode, k, a.seed))
            bands = strata if mode == "strat" else None
            ea = build_design(cohort, others_of, k, rng, bands=bands)
            eb = build_design(cohort, others_of, k, rng, exclude=frozenset(ea), bands=bands) \
                if (k and 2 * k <= max_opp) else None
            for g in gs:
                for half, edges in (("A", ea), ("B", eb)):
                    if edges is None:
                        continue
                    lines = [l for e in edges for l in pairs[e][:g]]
                    per = {}
                    opp = {}
                    for e in edges:
                        n = min(g, len(pairs[e]))
                        for x in e:
                            if x in cset:
                                per[x] = per.get(x, 0) + n
                                opp[x] = opp.get(x, 0) + 1
                    tag = "ss_%s_k%d_g%d_%s" % (mode, k, g, half)
                    jobs.append({"tag": tag, "mode": mode, "k": k, "g": g, "half": half,
                                 "lines": lines, "games_per_agent": sum(per.values()) / len(cset),
                                 "opp_per_agent": sum(opp.values()) / len(cset),
                                 "min_opp": min(opp.values()) if opp else 0})
    print("%d fits" % len(jobs))
    print("%-24s %8s %10s %10s %10s" % ("design", "fits", "games/agt", "opp/agt", "min opp"))
    for j in jobs:
        if j["half"] == "A":
            print("%-24s %8s %10.0f %10.1f %10d" % (j["tag"][:-2], "A+B" if any(
                x["tag"] == j["tag"][:-1] + "B" for x in jobs) else "A",
                j["games_per_agent"], j["opp_per_agent"], j["min_opp"]))
    if a.dry:
        return 0

    ref = read_standings(a.ref_standings)
    div_ids = {d: [x for x in cohort if division(x) == d] for d in ("openless", "opener8", "dil20")}

    def one(j):
        got = fit(j["tag"], base, j["lines"], a.work, a.roster, a.pin, rank_exe)
        j["st"] = read_standings(got["standings"])
        j["cores"] = got.get("cores")
        j["lines"] = None
        return j

    with ThreadPoolExecutor(max_workers=a.workers) as ex:
        done = list(ex.map(one, jobs))
    by = {j["tag"]: j for j in done}
    res = os.path.join(a.work, "sparse_results.tsv")
    cols = ["mode", "k", "g", "half", "games_per_agent", "opp_per_agent", "min_opp", "n", "rho", "worst",
            "rms", "max", "mean_shift", "med_pm", "over2pm", "rms_openless", "rms_opener8", "rms_dil20",
            "core_rho", "core_worst", "champs_same", "ab_rho", "ab_rms"]
    with open(res, "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, delimiter="\t")
        w.writerow(cols)
        for j in done:
            m = compare(j["st"], ref, cohort)
            for d, ids in div_ids.items():
                m["rms_" + d] = compare(j["st"], ref, ids)["rms"]
            m.update(core_compare(j["cores"], a.ref_cores) if j["cores"] else {})
            other = by.get(j["tag"][:-1] + ("B" if j["half"] == "A" else "A"))
            if other is not None:
                ab = compare(j["st"], other["st"], cohort)
                m["ab_rho"], m["ab_rms"] = ab["rho"], ab["rms"]
            row = {**{c: j.get(c) for c in ("mode", "k", "g", "half", "games_per_agent",
                                             "opp_per_agent", "min_opp")}, **m}
            w.writerow([("%.4g" % row[c]) if isinstance(row.get(c), float) else row.get(c, "")
                        for c in cols])
    print("results: " + res)
    return 0


# ------------------------------------------------------------------ selftest
def selftest():
    fails = []

    def check(ok, what):
        print(("PASS " if ok else "FAIL ") + what)
        if not ok:
            fails.append(what)

    cohort = ["c%02d" % i for i in range(30)]
    pool = ["p%03d" % i for i in range(70)]
    allx = cohort + pool
    others_of = {c: [o for o in allx if o != c] for c in cohort}
    rng = random.Random(3)
    ea = build_design(cohort, others_of, 16, rng)
    deg = {c: sum(1 for e in ea if c in e) for c in cohort}
    check(min(deg.values()) >= 16, "every cohort agent reaches k = 16 opponents (min %d)" % min(deg.values()))
    check(all(e & set(cohort) for e in ea), "every edge touches the cohort")
    eb = build_design(cohort, others_of, 16, rng, exclude=frozenset(ea))
    check(not (ea & eb), "A and B share no pair")
    degb = {c: sum(1 for e in eb if c in e) for c in cohort}
    check(min(degb.values()) >= 16, "B also reaches k = 16 (min %d)" % min(degb.values()))
    full = build_design(cohort, others_of, 0, rng)
    check(len(full) == 30 * 29 // 2 + 30 * 70, "k = 0 is every pair touching the cohort (%d)" % len(full))
    bands = {o: "q%d" % (int(o[1:]) % 4) for o in allx}
    es = build_design(cohort, others_of, 16, random.Random(5), bands=bands)
    spread = []
    for c in cohort:
        cnt = {}
        for e in es:
            if c in e:
                o = next(iter(e - {c}))
                cnt[bands[o]] = cnt.get(bands[o], 0) + 1
        spread.append(min(cnt.get("q%d" % q, 0) for q in range(4)))
    check(min(spread) >= 3, "stratified draw gives every band >= 3 of 16 opponents (min %d)" % min(spread))
    pairs = {e: ["x\n"] * 16 for e in ea}
    check(sum(len(pairs[e][:4]) for e in ea) == 4 * len(ea), "g = 4 caps games per pair")
    print("\nselftest: %d failure(s)" % len(fails))
    return 1 if fails else 0


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run")
    r.add_argument("--store", default="ranking/matches.jsonl")
    r.add_argument("--roster", default="ranking/roster.txt")
    r.add_argument("--cohort", required=True)
    r.add_argument("--pin", required=True)
    r.add_argument("--ref-standings", required=True)
    r.add_argument("--ref-cores", required=True)
    r.add_argument("--strata")
    r.add_argument("--k", default="8,16,32,64,128,0")
    r.add_argument("--g", default="2,4,8,16")
    r.add_argument("--modes", default="random,strat")
    r.add_argument("--seed", type=int, default=1)
    r.add_argument("--work", default="analysis/out/sparse_schedule")
    r.add_argument("--workers", type=int, default=1)
    r.add_argument("--dry", action="store_true")
    sub.add_parser("selftest")
    a = ap.parse_args(argv)
    return run(a) if a.cmd == "run" else selftest()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
