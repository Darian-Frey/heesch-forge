# Phase 3 — Heesch numbers on the bevelhex and octasquare grids

*heesch-forge / Layer 4 retrospective + short empirical paper.
Cut at commit `ad4dae5` of `main`, 1 May 2026.* This is the M3.5
deliverable; ROADMAP "Phase 3 / Milestones" treats it as the
short empirical paper that pulls M3.1–M3.4 into a
publication-ready draft.

## Abstract

Kaplan (2022; `KaplanA8`) catalogued Heesch numbers up to 4 on
polyhex and polyiamond, and up to 3 on polyomino, via an
exhaustive SAT-based search. The same code's `gen` and `sat`
binaries already support generation and classification on two
further grids — the (4.6.12) bevelhex Archimedean tiling and
the (4.8.8) octasquare tiling — but no published
Heesch-number results existed for either prior to this
report.

heesch-forge has run that search to first principles. Section
3 below records the bevelhex sweep through n = 10 (just over
two million free polyforms classified in five minutes wall on
a 2021 ThinkPad workstation) and the octasquare sweep through
total cell count s + o = 6 (463 free polyforms in 1.5 s).
Section 4 places both results next to Kaplan's published
catalogues to produce a five-grid comparison table.

The headline empirical findings:

- **Two Hc = 2 bevelhex polyforms exist**, found at n = 9 and
  n = 10. Both have Hh = 2 (no holes in either corona of the
  best chain). These are the first known Hc ≥ 2 polyforms on
  the (4.6.12) grid.
- **No Hc ≥ 3 polyforms are found on bevelhex through n = 10**,
  and **no Hc ≥ 2 polyforms are found on octasquare through
  s + o = 6**. By analogy with Kaplan's catalogues these
  thresholds plausibly lift one or two sizes higher; the
  compute cost to verify is bounded (~25 min for bevelhex
  n = 11, ~2.5 h for n = 12, single-digit minutes for
  octasquare s + o = 7..10).

Section 5 is the cross-grid analysis: polyhex remains the
most "Heesch-friendly" grid Kaplan studied (Hc = 4 at n = 11);
polyiamond is intermediate (n = 20); polyomino is the most
restrictive (no Hc = 4 found through n = 19, an open
question — RQ1 of this project). Bevelhex sits between
polyhex and polyiamond on Hc = 2 and is open at Hc = 3 / 4.

The classification is done with unmodified upstream
heesch-sat at SHA `1adb3720`; no algorithmic novelty is
claimed. The contribution is empirical — a small but
genuinely new dataset on two grids the literature had not yet
reached.

## 1. Background

A polyform's Heesch number `Hc(S)` is the largest k such that
some configuration of k complete coronas of congruent copies
of S surrounds it without holes. Whether every positive
integer is achievable as a Heesch number is open (Heesch
1968; `HeeschA1`).

Kaplan (2022; `KaplanA8`) computed `Hc` exhaustively on three
grids:

- **Polyomino** — the unit-square grid Z². The published
  record is **Hc = 3** at n = 17 (two shapes) and n = 19.
  No Hc ≥ 4 polyomino is known.
- **Polyhex** — the regular hexagonal lattice. The published
  record is **Hc = 4**, first appearing at n = 11.
- **Polyiamond** — the regular triangular lattice. The
  published record is also **Hc = 4**, first appearing at
  n = 20.

heesch-sat ships generation (`gen`) and classification
(`sat`) tools that also support two further grids:

- **Bevelhex (4.6.12)** — the Archimedean tiling whose
  vertices have one square, one hexagon, and one
  dodecagon. `gen -bevelhex` produces "polyforms" that
  are connected unions of cells from the (4.6.12)
  tiling.
- **Octasquare (4.8.8)** — the Archimedean tiling whose
  vertices have one square and two octagons. `gen
  -octasquare -sizes <s>,<o>` produces polyforms with `s`
  square cells and `o` octagonal cells.

Kaplan's 2022 paper does not include results on either
grid. Whether either of them admits Heesch ≥ 2 polyforms,
and at what size, is the question this report answers.

## 2. Method

The pipeline is unmodified upstream heesch-sat at vendor
SHA `1adb3720` (committed at `src/sat/`):

1. `gen -<grid> -<size args> -free > shapes.in` enumerates
   the free (i.e. up to D₄ / D₆ / etc. symmetry depending on
   grid) polyforms at the requested sizes.
2. `sat -isohedral -hh shapes.in -o shapes.out` classifies
   each shape into one of {nontiler, isohedral tiler,
   anisohedral tiler — never produced by upstream's
   detection — , inconclusive}. For nontilers, sat reports
   `Hc` and `Hh` (the Heesch number permitting holes in the
   outer corona).
3. The harnesses at `benchmarks/lateral/run_bevelhex_sweep.sh`
   and `benchmarks/lateral/run_octasquare_sweep.sh` orchestrate
   the per-(grid, size) loop, tally Hc / Hh / isohedral /
   inconclusive counts, and write per-(grid, size) CSVs and
   per-Hc-class shape catalogues.

No algorithmic modification to heesch-sat was made for this
report. The Phase-1 retrospective
(`paper/drafts/phase1-engineering-negatives.md`) established
that no straightforward solver-side improvement to the
upstream pipeline beats it on the workload it was designed
for; running it as-is is the right choice for this lateral
sweep.

Hardware: ThinkPad P15 Gen 2i, Ubuntu 24.04, g++ 13.3,
CryptoMiniSat 5.14 (built from source). Single thread per
binary. Absolute walls reported below will differ on other
hardware; relative ratios should be stable.

## 3. Results

### 3.1 Bevelhex sweep (M3.1 + M3.2)

`benchmarks/lateral/run_bevelhex_sweep.sh` invokes
`gen -bevelhex -size n -free` followed by `sat -isohedral -hh`
for each requested n. Below is the cumulative per-n
breakdown, n = 4..10, on a single thread:

```text
   n      shapes   wall      Hc=0    Hc=1   Hc=2   iso   inconc
   4          49   0.019s       44      5      0     0        0
   5         255   0.082s      244     11      0     0        0
   6       1,327   2.545s    1,252      3      0    65        7
   7       7,796   1.011s    7,780     16      0     0        0
   8      45,876   6.016s   45,835     41      0     0        0
   9     278,002  36.717s  277,958     43    *1*     0        0
  10   1,697,278 233.501s 1,697,081   196    *1*     0        0
   total 2,030,583 ~280 s 2,030,194    315      2     65        7
```

**Two Hc = 2 = Hh = 2 bevelhex polyforms found**, one at
n = 9 and one at n = 10. Their cell coordinates in upstream's
`gen` output format:

```text
n =  9:  B  9 -12   8 -10   9  -9  12  -9  10  -8   3  -6   6  -6   2  -4   0   0
n = 10:  B  9  -3  10  -2   0   0   3   0   6   0   9   0   2   2   8   2   0   3   6   3
```

These are heesch-forge's first non-trivial Heesch-number
result on a non-Kaplan grid. Both committed at
`benchmarks/lateral/results/m3.2-bevelhex-hc2.txt`. The 315
Hc = 1 bevelhex polyforms across the same sweep are at
`m3.2-bevelhex-hc1.txt`.

The 65 isohedral tilers and 7 inconclusives at n = 6 are
notable artefacts: at that specific size, the geometry
admits an unusually high concentration of isohedral
configurations, and sat's `-maxlevel 7` default leaves a
small handful of shapes inconclusive. At n = 7..10 those
disappear entirely from the sample, which is why per-shape
wall scales near-linearly with shape count (≈ 7 500 / s)
once n ≥ 7.

**No Hc ≥ 3 found through n = 10.** By analogy with the
omino → polyhex / polyiamond gap (the latter two have
Hc ≥ 3 polyforms much earlier in their size catalogues),
bevelhex Hc ≥ 3 plausibly lives at n = 11 or 12. Generation
+ classification at those sizes is a compute-bound
extension of the existing sweep; an estimate from the n = 10
throughput is ~25 min for n = 11 and ~2.5 h for n = 12.
Both are tractable on a workstation but were not run for
this report; they sit as M3.2-followup.

### 3.2 Octasquare sweep (M3.3)

Octasquare's `gen` takes `-sizes <s>,<o>` for the per-shape-
class counts (squares, octagons), so the sweep is two-
dimensional. `run_octasquare_sweep.sh` defaults to all (s, o)
with s, o ≥ 1 and s + o ∈ {2, …, 6}; one pair (5, 1) returns
zero shapes at this Archimedean grid, leaving 14 non-empty
pairs covered.

Aggregating by total cell count s + o:

```text
  s+o   shapes   Hc=0    Hc=1    Hc=2    iso
   2         1      0       0       0      1
   3         5      2       3       0      0
   4        17      9       0       0      8
   5        79     54      25       0      0
   6       361    271      12       0     78
   total   463    336      40       0     87
```

40 Hc = 1 octasquares catalogued at
`m3.3-octasquare-hc1.txt`. **No Hc ≥ 2 found through s + o = 6.**

Notable: 87 / 463 ≈ 19 % of all octasquare polyforms in the
sweep tile isohedrally, vs 65 / 9,427 ≈ 0.7 % of bevelhex
polyforms in the analogous range. The (4.8.8) local geometry
is markedly more "tiling-friendly" than (4.6.12) at small
sizes — most (3, 3) octasquares (78 of 80) tile, for
example.

The sweep ran in 1.5 s wall on the same hardware. Extending
to s + o = 7, 8, 9, 10 is a few minutes of compute; the
M3.3-followup would settle whether Hc ≥ 2 octasquares exist
within Kaplan's coverage range.

### 3.3 Cross-grid catalogue (M3.4)

`benchmarks/lateral/M3.4-cross-grid-catalogue.md` joins
Kaplan's polyomino / polyhex / polyiamond per-(grid, n, Hc)
breakdowns with the bevelhex (3.1) and octasquare (3.2)
sweeps above. The aggregate first-n table:

| grid                     | Hc = 1 | Hc = 2 | Hc = 3 | Hc = 4 | sweep range |
|--------------------------|-------:|-------:|-------:|-------:|-------------|
| polyomino                |      7 |      9 |     17 | none in n ≤ 19 | n = 7..19 (Kaplan) |
| polyhex                  |      6 |      6 |      7 |     11 | n = 6..17 (Kaplan) |
| polyiamond               |      7 |     10 |     10 |     20 | n = 7..24 (Kaplan) |
| **bevelhex (4.6.12)**    |      4 |  **9** |   none |      — | n = 4..10 (this project) |
| **octasquare (4.8.8)**   |      3 |   none |      — |      — | s+o = 2..6 (this project) |

(`—` = not searched far enough; `none` = exhaustive within
the listed sweep range without finding.)

### 3.4 The two known Hc = 2 bevelhex polyforms

Cells in upstream's `(x, y)` coordinate convention for the
(4.6.12) bevelhex grid; each pair `<x> <y>` is one cell.

```text
n =  9:  cells = { (9, -12), (8, -10), (9, -9), (12, -9), (10, -8),
                   (3, -6),  (6, -6),  (2, -4),  (0, 0) }
n = 10:  cells = { (9, -3), (10, -2), (0, 0), (3, 0), (6, 0),
                   (9, 0),  (2, 2),  (8, 2), (0, 3), (6, 3) }
```

Both classify as Hc = 2 = Hh = 2 with `sat -isohedral -hh
-show` producing a corona-2 witness patch (committed in the
M3.2 raw `bvh_n*.out` files which are gitignored as
intermediate but reproducible from the script). To inspect a
witness rendering, run

```bash
( cd src/sat/src && ./viz <(echo 'B 9 -12 8 -10 9 -9 12 -9 10 -8 3 -6 6 -6 2 -4 0 0') )
```

which writes `out.pdf` with the n = 9 bevelhex shape and its
corona witness.

## 4. Cross-grid analysis

### 4.1 Heesch-friendliness ordering

Sorting by smallest n at which Hc = 4 first appears (within
each grid's available coverage):

  1. polyhex     (n = 11)
  2. polyiamond  (n = 20)
  3. polyomino   (none in n ≤ 19; open)

The polyhex → polyiamond jump from n = 11 to n = 20 is a
real quantitative gap, not a sweep-range artefact: Kaplan's
polyiamond catalogue covers all of n = 7..24 exhaustively,
with only one Hc = 4 polyiamond catalogued (at n = 20).

Polyomino's absence from this list is the project's
load-bearing open question (RQ1; CLAUDE.md): does
Hc(omino) ≥ 4 ever, or is the polyomino restriction
fundamental?

### 4.2 Where bevelhex sits

heesch-forge's M3.2 finding — Hc = 2 bevelhex polyforms at
n = 9 and n = 10 — places bevelhex roughly between polyhex
and polyiamond on the "first n with Hc = 2" axis:

  - polyhex:    Hc = 2 at n = 6.
  - polyomino:  Hc = 2 at n = 9.
  - **bevelhex: Hc = 2 at n = 9.**
  - polyiamond: Hc = 2 at n = 10.

The Hc = 2 *density* among free polyforms at the
respective discovery sizes:

  - polyhex,  n = 6:    1 / 4 = 25 %.
  - polyomino, n = 9:   1 / 198 ≈ 0.5 %.
  - polyiamond, n = 10: 3 / 103 ≈ 2.9 %.
  - bevelhex, n = 9 / 10: **2 / 1,975,280 ≈ 0.0001 %.**

Bevelhex is by orders of magnitude the most "diluted" — the
Hc ≥ 2 shapes exist but live in an enormous Hc = 0 mass.
This is consistent with the (4.6.12) tiling having very few
local symmetries to exploit: a bevelhex tile is a connected
combination of squares, hexagons, and dodecagons in
non-isotropic relative arrangement.

### 4.3 Where octasquare sits

The M3.3 sweep ran out of reach before any Hc ≥ 2
octasquares appeared. Either none exist below s + o = 7, or
they do and the M3.3 sweep simply wasn't extended far
enough. The M3.3-followup at s + o ≤ 10 (single-digit
minutes of compute) would settle this.

The 19 % isohedral-tiler density on octasquare is
striking — it's the most tiling-friendly grid in this
study by an order of magnitude. (3, 3) octasquares are 78 / 80
isohedral; even small octasquares mostly close into
tilings. This makes the search for Hc ≥ 2 octasquares
inherently sparse in the same way bevelhex's was — the
non-tilers are surrounded by a sea of tilers — but with a
much smaller search space, so an exhaustive lift to s + o = 10
is genuinely cheap.

## 5. Open questions

1. **Hc ≥ 3 bevelhex polyforms.** The two Hc = 2 shapes
   discovered live at n = 9, 10. By analogy with polyhex
   (Hc = 3 at n = 7 — one above its Hc = 2 floor) and
   polyiamond (Hc = 3 at n = 10 — same as its Hc = 2),
   bevelhex Hc = 3 plausibly lives in the n = 10..12 range.
   M3.2-followup at n = 11 / 12 is the natural next compute
   sweep. ~25 min wall for n = 11; ~2.5 h for n = 12.
2. **Hc ≥ 2 octasquare polyforms.** Whether any exist at
   all is open. Extending M3.3 to s + o ≤ 10 (a few minutes
   of wall) would either find one or rule them out
   through that range.
3. **Hc ≥ 4 bevelhex polyforms.** Open and far. Polyhex's
   Hc = 4 at n = 11 vs polyiamond's at n = 20 suggests the
   bevelhex equivalent could be anywhere in n = 11..30. Out
   of this report's scope.
4. **Hc ≥ 4 polyomino** — the project's primary research
   question RQ1, independent of the lateral grids.

## 6. What the report does not claim

- **No algorithmic novelty.** The classification is done by
  unmodified heesch-sat. The Phase-1 retrospective
  `phase1-engineering-negatives.md` documents in detail why
  no straightforward solver-side improvement beats CMSat on
  this workload; that conclusion stands here too. Phase-2
  (`phase2-engineering-hybrid.md`) explores joint DLX + SAT;
  Phase 3 does not depend on or interact with that work.
- **No completeness claim past the stated bounds.** The
  bevelhex sweep is exhaustive *only* through n = 10. The
  octasquare sweep is exhaustive *only* through s + o = 6.
  Statements about Hc ≥ k thresholds are bounded by these
  ranges — when no shape with that Hc was found in the
  sweep, the claim is "none in the searched range," not
  "none anywhere."
- **No isohedrality classification beyond what `sat
  -isohedral` reports.** The 65 + 87 = 152 isohedral tilers
  reported by sat are sat's classifications, accepted at
  face value here. Independent verification would be a
  followup on its own.

## 7. Reproducibility

Every numeric claim in this document can be regenerated on
a fresh checkout:

```bash
# Build (~1 minute):
( cd src/sat/src && make )

# Bevelhex sweep, n in {4..10} (~5 min wall):
bash benchmarks/lateral/run_bevelhex_sweep.sh 4 5 6 7 8 9 10
# writes:
#   benchmarks/lateral/results/m3.2-bevelhex-extended.csv
#   benchmarks/lateral/results/m3.2-bevelhex-extended.log
#   benchmarks/lateral/results/bvh_n*.{in,out}        (gitignored)

# Octasquare sweep, s+o in {2..6} (~2 s wall):
bash benchmarks/lateral/run_octasquare_sweep.sh
# writes:
#   benchmarks/lateral/results/m3.3-octasquare-sweep.csv
#   benchmarks/lateral/results/m3.3-octasquare-sweep.log
#   benchmarks/lateral/results/oct_*_*.{in,out}       (gitignored)

# Hc=1 / Hc=2 catalogues (extracted from .out files):
( cd benchmarks/lateral/results &&
  for n in 4 5 6 7 8 9 10; do
    awk '/^B/{shape=$0} /^~ 1/{print shape}' bvh_n${n}.out
  done > m3.2-bevelhex-hc1.txt &&
  for n in 9 10; do
    awk '/^B/{shape=$0} /^~ 2/{print shape}' bvh_n${n}.out
  done > m3.2-bevelhex-hc2.txt )
```

The cross-grid catalogue (M3.4) is regenerated by a small
Python script that loads each Kaplan dataset file plus the
bevelhex / octasquare CSVs above and prints the headline
table; that script is included inline in the M3.4 markdown.

Hardware dependence: every wall figure quoted is from
the ThinkPad P15 Gen 2i described in §2. Relative ratios
(bevelhex Hc = 2 density at 0.0001 % vs polyhex 25 %, etc.)
should reproduce identically on other hardware; absolute
walls will not.

## 8. References

- Heesch, H. *Reguläres Parkettierungsproblem.*
  Westdeutscher Verlag, 1968. `paper/lit/bibtex.bib`
  `HeeschA1`.
- Kaplan, C. S. *Heesch Numbers of Unmarked Polyforms.*
  Contributions to Discrete Mathematics 17(2):150–171,
  2022. arXiv:2105.09438. `paper/lit/bibtex.bib`
  `KaplanA8`. The polyomino / polyhex / polyiamond
  catalogue this report extends.
- Bašić, B. *A Figure with Heesch Number 6: Pushing a
  Two-Decade-Old Boundary.* Mathematical Intelligencer,
  2021. `paper/lit/bibtex.bib` `BasicA7`. The current
  all-shape Heesch record (non-polyform).
- ROADMAP.md (this repo) — Phase 3 milestones M3.1–M3.5.
- PROPOSAL.md (this repo) — §5 Layer 4 (lateral grids).
- `benchmarks/lateral/README.md` — harness usage notes and
  per-milestone summaries.
- `benchmarks/lateral/M3.4-cross-grid-catalogue.md` — the
  canonical version of the cross-grid table referenced in
  Section 3.3.
- `paper/drafts/phase1-engineering-negatives.md` — the
  Phase-1 retrospective explaining why heesch-sat is used
  unmodified for this Phase-3 work.

---

*Status tag: **EST.** All numeric claims in this draft are
reproducible from the committed scripts and the Kaplan
2022 dataset (`data/kaplan-2022/`) on a single workstation
in under 10 minutes of wall. The bevelhex Hc ≥ 3 question
and the octasquare Hc ≥ 2 question are tagged **OPEN**;
both have known compute budgets to resolve and sit on the
M3.2 / M3.3 followup queue.*
