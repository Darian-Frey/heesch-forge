#!/usr/bin/env python3
"""
M2.5 scaling comparison: DLX-only pipeline vs CMSat baseline.

Joins M0.5 SAT per-shape timings (benchmarks/baseline/results/
m0.5-baseline.jsonl) with M2.4 DLX per-shape timings (CSVs under
benchmarks/hybrid/results/) by polyomino coordinates and emits a
per-shape comparison CSV plus an aggregate summary.

This is the empirical scaling evidence the Phase-2 retrospective
needs: where does DLX-only stop being competitive, and what
"crossover depth" (in terms of Hh) marks the transition? The
companion human-readable write-up is at
results/m2.5-comparison.md.

Usage:
    python benchmarks/hybrid/compare_dlx_sat.py \\
        --baseline benchmarks/baseline/results/m0.5-baseline.jsonl \\
        --dlx-glob 'benchmarks/hybrid/results/m2.4-hybrid-n*.csv' \\
        --out benchmarks/hybrid/results/m2.5-comparison.csv
"""
from __future__ import annotations

import argparse
import csv
import glob
import json
import pathlib
import statistics
import sys


def load_baseline(path: pathlib.Path):
    """Return dict coords_str -> {hh, hc, wall_ms, n, ...}."""
    out = {}
    for line in path.read_text().splitlines():
        if not line.strip():
            continue
        r = json.loads(line)
        out[r["coords"].strip()] = {
            "n": r["n"],
            "hc_sat": r["hc_sat"],
            "hh_sat": r["hh_sat"],
            "hc_dataset": r["hc_dataset"],
            "hh_dataset": r["hh_dataset"],
            "sat_wall_ms": (r["wall_ns"] or 0) / 1e6,
            "sat_calls": r["sat_calls"] or 0,
        }
    return out


def load_dlx_csvs(paths):
    """Return dict coords_str -> {hc, hh, wall_ms, ok, n}.

    The run_hybrid harness's stderr summary line has historically been
    mixed into the CSV files via an over-eager `2>&1`. Skip any line
    starting with `#` so the loader is robust to that.
    """
    out = {}
    for p in paths:
        with open(p) as fh:
            data_lines = [ln for ln in fh if not ln.lstrip().startswith("#")]
        for row in csv.DictReader(data_lines):
            coords = row["coords"].strip()
            out[coords] = {
                "n": int(row["n"]),
                "hc_computed": int(row["hc_computed"]),
                "hh_computed": int(row["hh_computed"]),
                "hc_dataset": int(row["hc_dataset"]),
                "hh_dataset": int(row["hh_dataset"]),
                "dlx_wall_ms": float(row["wall_ms"]),
                "dlx_oracle_calls": int(row["oracle_calls"]),
                "dlx_ok": row["ok"] == "1",
            }
    return out


def percentile(values, p):
    if not values:
        return float("nan")
    s = sorted(values)
    if len(s) == 1:
        return s[0]
    k = (len(s) - 1) * p / 100
    lo = int(k)
    hi = min(lo + 1, len(s) - 1)
    frac = k - lo
    return s[lo] + (s[hi] - s[lo]) * frac


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--baseline", type=pathlib.Path,
                    default=pathlib.Path("benchmarks/baseline/results/m0.5-baseline.jsonl"))
    ap.add_argument("--dlx-glob",
                    default="benchmarks/hybrid/results/m2.4-hybrid-n*.csv")
    ap.add_argument("--out", type=pathlib.Path,
                    default=pathlib.Path("benchmarks/hybrid/results/m2.5-comparison.csv"))
    args = ap.parse_args()

    baseline = load_baseline(args.baseline)
    dlx_files = [pathlib.Path(p) for p in glob.glob(args.dlx_glob)]
    if not dlx_files:
        sys.exit(f"no dlx csv files matched {args.dlx_glob}")
    dlx = load_dlx_csvs(dlx_files)

    common = sorted(set(baseline) & set(dlx),
                    key=lambda c: (dlx[c]["n"], c))
    if not common:
        sys.exit("no overlapping shapes between baseline and dlx data")

    fields = [
        "coords", "n", "hc_dataset", "hh_dataset",
        "hc_sat", "hh_sat", "hc_dlx", "hh_dlx",
        "dlx_ok", "sat_ok",
        "sat_wall_ms", "dlx_wall_ms", "ratio_dlx_over_sat",
        "sat_calls", "dlx_oracle_calls",
    ]
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=fields)
        w.writeheader()
        for coords in common:
            b = baseline[coords]
            d = dlx[coords]
            sat_ok = (b["hc_sat"] == b["hc_dataset"]
                      and b["hh_sat"] == b["hh_dataset"])
            sat_ms = b["sat_wall_ms"]
            dlx_ms = d["dlx_wall_ms"]
            ratio = (dlx_ms / sat_ms) if sat_ms > 0 else float("inf")
            w.writerow({
                "coords": coords, "n": d["n"],
                "hc_dataset": d["hc_dataset"], "hh_dataset": d["hh_dataset"],
                "hc_sat": b["hc_sat"], "hh_sat": b["hh_sat"],
                "hc_dlx": d["hc_computed"], "hh_dlx": d["hh_computed"],
                "dlx_ok": int(d["dlx_ok"]), "sat_ok": int(sat_ok),
                "sat_wall_ms": f"{sat_ms:.3f}",
                "dlx_wall_ms": f"{dlx_ms:.3f}",
                "ratio_dlx_over_sat": f"{ratio:.2f}",
                "sat_calls": b["sat_calls"],
                "dlx_oracle_calls": d["dlx_oracle_calls"],
            })

    # Aggregate summary on stdout.
    sat_walls = [baseline[c]["sat_wall_ms"] for c in common]
    dlx_walls = [dlx[c]["dlx_wall_ms"] for c in common]
    ratios = [d / s if s > 0 else float("inf")
              for s, d in zip(sat_walls, dlx_walls)]

    by_hh = {}
    for c in common:
        h = baseline[c]["hh_dataset"]
        by_hh.setdefault(h, []).append((sat_walls[len(by_hh.get(h, []))], dlx_walls[len(by_hh.get(h, []))]))

    # Recollect by_hh properly.
    by_hh = {}
    by_hc = {}
    for c in common:
        b = baseline[c]
        d = dlx[c]
        by_hh.setdefault(b["hh_dataset"], []).append(
            (b["sat_wall_ms"], d["dlx_wall_ms"], d["dlx_ok"]))
        by_hc.setdefault(b["hc_dataset"], []).append(
            (b["sat_wall_ms"], d["dlx_wall_ms"], d["dlx_ok"]))

    print(f"# M2.5 comparison: DLX-only vs CMSat (n shapes = {len(common)})")
    print(f"# baseline:  {args.baseline}")
    print(f"# dlx files: {len(dlx_files)} ({', '.join(p.name for p in dlx_files)})")
    print(f"# out csv:   {args.out}")
    print()

    print("# overall (ms)")
    print(f"  SAT  total={sum(sat_walls):8.1f}  "
          f"p50={percentile(sat_walls, 50):7.1f}  "
          f"p95={percentile(sat_walls, 95):7.1f}  "
          f"max={max(sat_walls):7.1f}")
    print(f"  DLX  total={sum(dlx_walls):8.1f}  "
          f"p50={percentile(dlx_walls, 50):7.1f}  "
          f"p95={percentile(dlx_walls, 95):7.1f}  "
          f"max={max(dlx_walls):7.1f}")
    finite_ratios = [r for r in ratios if r != float("inf")]
    if finite_ratios:
        print(f"  ratio DLX/SAT  p50={percentile(finite_ratios, 50):.1f}  "
              f"p95={percentile(finite_ratios, 95):.1f}  "
              f"max={max(finite_ratios):.1f}")

    def fmt_group(label, groups):
        print(f"\n# by {label}")
        for k in sorted(groups):
            rows = groups[k]
            sat = [r[0] for r in rows]
            dlx = [r[1] for r in rows]
            n_ok = sum(1 for r in rows if r[2])
            ratio_p50 = percentile(
                [d / s for s, d, _ in rows if s > 0], 50)
            print(f"  {label}={k:>3}  n={len(rows):3d}  "
                  f"dlx_match={n_ok:3d}/{len(rows):<3d}  "
                  f"sat_total_ms={sum(sat):8.1f}  "
                  f"dlx_total_ms={sum(dlx):8.1f}  "
                  f"ratio_p50={ratio_p50:.2f}")

    fmt_group("Hh", by_hh)
    fmt_group("Hc", by_hc)
    return 0


if __name__ == "__main__":
    sys.exit(main())
