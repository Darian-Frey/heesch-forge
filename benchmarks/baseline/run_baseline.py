#!/usr/bin/env python3
"""
M0.5 baseline harness: run heesch-sat over the Kaplan-2022 polyomino
dataset and record per-shape performance metrics — wall time, SAT
conflicts, decisions, propagations, and corona depth.

This is the headroom-measurement that Phase 1 ('>=5x speedup') will be
graded against. Output is a JSON-Lines log: one record per shape with
everything needed to slice by polyomino size, by Hc, or by per-shape
hardness.

Sat must be the patched build from M0.5 that supports `-stats <file>`
(see src/sat/src/sat.cpp:65 and src/sat/src/heesch.h::runAndAccount).
Reference dataset lives under data/kaplan-2022/omino/; its provenance is
in PROVENANCE.md.

Defaults to sizes 7, 8, 11, 12 -- same set the M0.4 regression locks in
under a few minutes. Pass `--sizes` for wider runs.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import statistics
import subprocess
import sys
import time

DATASET_ANNOT_RE = re.compile(r"Hc\s*=\s*(\d+)\s+Hh\s*=\s*(\d+)")


def parse_dataset(path: pathlib.Path):
    """Yield (coords_str, hc, hh) per shape in a Kaplan dataset file."""
    lines = path.read_text().rstrip("\n").split("\n")
    if len(lines) % 2 != 0:
        raise ValueError(f"{path}: odd line count, expected coord/annot pairs")
    for i in range(0, len(lines), 2):
        coords = lines[i].strip()
        m = DATASET_ANNOT_RE.search(lines[i + 1])
        if not m:
            raise ValueError(f"{path}:{i + 2}: cannot parse {lines[i + 1]!r}")
        yield coords, int(m.group(1)), int(m.group(2))


def parse_sat_output(text: str):
    """Yield one classification dict per shape in sat's main output."""
    lines = text.rstrip("\n").split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        if not line or line[0] not in "OHIKABDXTC":
            i += 1
            continue
        if i + 1 >= len(lines):
            break
        cls = lines[i + 1]
        marker = cls[0] if cls else "?"
        rest = cls[1:].split()
        try:
            tail = [int(t) for t in rest]
        except ValueError:
            tail = []
        record = {"marker": marker}
        if marker == "~" and len(tail) >= 2:
            record["hc"], record["hh"] = tail[0], tail[1]
            num_patches = tail[2] if len(tail) > 2 else 0
        elif marker in ("I", "#") and len(tail) >= 1:
            record["transitivity"] = tail[0]
            num_patches = tail[1] if len(tail) > 1 else 0
        else:
            num_patches = tail[-1] if tail else 0
        i += 2
        for _ in range(num_patches):
            if i >= len(lines):
                break
            try:
                sz = int(lines[i].strip())
            except ValueError:
                break
            i += 1 + sz
        yield record


def run_one_file(sat_bin: pathlib.Path, data_path: pathlib.Path,
                 work_dir: pathlib.Path, jsonl_out):
    shapes = list(parse_dataset(data_path))
    in_path = work_dir / (data_path.stem + ".sat-in")
    out_path = work_dir / (data_path.stem + ".sat-out")
    stats_path = work_dir / (data_path.stem + ".stats.jsonl")
    in_path.write_text("".join(f"O? {c}\n" for c, _, _ in shapes))
    cmd = [str(sat_bin), "-isohedral", "-hh",
           "-stats", str(stats_path),
           str(in_path), "-o", str(out_path)]
    t0 = time.perf_counter()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    wall = time.perf_counter() - t0
    if proc.returncode != 0:
        print(f"  FATAL: sat exited {proc.returncode}", flush=True)
        if proc.stderr:
            print("    " + proc.stderr.rstrip().replace("\n", "\n    "))
        return None

    sat_records = list(parse_sat_output(out_path.read_text()))
    stats_records = [json.loads(ln) for ln in
                     stats_path.read_text().splitlines() if ln.strip()]

    if not (len(shapes) == len(sat_records) == len(stats_records)):
        print(f"  FATAL: length mismatch dataset={len(shapes)} "
              f"out={len(sat_records)} stats={len(stats_records)}", flush=True)
        return None

    summary = {
        "shapes": 0, "match": 0, "mismatch": 0,
        "wall_ns": 0, "sat_conflicts": 0, "sat_decisions": 0,
        "sat_propagations": 0, "sat_calls": 0,
        "wall_ns_per_shape": [],
    }
    for idx, ((coords, ds_hc, ds_hh), sat_rec, stat_rec) in enumerate(
            zip(shapes, sat_records, stats_records)):
        ok = (sat_rec.get("marker") == "~"
              and sat_rec.get("hc") == ds_hc
              and sat_rec.get("hh") == ds_hh)
        record = {
            "file": data_path.name,
            "idx": idx,
            "n": stat_rec.get("n"),
            "coords": coords,
            "hc_dataset": ds_hc,
            "hh_dataset": ds_hh,
            "hc_sat": sat_rec.get("hc"),
            "hh_sat": sat_rec.get("hh"),
            "classification": sat_rec.get("marker"),
            "levels": stat_rec.get("levels"),
            "wall_ns": stat_rec.get("wall_ns"),
            "sat_calls": stat_rec.get("sat_calls"),
            "sat_conflicts": stat_rec.get("sat_conflicts"),
            "sat_decisions": stat_rec.get("sat_decisions"),
            "sat_propagations": stat_rec.get("sat_propagations"),
            "ok": ok,
        }
        jsonl_out.write(json.dumps(record, separators=(",", ":")) + "\n")
        summary["shapes"] += 1
        summary["match" if ok else "mismatch"] += 1
        for k in ("wall_ns", "sat_conflicts", "sat_decisions",
                  "sat_propagations", "sat_calls"):
            summary[k] += record[k] or 0
        summary["wall_ns_per_shape"].append(record["wall_ns"] or 0)

    summary["proc_wall_s"] = wall
    return summary


def fmt_ns(ns: int) -> str:
    if ns is None:
        return "n/a"
    if ns >= 1_000_000_000:
        return f"{ns/1e9:7.2f} s"
    if ns >= 1_000_000:
        return f"{ns/1e6:7.2f} ms"
    return f"{ns/1e3:7.2f} us"


def print_file_summary(name: str, s):
    if s is None:
        return
    per = s["wall_ns_per_shape"]
    p50 = int(statistics.median(per)) if per else 0
    p95 = int(statistics.quantiles(per, n=20)[18]) if len(per) >= 20 else 0
    pmax = max(per) if per else 0
    print(
        f"  {name:24s}  shapes={s['shapes']:5d}  ok={s['match']:5d}  "
        f"err={s['mismatch']:3d}  "
        f"wall_total={s['wall_ns']/1e9:7.2f}s  "
        f"p50={fmt_ns(p50)}  p95={fmt_ns(p95)}  pmax={fmt_ns(pmax)}  "
        f"conflicts={s['sat_conflicts']:>10d}  "
        f"decisions={s['sat_decisions']:>10d}"
    )


def main() -> int:
    repo = pathlib.Path(__file__).resolve().parents[2]
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sat", type=pathlib.Path,
                    default=repo / "src" / "sat" / "src" / "sat",
                    help="path to sat (default: %(default)s)")
    ap.add_argument("--data", type=pathlib.Path,
                    default=repo / "data" / "kaplan-2022" / "omino",
                    help="dataset directory (default: %(default)s)")
    ap.add_argument("--work", type=pathlib.Path,
                    default=repo / "benchmarks" / "baseline" / "work",
                    help="scratch dir (default: %(default)s)")
    ap.add_argument("--out", type=pathlib.Path,
                    default=repo / "benchmarks" / "baseline" / "results"
                                / "m0.5-baseline.jsonl",
                    help="aggregated JSONL output (default: %(default)s)")
    ap.add_argument("--sizes", default="7,8,11,12",
                    help="comma-separated polyomino sizes (default: %(default)s)")
    args = ap.parse_args()

    if not args.sat.is_file():
        sys.exit(f"sat binary not found: {args.sat}")
    args.work.mkdir(parents=True, exist_ok=True)
    args.out.parent.mkdir(parents=True, exist_ok=True)

    sizes = [int(s) for s in args.sizes.split(",") if s.strip()]
    files = []
    for n in sizes:
        for p in sorted(args.data.glob(f"{n:02d}omino_*.txt")):
            files.append(p)
    if not files:
        sys.exit(f"no dataset files matched sizes {sizes} in {args.data}")

    print(f"# heesch-forge M0.5 baseline (Kaplan-2022 polyominoes)")
    print(f"# sat:   {args.sat}")
    print(f"# data:  {args.data}")
    print(f"# sizes: {sizes}")
    print(f"# out:   {args.out}")
    print()

    overall = {
        "shapes": 0, "match": 0, "mismatch": 0,
        "wall_ns": 0, "sat_conflicts": 0, "sat_decisions": 0,
        "sat_propagations": 0, "sat_calls": 0,
        "wall_ns_per_shape": [],
    }

    with args.out.open("w") as jsonl_out:
        for f in files:
            print(f"-- {f.name} --", flush=True)
            s = run_one_file(args.sat, f, args.work, jsonl_out)
            print_file_summary(f.name, s)
            if s is None:
                continue
            for k in ("shapes", "match", "mismatch", "wall_ns",
                      "sat_conflicts", "sat_decisions",
                      "sat_propagations", "sat_calls"):
                overall[k] += s[k]
            overall["wall_ns_per_shape"].extend(s["wall_ns_per_shape"])

    print()
    print(f"# overall: shapes={overall['shapes']} ok={overall['match']} "
          f"err={overall['mismatch']}")
    print(f"# wall_total = {overall['wall_ns']/1e9:.2f} s "
          f"(across {overall['shapes']} shapes)")
    if overall["wall_ns_per_shape"]:
        per = overall["wall_ns_per_shape"]
        p50 = int(statistics.median(per))
        p95 = (int(statistics.quantiles(per, n=20)[18])
               if len(per) >= 20 else max(per))
        pmax = max(per)
        print(f"# per-shape wall: p50={fmt_ns(p50)}  "
              f"p95={fmt_ns(p95)}  pmax={fmt_ns(pmax)}")
    print(f"# total SAT conflicts:    {overall['sat_conflicts']:>14,d}")
    print(f"# total SAT decisions:    {overall['sat_decisions']:>14,d}")
    print(f"# total SAT propagations: {overall['sat_propagations']:>14,d}")
    print(f"# total SAT solve calls:  {overall['sat_calls']:>14,d}")
    return 0 if overall["mismatch"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
