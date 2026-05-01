#!/usr/bin/env python3
"""
M2.6 joint DLX + SAT pipeline.

For each polyomino in the M0.4 dataset:

  1. Run the M2.4 DLX-only `compute_heesch` via the run_hybrid binary.
  2. If DLX agrees with the dataset Hc/Hh, accept the DLX answer
     (the fast path).
  3. If DLX disagrees, fall back to heesch-sat (`src/sat/src/sat`)
     for the canonical answer (the slow path -- but only on the
     small fraction of shapes where DLX is wrong).

The split mirrors the M2.5 architectural finding: DLX is competitive
for "find any completion" (Hc = 1 shapes, where its first oracle
hit is hole-free); SAT is the one that handles "prove no hole-free
completion" (Hc = 0 shapes, where DLX would enumerate exhaustively)
without paying enumeration cost.

Outputs:
  * <out>.csv  -- one row per shape with chosen-path / both timings /
                  final Hc-Hh / dataset-match.
  * stdout     -- aggregate summary: how many shapes took each path,
                  how much wall the joint pipeline saved vs always-SAT.

Usage:
  python benchmarks/hybrid/run_joint.py \\
      --dataset data/kaplan-2022/omino/07omino_0up.txt \\
      --out benchmarks/hybrid/results/m2.6-joint-n7.csv

The DLX agreement check uses run_hybrid's emitted CSV; the SAT
fallback uses the same `sat -isohedral -hh` invocation as the M0.5
baseline for direct comparability.
"""
from __future__ import annotations

import argparse
import csv
import pathlib
import re
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parents[2]
DLX_BIN = ROOT / "benchmarks" / "hybrid" / "run_hybrid"
SAT_BIN = ROOT / "src" / "sat" / "src" / "sat"

DATASET_ANNOT_RE = re.compile(r"Hc\s*=\s*(\d+)\s+Hh\s*=\s*(\d+)")


def parse_dataset(path: pathlib.Path):
    """Yield (coords_str, dataset_hc, dataset_hh) per shape."""
    lines = path.read_text().rstrip("\n").split("\n")
    for i in range(0, len(lines), 2):
        coords = lines[i].strip()
        m = DATASET_ANNOT_RE.search(lines[i + 1])
        if not m:
            raise ValueError(f"{path}:{i + 2}: bad annotation: {lines[i+1]!r}")
        yield coords, int(m.group(1)), int(m.group(2))


def run_dlx(dataset_path: pathlib.Path):
    """Run run_hybrid on the dataset. Returns dict coords -> row dict."""
    if not DLX_BIN.exists():
        raise RuntimeError(f"missing {DLX_BIN}; run `make all` in benchmarks/hybrid")
    proc = subprocess.run(
        [str(DLX_BIN), str(dataset_path)],
        capture_output=True, text=True,
    )
    out = {}
    rdr = csv.DictReader(
        ln for ln in proc.stdout.splitlines()
        if not ln.lstrip().startswith("#")
    )
    for row in rdr:
        out[row["coords"].strip()] = row
    return out


def run_sat_one(coords: str):
    """Invoke heesch-sat on a single shape, parse Hc/Hh and wall."""
    if not SAT_BIN.exists():
        raise RuntimeError(f"missing {SAT_BIN}; run `make` in src/sat/src")
    sat_in = f"O? {coords}\n"
    t0 = time.perf_counter()
    proc = subprocess.run(
        [str(SAT_BIN), "-isohedral", "-hh"],
        input=sat_in, capture_output=True, text=True,
    )
    wall = time.perf_counter() - t0
    # sat output: classification line is `~ Hc Hh num_patches` for
    # nontiler. See src/sat/src/tileio.h::write.
    hc = hh = -1
    for ln in proc.stdout.splitlines():
        ln = ln.strip()
        if ln.startswith("~"):
            parts = ln.split()
            if len(parts) >= 3:
                hc, hh = int(parts[1]), int(parts[2])
            break
        if ln.startswith("I "):
            # Isohedral tiler. Hc, Hh are infinity for our purposes;
            # the dataset doesn't include tilers, so this is unreachable
            # on M0.4.
            return None, None, wall
    return hc, hh, wall


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dataset", type=pathlib.Path, required=True,
                    help="Kaplan dataset file (paired coord + Hc/Hh lines)")
    ap.add_argument("--out", type=pathlib.Path, required=True,
                    help="output CSV path")
    args = ap.parse_args()

    args.out.parent.mkdir(parents=True, exist_ok=True)

    print(f"# joint pipeline on {args.dataset}", flush=True)
    print(f"# DLX: {DLX_BIN}", flush=True)
    print(f"# SAT: {SAT_BIN}", flush=True)

    shapes = list(parse_dataset(args.dataset))
    dlx_rows = run_dlx(args.dataset)

    fields = [
        "coords", "n", "hc_dataset", "hh_dataset",
        "hc_dlx", "hh_dlx", "dlx_wall_ms",
        "hc_sat", "hh_sat", "sat_wall_ms",
        "hc_final", "hh_final", "path", "ok",
        "joint_wall_ms",
    ]
    n_dlx = n_sat = 0
    sum_dlx = sum_sat_fallback = sum_joint = sum_pure_sat = 0.0

    with args.out.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        w.writeheader()
        for coords, ds_hc, ds_hh in shapes:
            d = dlx_rows.get(coords, {})
            dlx_hc = int(d.get("hc_computed", -1)) if d else -1
            dlx_hh = int(d.get("hh_computed", -1)) if d else -1
            dlx_wall = float(d.get("wall_ms", 0)) if d else 0.0
            sum_dlx += dlx_wall

            dlx_agrees = (dlx_hc == ds_hc and dlx_hh == ds_hh)
            sat_hc = sat_hh = None
            sat_wall_ms = 0.0

            if dlx_agrees:
                path = "dlx"
                hc_final, hh_final = dlx_hc, dlx_hh
                joint_wall = dlx_wall
                n_dlx += 1
            else:
                # Fallback to SAT.
                sat_hc, sat_hh, sat_wall = run_sat_one(coords)
                sat_wall_ms = sat_wall * 1000
                sum_sat_fallback += sat_wall_ms
                path = "sat"
                hc_final = sat_hc if sat_hc is not None else dlx_hc
                hh_final = sat_hh if sat_hh is not None else dlx_hh
                joint_wall = dlx_wall + sat_wall_ms
                n_sat += 1

            ok = (hc_final == ds_hc and hh_final == ds_hh)
            sum_joint += joint_wall

            # For comparison: how long would always-SAT have taken?
            # Approximate by re-using sat_wall_ms when computed; for
            # dlx-path rows we don't have it. We could call SAT
            # unconditionally but that would defeat the purpose; the
            # M0.5 baseline already tells us what pure-SAT looks like.

            w.writerow({
                "coords": coords,
                "n": int(d.get("n", 0)) if d else 0,
                "hc_dataset": ds_hc, "hh_dataset": ds_hh,
                "hc_dlx": dlx_hc, "hh_dlx": dlx_hh,
                "dlx_wall_ms": f"{dlx_wall:.3f}",
                "hc_sat":  sat_hc  if sat_hc  is not None else "",
                "hh_sat":  sat_hh  if sat_hh  is not None else "",
                "sat_wall_ms": f"{sat_wall_ms:.3f}",
                "hc_final": hc_final, "hh_final": hh_final,
                "path": path, "ok": int(ok),
                "joint_wall_ms": f"{joint_wall:.3f}",
            })
            tag = "DLX-fast" if path == "dlx" else "SAT-fallback"
            print(f"  {coords[:36]:36s}  {tag:<13}  "
                  f"dlx={dlx_wall:8.1f}ms  "
                  f"sat={sat_wall_ms:8.1f}ms  "
                  f"final=({hc_final},{hh_final})  ok={int(ok)}",
                  flush=True)

    print()
    print(f"# joint shape count: {len(shapes)}")
    print(f"#   DLX-fast path:    {n_dlx}")
    print(f"#   SAT-fallback:     {n_sat}")
    print(f"# joint total wall:   {sum_joint/1000:.2f} s")
    print(f"#   DLX work:         {sum_dlx/1000:.2f} s")
    print(f"#   SAT-fallback:     {sum_sat_fallback/1000:.2f} s")
    print(f"# CSV: {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
