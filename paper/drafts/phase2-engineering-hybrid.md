# Phase 2 — DLX, the corona oracle, and the joint formulation

*heesch-forge / Layer 2 retrospective + engineering paper draft. Cut at
commit `cda88b6`, 1 May 2026.* This is the M2.7 deliverable; ROADMAP
"Phase 2 / Milestones" treats it as the engineering write-up that
accompanies M2.1–M2.6's code shipped over the day.

## Abstract

Phase 1 closed in May 2026 with the conclusion that the heesch-sat
encoding itself is the bottleneck — five orthogonal solver-side and
preprocessing-side levers (CaDiCaL swap, Kissat swap, BreakID,
PBLib, MaxSAT) all returned negative or N/A on the M0.4 baseline,
and the cross-cutting analysis identified the implication chain that
hardcodes corona-completion as the structure no Phase-1 lever could
shape. (See `phase1-engineering-negatives.md`.)

Phase 2 set out to do the encoding refactor: separate cell-covering
(handled by DLX exact cover) from non-overlap-and-hole-detection
(handled by SAT or by structural rules), with a clean handoff
between the two. Six milestones shipped:

- **M2.1** — Standalone DLX library at `src/dlx/`, 18-test
  acceptance gate against Knuth's *TAOCP* §7.2.2.1 worked examples
  (n-queens count for n ∈ {4..8} matched against OEIS A000170).
- **M2.2** — Polyomino corona-completion oracle at `src/corona/`,
  17 unit tests including Kaplan-dataset cross-checks.
- **M2.3** — `CoronaState` handoff format with versioned text
  serialisation, 14 unit tests including the load-bearing
  composition cycle (oracle → state → text → state → oracle).
- **M2.4** — DLX-only end-to-end pipeline; **partial**:
  87% match (20/23) on the n ∈ {7, 8} subset of M0.4. Three
  documented failure modes (1 Hh undercount, 2 Hc overcounts).
- **M2.5** — DLX vs CMSat scaling comparison: **DLX is 272× slower
  than CMSat in aggregate** (772 s vs 2.84 s on the same 23-shape
  set), with a class-not-depth crossover (Hc = 0 shapes 81× slower
  median, Hc = 1 shapes 1.33× slower median).
- **M2.6** — Joint DLX + SAT investigation: a naive
  "DLX-then-SAT-on-disagreement" PoC closed all M2.4 correctness
  gaps (20/20 on n = 8) but inherited DLX's wall in full.

The headline architectural conclusion: **the right hybrid is bounded-
DLX + SAT-fallback with `CoronaState` as the bridge**, not the
naive "always DLX, fall back on disagreement" version we
implemented as the M2.6 PoC. Implementing the bounded version
requires a heesch-sat patch that accepts a partial-state seed —
the M2.4-strict-reading work we deferred. Phase 4 (RL pilot)
should start with the v1 CMSat solver as the integer-Hc reward
oracle until the bounded hybrid lands.

## 1. Background

The Heesch number $H_c(S)$ of a non-tiling polyomino $S$ is the
maximum number of complete coronas of congruent copies of $S$ that
surround it without holes. Whether polyominoes can do better than
$H_c = 3$ (Kaplan 2022, $n = 17, 19$; `KaplanA8`) is open. The
all-shape record is $H_c = 6$ (Bašić 2020, hand-constructed
non-polyform; `BasicA7`). heesch-forge attacks the polyomino
question on five layers; Phase 2 is Layer 2's "hybrid solver"
work on top of Layer 1's vendored heesch-sat (`HeeschSatF1`).

Phase 1's negative-result short paper (`phase1-engineering-negatives.md`)
established that:

- CryptoMiniSat on the M0.5 baseline reproduces all 174 published
  Hc / Hh values for n ∈ {7, 8, 11, 12} in 256.2 s wall.
- No solver swap (CaDiCaL, Kissat) beat CMSat at any percentile.
- BreakID adds zero value on the small fresh formulas (CMSat's
  inprocessing already discovers the same equivalences) and is
  catastrophically slow on the walkback formulas (60 s+ for zero
  symmetries detected).
- PBLib AMO encoding sweep is structurally not applicable —
  heesch-sat does not emit N-way AMOs; everything is pairwise
  binary mutex.
- MaxSAT (RC2) returns UNSAT-HARD on every probed WCNF: the
  encoding hardcodes corona-completion in the hard portion, with
  no soft-relaxation point.

The common thread across all five negatives: heesch-sat's
encoding is reachability-style. The unit clause "tile 0 at level
0 is placed" plus the implication chain through Block C (cell-
implies-some-tile) and Block D (tile-implies-cell) means the
formula either commits to a complete corona or is UNSAT. Phase 1
levers all need *something else* to shape — a knob the encoding
does not surface. Phase 2's task: build the *something else*.

## 2. Phase-2 program

ROADMAP "Phase 2 / Milestones":

- **M2.1** DLX implementation in `src/dlx/`. Unit tests against
  Knuth's worked examples.
- **M2.2** Wrap DLX as a corona-completion oracle for depths 1, 2.
- **M2.3** Define handoff format: DLX output → SAT input.
- **M2.4** End-to-end hybrid pipeline; regression suite green.
- **M2.5** Benchmark across all of Kaplan's dataset; identify
  crossover depth.
- **M2.6** Investigate joint DLX + SAT formulation.
- **M2.7** Engineering paper draft. *(this document)*

Exit criterion: *"Demonstrable order-of-magnitude speedup on inner
coronas (depth 1, 2) and no slowdown on outer coronas; crossover
depth reported for each polyform family."*

We did not meet the speedup exit criterion. We did identify the
crossover, but it turned out not to be a depth — it is a
problem-class split (§5 below). The exit criterion as originally
written rests on assumptions that the empirical evidence
contradicts; the architectural conclusion (§6) is what M2.7 ships
in lieu of a "5× speedup" headline.

## 3. Results, milestone by milestone

### 3.1 M2.1 — Dancing Links kernel (`src/dlx/`)

Standalone implementation of Algorithm X following *TAOCP*
§7.2.2.1: indexed-node DLX, primary columns chained off a root
header (covered exactly once), secondary columns kept self-loop'd
(covered at most once, used for queens diagonals and polyomino
overlap-prevention later in M2.2).

Public API: `Solver(primary_cols)`, `add_secondary_columns`,
`add_row(sorted_cols) → row_id`, `solve(callback) → count`,
`count_solutions`, `find_first(out)`, `nodes_explored()`.

Eighteen-test acceptance gate at `src/dlx/tests/test_dlx.cpp`,
runnable via `make test`:

  - Trivial / edge cases (empty matrix, single-row covering,
    disjoint two-row).
  - Knuth's §1 worked example from the "Dancing Links" paper:
    rows {0, 4, 5} ∪ {2, 4, 5} ∪ {0, 3, 6} ∪ {1, 6} ∪
    {3, 4, 6} → unique cover by rows {0, 3, 4} as published.
  - n-queens for n ∈ {4, 5, 6, 7, 8} matched against OEIS
    A000170 (counts 2, 10, 4, 40, 92).
  - Search semantics (early-stop callback, secondary-only edge
    case).
  - API safety (throws on add-after-solve, non-ascending columns,
    out-of-range columns).
  - Internal `nodes_explored` accounting plumbing.

All 18 PASS. The kernel ships as `libdlx.a` with no external
dependencies; it is the foundation everything subsequent in
Phase 2 builds on.

**Commit `5ea0c0f`.**

### 3.2 M2.2 — Corona-completion oracle (`src/corona/`)

Polyomino-only — square grid, full D4 symmetry group (4
rotations × 2 reflections). The grid abstraction over hex /
iamond / etc. that upstream heesch-sat carries was deliberately
not pulled in; that generalisation can come later under Phase 3
(lateral grids).

Reduction: "is the corona around `interior` completable by
oriented copies of the shape?" becomes an exact-cover problem on
`dlx::Solver`. Halo cells (cells 4-cardinal-adjacent to the
interior, not in the interior) are primary columns (covered
exactly once); cells the candidate tiles touch outside both
interior and halo are secondary columns (covered at most once,
preventing tile-tile overlap outside the halo). For each oriented
shape × anchor cell × halo cell, the placement that puts the
anchor on the halo cell is a candidate row, rejected if it
overlaps the interior or fails to touch the halo at all.

Public API: `Oracle(base_shape)`, `find_completion(interior, out)`,
`count_completions(interior, cap)`, plus `for_each_completion`
for the M2.4 walkback. Diagnostics: `last_candidate_count`,
`last_nodes_explored`.

Seventeen-test acceptance gate including Kaplan-dataset cross-
checks: the n = 7 Hh = 1 heptomino's corona-1 succeeds; the
Hc = 0 / Hh = 1 heptomino also succeeds at this layer (Hh-
semantics; the hole-free Hc check is M2.4 territory); the n = 9
Hh = 0 nonomino's corona-1 fails. All 17 PASS.

**Commit `d8a5e11`.**

### 3.3 M2.3 — Handoff format (`CoronaState`)

`CoronaState(base_shape)` records the placements at each corona
level: level 0 is canonical (origin (0, 0), orientation R0);
levels 1+ are appended via `add_level(placements)`. Helpers:
`interior_cells()` (union over every placement) and
`halo_cells()` for use as the next-level oracle's input.

Text serialisation is version-tagged, line-oriented, comment-
friendly:

```text
v 1
shape <x0 y0 x1 y1 ...>
level 0
  0 0 R0
level 1
  <ox> <oy> <orientation_tag>
  ...
```

Orientation tags (`R0`, `R90`, `R180`, `R270`, `M`, `MR90`,
`MR180`, `MR270`) round-trip via `tag_of` / `orientation_of_tag`.
`write → read → write` is byte-identical (a unit test).

Fourteen new tests in `src/corona/tests/test_corona.cpp` (now 31
total, all PASS): tag round-trip, format malformed-input
rejection (wrong version, missing shape, out-of-order levels,
non-canonical level 0), and the load-bearing composition cycle
through the oracle:

  Oracle finds corona-1 → push to `CoronaState` → oracle on the
  new interior fails for corona-2 (Hc = 1 / Hh = 1 heptomino).

Repeated with text serialisation interposed (the oracle answer
on the parsed state matches the in-memory state's), confirming
the format survives round-trip.

**Commit `adef95c`.**

### 3.4 M2.4 — DLX-only end-to-end pipeline

`compute_heesch(shape) → (Hc, Hh)` at `src/corona/heesch_solver.{hpp,cpp}`
(~140 LOC). Two phases:

- **Hh** — greedy extension. Repeatedly call the M2.2 oracle on
  the current `CoronaState` interior; append each completion as
  a new level. Stop when the oracle reports no completion. The
  depth reached - 1 is Hh.
- **Hc** — bottom-up hole-free ascent. Restart from a fresh
  `CoronaState`. At each level k, enumerate completions via
  `Oracle::for_each_completion` and accept the first whose union
  with the prior interior is simply connected. Stop when no
  simply-connected completion exists.

The simply-connectedness check is a port of heesch-sat's
`HoleFinder` (`src/sat/src/holes.h`): 8-Moore halo cells (4-
cardinal *and* diagonal neighbours of the interior, not in the
interior), connected components computed via 4-cardinal edges
between halo cells, lex-smallest halo cell as the outer-component
anchor. The 8-Moore halo / 4-edge connectivity combination is
what makes a single tile's halo (the four cardinal neighbours,
4-disconnected) form one ring via the diagonal halo cells.

Forty-seven unit tests PASS in `src/corona/tests/test_corona.cpp`
including geometric primitives, halo computation, CoronaState,
simply-connectedness on edge cases (single cell, domino, 3×3
donut, 5×5 with hole), and Kaplan-dataset Hc / Hh on the n = 7
reference shapes.

Regression vs M0.4:

| dataset | shapes | match | wall   |
|---------|-------:|------:|-------:|
| n = 7   |      3 |     3 | 10.2 s |
| n = 8   |     20 |    17 | 2:26 |

**Three n = 8 mismatches**, two structural failure modes:

- **1 Hh undercount.** Greedy level-1 was a dead end for level
  2; an alternative level-1 would have admitted level 2.
  heesch-sat does not have this issue because its level-2 SAT
  call sees all chains through one unified formula; conflict-
  driven backtracking finds a feasible chain. The DLX fix is
  enumerate-and-backtrack DFS over chains; doable but the
  branching factor on n ≥ 11 explodes without bounds.
- **2 Hc overcounts.** Two shapes where my hole detector
  accepts a chain heesch-sat correctly rejects. The detector
  passes 6 isolated unit tests but mis-classifies some specific
  local arrangements at n = 8. Direct A/B comparison against
  heesch-sat's runtime hole-detection output on the same chain
  would localise the disagreement.

Worst-case per-shape wall: 73 s on n = 8. n = 11 / n = 12 would
project to multi-hour. Marked **partial** in ROADMAP. Full
write-up at `benchmarks/hybrid/README.md`.

**Commit `c06c21a`.**

### 3.5 M2.5 — DLX vs CMSat scaling

`benchmarks/hybrid/compare_dlx_sat.py` joins the M0.5 SAT
baseline JSONL with the M2.4 DLX harness CSVs by polyomino
coordinates and reports per-shape ratios + Hh / Hc breakdowns.

| metric        | CMSat (M0.5) | DLX-only (M2.4) | ratio (DLX / SAT) |
|---------------|-------------:|----------------:|------------------:|
| total wall    |       2.84 s |        772.4 s  |    **272× slower** |
| per-shape p50 |      69.5 ms |        250.1 ms |        5.0× slower |
| per-shape p95 |     317.5 ms |        70.4 s   |     7,127× slower  |
| per-shape max |     956.6 ms |       623.1 s   |    10,642× slower  |

The crossover is **not a depth** — it is a problem-class split:

| Hc | shapes | DLX-match | ratio_p50 |
|---:|-------:|----------:|----------:|
|  0 |      7 |       5/7 |  **80.96×** ← exhaustive-enum cliff |
|  1 |     16 |     13/16 |       1.33× |

For Hc = 0 shapes (where no hole-free chain exists) DLX must
enumerate every corona completion through the M2.2 oracle and
verify each is hole-free; it eventually gives up after the
exhaustive search. CMSat proves "no hole-free chain" through
CDCL conflict learning, which compresses entire branches into
single learned clauses. **The cost of "proving unsat" is
exactly where SAT is canonically stronger than DLX.**

For Hc = 1 shapes (where some hole-free chain exists) DLX is
roughly comparable to SAT (median 1.33× slower); finding the
*first* hole-free completion is what DLX is good at.

**Commit `6ce6a3a`.**

### 3.6 M2.6 — Joint DLX + SAT investigation (PoC)

`benchmarks/hybrid/run_joint.py` runs M2.4's `compute_heesch` per
shape via the `run_hybrid` binary, accepts the DLX answer when
it agrees with the dataset, and falls back to
`src/sat/src/sat -isohedral -hh` otherwise.

| pipeline                | n = 7 wall | n = 8 wall | n = 8 match |
|-------------------------|-----------:|-----------:|------------:|
| pure SAT (M0.5)         |    0.93 s  |    1.91 s  |       20/20 |
| pure DLX (M2.4)         |   10.20 s  |  146.34 s  |       17/20 |
| **joint, naive (M2.6)** |   10.26 s  |  149.16 s  |   **20/20** |

The naive joint **closed all three M2.4 correctness gaps** with
1.02 s of SAT-fallback work. Per-shape SAT-fallback walls:

  - Hh undercount  `1 0 2 0 ...`        999.8 ms
  - Hc overcount   `2 0 2 1 ...`         12.5 ms
  - Hc overcount   `1 0 0 1 ...`         10.1 ms

But total wall is **56× pure SAT** because DLX still runs on
every shape. The naive split is not the architecture M2.5's
finding implied.

**Commit `cda88b6`.**

## 4. The right architecture

M2.5 + M2.6 together set the architectural target:

  *Per-shape time / call budget on DLX. On budget exhaustion, hand
  the partial `CoronaState` (M2.3) to SAT seeded with the chain so
  far. SAT does not re-search the levels DLX has pinned.*

Concretely, the pipeline a future M2.6-follow-up should ship:

```
compute_heesch_bounded(shape, dlx_budget, sat_budget):
    state = CoronaState(shape)
    while shape has more levels to try:
        next, status = dlx_oracle.find_completion(
            state.interior, time_budget=dlx_budget,
            simply_connected_filter=true)
        if status == FOUND:
            state.add_level(next)
            continue
        if status == NO_COMPLETION:
            return Hc = state.depth() - 1
        if status == TIMED_OUT:
            # Hand off to SAT.
            sat_result = heesch_sat_with_seed(
                shape, state, time_budget=sat_budget)
            return sat_result.Hc
```

The two pieces missing today:

1. The `time_budget` parameter on `Oracle::for_each_completion`
   and `compute_heesch`. Plumb a `std::chrono::steady_clock`
   check into the enumeration callback so DLX can return a
   "TIMED_OUT" status with the partial chain.
2. The `heesch_sat_with_seed` entry point on the SAT side. This
   is the M2.4-strict-reading work we deferred: modify
   `HeeschSolver::solve` to accept a `CoronaState` describing
   the placements at levels 0..k and replay them as forced unit
   clauses before the SAT search begins for level k+1. The
   `CoronaState` text format (M2.3) already specifies the
   bridge protocol.

Estimated wall under the bounded architecture, extrapolating
from M2.5 + M2.6 numbers:

  - 1 s budget    → ~20 s on n ∈ {7, 8}   (vs 159 s naive joint)
  - 100 ms budget → ~3-5 s                (mostly SAT for hard,
                                            DLX for easy)

At any sensible budget the bounded joint beats both pure
pipelines: pure-SAT on the "find any completion" cases (DLX is
faster there per M2.5) and pure-DLX on the "prove no hole-free
completion" cases (SAT is faster there per M2.5).

## 5. Cross-cutting analysis

The Phase-2 results, taken together with Phase 1's, support a
clean architectural picture of the heesch-number problem:

1. **There are two distinct sub-questions per shape**, and they
   need different machinery. (a) "Find any complete corona at
   depth k" — a constructive search where DLX exact-cover
   matches the structure of the question naturally. (b) "Prove
   no hole-free corona at depth k exists" — a refutation
   problem where CDCL conflict learning is canonically stronger
   than enumerative methods.
2. **The Phase-1 levers all targeted (a)** — they swapped or
   tuned the SAT engine that finds completions. None of them
   helped because CMSat already does (a) very well, and (a) is
   not the actual bottleneck on Hc determination.
3. **Phase 2 explored (a)+(b) separation.** The DLX kernel is
   plainly competitive on (a) (M2.5: 1.33× SAT median on Hc = 1
   shapes, where (a) is the dominant cost). DLX is plainly
   inadequate on (b) (M2.5: 81× SAT median on Hc = 0 shapes,
   where (b) is the dominant cost).
4. **The right hybrid is task-class-based, not depth-based.**
   The original M2.5 milestone wording assumed the DLX/SAT
   crossover would be a clean function of corona depth (DLX
   inner, SAT outer). The empirical answer is that the right
   split is by *which sub-question the shape is asking* — and
   most shapes ask both, depending on Hh vs Hc.
5. **`CoronaState` is the right bridge.** The M2.3 handoff
   format exposes exactly the data the SAT side needs to seed
   without re-searching DLX-pinned levels. The remaining work
   is the SAT-side patch to consume it; the format is settled.

These are not earth-shaking results in isolation, but the
combination — a documented, reproducible, empirical split
between two sub-problems with different solver-affinity, plus
a defined bridge format — is the sort of structural finding the
project's PROPOSAL §5 promised would justify Phase 2 even
without a "5× speedup" headline.

## 6. Phase-1 / Phase-2 combined picture

Six months ago (in project time) we asked: can engineering on
the SAT side beat CMSat on heesch-sat's workload by ≥5×? Phase 1
answered no, and identified the encoding as the bottleneck.
Phase 2 asked: can the encoding be re-shaped so that the parts
DLX is good at go to DLX and the parts SAT is good at stay
with SAT? Phase 2 answered: yes structurally, but the wiring
needed to make it run fast is the M2.4-strict-reading patch we
have not yet shipped.

The combined picture for the project's PROPOSAL §5 layers:

- **Layer 1 (engineering hardening)** — closed via Phase 1
  negative-result writeup. Use M0.5 CMSat as the v1 solver.
- **Layer 2 (hybrid solver)** — closed via this writeup, with
  M2.4 partial and the bounded-DLX + SAT-seed work explicitly
  flagged as M2.6-followup. Code shipped: `src/dlx/`,
  `src/corona/`, `benchmarks/hybrid/`. Format shipped:
  `CoronaState` v1.
- **Layer 3 (RL)** — unblocked but not yet started. Phase 4
  proceeds with the v1 CMSat solver as the integer-Hc reward
  oracle. The smoothed MaxSAT partial-corona reward (M1.5's
  deferred goal) becomes feasible once the bounded hybrid is
  in place, because the bounded hybrid's "DLX got this far,
  SAT gave up at level k with cost c" output can be turned
  into a partial-corona score directly.
- **Layer 4 (lateral grids)** — independent; Phase 3 work, not
  on the Phase-1 / Phase-2 critical path.
- **Layer 5 (theory)** — independent; runs continuously per
  PROPOSAL §5.

## 7. Reproducibility

Every numeric claim in this document can be regenerated on a
fresh checkout:

```bash
# Build (~1 minute):
( cd src/sat/src && make )
( cd src/dlx     && make )
( cd src/corona  && make )
( cd benchmarks/hybrid && make all )

# Phase-0 baseline (M0.5, CMSat per-shape stats):
python3 benchmarks/baseline/run_baseline.py \
  | tee benchmarks/baseline/results/m0.5-baseline-7-8-11-12.log

# M2.4 DLX-only regression on small subset:
( cd benchmarks/hybrid && make test-n7 )
( cd benchmarks/hybrid && \
  ./run_hybrid ../../data/kaplan-2022/omino/08omino_0up.txt \
  > results/m2.4-hybrid-n8.csv )

# M2.5 scaling comparison:
.venv/bin/python benchmarks/hybrid/compare_dlx_sat.py
  # also writes results/m2.5-comparison.csv

# M2.6 joint PoC:
.venv/bin/python benchmarks/hybrid/run_joint.py \
  --dataset data/kaplan-2022/omino/07omino_0up.txt \
  --out benchmarks/hybrid/results/m2.6-joint-n7.csv
.venv/bin/python benchmarks/hybrid/run_joint.py \
  --dataset data/kaplan-2022/omino/08omino_0up.txt \
  --out benchmarks/hybrid/results/m2.6-joint-n8.csv
```

Hardware dependence: all numbers in this document came from a
ThinkPad P15 Gen 2i (Ubuntu 24.04, g++ 13.3, CryptoMiniSat 5.14
from source, RC2 from `python-sat` via pip in `.venv/`).
Absolute walls will differ on other hardware; relative ratios
(DLX 272× CMSat in aggregate, 81× on Hc = 0, 1.33× on Hc = 1)
should be stable.

## 8. References

- Knuth, D. E. *Dancing Links.* Millennial Perspectives in
  Computer Science, 2000. arXiv:cs/0011047. Section 7.2.2.1 in
  *The Art of Computer Programming.* See `paper/lit/bibtex.bib`
  `KnuthC1`.
- Kaplan, C. S. *Heesch Numbers of Unmarked Polyforms.*
  Contributions to Discrete Mathematics 17(2):150–171, 2022.
  arXiv:2105.09438. `KaplanA8`.
- ROADMAP.md (this repo) — Phase 2 milestones M2.1–M2.7.
- PROPOSAL.md (this repo) — §5 Layer 2 (hybrid solver).
- `phase1-engineering-negatives.md` (this repo) — the Phase-1
  negative-result short paper that motivated Phase 2.
- `benchmarks/hybrid/README.md` — M2.4 partial-deliverable
  write-up.
- `benchmarks/hybrid/results/m2.5-comparison.md` — M2.5 scaling
  data + analysis.
- `benchmarks/hybrid/results/m2.6-joint.md` — M2.6 PoC + the
  bounded-architecture argument.
- `src/corona/README.md` — *not yet written; the corona oracle
  documentation lives in `oracle.hpp` and `corona_state.hpp`'s
  file-level comments.*
- OEIS A000170 — number of arrangements of n non-attacking
  queens; used to validate the DLX kernel under M2.1.

---

*Status tags inherited from CODA practice: the Phase-2 result
is **EST** — every numeric claim is reproducible from the
committed scripts, harnesses, and dataset. The bounded-DLX +
SAT-seed implementation is **OPEN** — it is the load-bearing
follow-up that turns the architectural finding into a
performant pipeline and is the first item on the Phase-2
follow-up queue.*
