# Kaplan-2022 regression suite

Locks the heesch-forge fork's Layer 1 baseline against the published
polyomino Hc values in:

> Craig S. Kaplan. *Heesch numbers of unmarked polyforms.* Contributions to
> Discrete Mathematics 17(2):150–171, 2022.

If a future modification to `src/sat/` changes any Hc reported here, that's
either a bug, an improvement (e.g. resolving an inconclusive), or a
regression — but it's a deviation from upstream by definition. This suite
makes such deviations visible at PR time.

## Prerequisites

- `src/sat/src/sat` built (M0.3, see `src/sat/UPSTREAM.md`).
- `data/kaplan-2022/omino/` populated (M0.4, see
  `data/kaplan-2022/PROVENANCE.md`).
- Python 3.10+ from the standard library only — no `pip install` needed.

## Quick run (default: 7, 8, 11, 12)

```bash
python benchmarks/kaplan/run_regression.py
```

These four files contain 174 shapes total and finish in well under a minute
on a 2021-era ThinkPad. The set is chosen to cover both endpoints of the
M0.4 scope: the smallest n where non-tilers exist (7, 8) and the rare
Hc ≥ 2 shapes at the upper edge of the milestone (11, 12).

## Full M0.4 scope (sizes 7..12)

```bash
python benchmarks/kaplan/run_regression.py --sizes 7,8,9,10,11,12
```

This adds the full non-tiler enumerations for n = 9 (198 shapes) and n = 10
(1390 shapes). Expect multi-hour wall time — `sat` runs CryptoMiniSat once
per shape with default `-maxlevel 7`. Capture the log:

```bash
python benchmarks/kaplan/run_regression.py --sizes 7,8,9,10,11,12 \
  | tee benchmarks/kaplan/results/m0.4-full.log
```

## Beyond M0.4 (n ≥ 13)

The dataset includes Hc ≥ 2 files for n = 13..19. Pass `--sizes 13` etc. to
spot-check specific sizes. The Hc = 3 polyominoes — the current record —
appear at n = 17 (`17omino_2up.txt`) and n = 19. Verifying those is
expensive but high-signal; treat it as a Phase-1 milestone, not an M0.4
prerequisite.

## What the harness does

1. Reads each `data/kaplan-2022/omino/<file>.txt`. Each shape is two lines:
   coordinates, then `Hc = <n> Hh = <m>`.
2. Rewrites the coordinates as sat's `O? <coords>` UNKNOWN-record input
   and writes it to `benchmarks/kaplan/work/<stem>.sat-in`.
3. Runs `sat -isohedral <input> -o <output>`. The `-isohedral` flag makes
   sat run the isohedral-tiling check, so a dataset shape that sat
   classifies as a tiler (it shouldn't — the dataset is non-tilers only)
   would surface as a regression rather than slip through as inconclusive.
4. Parses sat's output by the format documented in
   `src/sat/src/tileio.h:288–337`:
   - `O <coords>\n~ hc hh num_patches` — non-tiler with Heesch numbers.
   - `O <coords>\n! num_patches` — inconclusive (couldn't classify within
     `-maxlevel`).
   - `O <coords>\nI transitivity num_patches` — isohedral tiler.
5. Compares each shape's sat-derived `(hc, hh)` to the dataset annotation
   and reports counts. Exit code 0 iff every shape matches exactly.

## Failure modes worth knowing

- **`isohedral` count > 0.** sat believes a shape Kaplan listed as
  non-tiling actually tiles. Either Kaplan's dataset includes a
  classification error or our build is wrong; both warrant manual
  inspection of the offending shape.
- **`inconclusive` count > 0.** sat hit `-maxlevel` without deciding. The
  dataset was generated with the same default, so this should never
  happen for shapes already in `_*up.txt` files — they were classifiable
  enough to be tabulated. Re-running with `--maxlevel 9` (a future
  harness flag) would settle it.
- **`mismatch` count > 0** with both sides reporting `~`. Real Hc
  disagreement. This is the failure mode the regression is designed to
  catch and demands investigation before any solver change is merged.

## Output layout

- `work/` — sat input/output scratch files. Gitignored.
- `results/` — committed run logs (the `tee`d console output above).
  These are the canonical record of what shape counts each baseline run
  produced; they are referenced from PR descriptions when modifying
  `src/sat/`.
