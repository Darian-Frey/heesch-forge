#!/usr/bin/env python3
"""M2.6-followup-A: bounded-DLX + SAT partial-state seed orchestrator.

Architecture (cf. M2.6 finding / paper/drafts/phase2-engineering-hybrid.md):

  for each shape:
      result = bounded_dlx(shape, budget=B)
      if result.complete:
          accept (result.hc_dlx, result.hh_dlx)
      else:
          ans = sat -isohedral -hh -seed result.partial_state_path
          accept (ans.hc, ans.hh)

This realises the "task-class-based hybrid" recommendation from M2.7:
DLX handles the find-any-completion fast path; SAT is invoked only on
shapes where DLX would have run away under unlimited budget.

Wall-time budget B is in DLX search-tree nodes. Reasonable values for
n in 7..12 land between 10^4 (very tight, mostly SAT) and 10^7 (loose,
mostly DLX). The benchmark sweep below tries a few.
"""
from __future__ import annotations

import argparse
import csv
import pathlib
import re
import subprocess
import sys
import time
from dataclasses import dataclass

ROOT = pathlib.Path(__file__).resolve().parents[2]
BOUNDED_DLX_BIN = ROOT / "benchmarks" / "hybrid" / "bounded_dlx"
SAT_BIN = ROOT / "src" / "sat" / "src" / "sat"

SAT_RESPONSE_RE = re.compile(r"^~\s+(\d+)\s+(\d+)")


@dataclass
class JointRow:
    coords: str
    n: int
    hc_dataset: int
    hh_dataset: int
    path: str            # "dlx" or "sat-seed" or "sat-no-seed"
    hc_final: int
    hh_final: int
    ok: bool
    wall_dlx_ms: float
    wall_sat_ms: float
    partial_depth: int


def run_bounded_dlx(dataset: pathlib.Path, budget: int,
                    state_dir: pathlib.Path) -> list[dict]:
    state_dir.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        [str(BOUNDED_DLX_BIN), str(dataset), str(budget), str(state_dir)],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr)
        raise RuntimeError(f"bounded_dlx exit {proc.returncode}")
    rdr = csv.DictReader(proc.stdout.splitlines())
    return list(rdr)


def call_sat(coords: str, seed_file: str | None, maxlevel: int = 7
             ) -> tuple[int, int, float]:
    cmd = [str(SAT_BIN), "-isohedral", "-hh", "-maxlevel", str(maxlevel)]
    if seed_file:
        cmd.extend(["-seed", seed_file])
    t0 = time.perf_counter()
    proc = subprocess.run(
        cmd, input=f"O? {coords}\n",
        capture_output=True, text=True, timeout=120,
    )
    wall = (time.perf_counter() - t0) * 1000.0
    if proc.returncode != 0:
        raise RuntimeError(
            f"sat exit {proc.returncode}; stderr={proc.stderr!r}")
    for line in proc.stdout.splitlines():
        m = SAT_RESPONSE_RE.match(line.strip())
        if m:
            return int(m.group(1)), int(m.group(2)), wall
        if line.startswith("I"):
            return 0, 0, wall                # isohedral tiler
    raise RuntimeError(
        f"unparseable sat output. cmd={cmd!r} "
        f"stdout={proc.stdout!r} stderr={proc.stderr!r}")


def run(dataset: pathlib.Path, budget: int,
        state_dir: pathlib.Path) -> list[JointRow]:
    dlx_rows = run_bounded_dlx(dataset, budget, state_dir)
    out: list[JointRow] = []
    for row in dlx_rows:
        coords = row["coords"].strip().strip('"')
        n = int(row["n"])
        hc_ds = int(row["hc_dataset"])
        hh_ds = int(row["hh_dataset"])
        complete = int(row["complete"]) == 1
        wall_dlx = float(row["wall_ms"])
        partial_depth = int(row["partial_depth"])

        if complete:
            hc_final = int(row["hc_dlx"])
            hh_final = int(row["hh_dlx"])
            wall_sat = 0.0
            path = "dlx"
        else:
            # If DLX exhausted before completing level 1, the seed
            # file only carries the level-0 anchor and adds no useful
            # information for sat -- skip -seed to dodge the empty-
            # seed corner case in heesch.h. Otherwise hand the partial
            # state to sat -seed so it picks up where DLX left off.
            seed_path = row["partial_state_path"] or None
            if partial_depth <= 1:
                hc_final, hh_final, wall_sat = call_sat(coords, None)
                path = "sat-no-seed"
            else:
                hc_final, hh_final, wall_sat = call_sat(coords, seed_path)
                path = "sat-seed"

        ok = (hc_final == hc_ds) and (hh_final == hh_ds)
        out.append(JointRow(
            coords=coords, n=n,
            hc_dataset=hc_ds, hh_dataset=hh_ds,
            path=path, hc_final=hc_final, hh_final=hh_final, ok=ok,
            wall_dlx_ms=wall_dlx, wall_sat_ms=wall_sat,
            partial_depth=partial_depth,
        ))
    return out


def write_csv(rows: list[JointRow], path: pathlib.Path) -> None:
    with path.open("w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow([
            "coords", "n", "hc_dataset", "hh_dataset",
            "path", "hc_final", "hh_final", "ok",
            "wall_dlx_ms", "wall_sat_ms", "partial_depth",
        ])
        for r in rows:
            w.writerow([
                r.coords, r.n, r.hc_dataset, r.hh_dataset,
                r.path, r.hc_final, r.hh_final, int(r.ok),
                f"{r.wall_dlx_ms:.3f}", f"{r.wall_sat_ms:.3f}",
                r.partial_depth,
            ])


def summarise(rows: list[JointRow]) -> dict:
    total = len(rows)
    by_path: dict[str, int] = {}
    for r in rows:
        by_path[r.path] = by_path.get(r.path, 0) + 1
    matches = sum(1 for r in rows if r.ok)
    wall_dlx = sum(r.wall_dlx_ms for r in rows)
    wall_sat = sum(r.wall_sat_ms for r in rows)
    return {
        "total": total,
        "ok": matches,
        "by_path": by_path,
        "wall_dlx_ms_total": round(wall_dlx, 3),
        "wall_sat_ms_total": round(wall_sat, 3),
        "wall_combined_ms_total": round(wall_dlx + wall_sat, 3),
    }


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--dataset", type=pathlib.Path, required=True)
    p.add_argument("--budget", type=int, required=True,
                   help="DLX node budget per shape")
    p.add_argument("--out", type=pathlib.Path, required=True,
                   help="output CSV path")
    p.add_argument("--state-dir", type=pathlib.Path,
                   default=ROOT / "benchmarks" / "hybrid" /
                           "results" / "m2.6-followup-A-states")
    args = p.parse_args(argv)

    rows = run(args.dataset, args.budget, args.state_dir)
    write_csv(rows, args.out)
    summary = summarise(rows)

    sys.stderr.write(
        f"# {args.dataset.name}  budget={args.budget:>10d}  "
        f"ok={summary['ok']}/{summary['total']}  "
        f"by_path={summary['by_path']}  "
        f"wall(dlx+sat)={summary['wall_combined_ms_total']:.1f}ms\n"
    )
    return 0 if summary["ok"] == summary["total"] else 1


if __name__ == "__main__":
    sys.exit(main())
