# Phase 3 — lateral grids (bevelhex, octasquare, …)

heesch-sat (`HeeschSatF1`) ships generation and classification code
for several Archimedean and related polyform grids that Kaplan
never published Heesch-number results for. This directory is the
heesch-forge sweep harness over those grids: build, validate, run,
record.

PROPOSAL §5 Layer 4 motivation:

> Search the bevelhex and octasquare grids that Kaplan added but
> never published Heesch results for.

The bevelhex grid is the (4.6.12) Archimedean tiling — square,
hexagon, dodecagon meeting at every vertex. The octasquare grid is
the (4.8.8) tiling — square + regular octagon. Both are supported
by upstream's `gen` and `sat` tools but have no published H-value
catalogue. Phase 3 produces one.

## Status

- **M3.1 (bevelhex generation verified, n = 4..7)** — closed;
  results in `results/m3.1-bevelhex-sweep.csv`. Discussion
  below.
- **M3.2 (bevelhex extended, n = 4..10)** — closed; full sweep
  in `results/m3.2-bevelhex-extended.csv`. **Two Hc = 2
  bevelhex polyforms discovered** (n = 9 and n = 10), the
  project's first non-trivial Heesch-number result on a
  non-omino / non-hex / non-iamond grid. Catalogues at
  `results/m3.2-bevelhex-hc1.txt` (315 shapes) and
  `results/m3.2-bevelhex-hc2.txt` (2 shapes).
- **M3.3 (octasquare sweep, s + o ≤ 6)** — closed; results in
  `results/m3.3-octasquare-sweep.csv`. Discussion below.
- **M3.3-followup (octasquare extension, s + o ≤ 10)** — closed;
  results in `results/m3.3-followup-octasquare-extended.csv`.
  **20 Hc = 2 octasquare polyforms discovered** at s + o = 7
  (smallest), 9, and 10. Catalogue at
  `results/m3.3-followup-octasquare-hc2.txt`.
- **M3.4 (Hc = 4 catalogue analogue)** — closed; cross-grid
  table at `M3.4-cross-grid-catalogue.md`. Updated in light of
  the M3.3-followup to include octasquare's first-Hc = 2 size
  at s + o = 7.

## M3.1 — bevelhex sweep, sizes 4–7

The canonical run is reproducible via `bash run_bevelhex_sweep.sh`
(defaults to sizes 4..7; pass sizes as positional arguments to
override). Output is a per-size CSV and a log with `sat`'s raw
classification stream.

| size | shapes | wall    | Hc = 0 | Hc = 1 | Hc = 2 | iso | inconc |
|-----:|-------:|--------:|-------:|-------:|-------:|----:|-------:|
|  4   |     49 |  0.021s |     44 |      5 |      0 |   0 |      0 |
|  5   |    255 |  0.083s |    244 |     11 |      0 |   0 |      0 |
|  6   |  1,327 |  2.504s |  1,252 |      3 |      0 |  65 |      7 |
|  7   |  7,796 |  1.014s |  7,780 |     16 |      0 |   0 |      0 |

(Walls are single-thread on a ThinkPad P15 Gen 2i; absolute
numbers will differ on other hardware.)

### Findings

1. **Upstream's bevelhex code works as-is.** No bugs found. `gen
   -bevelhex -size n -free` produces non-empty shape sets at every
   tested size, and `sat -isohedral -hh` classifies them without
   error. M3.1's literal task ("verify Kaplan's bevelhex grid
   generation code; fix bugs if found") is closed with a no-bugs
   report.
2. **Hc ≥ 2 not found at sizes 4–7.** 35 Hc = 1 shapes total
   across all four sizes. The full list of Hc = 1 shapes is at
   `results/m3.1-bevelhex-hc1.txt` (~5+11+3+16 lines, one per
   shape, in `gen` output format). All Hc and Hh are 1; no
   shape walked back to a hole-free 2-corona via heesch-sat's
   default search.
3. **Size-6 wall is dominated by isohedral checks.** 65
   isohedral tilers and 7 inconclusives at size 6 take ~2.5 s,
   while size 7's 7 796 shapes (no isohedral tilers in the
   sample) finish in 1 s. Isohedral verification is the
   expensive piece; size-7 happens to have none.
4. **Push to bigger n is the natural M3.2 work.** The Kaplan
   omino dataset's first Hc = 2 polyomino is at n = 11; bevelhex's
   per-shape geometric complexity is comparable, so Hc = 2
   bevelhex polyforms (if any) are plausibly at n ≈ 8–12.
   Generation at n = 8 would produce ≈ 50 k shapes; at the
   sat-classification rates above (~10⁴/s when no isohedral
   checks), that is a few seconds of wall. Worth running before
   declaring "no Hc ≥ 2 bevelhex polyforms exist at modest n."

### What we did NOT find

No isohedral tilers at sizes 4, 5, 7. Sixty-five at size 6.
That is consistent with the way upstream's isohedrality test
fires: it requires a complete corona with translational closure,
which most small bevelhex shapes do not have. The seven size-6
inconclusives ran past `sat`'s default `-maxlevel 7` without
deciding; with `--maxlevel 9` or higher they may resolve, but
they are not Heesch candidates either way.

## Reproducing

```bash
( cd src/sat/src && make gen sat )      # one-time
bash benchmarks/lateral/run_bevelhex_sweep.sh         # default sizes 4..7
bash benchmarks/lateral/run_bevelhex_sweep.sh 8 9     # extend
```

`bvh_n*.in` and `bvh_n*.out` are intermediate files written by
the script; they are gitignored because they are regenerable from
the script and the small ones add ~700 kB total. `m3.1-bevelhex-
sweep.csv`, `m3.1-bevelhex-sweep.log`, and `m3.1-bevelhex-hc1.txt`
are the canonical record and are committed.

## M3.2 — bevelhex extension to n = 10

The M3.1 sweep was extended to n ∈ {8, 9, 10} via the same
`run_bevelhex_sweep.sh` harness; the canonical extended CSV is
`results/m3.2-bevelhex-extended.csv`. Cumulative numbers (n = 4 → 10):

| n | shapes | wall    | Hc = 0 | Hc = 1 | Hc = 2 | iso | inconc |
|--:|-------:|--------:|-------:|-------:|-------:|----:|-------:|
| 4 |     49 |  0.019s |     44 |      5 |      0 |   0 |      0 |
| 5 |    255 |  0.082s |    244 |     11 |      0 |   0 |      0 |
| 6 |  1,327 |  2.545s |  1,252 |      3 |      0 |  65 |      7 |
| 7 |  7,796 |  1.011s |  7,780 |     16 |      0 |   0 |      0 |
| 8 | 45,876 |  6.016s | 45,835 |     41 |      0 |   0 |      0 |
| 9 | 278,002 | 36.717s | 277,958 |    43 |  **1** |   0 |      0 |
| 10 | 1,697,278 | 233.501s | 1,697,081 |  196 |  **1** |   0 |      0 |
| **total** | **2,030,583** | ~280 s | **2,030,194** | **315** | **2** | **65** | **7** |

### Hc = 2 bevelhex polyforms

Two distinct Hc = 2 = Hh = 2 bevelhex polyforms found in the
sweep, in `gen` output coordinates (committed at
`results/m3.2-bevelhex-hc2.txt`):

```text
n = 9:   B  9 -12   8 -10   9  -9  12  -9  10  -8   3  -6   6  -6   2  -4   0   0
n = 10:  B  9  -3  10  -2   0   0   3   0   6   0   9   0   2   2   8   2   0   3   6   3
```

These are **the first known Hc ≥ 2 bevelhex polyforms.** Kaplan
2022 (`KaplanA8`) catalogued Hc up to 4 on polyhex / polyiamond
and up to 3 on polyomino; the (4.6.12) bevelhex grid was
generation-supported in heesch-sat but had no published
Heesch-number results before this sweep.

### What was not found

No Hc ≥ 3 bevelhex polyforms through n = 10. By analogy with
Kaplan's polyomino catalogue (where the first Hc = 2 polyomino
appears at n = 11 and the first Hc = 3 polyomino at n = 17),
Hc = 3 bevelhex polyforms (if any) plausibly need n ≥ 12.
Extending the sweep to n = 11 would take ~25 minutes of wall on
this hardware (extrapolating from the n = 10 throughput of
~7,300 shapes/s and the size-9-to-size-10 multiplier of ≈ 6×);
n = 12 ≈ 2.5 h. Both are tractable but expensive enough to
sit under "M3.2-followup" rather than blocking M3.4.

### Throughput notes

Generation + classification scales near-linearly with shape
count from n = 7 onwards: ~7,500-8,000 shapes/s. The size-6
walls were dominated by the 65 isohedral checks rather than the
non-tiler classification cost; once shapes get above n = 7 those
do not appear, so wall correlates cleanly with the shape-count
multiplier (× 5.9 from n = 8 → 9, × 6.1 from n = 9 → 10).

## M3.3 — octasquare sweep, (s, o) with s + o ≤ 6

The octasquare grid (Archimedean (4.8.8) — squares + regular
octagons) needs a 2-D sweep: `gen -octasquare -sizes <s>,<o>`
takes the per-shape-class counts independently. The harness at
`run_octasquare_sweep.sh` defaults to all (s, o) with s, o ≥ 1
and s + o ∈ {2, …, 6}.

Aggregate over the 14 non-empty (s, o) pairs (463 shapes,
1.5 s wall):

| s + o | shapes | Hc = 0 | Hc = 1 | Hc = 2 | iso | inconc |
|------:|-------:|-------:|-------:|-------:|----:|-------:|
| 2     |      1 |      0 |      0 |      0 |   1 |      0 |
| 3     |      5 |      2 |      3 |      0 |   0 |      0 |
| 4     |     17 |      9 |      0 |      0 |   8 |      0 |
| 5     |     79 |     54 |     25 |      0 |   0 |      0 |
| 6     |    361 |    271 |     12 |      0 |  78 |      0 |
| **total** | **463** | **336** | **40** | **0** | **87** | **0** |

(One (5, 1) pair returned 0 shapes — `gen` reports no valid free
polyforms with five squares and one octagon at this Archimedean
grid; recorded in the CSV.)

### Findings

1. **Octasquare is much "denser" than bevelhex.** Hc = 1 rate
   is 40/463 = 8.6 % vs bevelhex's 35/9,427 = 0.37 %. Isohedral
   tiler rate is 87/463 = 18.8 % vs bevelhex's 65/9,427 =
   0.69 %. Most octasquare shapes are interesting (tile,
   surround, or both); most bevelhex shapes are trivial
   non-tilers.
2. **(3, 3) is dominated by tilers.** 78 of 80 (3,3)
   octasquares tile isohedrally. The other 2 are Hc = 0 and
   Hc = 1.
3. **No Hc ≥ 2 found through s + o = 6.** The 40 Hc = 1
   shapes are catalogued at `results/m3.3-octasquare-hc1.txt`,
   one per line in `gen` output format with `o` prefix
   (lowercase, octasquare's grid abbreviation in upstream
   tileio.h).
4. **Wall scales gently.** The hardest single (s, o) was
   (2, 3) with 36 shapes and 0.65 s. (2, 4) had 164 shapes
   and 0.29 s. The 6-cell sweep finished in well under a
   second total — extending to s + o ≤ 8 or 10 is a few-
   minute job, well within an interactive session.

### Bevelhex vs octasquare: comparison

| metric                | bevelhex (M3.1) | octasquare (M3.3) |
|-----------------------|----------------:|------------------:|
| size sweep            | n ∈ {4, …, 7}    | s + o ∈ {2, …, 6} |
| total shapes          |           9,427 |               463 |
| Hc = 0                |   9,320 (98.9 %) |       336 (72.6 %) |
| Hc = 1                |        35 (0.37 %) |        40 (8.6 %) |
| Hc ≥ 2                |               0 |                 0 |
| isohedral             |        65 (0.69 %) |        87 (18.8 %) |
| inconclusive          |        7 (0.07 %) |                 0 |

The two grids differ qualitatively. Octasquare's local geometry
(square + octagon, with each square sandwiched between octagons)
admits many more isohedral-tiling configurations and
correspondingly fewer pure non-tilers per shape. Bevelhex's
local geometry (the 4.6.12 vertex configuration is mixed) is
much less "tiling-friendly" at small sizes.

Neither grid has produced an Hc ≥ 2 shape yet. The Kaplan-2022
polyomino catalogue's first Hc = 2 entries are at n = 11; if
analogous behaviour holds on these grids, Hc ≥ 2 octasquares
might appear around s + o = 7–10 and Hc ≥ 2 bevelhex polyforms
around n = 8–12. Pushing the sweep to those sizes is M3.2
(bevelhex) and a corresponding extension of M3.3 (octasquare).

## What's next under M3

- **M3.2 — Enumerate bevelhex polyforms up to n ≈ 12.**
  Beyond M3.1's smoke-tests. Where do Hc = 2 bevelhex
  polyforms first appear? If the answer is "nowhere, even at
  n = 12," that is a publishable lateral result on its own.
- **M3.3 — Octasquare polyforms similarly.** `gen
  -octasquare -sizes <a,b>` generates poly-(4.8.8) tiles
  where `a` is squares and `b` is octagons; the same harness
  pattern applies, but the size-pair sweep is two-dimensional.
- **M3.4 — Tabulate analogues of Kaplan's polyhex / iamond
  Hc = 4 catalogue.** The polyhex/iamond record is Hc = 4
  (Kaplan 2022); a comparable result on bevelhex or
  octasquare would be a new published Heesch number for those
  grids.

Phase 3 is independent of Phase 4 (RL pilot) and can run in
parallel; both depend only on the v1 CMSat solver from M0.5.
