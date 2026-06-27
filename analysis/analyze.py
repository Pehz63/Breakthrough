#!/usr/bin/env python3
"""Query the Breakthrough ML datastore with DuckDB.

The C++ side writes append-only JSONL under data/ (positions, evaluations, labels,
agents, models, metrics). This script runs SQL straight over those files -- no
import step -- to answer the questions we care about:

    python analysis/analyze.py top-agents          # highest-Elo agents
    python analysis/analyze.py models              # trained models + metrics
    python analysis/analyze.py avg-eval            # average eval per position
    python analysis/analyze.py fairest             # most fairly-matched positions
    python analysis/analyze.py position <hash>     # everything about one position
    python analysis/analyze.py sql "SELECT ..."    # arbitrary query

DuckDB reads JSONL via read_json_auto(); missing files are reported, not fatal.
"""
import os
import sys

DATA = os.path.join(os.path.dirname(__file__), "..", "data")


def _con():
    try:
        import duckdb
    except ImportError:
        sys.exit("duckdb not installed. Run: pip install -r requirements.txt")
    return duckdb.connect()


def _src(name):
    """A DuckDB table expression over a JSONL file, or None if it is absent/empty."""
    path = os.path.join(DATA, name)
    if not os.path.exists(path) or os.path.getsize(path) == 0:
        return None
    return f"read_json_auto('{path.replace(chr(92), '/')}')"


def _run(sql, title=None):
    con = _con()
    if title:
        print(f"\n== {title} ==")
    try:
        print(con.sql(sql))
    except Exception as e:  # noqa: BLE001
        print(f"(query failed: {e})")


def top_agents():
    s = _src("agents.jsonl")
    if not s:
        return print("No data/agents.jsonl yet. Run: train.exe tournament")
    _run(f"SELECT name, elo, desc FROM {s} ORDER BY elo DESC", "Agents by Elo")


def models():
    s = _src("models.jsonl")
    if not s:
        return print("No data/models.jsonl yet. Run a training command.")
    _run(f"SELECT * FROM {s} ORDER BY epoch", "Models")


def avg_eval():
    ev = _src("evaluations.jsonl")
    if not ev:
        return print("No data/evaluations.jsonl yet. Run selfplay-supervised.")
    pos = _src("positions.jsonl")
    join = f"LEFT JOIN (SELECT DISTINCT hash, enc, side FROM {pos}) p USING(hash)" if pos else ""
    _run(
        f"""
        SELECT e.hash, ANY_VALUE(p_enc) AS enc, ANY_VALUE(p_side) AS side,
               COUNT(*) AS n, ROUND(AVG(eval), 1) AS avg_eval
        FROM (SELECT hash, eval FROM {ev}) e
        LEFT JOIN (SELECT DISTINCT hash, enc AS p_enc, side AS p_side
                   FROM {pos}) USING(hash)
        GROUP BY e.hash ORDER BY n DESC, avg_eval DESC LIMIT 20
        """ if pos else
        f"SELECT hash, COUNT(*) n, ROUND(AVG(eval),1) avg_eval FROM {ev} "
        f"GROUP BY hash ORDER BY n DESC LIMIT 20",
        "Average evaluation per position (top by sample count)",
    )


def fairest():
    """Positions whose outcome label averages nearest 0.5 -- the most balanced /
    'worth evaluating' positions in the data so far (with at least 2 observations)."""
    lb = _src("labels.jsonl")
    if not lb:
        return print("No data/labels.jsonl yet. Run selfplay-supervised.")
    pos = _src("positions.jsonl")
    enc = f", ANY_VALUE(enc) AS enc" if pos else ""
    join = f"LEFT JOIN (SELECT DISTINCT hash, enc FROM {pos}) USING(hash)" if pos else ""
    _run(
        f"""
        SELECT hash, COUNT(*) AS n, ROUND(AVG(value), 3) AS avg_outcome,
               ROUND(ABS(AVG(value) - 0.5), 3) AS imbalance {enc}
        FROM {lb} {join}
        WHERE kind = 'outcome'
        GROUP BY hash HAVING COUNT(*) >= 2
        ORDER BY imbalance ASC, n DESC LIMIT 20
        """,
        "Most fairly-matched positions (outcome label nearest 0.5)",
    )


def position(h):
    pos, ev, lb = _src("positions.jsonl"), _src("evaluations.jsonl"), _src("labels.jsonl")
    if pos:
        _run(f"SELECT DISTINCT * FROM {pos} WHERE hash = {h}", f"Position {h}")
    if ev:
        _run(f"SELECT model, AVG(eval) avg_eval, COUNT(*) n FROM {ev} WHERE hash = {h} GROUP BY model",
             "Evaluations")
    if lb:
        _run(f"SELECT kind, AVG(value) avg_value, COUNT(*) n FROM {lb} WHERE hash = {h} GROUP BY kind",
             "Labels")


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    cmd = sys.argv[1]
    if cmd == "top-agents":
        top_agents()
    elif cmd == "models":
        models()
    elif cmd == "avg-eval":
        avg_eval()
    elif cmd == "fairest":
        fairest()
    elif cmd == "position" and len(sys.argv) > 2:
        position(sys.argv[2])
    elif cmd == "sql" and len(sys.argv) > 2:
        _run(sys.argv[2])
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
