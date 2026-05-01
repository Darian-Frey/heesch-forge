# M2.4 hybrid pipeline (DLX-only)

DLX-based end-to-end Heesch solver: `compute_heesch(shape) → (Hc, Hh)`
using only the M2.1 DLX library + the M2.2 corona oracle + the M2.3
`CoronaState` + a heesch-sat-style halo-component hole detector. No
SAT solver involved. Lives at `src/corona/heesch_solver.{hpp,cpp}`;
this directory is the regression harness that drives it over the
Kaplan-2022 dataset.

## Status (1 May 2026)

**Partial deliverable.** Infrastructure complete; 87 % of the small
M0.4 subset matches; three structural gaps remain that need follow-up
work before "regression suite green" (the M2.4 exit criterion) can be
declared.

| dataset | shapes | match | wall  |
|---------|-------:|------:|------:|
| n = 7   |      3 |     3 | 10.2 s |
| n = 8   |     20 |    17 |  2:26 |
| n ≥ 9   |      — |     — | not yet run; see scaling note below |

Mismatches on n = 8:

| shape | dataset (Hc,Hh) | computed (Hc,Hh) | failure mode |
|-------|----------------:|-----------------:|--------------|
| `1 0 2 0 1 1 0 2 1 2 1 3 2 3 3 3` | (1, 2) | (1, 1) | **Hh undercount** — greedy level-1 was a dead end for level 2 |
| `2 0 2 1 0 2 1 2 2 2 3 2 3 3 4 3` | (0, 1) | (1, 1) | **Hc overcount** — my hole detector accepts a chain heesch-sat rejects |
| `1 0 0 1 1 1 2 1 3 1 4 1 1 2 4 2` | (0, 1) | (1, 1) | **Hc overcount** — same as above |

## Run

```bash
make all                           # builds run_hybrid + libcorona.a + libdlx.a
./run_hybrid <dataset.txt> > out.csv
make test-n7                       # 3 shapes, ≈10 s
make test-small                    # n ∈ {7, 8, 12} — ~2-5 min
make test-full                     # n ∈ {7, 8, 11, 12} — multi-hour, see below
```

CSV columns: `coords, n, hc_dataset, hh_dataset, hc_computed, hh_computed,
ok, oracle_calls, wall_ms`. Exit code is non-zero if any row mismatches.

## Method

The implementation is in `src/corona/heesch_solver.cpp`. Two phases:

1. **Hh — greedy extension.** Repeatedly call the M2.2 oracle on the
   current `CoronaState` interior; append each completion as a new
   level. Stop when the oracle reports no completion. The depth
   reached minus 1 is Hh.

2. **Hc — bottom-up hole-free ascent.** Restart from a fresh
   `CoronaState`. At each level k, enumerate completions via
   `Oracle::for_each_completion` and accept the first whose union with
   the prior interior is simply connected (per the heesch-sat-style
   hole detector at `heesch_solver.cpp::is_simply_connected`). When
   no simply-connected completion exists, Hc stops at the previous
   level.

Hole detection ports `src/sat/src/holes.h`'s `HoleFinder`: 8-Moore
halo cells (4-cardinal *and* diagonal neighbours of the interior, not
in the interior), connected components computed via 4-cardinal edges
between halo cells. The lex-smallest halo cell is on the outer
boundary; any component not containing it is an enclosed hole.

## Known gaps

### 1. Hh undercount: greedy chain dead-ends

When the greedy level-1 completion the oracle returns admits no
level-2 completion, the algorithm reports Hh = 1 even when an
alternative level-1 would have admitted level 2 (the dataset case
above). heesch-sat does not have this issue because its SAT call at
level 2 sees all possible level-1 + level-2 chains simultaneously
through the unified formula, finding a feasible chain via
conflict-driven backtracking.

The DLX fix is **enumerate-and-backtrack**: if level-(k+1) fails,
back off to level-k, ask `Oracle::for_each_completion` for an
alternative level-k completion, retry. This is a depth-bounded
DFS over the chain; the branching factor is the number of
completions per level, which can be large (millions for n ≥ 11).
A naive implementation would explode; bounding the branching
(picking, say, the K lexicographically smallest completions at each
level) would make it tractable but no longer guaranteed exhaustive.

### 2. Hc overcount: hole detector disagrees with heesch-sat

Two n = 8 shapes where my code reports Hc = 1 but the dataset says
Hc = 0. The hole detector in `is_simply_connected` is a port of
heesch-sat's `HoleFinder` (8-Moore halo, 4-cardinal connectivity), but
some subtle case is being mis-classified — for those two specific
chains, my code calls "simply connected" while heesch-sat finds a
hole.

The detector passes 6 isolated unit tests (single cell, domino,
2×2 square, Knuth-style 3×3 donut, 5×5 with a hole, and the n = 7
Hh = 1 heptomino's level-1 chain). The failing cases are larger
patches where some specific local arrangement is being missed.
Direct comparison against heesch-sat's runtime hole-detection
output on the same chain would identify the exact disagreement;
that debugging is M2.4-followup work.

### 3. Scaling

Per-shape wall on the harder n = 7 / n = 8 shapes already reaches
9-73 s. n = 11 and n = 12 grow the corona-1 candidate count
roughly linearly with shape size (a few hundred candidates per
position) and the corona-2 count by another order of magnitude;
worst-case wall per shape on n = 12 could be in the
several-minutes range. The full 174-shape M0.4 regression on this
hardware would not finish in a single working day.

The walkback enumeration burden is intrinsic: DLX exact-cover
cannot prove "no hole-free chain exists" by anything other than
exhaustive search, while heesch-sat's CDCL conflict learning gets
that for free. This was foreshadowed in the Phase-1 retrospective
(`paper/drafts/phase1-engineering-negatives.md` §4): the
heesch-sat encoding is structurally tuned for SAT; equivalent
performance from DLX requires either new heuristics or actual
hybrid handoff.

## Path forward (M2.5 / M2.6)

The Phase-2 milestones M2.5 ("benchmark across all of Kaplan's
dataset; identify crossover depth") and M2.6 ("investigate joint
DLX+SAT formulation") are exactly the work that addresses these
gaps:

- M2.5 is where the empirical evidence about the DLX scaling cliff
  gets collected, motivating the SAT handoff.
- M2.6 is where the actual hybrid (DLX inner + SAT outer with shared
  state via M2.3's CoronaState) is built. The SAT side gets to do
  conflict-driven backtracking on the harder Hc cases.

For the M2.4 partial-deliverable scope, the infrastructure (oracle,
state, walkback, halo-component hole detector) is committed and
unit-tested; the gaps above are the agenda for the next phase.
