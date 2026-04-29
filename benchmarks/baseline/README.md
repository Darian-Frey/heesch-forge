# M0.5 baseline harness

Records per-shape performance metrics for our heesch-sat fork over Kaplan's
2022 polyomino dataset: wall time, SAT conflicts, decisions, propagations,
SAT-solve count, corona depth reached, and dataset-vs-sat agreement on Hc/Hh.

The numbers in this directory's logs are the headroom against which Phase 1
("≥5× speedup") is graded. Any change to `src/sat/` that claims a speedup
must come with a comparable run here so the comparison is apples-to-apples.

## Prerequisites

- M0.3 build of `src/sat/src/sat` *with M0.5 instrumentation*. The patched
  `sat` accepts `-stats <file>` and emits one JSON object per processed
  shape; the patch lives in `src/sat/src/sat.cpp` and
  `src/sat/src/heesch.h::runAndAccount`. If you're on an unpatched build
  the harness will exit fatal at the first file because the stats stream
  won't exist.
- Kaplan dataset extracted at `data/kaplan-2022/omino/` (M0.3 / M0.4).
- Python 3.10+ stdlib only.

## Quick run (default sizes 7, 8, 11, 12)

```bash
python benchmarks/baseline/run_baseline.py
```

Same 174-shape set as the M0.4 regression. Wall on a ThinkPad P15 Gen 2i is
about 4–5 minutes. Writes the per-shape JSONL to
`benchmarks/baseline/results/m0.5-baseline.jsonl` and prints a per-file +
overall summary on stdout.

To preserve the run as a committed baseline:

```bash
python benchmarks/baseline/run_baseline.py \
  | tee benchmarks/baseline/results/m0.5-baseline-7-8-11-12.log
```

## Wider runs

```bash
# Full M0.4 dataset scope (adds 1390-shape dekomino set; multi-hour).
python benchmarks/baseline/run_baseline.py --sizes 7,8,9,10,11,12 \
  --out benchmarks/baseline/results/m0.5-full.jsonl \
  | tee benchmarks/baseline/results/m0.5-full.log

# Probe Hc=3 polyominoes (n=17 and n=19 — current Heesch record).
# Each shape can take many minutes; budget hours-to-overnight.
python benchmarks/baseline/run_baseline.py --sizes 17,19 \
  --out benchmarks/baseline/results/m0.5-record.jsonl
```

## JSONL record schema

One object per shape, in dataset order. Field-by-field:

| field | type | source | meaning |
|-------|------|--------|---------|
| `file` | string | dataset filename | which `omino/*.txt` produced this shape |
| `idx` | int | dataset position (0-based) | for round-tripping back to the dataset row |
| `n` | int | sat `-stats` (`info.getShape().size()`) | number of cells |
| `coords` | string | dataset line | original Kaplan coordinates, unmodified |
| `hc_dataset` | int | dataset annotation | Kaplan's published Hc |
| `hh_dataset` | int | dataset annotation | Kaplan's published Hh |
| `hc_sat` | int? | sat output (`~ hc hh` line) | this build's Hc, or `null` if not classified as nontiler |
| `hh_sat` | int? | sat output | this build's Hh |
| `classification` | string | sat output marker | `~`=nontiler, `I`=isohedral, `!`=inconclusive, `O`=hole, `?`=unknown |
| `levels` | int | sat `-stats` | highest corona level attempted (`HeeschSolver::getLevel()` post-solve) |
| `wall_ns` | int | sat `-stats` | wall-clock duration of `HeeschSolver::solve()` in nanoseconds |
| `sat_calls` | int | sat `-stats` | number of `CMSat::SATSolver::solve()` invocations |
| `sat_conflicts` | int | sat `-stats` (`get_last_conflicts` summed) | conflict count across all SAT calls |
| `sat_decisions` | int | sat `-stats` | decision count across all SAT calls |
| `sat_propagations` | int | sat `-stats` | propagation count across all SAT calls |
| `ok` | bool | derived | true iff `(hc_sat, hh_sat) == (hc_dataset, hh_dataset)` and classification is `~` |

Stats are aggregated across all SAT-solver instances reachable from
`HeeschSolver::solve()` (the default, non-failsafe code path). The
`-old` / `-failsafe` mode is *not* instrumented; baseline runs use the
default path.

## What "wall_ns" measures

The clock starts immediately before `HeeschSolver::solve()` and stops
immediately after. It includes:
- All `getClauses` / `new_vars` clause construction.
- All CMS solve calls.
- The corona-walkback search inside the solver.
- Calls to `getSolution` if `-show` was passed (the harness does *not*
  pass `-show`, so this cost is excluded).

It excludes:
- Process startup, input parsing, output writing.
- Dataset loading and JSONL serialisation in the harness itself.

For 174-shape runs the harness wraps around 0.5 s of process overhead
on top of the summed `wall_ns` total — negligible at this scale.

## Reproducibility

The harness is deterministic given the same `sat` binary and CryptoMiniSat
build. CryptoMiniSat is not bit-for-bit deterministic between threads, but
sat does not enable threading (no `set_num_threads` call), so for our runs
the reported counters are stable across invocations. If a future change
introduces threading, expect ±a few percent on conflict / decision counts.

## Output layout

- `work/` — sat input/output/stats scratch files. Gitignored.
- `results/` — committed canonical run logs (`*.log`) and JSONL
  (`*.jsonl`). When updating any of these, also update the corresponding
  status row in `ROADMAP.md`.
