"""Migrate the `ab` explorer identity from @2 to @3 for the time= enforcement fix.

WHY THE BUMP: `budgetTripped()` sampled the wall clock once per 4096 nodes and
kept no memory of the answer, and the iterative-deepening loop had no
pre-iteration check, so a `time=` head overshot its budget by 3.9x to 7.1x. A
`time=` agent's stored games were therefore played by a search that is not what
its id describes. Full account: `TIME BUDGET NOT ENFORCED`, `Docs/corrections.md`.

WHY A MIGRATION AND NOT A PLAIN BUMP: module versions have no finer grain than
per-module, so bumping `ab` re-identifies every `ab(...)` agent, including the
two thirds of the store that never touched a wall clock. Those games are
provably byte-identical across the fix (`rank.exe determinism --replicas 2` over
16 `nodes=200k` agents reproduced exactly, node counts included), so discarding
them would be throwing away valid history to correct someone else's rows.
Developer decision 2026-09-05: renumber the survivors in place, drop only the
rows a `time=` head actually played.

RULES
  match rows : drop the row if EITHER side's id contains `time=`, else rewrite
               `ab(<flags>)@2` to `ab(<flags>)@3` wherever it appears.
               Unparseable rows are dropped (the reader already skips them).
  rosters    : rewrite ids. `time=` lines are KEPT: they become @3 identities
               with zero games, which is exactly right, they are awaiting
               re-certification rather than deleted.
  pin files  : rewrite ids, but DROP `time=` rows. A pin freezes an agent at a
               stored Elo, and those agents no longer have the games that Elo
               was fitted from (`PINNED AT LOW GAME COUNT`, Docs/corrections.md).

The three store parts are git-tracked and committed, so `git checkout --
ranking/` restores everything this touches. Run without --apply first.

Usage:
    python tools/migrate_ab_v3.py              # dry run, prints the counts
    python tools/migrate_ab_v3.py --apply
"""
import argparse, glob, json, os, re, sys

AB2 = re.compile(r"(ab\([^)]*\))@2")


def bump(text):
    return AB2.sub(r"\1@3", text)


def store_files(rank="ranking"):
    """Every part the loader reads, plus the side stores that carry the same ids."""
    out = []
    idx = os.path.join(rank, "matches.index.txt")
    if os.path.exists(idx):
        for l in open(idx, encoding="utf-8"):
            l = l.split("#")[0].strip()
            if l and os.path.exists(os.path.join(rank, l)):
                out.append(os.path.join(rank, l))
    for extra in ("matches.jsonl", "matches_open.jsonl", "matches_screen.jsonl"):
        p = os.path.join(rank, extra)
        if os.path.exists(p) and p not in out:
            out.append(p)
    return out


def migrate_store(path, apply):
    kept = dropped = torn = touched = 0
    out = []
    for line in open(path, encoding="utf-8", errors="replace"):
        if not line.strip():
            continue
        try:
            r = json.loads(line)
        except Exception:
            torn += 1
            continue
        if "time=" in r.get("w", "") or "time=" in r.get("b", ""):
            dropped += 1
            continue
        new = bump(line.rstrip("\n"))
        if new != line.rstrip("\n"):
            touched += 1
        out.append(new)
        kept += 1
    if apply:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            for l in out:
                f.write(l + "\n")
    return kept, dropped, torn, touched


def migrate_text(path, apply, drop_time_rows):
    kept = dropped = touched = 0
    out = []
    for line in open(path, encoding="utf-8", errors="replace"):
        raw = line.rstrip("\n")
        body = raw.split("#")[0]
        if drop_time_rows and "time=" in body and "ab(" in body:
            dropped += 1
            continue
        new = bump(raw)
        if new != raw:
            touched += 1
        out.append(new)
        kept += 1
    if apply and touched + dropped:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            for l in out:
                f.write(l + "\n")
    return kept, dropped, touched


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    a = ap.parse_args()
    tag = "APPLY" if a.apply else "DRY RUN"
    print("=== ab @2 -> @3 migration (%s) ===\n" % tag)

    tk = td = tt_ = ttouch = 0
    print("-- match stores (drop time= rows, renumber the rest)")
    for p in store_files():
        k, d, t, ch = migrate_store(p, a.apply)
        tk += k; td += d; tt_ += t; ttouch += ch
        print("   %-44s kept %7d  dropped %7d  torn %d  renumbered %7d"
              % (os.path.basename(p), k, d, t, ch))
    print("   TOTAL kept %d, dropped %d (%.1f%%), torn %d\n"
          % (tk, td, 100.0 * td / max(tk + td, 1), tt_))

    print("-- rosters and cohorts (renumber, keep time= lines as new identities)")
    txts = sorted(set(glob.glob("ranking/**/*.txt", recursive=True)))
    for p in txts:
        if os.path.basename(p) == "matches.index.txt":
            continue
        k, d, ch = migrate_text(p, a.apply, drop_time_rows=False)
        if ch:
            print("   %-52s renumbered %5d" % (p.replace("\\", "/"), ch))

    # Derived fit outputs are DELETED and regenerated, never migrated: rewriting a
    # ratings table by hand would publish numbers no fit ever produced. Determinism
    # and re-certification snapshots keep their @2 ids for the same reason a results
    # doc does, they are records of what @2 measured.
    DERIVED = ("ratings.tsv", "ratings_pinned.tsv", "standings.tsv",
               "standings_pinned.tsv", "games.tsv", "games_pinned.tsv",
               "report.md", "report_pinned.md")
    print("\n-- derived fit outputs (deleted, regenerate with `rank.exe rate`)")
    for n in DERIVED:
        p = os.path.join("ranking", n)
        if os.path.exists(p):
            print("   %-52s delete" % p.replace("\\", "/"))
            if a.apply:
                os.remove(p)

    print("\n-- pin files (renumber, and drop time= rows: their games are gone)")
    for p in sorted(set(glob.glob("ranking/**/*.tsv", recursive=True))):
        b = os.path.basename(p)
        if b in DERIVED or b.startswith("det_") or "recert_snapshots" in p.replace("\\", "/"):
            continue
        k, d, ch = migrate_text(p, a.apply, drop_time_rows=True)
        if ch or d:
            print("   %-52s renumbered %5d  dropped %4d" % (p.replace("\\", "/"), ch, d))
    for p in ["ranking/CHAMPION.md"]:
        if os.path.exists(p):
            k, d, ch = migrate_text(p, a.apply, drop_time_rows=False)
            if ch:
                print("   %-52s renumbered %5d" % (p, ch))

    if not a.apply:
        print("\nNothing was written. Re-run with --apply.")
    else:
        print("\nDone. Rebuild every binary, then `rank.exe check` and `rank.exe rate`.")


if __name__ == "__main__":
    main()
