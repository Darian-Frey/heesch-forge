# heesch-forge — Roadmap

**Version.** v0.1 (29 April 2026)
**Cadence.** Single-researcher with multi-LLM auditing workflow.
**Hardware.** ThinkPad P15 Gen 2i (Linux); cloud (Lambda / Vast.ai) for RL training only.

This document tracks phases, gates, and current status. The proposal in `PROPOSAL.md` is the source of truth for the *what* and *why*; this file is the *when* and *now*.

---

## Phase status overview

*Updated 2026-04-30: Phase 0 closed; Phase 1 active (M1.1 + M1.2a done; M1.2 in progress).*

| Phase | Layers | Status | Target completion |
|-------|--------|--------|-------------------|
| 0 — Setup | — | ✅ done (6/6) | met |
| 1 — Engineering | L1 | 🟢 active (1/7) | +8 weeks |
| 2 — Hybrid solver | L2 | ⚪ not started | +16 weeks |
| 3 — Lateral grids | L4 | ⚪ not started | parallel from Phase 2 |
| 4 — RL pilot | L3 | ⚪ not started | +28 weeks |
| 5 — RL scale | L3 | ⚪ not started | +40 weeks |
| 6 — Theory | L5 | ⚪ continuous from Phase 1 | +52 weeks |

Legend: ✅ done · 🟢 active · 🟡 in progress · ⚪ not started · 🔴 blocked

---

## Phase 0 — Setup (2 weeks)

**Goal.** Repository scaffolded, baseline reproduced, regression suite green.

### Milestones

- [x] M0.1 — Git repo created; scaffold pushed (5 .md files + directory layout + .gitignore).
- [x] M0.2 — Vendored `isohedral/heesch-sat` into `src/sat/` (upstream SHA `1adb3720`); Linux build patched (g++/c++20, missing `isohedral.o`, missing `<map>` include); all five binaries (gen/sat/viz/surrounds/report) build and run on Ubuntu 24.04.
- [x] M0.3 — Kaplan 2022 dataset (`heesch_dataset.tar.gz`, sha256 `c655cf54…0929b`) extracted to `data/kaplan-2022/omino/`; provenance recorded in `data/kaplan-2022/PROVENANCE.md`.
- [x] M0.4 — Regression baseline locked: `benchmarks/kaplan/run_regression.py` reproduces all 174 published Hc/Hh values for n ∈ {7, 8, 11, 12} (with `-isohedral -hh`); raw log at `benchmarks/kaplan/results/m0.4-baseline-7-8-11-12.log`. Full n ≤ 12 sweep deferred (dekomino set is multi-hour).
- [x] M0.5 — Benchmark harness `benchmarks/baseline/` records per-shape wall time, SAT conflicts/decisions/propagations, solve-call counts, and corona depth via a new `-stats` flag in `src/sat/src/sat.cpp` plus a `runAndAccount` accumulator in `HeeschSolver`. Baseline JSONL at `benchmarks/baseline/results/m0.5-baseline.jsonl`; canonical run log at `benchmarks/baseline/results/m0.5-baseline-7-8-11-12.log`. Failsafe (`-old`) path is not instrumented.
- [x] M0.6 — `LITERATURE.md` carries 25 annotated entries (Heesch primary, aperiodic monotile, search techniques, ML, theory, software). Canonical BibTeX at `paper/lit/bibtex.bib`; open-access PDFs are fetched on demand by `paper/lit/fetch_pdfs.sh` (PDFs gitignored). Verification roster at the top of `LITERATURE.md`: A.8, F.1, F.3 promoted to [EST] from M0.2/M0.3/M0.4 work, the rest are [VERIFY] until a maintainer reads them.

### Exit criteria

`make test` green on baseline; benchmark report for unmodified `heesch-sat` archived.

---

## Phase 1 — Engineering (4–6 weeks)

**Goal.** `src/sat/` modernised with measurable per-shape speedup.

### Milestones

- [x] M1.1 — CaDiCaL backend lives alongside CryptoMiniSat behind a single `SATSolverImpl` typedef in `src/sat/src/heesch.h`, selected at compile time by `-DHEESCH_BACKEND_CADICAL`; the Makefile builds both `sat` (CMSat) and `sat-cadical` (CaDiCaL). M0.4 regression: 174/174 shapes match for both backends. Performance against the M0.5 baseline (n ∈ {7, 8, 11, 12}, 174 shapes): CaDiCaL 569.2 s vs CMSat 256.2 s — **2.22× slower at total wall, 3.0× at p95, 3.3× at max**, with `sat_calls` count preserved (1046 vs 1047). Side-by-side at `benchmarks/baseline/results/m1.1-comparison.md`. Single-solver swap is not the Phase-1 win; M1.2 (portfolio first-to-finish + clause sharing) and M1.3 (BreakID symmetry breaking) need to deliver the headline speedup.
- [ ] M1.2 — Portfolio harness (CaDiCaL + Kissat + Glucose) with first-to-finish termination. **In progress: M1.2a (Kissat backend) closed; Glucose + portfolio harness pending.**
  - [x] M1.2a — Kissat backend wired in via `src/sat/src/kissat_backend.h` and a `-DHEESCH_BACKEND_KISSAT` build (`sat-kissat`). M0.4 regression: 64/64 match on sizes 7, 8, 12. Performance vs CMSat (same subset, 64 shapes): **7.33× slower at total wall**, p95 32 s vs CMSat's 3.6 s. n = 11 batch skipped because Kissat's non-incremental design forces full clause-rebuilds in `iterateUntilSimplyConnected`'s `while-solve-then-add-clauses` loop, pushing per-shape cost above the 5-min ceiling. Three-way write-up at `benchmarks/baseline/results/m1.2a-comparison.md`. Headline architectural finding: none of the three modern CDCL competitors (CaDiCaL, Kissat, soon Glucose) beats CryptoMiniSat on this workload, so the M1.2 portfolio must *include* CMSat rather than replace it. PROPOSAL §5's "replace single-solver SAT call with a portfolio" wording needs to be read as *augment*, not *replace*.
- [ ] M1.3 — BreakID integration; per-shape comparison with/without symmetry breaking.
- [ ] M1.4 — PBLib AMO encoding sweep; record per-corona-depth optimum.
- [ ] M1.5 — MaxSAT path: drop in RC2 / EvalMaxSAT; expose partial-corona score.
- [ ] M1.6 — Structured logging: JSON per-shape per-corona records.
- [ ] M1.7 — Benchmark report v1: speedup factor, MaxSAT score distribution, sensitivity to encoding choice.

### Exit criteria

≥5× speedup on Kaplan's H=2 and H=3 shapes; MaxSAT path returns partial-corona scores within 10% wall-time of yes/no SAT.

### Decision gate

If speedup is <2× across the board, pause Layer 1 work, write up findings as a negative-result short paper, and proceed directly to Layer 3 (RL) using v1 solver.

---

## Phase 2 — Hybrid solver (6–8 weeks)

**Goal.** Algorithm X / Dancing Links integrated; crossover characterised.

### Milestones

- [ ] M2.1 — DLX implementation in `src/dlx/`. Unit tests against Knuth's worked examples.
- [ ] M2.2 — Wrap DLX as a corona-completion oracle for depths 1, 2.
- [ ] M2.3 — Define handoff format: DLX output → SAT input.
- [ ] M2.4 — End-to-end hybrid pipeline; regression suite green.
- [ ] M2.5 — Benchmark across all of Kaplan's dataset; identify crossover depth.
- [ ] M2.6 — Investigate joint DLX+SAT formulation.
- [ ] M2.7 — Engineering paper draft.

### Exit criteria

Demonstrable order-of-magnitude speedup on inner coronas (depth 1, 2) and no slowdown on outer coronas; crossover depth reported for each polyform family.

---

## Phase 3 — Lateral grids (parallel from Phase 2)

**Goal.** First published Heesch numbers for bevelhex and octasquare polyforms.

### Milestones

- [ ] M3.1 — Verify Kaplan's bevelhex grid generation code; fix bugs if found.
- [ ] M3.2 — Enumerate bevelhex polyforms up to n_max ≈ 12 (revisit upper bound based on cost).
- [ ] M3.3 — Enumerate octasquare polyforms similarly.
- [ ] M3.4 — Tabulate analogues of Kaplan's H=4 catalogue.
- [ ] M3.5 — Short empirical paper draft.

### Exit criteria

Smallest n with H=k catalogued for both grids, k ∈ {1, 2, 3, 4(+)}.

---

## Phase 4 — RL pilot (8–12 weeks)

**Goal.** Recover Kaplan's H=3 polyominoes at n=17 with substantially less compute than exhaustive enumeration.

### Milestones

- [ ] M4.1 — Define MDP: state encoding, action space, episode termination.
- [ ] M4.2 — GNN policy network (PyTorch Geometric); baseline random-policy reward distribution.
- [ ] M4.3 — Implement deep cross-entropy training loop (Wagner 2021).
- [ ] M4.4 — Reward = MaxSAT-derived score from Layer-1+2 solver.
- [ ] M4.5 — Pilot run on n ≤ 12: verify discovery of all known H=2 shapes.
- [ ] M4.6 — Scale to n=17: target rediscovery of Kaplan's two H=3 shapes.
- [ ] M4.7 — Compute audit: report total CPU-hours vs. exhaustive enumeration baseline.

### Exit criteria

≥80% of Kaplan's H=3 shapes rediscovered using ≤10% of the compute budget Kaplan reported for exhaustive enumeration at n=17.

### Decision gate

If pilot fails to recover H=3 shapes, pivot: (a) add MCTS-with-priors; (b) imitation learning warm-start from Kaplan's H=2 catalogue; (c) richer action space (allow square removal as well as addition).

---

## Phase 5 — RL scale (8–12 weeks)

**Goal.** Push search to n=20–25 polyominoes; address RQ1 (does H≥4 polyomino exist?).

### Milestones

- [ ] M5.1 — Curriculum: warm-start at n=17 from Phase-4 model; expand by one square per curriculum step.
- [ ] M5.2 — Cloud GPU run (Lambda / Vast).
- [ ] M5.3 — Catalogue all candidates with MaxSAT score above the H=2/H=3 boundary.
- [ ] M5.4 — Rigorous SAT verification of all candidates (no MaxSAT shortcuts in publication path).
- [ ] M5.5 — RL paper draft.

### Exit criteria

Either: (a) a verified H≥4 polyomino is found and announced; (b) a high-confidence empirical absence claim is established with auditable search-coverage statistics.

---

## Phase 6 — Theory (continuous, ~12 months)

**Goal.** New upper-bound obstructions or formal characterisation of failure modes.

### Milestones

- [ ] M6.1 — Formalise corona-completion as equivariant lifting.
- [ ] M6.2 — Compute boundary edge-words for known H ≥ 2 polyominoes; identify group-theoretic patterns.
- [ ] M6.3 — Test candidate cohomological obstruction against Kaplan's full dataset.
- [ ] M6.4 — Cross-pollinate with hat einstein substitution machinery.
- [ ] M6.5 — Theory paper draft.

### Exit criteria

Either a new published obstruction strictly stronger than Ammann's parity argument, or a formal articulation of why such obstructions do not exist (which is itself a positive structural result).

---

## Cross-cutting artefacts

These run alongside all phases and are updated continuously.

**Live now:**

- `PROPOSAL.md` — master research proposal.
- `LITERATURE.md` — annotated bibliography.
- `CLAUDE.md` — current state for handoff into Claude Code or other LLMs.
- `README.md` — public entry point.
- `ROADMAP.md` — this file.

**Planned (single source of truth — do not invent or stub these elsewhere):**

| File | Trigger | Purpose |
|------|---------|---------|
| `.gitignore` | M0.1 (repo creation) | Standard Python/C++ ignore patterns plus `data/results/raw/`. |
| `LICENSE` | M0.1 (repo creation) | BSD-3-Clause for code (matches vendored `heesch-sat` upstream); CC-BY 4.0 for `data/` and `paper/` via per-directory `LICENSE` files. |
| `EXPERIMENTS.md` | First Phase-1 experiment | Running log of attempts and outcomes, including negatives. Append-only, latest first. |
| `OPEN_QUESTIONS.md` | First side-question that doesn't fit roadmap | Accumulating list of side-questions with EST/SPEC/OPEN/SEALED tags. |
| `BENCHMARKS.md` | M0.5 (benchmark harness) | Formal benchmark protocol once regression suite exists. |
| `CITATION.cff` | First public release (engineering paper or dataset) | Standard citation metadata. |
| `paper/engineering/main.tex` | Phase 2 active | Layer 1+2 solver paper. |
| `paper/lateral/main.tex` | Phase 3 active | Bevelhex / octasquare empirical paper. |
| `paper/rl/main.tex` | Phase 4 active | Layer 3 RL-discovery paper. |
| `paper/theory/main.tex` | Phase 6 has a candidate obstruction | Layer 5 theory paper. |

When a planned file is created, move its row from this table into the "Live now" list above and update the date in the status notes.

---

## Status notes (latest first)

**30 April 2026 (later).** M1.2a closed; M1.2 still open (Glucose backend + portfolio harness pending).

- M1.2a: third compile-time backend wired in via `src/sat/src/kissat_backend.h` (Kissat's C API, `quiet`-mode init), produces `sat-kissat`. Three-way regression / baseline pass on n ∈ {7, 8, 12} (64 shapes, 64/64 match every backend); the n = 11 batch was attempted at first but pulled after one shape ran past 14 minutes — Kissat's non-incremental design forces a full clause-rebuild on every `solve()` call inside `iterateUntilSimplyConnected`'s `while-solve-then-add-clauses` loop, and the n = 11 walkback compounds that into infeasibility on this hardware.
- Performance on the common n ∈ {7, 8, 12} subset (64 shapes): CMSat 61.4 s, CaDiCaL 119.6 s, Kissat 450.3 s. Kissat is **7.33× slower than CMSat at total wall**, p95 32 s vs CMSat's 3.6 s. `sat_calls` matches across all three (336–337) — same algorithm, the gap is purely solver behaviour. Three-way write-up at `benchmarks/baseline/results/m1.2a-comparison.md`.
- Phase-1 architectural finding (load-bearing): **none of the three modern CDCL competitors beats CryptoMiniSat on this workload.** PROPOSAL §5's "replace single-solver SAT call with a portfolio" wording must be read as *augment*, not *replace* — a first-to-finish portfolio of CaDiCaL + Kissat ± Glucose with CMSat *excluded* would be strictly slower than CMSat solo. Concrete consequence for M1.2b: the portfolio MUST include CryptoMiniSat. Concrete consequence for Phase 1 overall: the ≥5× speedup target is not reachable by solver swap; M1.3 (BreakID symmetry breaking, orthogonal to solver) and M1.4 (encoding sweep) have to do the lifting.

**30 April 2026.** M1.1 closed; Phase 1 opened.

- M1.1: introduced an `#ifdef HEESCH_BACKEND_CADICAL`-driven `SATSolverImpl` typedef in `src/sat/src/heesch.h`, written a CaDiCaL adapter at `src/sat/src/cadical_backend.h` mimicking the CMSat::SATSolver subset that HeeschSolver actually uses (`new_vars`, `add_clause(vector<Lit>)`, `solve()->lbool`, `get_model()`, plus the three `get_last_*` counter accessors), and updated the Makefile to build both `sat` (CMSat) and `sat-cadical` (CaDiCaL) from the same sources. CMSat::Lit/lbool remain the project-wide literal types in both builds, so the rest of `heesch.h` is backend-agnostic.
- Correctness: M0.4 regression returns 174/174 match for both backends.
- Performance (n ∈ {7, 8, 11, 12}, 174 shapes): CaDiCaL 569.2 s vs CMSat 256.2 s — **2.22× slower at total wall, 3.0× at p95, 3.3× at max**. SAT solve-call counts are identical to within one (1046 vs 1047), confirming the heesch-sat algorithm is unchanged. Side-by-side at `benchmarks/baseline/results/m1.1-comparison.md`.
- Caveat: CaDiCaL 1.7's public API does not expose conflicts/decisions/propagations counters, so `sat-cadical` writes zeros in those JSONL fields; recorded in `src/sat/UPSTREAM.md` and `benchmarks/baseline/README.md`. If a future CaDiCaL release adds the accessors, only `cadical_backend.h` changes.

Single-solver swap is **not** the Phase-1 win. M1.2 (portfolio with clause sharing) and M1.3 (BreakID) need to do most of the heavy lifting toward the ≥5× target.

**29 April 2026 (night).** M0.6 closed; **Phase 0 done**.

- M0.6: structural literature pass. `LITERATURE.md` (already drafted in v0.1 with 25 annotated entries across six topic groups) is now backed by `paper/lit/bibtex.bib` (29 BibTeX records, citation keys mirror section IDs — `KaplanA8` etc.), a `paper/lit/fetch_pdfs.sh` script for open-access PDFs, and a `paper/lit/.gitignore` that keeps fetched PDFs local-only (rights vary, repo stays small). `LITERATURE.md` gains a "Bibliography metadata and verification" section that promotes A.8 (Kaplan 2022 polyforms), F.1 (heesch-sat), and F.3 (Kaplan dataset) to `[EST]` on the back of M0.2–M0.4 hands-on work; everything else is implicitly `[VERIFY]` per CLAUDE.md's "never cite what hasn't been read" rule. The most suspicious tag flagged for next-pass attention: E.2 `arXiv:2603.27827` — submission stamp encodes 2026-03, confirm the ID before citing.

Phase 0 exit criterion (`make test` green on baseline; benchmark report archived) is met by M0.4 + M0.5 jointly. Ready to open Phase 1.

**29 April 2026 (late evening).** M0.5 closed.

- M0.5: `src/sat/` patched to instrument the default-path SAT solve loop. `HeeschSolver` gained four `uint64_t` cumulative counters (conflicts, decisions, propagations, solve-call count) populated by a new private `runAndAccount` helper that wraps every `CMSat::SATSolver::solve()` reachable from `HeeschSolver::solve()` (five sites: the main corona loop, the two `-hh` final-solver fallbacks, `iterateUntilSimplyConnected`, and `checkIsohedralTiling`). `sat` gained a `-stats <file>` CLI flag that emits one JSON object per processed shape (schema in `benchmarks/baseline/README.md`). Wrapping that, `benchmarks/baseline/run_baseline.py` produces a per-shape JSONL of dataset Hc/Hh, sat Hc/Hh, classification, levels, wall time, and SAT counts. Baseline numbers on n ∈ {7, 8, 11, 12} (174 shapes, 256.2 s wall): p50 1.42 s/shape, p95 3.53 s, max 4.53 s; 612 k SAT conflicts, 4.0 M decisions, 174 M propagations across 1047 SAT solve calls. M0.4 regression rerun confirms instrumentation is byte-equivalent (174/174 still match).

Next action: M0.6 (LITERATURE.md primary references and PDFs filed).

**29 April 2026 (evening).** M0.1–M0.4 closed in one session.

- M0.1: scaffold (5 .md + directory layout + `.gitignore`) pushed; commit `7af2f94` on `main`.
- M0.2: vendored upstream `heesch-sat` at SHA `1adb3720` as `src/sat/` (commit `7767cd4` on branch `m0.2-vendor-upstream`); Linux build patch on top (commit `a750519` on `m0.3-linux-build`) — fixed `-std=c++17` → c++20 (templated lambdas in `tileio.h`), missing `isohedral.o` in `OBJECTS` (sat & surrounds need it for `IsohedralChecker`), and missing `#include <map>` in `isohedral.cpp` (libstdc++ doesn't pull it in transitively the way libc++ does). All five binaries build and run; `ldd ./sat` confirms libcryptominisat5 resolved via `-Wl,-rpath,/usr/local/lib`.
- M0.3: Kaplan dataset tarball downloaded, sha256 `c655cf54…0929b`, `omino/` files extracted into `data/kaplan-2022/`. Provenance and licensing notes in `data/kaplan-2022/PROVENANCE.md`.
- M0.4: regression harness `benchmarks/kaplan/run_regression.py` reproduces all 174 published Hc/Hh values for n ∈ {7, 8, 11, 12} (sat invoked with `-isohedral -hh`). 269.6 s wall on a ThinkPad P15 Gen 2i. Raw log committed at `benchmarks/kaplan/results/m0.4-baseline-7-8-11-12.log`. The full n ≤ 12 sweep (adds 1390 dekominoes; estimated multi-hour) has been left for a separate, longer run.

Next action: M0.5 (benchmark harness with per-shape timing/conflicts/decisions) and M0.6 (LITERATURE.md primary references).

**29 April 2026 (afternoon).** Proposal v0.1 drafted. Roadmap v0.1 drafted. Repo live at github.com/Darian-Frey/heesch-forge (empty, M0.1 effectively complete pending push). Next action: M0.2 (fork heesch-sat into src/sat/).
