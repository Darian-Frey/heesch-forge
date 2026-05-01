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

- **M3.1 (bevelhex generation verified)** — closed; results in
  `results/m3.1-bevelhex-sweep.csv` and discussion below.
- M3.2 (bevelhex enumeration up to a useful $n$) — see
  ROADMAP for the open scope.
- M3.3 (octasquare) — pending.
- M3.4 (Hc = 4 catalogue analogue) — pending.

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
