#!/usr/bin/env python3
"""
M1.5 partial-corona scorer.

Walks every `.wcnf` file under a directory of WCNF dumps produced by
sat-wcnf-dump (the -DHEESCH_BACKEND_WCNF_DUMP build), runs RC2 (from
pysat) on each, and emits a per-instance CSV with the partial-corona
score:

    score = num_softs - cost

i.e. the maximum number of cell-var soft clauses RC2 could satisfy in
the same hard formula. For the level-(Hc+1) corona-loop UNSAT this is
the partial-corona size we want as Layer-3's reward signal; for the
isohedral-check and the -hh final-check WCNFs that also arrive in the
same dump dir the score has a different interpretation but the
arithmetic is identical.

Per-WCNF wall time is capped (default 60 s) so a single hard instance
cannot wedge the run; timed-out rows record the cap with a `timeout`
flag.

Usage:
    .venv/bin/python benchmarks/maxsat/run_rc2.py \\
        --dump-dir benchmarks/maxsat/work \\
        --out benchmarks/maxsat/results/m1.5-scores.csv
"""
from __future__ import annotations

import argparse
import csv
import pathlib
import signal
import sys
import time

from pysat.examples.rc2 import RC2
from pysat.formula import WCNF


class _Timeout(Exception):
    pass


def _alarm(_sig, _frame):
    raise _Timeout()


def score_one(path: pathlib.Path, time_cap_s: float):
    wcnf = WCNF(from_file=str(path))
    nhard = len(wcnf.hard)
    nsoft = len(wcnf.soft)
    t0 = time.perf_counter()
    signal.signal(signal.SIGALRM, _alarm)
    signal.setitimer(signal.ITIMER_REAL, time_cap_s)
    try:
        with RC2(wcnf) as solver:
            model = solver.compute()
            cost = solver.cost
        wall = time.perf_counter() - t0
        signal.setitimer(signal.ITIMER_REAL, 0)
        if model is None:
            return {"path": path.name, "nvars": wcnf.nv, "nhard": nhard,
                    "nsoft": nsoft, "cost": None, "score": None,
                    "wall_s": wall, "timeout": False, "unsat_hard": True}
        return {"path": path.name, "nvars": wcnf.nv, "nhard": nhard,
                "nsoft": nsoft, "cost": cost,
                "score": nsoft - cost, "wall_s": wall,
                "timeout": False, "unsat_hard": False}
    except _Timeout:
        return {"path": path.name, "nvars": wcnf.nv, "nhard": nhard,
                "nsoft": nsoft, "cost": None, "score": None,
                "wall_s": time_cap_s, "timeout": True, "unsat_hard": False}
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dump-dir", type=pathlib.Path, required=True,
                    help="directory containing inst-*.wcnf files")
    ap.add_argument("--out", type=pathlib.Path, required=True,
                    help="CSV output path")
    ap.add_argument("--cap", type=float, default=60.0,
                    help="per-WCNF RC2 wall cap in seconds (default: %(default)s)")
    args = ap.parse_args()

    files = sorted(args.dump_dir.glob("*.wcnf"))
    if not files:
        sys.exit(f"no .wcnf files under {args.dump_dir}")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    fields = ["path", "nvars", "nhard", "nsoft", "cost", "score",
              "wall_s", "timeout", "unsat_hard"]
    with args.out.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        w.writeheader()
        print(f"# {len(files)} wcnf file(s) under {args.dump_dir}")
        print(f"# per-file cap: {args.cap}s; output: {args.out}")
        print()
        for p in files:
            r = score_one(p, args.cap)
            w.writerow(r)
            fh.flush()
            tag = ("TIMEOUT" if r["timeout"]
                   else "UNSAT-HARD" if r["unsat_hard"]
                   else "OK")
            print(f"  {p.name:36s}  vars={r['nvars']:5d} "
                  f"hard={r['nhard']:6d} soft={r['nsoft']:4d}  "
                  f"score={r['score']!s:>5}  cost={r['cost']!s:>5}  "
                  f"wall={r['wall_s']:6.2f}s  {tag}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
