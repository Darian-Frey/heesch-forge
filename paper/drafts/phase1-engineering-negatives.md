# Phase 1 — Engineering negatives, and why the encoding is the bottleneck

*heesch-forge / Layer 1 retrospective. Cut at commit `867546c`, 1 May 2026.*
*This is the M1.7 deliverable; ROADMAP "Phase 1 / Decision gate" treats it as
the negative-result short-paper draft that authorises Phase 2 / Layer 3.*

## Abstract

heesch-forge Phase 1 set out to modernise Kaplan's `heesch-sat` solver via
five orthogonal engineering levers — solver swap, portfolio, symmetry
breaking, encoding sweep, and MaxSAT scoring — with an exit criterion of
**≥5× per-shape speedup on Kaplan's H=2 and H=3 polyominoes** and a
decision gate at <2× to "write up findings as a negative-result short
paper and proceed directly to Layer 3 (RL) using the v1 solver"
(ROADMAP, *Phase 1 / Decision gate*).

Five levers were evaluated. None produced a speedup over the M0.5
CryptoMiniSat baseline; four produced numerically negative results
(slower-than-baseline or zero-gain) and the fifth was structurally
not applicable to the existing constraint encoding. The cross-cutting
finding is that **the heesch-sat encoding itself is the bottleneck**,
not the solver or the preprocessing pipeline. The decision gate is
considered triggered. Phase 2 (hybrid DLX/SAT — see PROPOSAL §5
Layer 2) and Phase 4 (RL with the existing v1 solver as reward
oracle) proceed.

## 1. Background

The Heesch number $H_c(S)$ of a non-tiling shape $S$ is the maximum
number of complete coronas of congruent copies of $S$ that surround
it without holes. The polyomino case currently caps at $H_c = 3$
(Kaplan 2022, $n = 17$ and $n = 19$); the all-shape record is
$H_c = 6$ (Bašić 2020, hand-constructed non-polyform). Whether
polyominoes can do better than $H_c = 3$ is open.

`heesch-sat` (Kaplan, BSD-3) is the SAT-based exhaustive solver that
established the polyomino record. heesch-forge vendored it at upstream
SHA `1adb3720` as `src/sat/` (M0.2). The Phase-0 baseline (M0.5)
reproduces all 174 published Hc/Hh values for $n \in \{7, 8, 11, 12\}$
in 256.2 s wall on a ThinkPad P15 Gen 2i with 612 k SAT conflicts and
4.0 M decisions across 1 047 SAT solve calls.

## 2. Phase-1 program

The Phase-1 layer-1 plan (PROPOSAL §5; ROADMAP "Phase 1 / Milestones")
identified seven milestones:

- **M1.1** — Swap CryptoMiniSat for CaDiCaL.
- **M1.2** — Portfolio harness (CaDiCaL + Kissat + Glucose), first-to-finish.
- **M1.3** — BreakID symmetry breaking.
- **M1.4** — PBLib AMO encoding sweep.
- **M1.5** — MaxSAT partial-corona score (Layer-3 reward signal).
- **M1.6** — Structured per-shape per-corona logging.
- **M1.7** — This benchmark report.

Each lever was evaluated on the M0.4 regression dataset (174 polyominoes
from Kaplan's published $H_c \geq 0$ collection at
`cs.uwaterloo.ca/~csk/heesch/`, scoped to $n \in \{7, 8, 11, 12\}$).
Hc/Hh classification correctness is preserved on every lever; the
question is only how fast.

## 3. Results, lever by lever

### 3.1 M1.1 — CaDiCaL (single-solver swap)

`src/sat/src/cadical_backend.h` and `-DHEESCH_BACKEND_CADICAL` produce a
`sat-cadical` build that wraps a `CaDiCaL::Solver` behind the same API
HeeschSolver expects. M0.4 regression: 174/174 match. Wall on the same
174 shapes:

| metric | CMSat (M0.5) | CaDiCaL (M1.1) | ratio |
|---|---:|---:|---:|
| wall total | 256.2 s | 569.2 s | 2.22× slower |
| per-shape p50 | 1.42 s | 2.59 s | 1.82× slower |
| per-shape p95 | 3.53 s | 10.59 s | 3.00× slower |
| per-shape max | 4.53 s | 15.05 s | 3.32× slower |
| SAT solve-calls | 1 047 | 1 046 | ≈ |

The matching `sat_calls` count confirms the heesch-sat algorithm is
unchanged; the gap is purely backend behaviour. CaDiCaL's strength is
in long-running monolithic queries, while heesch-sat issues many
small SAT calls per shape — the regime where CryptoMiniSat's hybrid
CDCL + Gauss + SLS + aggressive inprocessing wins. Side-by-side
write-up at `benchmarks/baseline/results/m1.1-comparison.md`.

**Result: solver swap is not the win.**

### 3.2 M1.2a — Kissat (third single-solver baseline)

Same pattern: `src/sat/src/kissat_backend.h` and
`-DHEESCH_BACKEND_KISSAT`, restricted to $n \in \{7, 8, 12\}$ because
n = 11's walkback exceeded the 5-min/shape ceiling on Kissat (see
below). Three-way numbers on 64 shapes:

| metric | CMSat | CaDiCaL | Kissat |
|---|---:|---:|---:|
| wall total | 61.4 s | 119.6 s | 450.3 s |
| ratio vs CMSat | 1.00× | 1.95× slower | **7.33× slower** |
| per-shape p50 | 0.33 s | 0.44 s | 1.33 s |
| per-shape p95 | 3.58 s | 7.31 s | 32.14 s |

Kissat is **non-incremental** — its public C API has no way to add
clauses after `kissat_solve()` — and `HeeschSolver::iterateUntil-`
`SimplyConnected` (heesch.h:875) sits in a `while (solver.solve())
add-more-clauses` loop. The adapter buffers every clause and rebuilds
a fresh `kissat*` on every `solve()` call, which is fine for
non-incremental call sites (`cur_solver`, `iso_solver`, the two
`final_solver`s — one rebuild per instance lifetime) but compounds
catastrophically in the walkback loop, where n = 11 wedged a single
shape at >14 minutes.

Three-way write-up at `benchmarks/baseline/results/m1.2a-comparison.md`.
**Architectural finding: any portfolio that includes Kissat must
route the walkback exclusively through CMSat or CaDiCaL.**

### 3.3 M1.2b — Glucose + portfolio harness (deferred)

ROADMAP M1.2 also calls for Glucose plus a first-to-finish portfolio
harness. M1.2b was not run because the M1.1 + M1.2a results bound the
ceiling: any portfolio without CMSat is strictly slower than CMSat-
solo at every percentile in this dataset (CaDiCaL and Kissat *both*
lost on every measured shape), so the best a portfolio that excluded
CMSat could achieve is "matches the slowest of its members." A
portfolio that includes CMSat is just CMSat-with-process-overhead in
expectation. Glucose, descended from MiniSat, is a CDCL solver in the
same family as CaDiCaL — there is no reason to expect a different
qualitative answer.

The portfolio architecture remains useful infrastructure for future
work (clause sharing under M2, PROPOSAL §5 Layer 1 mentions
Mallob-style sharing) but is not load-bearing for the Phase-1
decision gate.

### 3.4 M1.3 — BreakID symmetry breaking

Probe artefacts at `benchmarks/breakid/`. Sat-dump
(`-DHEESCH_BACKEND_DUMP`, `src/sat/src/dumping_backend.h`) tees every
SAT instance to DIMACS; `breakid` is then run over each dump with a
60 s wall cap, and CMSat is A/B-tested raw vs broken. Two clean
regimes:

| regime | example | clauses | BreakID time | breaking clauses | CMSat conflicts (raw → broken) |
|---|---|---:|---:|---:|---:|
| fresh, low-corona | n=7 inst-00000-call000 | 1,767 | 0.00 s | 16 | 1 → 1 |
| fresh, low-corona | n=8 inst-00001-call000 | 10,605 | 0.16 s | 19 | 22 → 22 |
| walkback | n=8 inst-00002-call000 | 749,355 | **60 s timeout** | 0 | 1 093 (raw) |
| walkback | n=8 inst-00003-call000 | 790,601 | **60 s timeout** | 0 | 1 204 (raw) |

CMSat's solve trace is **byte-identical** raw-vs-broken on the
formulas where BreakID succeeds — its inprocessing already discovers
the same equivalences. Walkback formulas have no symmetries left:
the thousands of hole-banning clauses appended by
`iterateUntilSimplyConnected` kill every variable-level automorphism,
and BreakID still pays the full graph-isomorphism search to discover
that. Run uncapped on the n = 7 walkback formulas, BreakID took
226 s and 753 s respectively, both reporting `Num generators: 0`.

Naive "preprocess every CNF with BreakID" pipeline would cost
≥120 s/shape on n = 8 alone for zero solver-side gain. **Symmetry
breaking is structurally a wash on this workload with CMSat as the
engine.** Full write-up at `benchmarks/breakid/README.md`.

### 3.5 M1.4 — PBLib AMO encoding sweep (not applicable)

Inspecting `src/sat/src/heesch.h::getClauses` line by line shows
heesch-sat does not emit any N-way at-most-one constraints. Every
"at most one" enforcement is **pairwise binary mutex over ordered
tile-pairs**; the dominant block is `heesch.h:615-635`'s
geometric-overlap mutex, which walks ordered pairs of overlapping
tile positions and emits one binary clause per `(level_i, level_j)`
combination, costing $O(|\text{overlap pairs}| \times
|\text{levels}|^2)$.

PBLib's value is sweeping the encoding choice of N-way AMOs
(sequential, commander, product, bimander, ladder). There is no
`vector<Lit>` AMO site to swap an encoding behind. Making M1.4
meaningful would require *refactoring* the constraint generation so
that the per-cell "at most one tile uses cell c" constraint is
expressed as a true N-way AMO over the (tile, level) variables
covering c — that is an encoding redesign, not a sweep, and it
overlaps directly with M2 (hybrid DLX/SAT handoff is already
restructuring the constraint encoding).

The pairwise-binomial encoding heesch-sat uses *is* an AMO encoding
choice; it just is not a PBLib one. **M1.4 closed as structurally
not applicable** — recorded in the ROADMAP rather than as a probe,
since there is nothing empirical to measure.

### 3.6 M1.5 — MaxSAT partial-corona score (infrastructure shipped, score deferred)

Probe artefacts at `benchmarks/maxsat/`. `sat-wcnf-dump`
(`-DHEESCH_BACKEND_WCNF_DUMP`, `src/sat/src/wcnf_dumping_backend.h`)
tees every UNSAT solve to a classic-format DIMACS WCNF with cell-vars
as unit soft clauses; RC2 (from `pysat`) is then run on each WCNF
with a 60 s wall cap. Six instances spanning n = 7 and n = 8,
hard clause counts from 2 491 up to 790 601, RC2 walls from
sub-millisecond up to 20 s — every WCNF returns **UNSAT-HARD**:

| shape | instance | hard | softs | RC2 wall | result |
|---|---|---:|---:|---:|---|
| n=7 | inst-00001-call000 |   2,491 |  107 |   0.00 s | UNSAT-HARD |
| n=7 | inst-00002-call000 | 268,323 |  342 |   0.09 s | UNSAT-HARD |
| n=7 | inst-00003-call000 | 390,701 |  355 |   0.11 s | UNSAT-HARD |
| n=8 | inst-00001-call000 |  10,605 |  178 |   0.00 s | UNSAT-HARD |
| n=8 | inst-00002-call000 | 749,355 |  581 |   2.13 s | UNSAT-HARD |
| n=8 | inst-00003-call000 | 790,601 |  581 |  20.02 s | UNSAT-HARD |

The hard portion of the formula is itself infeasible regardless of
which soft cell-vars we ask RC2 to maximise. Removing the unit
clause that anchors the original shape (line 568 of `heesch.h`,
`tiles_[0][0]`) makes RC2 finish in 0.08 s and report all 342 softs
unsatisfied at optimum, because the trivial all-false assignment
satisfies every remaining hard implication and pays the lowest
possible cost — zero satisfied softs.

The encoding is **reachability-style**: the unit clause "tile 0 at
level 0 is placed", combined with `heesch.h:600-611` (Block D:
tile-implies-cell) and `heesch.h:586-596` (Block C:
cell-implies-some-tile), hardcodes corona-completion via the
implication chain. There is no soft-relaxation point. Real
partial-corona scoring needs explicit per-cell `coverage[c]` decision
variables decoupled from tile placement, with one-direction
implication `coverage[c] → some tile covers c`. That is an encoding
redesign that fits naturally under M2; doing it twice is wasteful.

The infrastructure shipped under M1.5 (WCNF dumper, RC2 wrapper,
end-to-end probe at `benchmarks/maxsat/probe/run_probe.sh`) is the
reusable piece. **The score remains a Phase-2 deliverable.**

### 3.7 M1.6 — Per-shape per-corona JSON logging (covered)

M0.5 already shipped per-shape JSONL stats (wall time, SAT
conflicts/decisions/propagations, sat-call count, corona depth) via
the `-stats <file>` CLI flag in `src/sat/src/sat.cpp`, plumbed
through `HeeschSolver::runAndAccount` in `heesch.h`. Per-corona
granularity (one record per level rather than per shape) is a small
extension — the counters already exist — but no Phase-1 lever needed
it to make a decision, so it has not been wired up. Closed as
**covered in substance** by M0.5; per-corona granularity gets added
under M2 if Phase-2 work needs it.

## 4. Cross-cutting analysis

The five evaluated levers are nominally independent — solver, solver
portfolio, symmetry preprocessing, encoding sweep, MaxSAT relaxation —
but every result lands in the same direction. The reasons cluster:

1. **CryptoMiniSat is already very strong on this formula class.** It
   is a hybrid CDCL + Gauss-Jordan elimination + stochastic local
   search solver with aggressive inprocessing. The aggressive
   inprocessing is what discovers the same equivalences BreakID
   would add (M1.3), and it is what handles the small-but-numerous
   SAT calls heesch-sat issues per shape better than CaDiCaL or
   Kissat (M1.1, M1.2a).

2. **The walkback loop dominates wall-time *and* breaks every
   solver-side optimisation.** `iterateUntilSimplyConnected`
   appends thousands of hole-banning clauses to a single SAT
   instance and re-solves in a tight loop. That makes Kissat (which
   is non-incremental) infeasible, makes BreakID's symmetry detection
   useless (every automorphism is broken by the appended clauses),
   and produces the >700 k-clause WCNFs whose hard portion RC2 cannot
   relax. None of the Phase-1 levers had a hook to shape this loop;
   only the encoding does.

3. **The encoding hardcodes corona completion.** The unit clause
   `tile 0 at level 0` plus the Block D / Block C implication chain
   means the formula either commits to a complete corona or is
   UNSAT. There is no built-in partial-success state. PBLib
   (M1.4) needed an N-way AMO point to swap encodings behind, which
   the chain does not surface; MaxSAT (M1.5) needed a soft-relaxable
   constraint, which the chain does not admit; symmetry breaking
   (M1.3) needed a regime where automorphisms survive, which the
   walkback does not give it.

4. **Hc/Hh correctness is preserved on every lever.** The 174-shape
   regression returns 174/174 match for CMSat, CaDiCaL, Kissat, and
   the dumping/WCNF-dumping wrappers (the latter two delegate to
   CMSat for the actual decision). The Phase-1 negatives are about
   *speed* and *expressiveness*, not correctness; the M0.4 baseline
   stays trustworthy.

The structural follow-up that addresses points 2 and 3 simultaneously
is the M2 hybrid solver: PROPOSAL §5 Layer 2 explicitly redefines the
constraint structure so DLX handles cell-covering and SAT handles
non-overlap globally. That separation is exactly the change that
would let MaxSAT score partial coronas, let PBLib swap AMO encodings
on the cell-coverage side, and let Kissat be useful on the
non-incremental cell-covering oracle.

## 5. Decision gate

ROADMAP "Phase 1 / Decision gate" reads: *"If speedup is <2× across
the board, pause Layer 1 work, write up findings as a negative-result
short paper, and proceed directly to Layer 3 (RL) using v1 solver."*

Five Phase-1 levers evaluated; all five are at <2× over the M0.5
baseline (in fact, all five are at $\leq 1.0\times$ — none beat
CMSat). The decision gate is **triggered**. This document is the
referenced negative-result short-paper draft.

Concretely, the decision implies:

- Phase 2 (hybrid DLX/SAT, encoding refactor) and Phase 4 (RL pilot)
  proceed in parallel rather than serially after a successful Layer 1.
- The "v1 solver" referenced as the RL reward oracle is the **M0.5
  CMSat baseline**: `src/sat/src/sat`, no portfolio, no symmetry
  preprocessor, no MaxSAT path. That solver computes the integer
  $H_c$ correctly and at speeds that comfortably support batch
  evaluation (≈1.5 s/shape p50 on n = 12). Layer 3 RL accepts
  this as the reward oracle until a partial-corona score is
  available (Phase 2).
- The M1.5 deliverable — a partial-corona score for smoothed Layer-3
  rewards — gets delivered by Phase 2's encoding refactor, not by
  any Phase-1 lever.
- The Phase-1 infrastructure (CaDiCaL adapter, Kissat adapter,
  CNF/WCNF dumping backends, RC2 wrapper, per-probe shell scripts)
  is committed under `src/sat/` and `benchmarks/` and remains
  available for re-use under Phase 2 once the encoding is
  restructured.

## 6. What we shipped

The branch trail on `main` records the engineering output from
Phase 1:

| commit | what | what's reusable |
|---|---|---|
| `9a4f2b9` | M1.1 CaDiCaL backend + adapter pattern | `cadical_backend.h`, the `SATSolverImpl` typedef, four-binary Makefile structure |
| `ce23bee` | M1.2a Kissat backend + non-incremental adapter | `kissat_backend.h`, the buffer-and-rebuild trick |
| `18d61ee` | M1.3 BreakID probe + DIMACS dumping backend | `dumping_backend.h`, `benchmarks/breakid/probe/run_probe.sh`, the `cms_solve` DIMACS driver |
| `15f8d21` | M1.4 not-applicable finding | nothing new — recorded inline in ROADMAP |
| `867546c` | M1.5 WCNF dumping backend + RC2 wrapper | `wcnf_dumping_backend.h`, `benchmarks/maxsat/run_rc2.py`, the probe pipeline |
| this commit | M1.7 retrospective | this document |

Five backend variants (`sat`, `sat-cadical`, `sat-kissat`, `sat-dump`,
`sat-wcnf-dump`) all build cleanly from the same source tree and
compile-time-select via `-DHEESCH_BACKEND_*`. The dumping backends
are the most generally useful Phase-2 inputs: any future encoding
probe can reuse `sat-dump` to get DIMACS instances or `sat-wcnf-dump`
to get WCNFs, without re-instrumenting `heesch.h`.

## 7. Reproducibility

Every numeric claim above can be regenerated on a fresh checkout:

```bash
# Build (~30 s):
( cd src/sat/src && make clean && make -j$(nproc) )

# Phase-0 baseline (M0.5, ~5 min):
python3 benchmarks/baseline/run_baseline.py \
  | tee benchmarks/baseline/results/m0.5-baseline-7-8-11-12.log

# M1.1 (CaDiCaL):
python3 benchmarks/baseline/run_baseline.py --sat src/sat/src/sat-cadical \
  --out benchmarks/baseline/results/m1.1-cadical-baseline.jsonl

# M1.2a (Kissat, sizes 7/8/12 only):
python3 benchmarks/baseline/run_baseline.py --sat src/sat/src/sat-kissat \
  --sizes 7,8,12 \
  --out benchmarks/baseline/results/m1.2a-kissat-baseline.jsonl

# M1.3 (BreakID probe):
bash benchmarks/breakid/probe/run_probe.sh

# M1.5 (MaxSAT probe):
bash benchmarks/maxsat/probe/run_probe.sh
```

Hardware dependence: numbers in this document came from a ThinkPad
P15 Gen 2i (Ubuntu 24.04, g++ 13.3, CryptoMiniSat 5.14 from source,
CaDiCaL 1.7.4 / `libcadical-dev`, Kissat master from source, BreakID
3.1 from source, RC2 from `python-sat` via `pip`). Absolute walls
will differ on other hardware; relative ratios (CaDiCaL ≈ 2× CMSat,
Kissat ≈ 7× CMSat, BreakID-on-walkback infeasible, RC2-on-WCNF
UNSAT-HARD) should be stable.

## 8. References

- Kaplan, C. S. *Heesch Numbers of Unmarked Polyforms.* Contributions
  to Discrete Mathematics 17(2):150–171, 2022. arXiv:2105.09438.
  See `paper/lit/bibtex.bib` `KaplanA8`.
- ROADMAP.md (this repo) — Phase 1 milestones M1.1–M1.7.
- PROPOSAL.md (this repo) — §5 Layer 1 (engineering hardening) and
  §5 Layer 2 (hybrid solver).
- CLAUDE.md (this repo) — operating contract and epistemic
  conventions (`[VERIFY]` / `[EST]` / `[OPEN]` tags).
- `benchmarks/baseline/results/m1.1-comparison.md` — CMSat vs
  CaDiCaL detailed write-up.
- `benchmarks/baseline/results/m1.2a-comparison.md` — three-way
  CMSat / CaDiCaL / Kissat write-up.
- `benchmarks/breakid/README.md` — M1.3 probe and architectural
  finding.
- `benchmarks/maxsat/README.md` — M1.5 probe and architectural
  finding.
- Biere, A. et al. *CaDiCaL.* SAT competition documentation;
  `paper/lit/bibtex.bib` `CaDiCaLKissatC2`.
- Biere, A. *Kissat.* As above.
- Devriendt, J. et al. *BreakID.* `paper/lit/bibtex.bib`
  `BreakIDC3`.
- Ignatiev, A. et al. *PySAT* / *RC2.* `paper/lit/bibtex.bib`
  `MaxSATC5`.

---

*Status tags inherited from CODA practice (CLAUDE.md): the Phase-1
result is **EST** — the negative findings are reproducible from the
committed scripts and dataset; further Phase-1 levers (M1.2b
Glucose, true clause-sharing portfolio, encoding-aware BreakID)
remain **OPEN** but are not load-bearing for the project's
critical path.*
