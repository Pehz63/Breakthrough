"""Pooled, inverse-variance-weighted retain-minus-plain effect, both tracks.

Backs theory 70. Run from the repo root, after a pinned fit of the wall-clock
ladder has written ranking/standings_pinned.tsv:

    rank.exe rate --roster ranking/q7/roster_timeladder.txt --pin ranking/standings.tsv
    python analysis/pool_retain_effect.py

The node-track contrasts are transcribed constants, not read from a file: that
fit's tsv has since been replaced by the time-ladder fit, and absolute Elo is
never comparable across fits, so the per-cell DIFFERENCES are what carries
forward. They are sourced from plans/budget-parity-results-2-steady-meridian.md.

Rigour notes, because the earlier write-ups leaned on eyeballed per-cell numbers:
  * Each contrast is a difference of two Elo estimates INSIDE one fit, so it is
    comparable across cells of that fit but never across fits.
  * Cells are pooled with weights 1/SE^2 (inverse variance), giving the minimum
    variance unbiased combination under independence.
  * Independence is imperfect: cells of one fit share the pinned pool, so the
    pooled SE is a lower bound and the reported z is an upper bound on |z|.
    Stated rather than ignored.
  * Q and I^2 test whether the cores agree. A large I^2 means the pooled mean is
    averaging genuinely different per-core effects and should not be quoted alone.
"""
import csv, math

def load(path):
    rows = [l for l in open(path, encoding="utf-8") if not l.startswith("#")]
    return {r["id"]: r for r in csv.DictReader(rows, delimiter="\t")}


def pool(pairs):
    """pairs: list of (delta, se). Returns mean, se, z, Q, df, I2."""
    w = [1.0 / (s * s) for _, s in pairs]
    sw = sum(w)
    m = sum(wi * d for wi, (d, _) in zip(w, pairs)) / sw
    se = math.sqrt(1.0 / sw)
    Q = sum(wi * (d - m) ** 2 for wi, (d, _) in zip(w, pairs))
    df = len(pairs) - 1
    I2 = max(0.0, (Q - df) / Q) * 100 if Q > 0 else 0.0
    return m, se, m / se, Q, df, I2


# ---------------------------------------------------------------- NODE track
NODE_CORES = [
    ("chip counter",         "classic(chip=100)@2"),
    ("tdleaf_self lin 169",  "learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1"),
    ("tdleaf_self lin 349",  "learned(model=349,5ee50d5c,tdleaf_self,lin,shape=129-1)@1"),
    ("position_elo mlp 113", "learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1"),
    ("pool_games lin 97",    "learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1"),
]
# Node-track numbers are quoted from the rem=0 fit recorded in
# plans/budget-parity-results-2-steady-meridian.md (that fit's tsv has since been
# replaced by the time-ladder fit, and absolute Elo is never comparable across
# fits, so the CONTRASTS are carried forward, not the levels).
NODE = {  # core -> budget -> (retain(rem=70,retain) minus default(rem=0), se)
    "chip counter":         {100: (18, 16), 200: (-38, 15), 300: (24, 16), 400: (0, 16)},
    "tdleaf_self lin 169":  {100: (137, 17), 200: (16, 18), 300: (1, 20), 400: (26, 20)},
    "tdleaf_self lin 349":  {100: (104, 16), 200: (45, 18), 300: (-37, 18), 400: (34, 19)},
    "position_elo mlp 113": {100: (25, 16), 200: (41, 16), 300: (17, 16), 400: (-4, 16)},
    "pool_games lin 97":    {100: (-12, 16), 200: (-5, 16), 300: (1, 16), 400: (22, 16)},
}

print("=== NODE TRACK: rem=70,retain minus rem=0 (the engine default) ===")
print("pooled across the 5 cores at each budget\n")
print("%-10s %10s %8s %8s %8s %7s" % ("budget", "pooled", "SE", "z", "Q(df=4)", "I2"))
for b in (100, 200, 300, 400):
    ps = [NODE[n][b] for n, _ in NODE_CORES]
    m, se, z, Q, df, I2 = pool(ps)
    print("%-10s %+10.1f %8.1f %8.2f %8.1f %6.0f%%" % ("%dk" % b, m, se, z, Q, I2))
lo = [NODE[n][100] for n, _ in NODE_CORES]
hi = [NODE[n][b] for n, _ in NODE_CORES for b in (200, 300, 400)]
m, se, z, Q, df, I2 = pool(lo)
print("\nlow budget only (100k):        %+.1f +-%.1f  (z=%.2f, I2=%.0f%%)" % (m, se, z, I2))
m2, se2, z2, Q2, df2, I22 = pool(hi)
print("moderate budgets (200-400k):   %+.1f +-%.1f  (z=%.2f, I2=%.0f%%)" % (m2, se2, z2, I22))
d = m - m2; ds = math.hypot(se, se2)
print("low minus moderate:            %+.1f +-%.1f  (z=%.2f)" % (d, ds, d / ds))

# ---------------------------------------------------------------- TIME track
T = load("ranking/standings_pinned.tsv")
TCORES = [
    ("chip counter",         "classic(chip=100)@2"),
    ("tdleaf_self lin 169",  "learned(model=169,4975683c,tdleaf_self,lin,shape=129-1)@1"),
    ("position_elo mlp 113", "learned(model=113,e3cc8b4e,position_elo,mlp,mu_shape=129-512-8-1,sigma_shape=129-64-1)@1"),
    ("pool_games lin 97",    "learned(model=97,87a5093d,pool_games,lin,shape=129-1)@1"),
]
MS = [25, 50, 100, 200, 400]


def tc(core, ms, ret):
    return T.get("ab(deep=12,tt,ord%s,time=%dms)@3.%s" % (",retain" if ret else "", ms, core))


print("\n\n=== TIME TRACK: retain minus plain at the SAME FLAG ===")
print("(retain spends about 2x here, so this is not a matched-CPU contrast)\n")
print("%-10s %10s %8s %8s %8s %7s" % ("flag", "pooled", "SE", "z", "Q(df=3)", "I2"))
for m_ in MS:
    ps = []
    for n, core in TCORES:
        a, b = tc(core, m_, False), tc(core, m_, True)
        ps.append((int(b["elo"]) - int(a["elo"]), math.hypot(float(a["pm"]), float(b["pm"]))))
    mm, se, z, Q, df, I2 = pool(ps)
    print("%-10s %+10.1f %8.1f %8.2f %8.1f %6.0f%%" % ("%dms" % m_, mm, se, z, Q, I2))

print("\n=== TIME TRACK: matched CPU, retain@X vs plain@2X ===")
print("%-16s %10s %8s %8s %8s %7s" % ("pair", "pooled", "SE", "z", "Q(df=3)", "I2"))
allp = []
for i in range(4):
    ps = []
    for n, core in TCORES:
        a, b = tc(core, MS[i], True), tc(core, MS[i + 1], False)
        ps.append((int(a["elo"]) - int(b["elo"]), math.hypot(float(a["pm"]), float(b["pm"]))))
    allp += ps
    mm, se, z, Q, df, I2 = pool(ps)
    print("%-16s %+10.1f %8.1f %8.2f %8.1f %6.0f%%" % ("r@%d vs p@%d" % (MS[i], MS[i + 1]), mm, se, z, Q, I2))
mm, se, z, Q, df, I2 = pool(allp)
print("%-16s %+10.1f %8.1f %8.2f %8.1f %6.0f%%  (all 16 cells)" % ("OVERALL", mm, se, z, Q, I2))

print("\n=== BUDGET REALIZATION: fraction of the flag actually spent ===")
print("%-24s %12s %12s" % ("core", "plain", "retain"))
pf, rf = [], []
for n, core in TCORES:
    p = [float(tc(core, m_, False)["cpu_ms_move"]) / m_ for m_ in MS]
    r = [float(tc(core, m_, True)["cpu_ms_move"]) / m_ for m_ in MS]
    pf += p; rf += r
    print("%-24s %6.2f-%.2f %7.2f-%.2f" % (n, min(p), max(p), min(r), max(r)))
print("%-24s %6.3f      %7.3f   (mean over all 20 cells)"
      % ("MEAN", sum(pf) / len(pf), sum(rf) / len(rf)))
print("%-24s %6.3f      %7.3f   (sd)"
      % ("", (sum((x - sum(pf)/len(pf))**2 for x in pf)/(len(pf)-1))**0.5,
             (sum((x - sum(rf)/len(rf))**2 for x in rf)/(len(rf)-1))**0.5))
