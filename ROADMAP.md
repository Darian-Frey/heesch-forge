# heesch-forge — Roadmap

**Version.** v0.1 (29 April 2026)
**Cadence.** Single-researcher with multi-LLM auditing workflow.
**Hardware.** ThinkPad P15 Gen 2i (Linux); cloud (Lambda / Vast.ai) for RL training only.

This document tracks phases, gates, and current status. The proposal in `PROPOSAL.md` is the source of truth for the *what* and *why*; this file is the *when* and *now*.

---

## Phase status overview

*Updated 2026-04-29: M0.1–M0.4 complete; phase-0 progress 4/6 milestones.*

| Phase | Layers | Status | Target completion |
|-------|--------|--------|-------------------|
| 0 — Setup | — | 🟢 active (4/6) | +2 weeks |
| 1 — Engineering | L1 | ⚪ not started | +8 weeks |
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
- [ ] M0.5 — Set up benchmark harness (`benchmarks/`) with per-shape timing, conflict counts, decision counts.
- [ ] M0.6 — Initial LITERATURE.md populated with primary references and PDFs filed.

### Exit criteria

`make test` green on baseline; benchmark report for unmodified `heesch-sat` archived.

---

## Phase 1 — Engineering (4–6 weeks)

**Goal.** `src/sat/` modernised with measurable per-shape speedup.

### Milestones

- [ ] M1.1 — Swap upstream's CryptoMiniSat backend for CaDiCaL as a single-solver baseline; verify no regression against M0.4 reference values.
- [ ] M1.2 — Portfolio harness (CaDiCaL + Kissat + Glucose) with first-to-finish termination.
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

**29 April 2026 (evening).** M0.1–M0.4 closed in one session.

- M0.1: scaffold (5 .md + directory layout + `.gitignore`) pushed; commit `7af2f94` on `main`.
- M0.2: vendored upstream `heesch-sat` at SHA `1adb3720` as `src/sat/` (commit `7767cd4` on branch `m0.2-vendor-upstream`); Linux build patch on top (commit `a750519` on `m0.3-linux-build`) — fixed `-std=c++17` → c++20 (templated lambdas in `tileio.h`), missing `isohedral.o` in `OBJECTS` (sat & surrounds need it for `IsohedralChecker`), and missing `#include <map>` in `isohedral.cpp` (libstdc++ doesn't pull it in transitively the way libc++ does). All five binaries build and run; `ldd ./sat` confirms libcryptominisat5 resolved via `-Wl,-rpath,/usr/local/lib`.
- M0.3: Kaplan dataset tarball downloaded, sha256 `c655cf54…0929b`, `omino/` files extracted into `data/kaplan-2022/`. Provenance and licensing notes in `data/kaplan-2022/PROVENANCE.md`.
- M0.4: regression harness `benchmarks/kaplan/run_regression.py` reproduces all 174 published Hc/Hh values for n ∈ {7, 8, 11, 12} (sat invoked with `-isohedral -hh`). 269.6 s wall on a ThinkPad P15 Gen 2i. Raw log committed at `benchmarks/kaplan/results/m0.4-baseline-7-8-11-12.log`. The full n ≤ 12 sweep (adds 1390 dekominoes; estimated multi-hour) has been left for a separate, longer run.

Next action: M0.5 (benchmark harness with per-shape timing/conflicts/decisions) and M0.6 (LITERATURE.md primary references).

**29 April 2026 (afternoon).** Proposal v0.1 drafted. Roadmap v0.1 drafted. Repo live at github.com/Darian-Frey/heesch-forge (empty, M0.1 effectively complete pending push). Next action: M0.2 (fork heesch-sat into src/sat/).
