#!/usr/bin/env python3
"""
M0.4 regression: verify our build of upstream heesch-sat reproduces the
published Hc values from Kaplan's 2022 dataset.

Reads polyomino shapes + their published (Hc, Hh) from the files in
data/kaplan-2022/omino/, feeds them through sat with -isohedral enabled,
parses sat's classification output, and reports per-file pass/fail counts.

Defaults run only sizes 7, 8, 11, 12 because those four files together
contain just 174 shapes and finish in tens of seconds. The full n <= 12
suite (3524 shapes including 1390 dekominoes) is multi-hour and should be
launched explicitly with --sizes 7,8,9,10,11,12.

Exit code: 0 if every shape's sat classification is NONTILER with hc
matching the dataset; 1 otherwise.
"""
from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys
import time

DATASET_ANNOT_RE = re.compile(r"Hc\s*=\s*(\d+)\s+Hh\s*=\s*(\d+)")


def parse_dataset(path: pathlib.Path):
    """Yield (coords_str, dataset_hc, dataset_hh) tuples."""
    lines = path.read_text().rstrip("\n").split("\n")
    if len(lines) % 2 != 0:
        raise ValueError(f"{path}: odd line count, expected coord/annot pairs")
    for i in range(0, len(lines), 2):
        coords = lines[i].strip()
        m = DATASET_ANNOT_RE.search(lines[i + 1])
        if not m:
            raise ValueError(
                f"{path}:{i + 2}: cannot parse annotation {lines[i + 1]!r}"
            )
        yield coords, int(m.group(1)), int(m.group(2))


def make_sat_input(shapes) -> str:
    """Convert dataset shapes to sat's 'O? <coords>' UNKNOWN-record format."""
    return "".join(f"O? {coords}\n" for coords, _, _ in shapes)


def parse_sat_output(text: str):
    """
    Parse sat output. Format (per src/sat/src/tileio.h):
      <grid_abbrev> <coords...>
      <marker> [hc hh | transitivity] <num_patches>
      [<patch_size> <coord pairs>...] * num_patches

    Yields one dict per shape.
    """
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
        record = {"shape_line": line, "marker": marker}
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
                 work_dir: pathlib.Path):
    shapes = list(parse_dataset(data_path))
    in_path = work_dir / (data_path.stem + ".sat-in")
    out_path = work_dir / (data_path.stem + ".sat-out")
    in_path.write_text(make_sat_input(shapes))
    # -isohedral: surface anything Kaplan listed as a non-tiler that our
    #   build now thinks tiles, instead of letting it slip through as
    #   "inconclusive".
    # -hh: compute the Hh (holes-permitted) Heesch number alongside Hc.
    #   Kaplan's dataset reports both; without this flag sat fills in
    #   Hh = Hc and every Hc=N, Hh=N+1 row in the dataset would look like
    #   a mismatch.
    cmd = [str(sat_bin), "-isohedral", "-hh", str(in_path), "-o", str(out_path)]
    t0 = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = time.time() - t0
    if proc.returncode != 0:
        return {
            "file": data_path.name, "shapes": len(shapes),
            "elapsed": elapsed, "fatal": True,
            "stdout": proc.stdout, "stderr": proc.stderr,
        }
    sat_results = list(parse_sat_output(out_path.read_text()))
    matches = mismatches = inconclusive = isohedral_hits = 0
    diffs = []
    if len(sat_results) != len(shapes):
        return {
            "file": data_path.name, "shapes": len(shapes),
            "elapsed": elapsed, "fatal": True,
            "parse_error": (
                f"shape count mismatch: dataset={len(shapes)}, "
                f"sat={len(sat_results)}"
            ),
        }
    for (coords, ds_hc, ds_hh), sat_res in zip(shapes, sat_results):
        m = sat_res["marker"]
        if m == "~":
            if sat_res.get("hc") == ds_hc and sat_res.get("hh") == ds_hh:
                matches += 1
            else:
                mismatches += 1
                diffs.append((coords, (ds_hc, ds_hh),
                              (sat_res.get("hc"), sat_res.get("hh"))))
        elif m == "!":
            inconclusive += 1
            diffs.append((coords, (ds_hc, ds_hh), "inconclusive"))
        elif m == "I":
            isohedral_hits += 1
            diffs.append((coords, (ds_hc, ds_hh),
                          f"I (transitivity={sat_res.get('transitivity')})"))
        else:
            mismatches += 1
            diffs.append((coords, (ds_hc, ds_hh), f"marker={m!r}"))
    return {
        "file": data_path.name, "shapes": len(shapes),
        "matches": matches, "mismatches": mismatches,
        "inconclusive": inconclusive, "isohedral": isohedral_hits,
        "elapsed": elapsed, "diffs": diffs[:25], "fatal": False,
    }


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    repo = pathlib.Path(__file__).resolve().parents[2]
    ap.add_argument("--sat", type=pathlib.Path,
                    default=repo / "src" / "sat" / "src" / "sat",
                    help="path to sat binary (default: %(default)s)")
    ap.add_argument("--data", type=pathlib.Path,
                    default=repo / "data" / "kaplan-2022" / "omino",
                    help="dataset directory (default: %(default)s)")
    ap.add_argument("--work", type=pathlib.Path,
                    default=repo / "benchmarks" / "kaplan" / "work",
                    help="scratch directory for sat I/O (default: %(default)s)")
    ap.add_argument("--sizes", default="7,8,11,12",
                    help="comma-separated polyomino sizes (default: %(default)s)")
    args = ap.parse_args()

    if not args.sat.is_file():
        sys.exit(f"sat binary not found: {args.sat}")
    if not args.data.is_dir():
        sys.exit(f"dataset directory not found: {args.data}")
    args.work.mkdir(parents=True, exist_ok=True)

    sizes = [int(s) for s in args.sizes.split(",") if s.strip()]
    files = []
    for n in sizes:
        for p in sorted(args.data.glob(f"{n:02d}omino_*.txt")):
            files.append(p)
    if not files:
        sys.exit(f"no dataset files matched sizes {sizes} in {args.data}")

    print(f"# heesch-forge M0.4 regression vs Kaplan (2022)")
    print(f"# sat binary: {args.sat}")
    print(f"# dataset:    {args.data}")
    print(f"# sizes:      {sizes}")
    print(f"# files:      {len(files)}")
    print()

    total_match = total_mis = total_inc = total_iso = total_fatal = 0
    total_elapsed = 0.0
    for f in files:
        print(f"-- {f.name} --", flush=True)
        r = run_one_file(args.sat, f, args.work)
        total_elapsed += r["elapsed"]
        if r["fatal"]:
            print(f"  FATAL after {r['elapsed']:.2f}s")
            for k in ("parse_error", "stderr", "stdout"):
                if k in r and r[k]:
                    print(f"  {k}:\n    " + r[k].rstrip().replace("\n", "\n    "))
            total_fatal += 1
            continue
        rate = r["shapes"] / r["elapsed"] if r["elapsed"] > 0 else float("inf")
        print(
            f"  shapes={r['shapes']:5d}  match={r['matches']:5d}  "
            f"mismatch={r['mismatches']:3d}  inconclusive={r['inconclusive']:3d}  "
            f"isohedral={r['isohedral']:3d}  "
            f"{r['elapsed']:7.2f}s  ({rate:.1f}/s)"
        )
        for coords, expected, got in r["diffs"]:
            print(f"    diff: dataset Hc={expected[0]} Hh={expected[1]}  "
                  f"sat={got}  shape: {coords}")
        total_match += r["matches"]
        total_mis += r["mismatches"]
        total_inc += r["inconclusive"]
        total_iso += r["isohedral"]

    print()
    print(f"# overall: match={total_match} mismatch={total_mis} "
          f"inconclusive={total_inc} isohedral={total_iso} "
          f"fatal_files={total_fatal} wall={total_elapsed:.2f}s")
    return 0 if (total_mis == 0 and total_inc == 0
                 and total_iso == 0 and total_fatal == 0) else 1


if __name__ == "__main__":
    sys.exit(main())
