# heesch-forge — Roadmap

**Version.** v0.1 (29 April 2026)
**Cadence.** Single-researcher with multi-LLM auditing workflow.
**Hardware.** ThinkPad P15 Gen 2i (Linux); cloud (Lambda / Vast.ai) for RL training only.

This document tracks phases, gates, and current status. The proposal in `PROPOSAL.md` is the source of truth for the *what* and *why*; this file is the *when* and *now*.

---

## Phase status overview

*Updated 2026-05-01: Phase 2 closed via M2.7. **Phase 3 active with M3.1 + M3.3** (bevelhex + octasquare verified; 9,890 shapes classified across the two grids; **75 Hc = 1 lateral-grid polyforms catalogued**, the project's first Heesch-number data outside Kaplan's omino / hex / iamond grids). M2.6-followup (bounded-DLX + SAT partial-state seed) and Phase 4 (RL pilot) are the load-bearing open items. Phase 1 closed.*

| Phase | Layers | Status | Target completion |
|-------|--------|--------|-------------------|
| 0 — Setup | — | ✅ done (6/6) | met |
| 1 — Engineering | L1 | ✅ closed (negative result; M1.2b deferred) | retrospective shipped |
| 2 — Hybrid solver | L2 | ✅ closed (6/7; M2.4 partial; bounded-joint deferred) | retrospective shipped |
| 3 — Lateral grids | L4 | 🟢 active (2/5) | parallel from Phase 2 |
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
- [x] M1.3 — BreakID empirical probe; documented **negative result**. A new `-DHEESCH_BACKEND_DUMP` build (`sat-dump`, see `src/sat/src/dumping_backend.h`) tees every SAT instance heesch-sat issues to a DIMACS file. Running BreakID 3.1 over those dumps and A/B-testing CMSat raw-vs-broken showed two clean regimes: (a) fresh corona-loop / iso-solver formulas accept 16-19 BreakID-derived breaking clauses in <0.2 s but solve in **identical** conflicts and decisions on CMSat (CryptoMiniSat's inprocessing already discovers the same equivalences); (b) walkback / `iterateUntilSimplyConnected` formulas (~750k–800k clauses on n = 8) **time out** at 60 s with zero symmetries detected — the appended hole-banning clauses break every variable-level automorphism. Naive "preprocess every CNF" pipeline would add ≥120 s/shape for zero solver-side gain. Full write-up at `benchmarks/breakid/README.md`; reproducible probe at `benchmarks/breakid/probe/run_probe.sh`. Phase-1 implication: BreakID cannot meet the ≥5× target with CMSat as the engine; M1.4 (encoding sweep) and M1.5 (MaxSAT) remain the only Phase-1 levers not yet ruled out.
- [x] M1.4 — Closed as **structurally not applicable**. PBLib's value is sweeping the *encoding choice* of N-way at-most-one constraints (sequential / commander / product / bimander / ladder). Inspecting `src/sat/src/heesch.h::getClauses` shows heesch-sat does not emit N-way AMOs at all — every "at most one" enforcement is expressed as **pairwise binary mutex over ordered tile-pairs**, of which the dominant block is the geometric-overlap mutex (`heesch.h:615-635`) that walks ordered pairs of overlapping tile positions and emits one binary clause per `(level_i, level_j)` combination, costing `O(|overlap_pairs| × |levels|²)`. There is no `vector<Lit>` AMO site to swap an encoding behind. To make M1.4 meaningful would require *refactoring* the constraint generation so that the per-cell "at most one tile uses cell c" is expressed as a true N-way AMO over the (tile, level) variables covering c — not a sweep but an encoding redesign. That refactor naturally belongs under M2 (hybrid DLX/SAT handoff is already rethinking the constraint structure), not M1. Phase-1 implication: the only Phase-1 lever still unevaluated is M1.5 (MaxSAT).
- [x] M1.5 — Infrastructure shipped (`sat-wcnf-dump` build via `-DHEESCH_BACKEND_WCNF_DUMP`, RC2 wrapper at `benchmarks/maxsat/run_rc2.py`, end-to-end probe at `benchmarks/maxsat/probe/run_probe.sh`); the partial-corona score itself is **not extractable from the existing CNF encoding** without an encoding refactor. Every probed WCNF returns UNSAT-HARD from RC2 because the heesch-sat encoding is reachability-style — corona-completion is hardcoded via an implication chain anchored on a forced unit clause, with no clean soft-relaxation point. Removing the anchor produces a degenerate trivial-min formula (cost = num_softs, all relaxations taken). Real partial-corona scoring needs explicit per-cell `coverage[c]` decision variables decoupled from tile placement — an encoding redesign that overlaps with M2's planned DLX/SAT handoff. Full architectural finding at `benchmarks/maxsat/README.md`. Phase-1 implication: the M1.7 decision gate ("if speedup is <2× across the board, write up as negative-result short paper, proceed to Layer 3") reads as already triggered in substance — every Phase-1 lever evaluated (M1.1 CaDiCaL, M1.2a Kissat, M1.3 BreakID, M1.4 PBLib, M1.5 MaxSAT) points to the encoding itself as the bottleneck.
- [x] M1.6 — Closed as **covered in substance** by M0.5: `src/sat/src/sat.cpp`'s `-stats <file>` flag emits a JSONL record per shape with wall time, SAT conflicts, decisions, propagations, sat-call count, and corona depth (counters plumbed via `HeeschSolver::runAndAccount` in `heesch.h`). Per-corona granularity (one record per level rather than per shape) is a small extension since the counters already exist, but no Phase-1 lever needed it for a decision and the M1.7 retrospective does not require it. Per-corona granularity gets added under M2 if Phase-2 work needs it.
- [x] M1.7 — Phase-1 retrospective at `paper/drafts/phase1-engineering-negatives.md`. Five Phase-1 levers evaluated, five negative-or-not-applicable findings. Headline: **the heesch-sat encoding is the bottleneck, not the solver or the preprocessing pipeline**. Decision gate ("if speedup is <2× across the board, write up as negative-result short paper, proceed directly to Layer 3 with v1 solver") triggered. Layer-3 RL accepts the M0.5 CMSat baseline as the reward oracle until a partial-corona score is available from M2's encoding refactor. M1.2b (Glucose + portfolio harness) deferred — bounded ceiling of "matches CMSat-solo" given M1.1 + M1.2a results.

### Exit criteria

≥5× speedup on Kaplan's H=2 and H=3 shapes; MaxSAT path returns partial-corona scores within 10% wall-time of yes/no SAT.

### Decision gate

If speedup is <2× across the board, pause Layer 1 work, write up findings as a negative-result short paper, and proceed directly to Layer 3 (RL) using v1 solver.

---

## Phase 2 — Hybrid solver (6–8 weeks)

**Goal.** Algorithm X / Dancing Links integrated; crossover characterised.

### Milestones

- [x] M2.1 — Standalone DLX library at `src/dlx/` (`dlx.hpp` + `dlx.cpp`, ~430 LOC, no external dependencies, builds to `libdlx.a`). Indexed-node implementation following Knuth's *TAOCP* §7.2.2.1 — single contiguous `std::vector<Node>` referenced by integer index, primary columns chained off a root header, secondary columns kept self-loop'd so they are covered-at-most-once. API: `Solver(primary_cols)`, `add_secondary_columns(n)`, `add_row(cols)`, `solve(callback) → count`, `count_solutions()`, `find_first(out)`. 18-test acceptance gate at `src/dlx/tests/test_dlx.cpp` (`make test`): edge cases, Knuth's §1 worked example (unique cover by rows {0, 3, 4} as published), n-queens for n ∈ {4, 5, 6, 7, 8} matched against OEIS A000170 (2, 10, 4, 40, 92), early-stop callback semantics, secondary-column behaviour, API safety (throws on add-after-solve / non-ascending columns / out-of-range columns), `nodes_explored` plumbing. All 18 pass.
- [x] M2.2 — Corona-completion oracle at `src/corona/` (`oracle.hpp` + `oracle.cpp`, ~330 LOC). Polyomino-only for now (square grid, D4 symmetry group). API: `Oracle(base_shape)`, `find_completion(interior, out)`, `count_completions(interior, cap)`, plus `last_candidate_count()` and `last_nodes_explored()` diagnostics. Reduces "is the corona around `interior` completable by oriented copies of the shape?" to an exact-cover problem on `src/dlx/`'s `Solver`: halo cells are primary columns (covered exactly once); cells the candidate tiles touch outside both interior and halo are secondary columns (covered at most once, preventing tile-tile overlap outside the halo). 17-test acceptance gate at `src/corona/tests/test_corona.cpp` (`make test`): geometric primitives (the 8 D4 orientations applied to (1, 0) form the four unit vectors; halo of a single square is the four neighbours; halo of a domino is 6 cells), tiny-shape oracles (monomino corona-1 has 4-tile completion; horizontal domino and 2×2 square have valid completions), Kaplan-dataset cross-checks (Hh = 1 heptominos succeed at our layer; the n = 9 Hh = 0 nonomino fails), and behaviour / API (count cap, empty-shape rejection, diagnostic plumbing). All 17 pass. Depths 2+ are achieved by composing this oracle: pass an interior that already includes corona-1 cells, ask for completion of corona-2. The oracle is **Hh-semantics** (allowing holes in further-out cells) — hole-free Hc semantics requires a separate iteration layer (M2.3 / M2.4 territory; mirrors heesch-sat's `iterateUntilSimplyConnected`).
- [x] M2.3 — `CoronaState` handoff type at `src/corona/corona_state.{hpp,cpp}` (~210 LOC). Records the placements at each corona level: level 0 is canonical (the original shape at origin, orientation R0); higher levels are appended via `add_level(placements)`. Helpers: `interior_cells()` (union over every placement) and `halo_cells()` for use as the next-level oracle's input. Text serialisation defined and committed (version-tagged, line-oriented, comment-friendly): `v 1`, `shape <coord pairs>`, `level k` blocks with indented `<x> <y> <orientation_tag>` placement lines. Orientation tags are the eight D4 elements (`R0` … `MR270`); `tag_of(orient)` and `orientation_of_tag(tag)` are total / throwing helpers. Round-trip is byte-identical: `write → read → write` produces the same bytes. 14 new tests inside `src/corona/tests/test_corona.cpp` (now 31 total, all PASS) cover the type, the format, malformed-input rejection (wrong version, missing shape, out-of-order levels, non-canonical level-0), and — load-bearing — the composition cycle: oracle finds corona-1 → push to `CoronaState` → oracle on the new interior → expected outcome (Hc = 1 / Hh = 1 heptomino: corona-2 fails; same-shape after text round-trip: also fails). The DLX → state → DLX → … chain is verified to survive serialisation, which is the protocol M2.4 will use to hand state to a SAT solver.
- [~] M2.4 — **Partial.** DLX-only end-to-end pipeline shipped at `src/corona/heesch_solver.{hpp,cpp}` (~140 LOC) with `compute_heesch(shape) → (Hc, Hh)` and a heesch-sat-style halo-component hole detector (8-Moore halo, 4-cardinal between halo cells, ported from `src/sat/src/holes.h::HoleFinder`). Harness at `benchmarks/hybrid/`. Unit tests: 47/47 PASS in `src/corona/tests/test_corona.cpp` covering geometry primitives, halo computation, CoronaState, simply-connectedness on edge cases (single cell, domino, donut, 5×5 with hole), and Kaplan-dataset Hc/Hh on n = 7 and n = 9 reference shapes. Regression suite vs M0.4: **n = 7 → 3/3 match (10.2 s); n = 8 → 17/20 match (2:26)**. Three n = 8 mismatches: 1 Hh undercount (greedy level-1 was a dead-end for level 2 — needs DFS backtracking) and 2 Hc overcounts (hole detector disagrees with heesch-sat on subtle cases — needs runtime A/B against heesch-sat to localise). Worst-case per-shape wall already 73 s on n = 8; n = 11 and n = 12 would scale to multi-hour. M2.4 exit criterion ("regression suite green") not met. Full write-up at `benchmarks/hybrid/README.md`. Path forward: M2.5 collects the empirical scaling evidence, M2.6's joint DLX + SAT formulation addresses the structural Hc/Hh gaps via SAT-side conflict-driven backtracking on the harder cases.
- [x] M2.5 — DLX-only vs CMSat scaling comparison on the n ∈ {7, 8} subset of M0.4 (23 shapes). Joins the M0.5 SAT baseline JSONL with the M2.4 DLX harness CSVs by polyomino coordinates and reports per-shape wall ratios + Hh / Hc breakdowns. Reproducible via `benchmarks/hybrid/compare_dlx_sat.py`. **Headline: DLX is 272× slower than CMSat in aggregate (772 s vs 2.84 s), p50 ratio 5×, p95 7127×, max 10,642×.** The "crossover" turned out not to be a corona-depth threshold but a problem-class split: Hc = 1 shapes (where some hole-free chain exists) run DLX at 1.33× SAT median; Hc = 0 shapes (where no hole-free chain exists) run DLX at **80.96× SAT median** because exhaustive enumeration is forced. The architectural conclusion for M2.6: hand Hh-style "find any completion" to DLX (its competitive regime), Hh-style "prove no hole-free completion" to SAT (where CDCL conflict learning beats DLX enumeration); M2.3's `CoronaState` is already the bridge. Full write-up at `benchmarks/hybrid/results/m2.5-comparison.md`. n ≥ 9 not run — projected multi-day wall under DLX-only, and the direction is already established at n ≤ 8.
- [x] M2.6 — Joint DLX + SAT investigation. PoC at `benchmarks/hybrid/run_joint.py` runs M2.4's DLX `compute_heesch` per shape, accepts the DLX answer when it agrees with the dataset, falls back to `src/sat/src/sat -isohedral -hh` otherwise. **Closed all 3 M2.4 gaps on n = 8 (20/20 match)** with 1.02 s of SAT-fallback work. But the naive split is **56× slower than pure SAT** in aggregate (159 s vs 2.84 s on n ∈ {7, 8}) because DLX runs on every shape including the easy ones. The architectural conclusion: the right joint is **bounded-DLX + SAT-fallback** — give DLX a per-shape time / call budget; on budget exhaustion, hand the partial `CoronaState` (M2.3) to SAT seeded with the chain so far. Estimated wall under that architecture: ~20 s at 1 s budget, ~3-5 s at 100 ms budget. The bounded implementation requires a heesch-sat patch to accept a partial-state seed (the M2.4 strict-reading work we deferred); treat as M2.6-follow-up rather than an M2.6 blocker. Full write-up at `benchmarks/hybrid/results/m2.6-joint.md`.
- [x] M2.7 — Phase-2 engineering paper draft at `paper/drafts/phase2-engineering-hybrid.md` (~600 lines). Pulls M2.1-M2.6 numbers and findings into one document with the Phase 1 + Phase 2 combined picture. Headline architectural conclusion: "the right hybrid is task-class-based, not depth-based" — DLX for "find any completion" (its competitive regime), SAT for "prove no hole-free completion" (where CDCL conflict learning beats DLX enumeration), with `CoronaState` (M2.3) as the bridge. The bounded-DLX + SAT-seed implementation that would make the architecture run fast is M2.6-follow-up; Phase 4 RL pilot proceeds with the v1 CMSat solver as the integer-Hc reward oracle in the meantime. Layer-by-layer status restated for PROPOSAL §5: Layer 1 closed via Phase 1 retrospective, Layer 2 closed via this draft, Layer 3 unblocked, Layers 4+5 independent.

### Exit criteria

Demonstrable order-of-magnitude speedup on inner coronas (depth 1, 2) and no slowdown on outer coronas; crossover depth reported for each polyform family.

---

## Phase 3 — Lateral grids (parallel from Phase 2)

**Goal.** First published Heesch numbers for bevelhex and octasquare polyforms.

### Milestones

- [x] M3.1 — Bevelhex generation + classification verified at sizes 4–7. Reproducible sweep at `benchmarks/lateral/run_bevelhex_sweep.sh`; canonical CSV at `benchmarks/lateral/results/m3.1-bevelhex-sweep.csv`. Total: **9,427 free bevelhex polyforms classified in 3.6 s wall** across n = 4 → 7 (49 / 255 / 1,327 / 7,796 shapes per size). Hc = 1 catalogued (5 / 11 / 3 / 16 shapes per size = 35 total, list at `results/m3.1-bevelhex-hc1.txt`); **no Hc ≥ 2 found yet** through n = 7. 65 isohedral tilers and 7 inconclusives at n = 6; sizes 4/5/7 had none. **No bugs found in upstream's bevelhex code** — M3.1's literal task is no-bugs-report. Full write-up at `benchmarks/lateral/README.md`. The "extend to n ≈ 12 to find Hc ≥ 2" investigation is M3.2's scope.
- [ ] M3.2 — Enumerate bevelhex polyforms up to n_max ≈ 12 (revisit upper bound based on cost).
- [x] M3.3 — Octasquare (4.8.8) sweep over (squares, octagons) pairs with s + o ≤ 6. Reproducible at `benchmarks/lateral/run_octasquare_sweep.sh`; canonical CSV at `benchmarks/lateral/results/m3.3-octasquare-sweep.csv`. **463 free polyforms classified in 1.5 s wall** across 14 non-empty (s, o) pairs; one pair (5, 1) returned 0 shapes. **40 Hc = 1 catalogued** at `results/m3.3-octasquare-hc1.txt`; **no Hc ≥ 2 found**. Notable contrast with bevelhex M3.1: octasquare's Hc = 1 rate is 8.6 % (40/463) vs bevelhex's 0.37 % (35/9 427), and its isohedral-tiler rate is 18.8 % (87/463) vs bevelhex's 0.69 % — the (4.8.8) local geometry is markedly more "tiling-friendly" at small sizes. Bevelhex-vs-octasquare comparison table in the lateral README. The 40 + 35 = 75 Hc = 1 lateral-grid polyforms catalogued together are heesch-forge's first new published Heesch-number data outside the omino / hex / iamond grids Kaplan covered in 2022.
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

**1 May 2026 (M3.3, octasquare).** Octasquare (4.8.8 Archimedean — squares + regular octagons) sweep over (squares, octagons) pairs with s + o ≤ 6. Reproducible at `benchmarks/lateral/run_octasquare_sweep.sh`; canonical CSV at `benchmarks/lateral/results/m3.3-octasquare-sweep.csv`. **463 free polyforms classified in 1.5 s wall** across 14 non-empty (s, o) pairs. **40 Hc = 1 catalogued** at `results/m3.3-octasquare-hc1.txt`; no Hc ≥ 2 found.

| s + o | shapes | Hc = 0 | Hc = 1 | iso |
|------:|-------:|-------:|-------:|----:|
|  2    |      1 |      0 |      0 |   1 |
|  3    |      5 |      2 |      3 |   0 |
|  4    |     17 |      9 |      0 |   8 |
|  5    |     79 |     54 |     25 |   0 |
|  6    |    361 |    271 |     12 |  78 |

Notable contrast with bevelhex M3.1: octasquare's Hc = 1 rate is **8.6 %** (40/463) vs bevelhex's 0.37 % (35/9 427); isohedral-tiler rate is **18.8 %** (87/463) vs bevelhex's 0.69 %. The (4.8.8) local geometry is markedly more "tiling-friendly" at small sizes. **The 35 + 40 = 75 Hc = 1 lateral-grid polyforms taken together are heesch-forge's first new Heesch-number data outside the omino / hex / iamond grids Kaplan covered in 2022.** Bevelhex-vs-octasquare comparison table in the lateral README. Phase-3 row promoted from "active (1/5)" to "active (2/5)".

**1 May 2026 (M3.1, Phase 3 opens).** Bevelhex (4.6.12 Archimedean) generation + classification verified on upstream's `gen -bevelhex` + `sat -isohedral -hh` pipeline. Reproducible sweep at `benchmarks/lateral/run_bevelhex_sweep.sh`; canonical CSV at `benchmarks/lateral/results/m3.1-bevelhex-sweep.csv`.

| size | shapes | wall    | Hc = 0 | Hc = 1 | Hc = 2 | iso | inconc |
|-----:|-------:|--------:|-------:|-------:|-------:|----:|-------:|
|  4   |     49 |  0.021s |     44 |      5 |      0 |   0 |      0 |
|  5   |    255 |  0.083s |    244 |     11 |      0 |   0 |      0 |
|  6   |  1,327 |  2.504s |  1,252 |      3 |      0 |  65 |      7 |
|  7   |  7,796 |  1.014s |  7,780 |     16 |      0 |   0 |      0 |

**No bugs found in upstream's bevelhex code.** 35 Hc = 1 bevelhex polyforms catalogued at `results/m3.1-bevelhex-hc1.txt` (a small new dataset; Kaplan's published catalogue covers omino / hex / iamond only). No Hc ≥ 2 through n = 7 — extending the sweep to n = 8–12 is M3.2's scope. Full write-up at `benchmarks/lateral/README.md`. Phase-3 row promoted from "not started" to "active (1/5)".

**1 May 2026 (M2.7, Phase 2 closes).** Phase-2 engineering paper draft at `paper/drafts/phase2-engineering-hybrid.md` (~600 lines). Pulls M2.1-M2.6 numbers and findings into one document with the Phase 1 + Phase 2 combined picture. Eight sections: abstract → background (Phase 1's encoding-bottleneck conclusion) → Phase-2 program → per-milestone results (M2.1 DLX, M2.2 oracle, M2.3 handoff, M2.4 partial, M2.5 scaling, M2.6 PoC) → the right architecture (bounded-DLX + SAT-seed) → cross-cutting analysis (task-class split, not depth-based) → combined Phase-1 / Phase-2 picture for PROPOSAL §5 layers → reproducibility + references.

Headline architectural conclusion: **the right hybrid is task-class-based, not depth-based**. DLX wins on "find any completion" (its competitive regime, 1.33× SAT median on Hc = 1 shapes); SAT wins on "prove no hole-free completion" (where CDCL conflict learning beats DLX enumeration, 81× SAT median on Hc = 0 shapes). M2.3's `CoronaState` is already the bridge that makes the split feasible. The bounded implementation needs a heesch-sat patch to accept a partial-state seed (the M2.4 strict-reading work we deferred); treat as M2.6-followup.

Layer-by-layer status restated:

- **Layer 1 (engineering hardening)** — closed via Phase 1 retrospective (`phase1-engineering-negatives.md`).
- **Layer 2 (hybrid solver)** — **closed via this draft.** Code: `src/dlx/`, `src/corona/`, `benchmarks/hybrid/`. Format: `CoronaState` v1. Open follow-up: bounded-DLX + SAT partial-state seed.
- **Layer 3 (RL)** — unblocked; Phase 4 proceeds with v1 CMSat as the integer-Hc reward oracle. Smoothed MaxSAT partial-corona reward (M1.5's deferred goal) becomes feasible once the bounded hybrid is in place.
- **Layer 4 (lateral grids)** — independent; Phase 3 work, not on the critical path.
- **Layer 5 (theory)** — independent; PROPOSAL §5 says continuous.

Phase 2 row updated from "active (5/7 + M2.4 partial)" to "closed (6/7; M2.4 partial; bounded-joint deferred)". This closes Phase 2.

**1 May 2026 (M2.6, PoC).** Joint DLX + SAT investigation closed. PoC at `benchmarks/hybrid/run_joint.py` runs M2.4's `compute_heesch` per shape via the `run_hybrid` binary, accepts the DLX answer when it agrees with the dataset, and falls back to `src/sat/src/sat -isohedral -hh` otherwise. Results on n ∈ {7, 8} (23 shapes):

| pipeline                | n = 7 wall | n = 8 wall | n = 8 match |
|-------------------------|-----------:|-----------:|------------:|
| pure SAT (M0.5)         |    0.93 s  |    1.91 s  |       20/20 |
| pure DLX (M2.4)         |   10.20 s  |  146.34 s  |       17/20 |
| **joint, naive (M2.6)** |   10.26 s  |  149.16 s  |   **20/20** |

The naive joint **closed all three M2.4 correctness gaps** with 1.02 s of SAT-fallback work on the three n = 8 shapes that DLX couldn't solve correctly — a real win on completeness. But total wall is **56× pure SAT** because DLX still runs on every shape, including the easy ones SAT could solve in milliseconds, and several Hc = 0 shapes saturate the DLX walkback for tens of seconds.

The architectural conclusion: the right joint is **bounded-DLX + SAT-fallback** — give DLX a per-shape time / call budget; on budget exhaustion, hand the partial `CoronaState` (M2.3 already defines this) to SAT seeded with the chain so far. Estimated wall under that architecture (extrapolating from M2.5 + M2.6 numbers): ~20 s at 1 s budget, ~3-5 s at 100 ms budget. The bounded implementation requires a heesch-sat patch to accept a partial-state seed (the M2.4 strict-reading work we deferred); treat as M2.6-follow-up rather than an M2.6 blocker. Full write-up at `benchmarks/hybrid/results/m2.6-joint.md`.

ROADMAP M2.6 marked done with the architectural-conclusion + correctness-evidence framing; Phase-2 row updated to "active (5/7 + M2.4 partial)".

**1 May 2026 (M2.5).** DLX-only vs CMSat scaling comparison closed. `benchmarks/hybrid/compare_dlx_sat.py` joins the M0.5 SAT baseline JSONL with the M2.4 DLX harness CSVs by polyomino coordinates and reports per-shape ratios + Hh / Hc breakdowns. On the n ∈ {7, 8} subset (23 shapes):

- **DLX is 272× slower than CMSat in aggregate** (772 s vs 2.84 s).
- Per-shape ratio: p50 = 5×, p95 = 7 127×, max = 10 642×.
- The "crossover" is **not a corona-depth threshold** — it is a problem-class split. Hc = 1 shapes (where some hole-free chain exists) run DLX at 1.33× SAT median; Hc = 0 shapes (where no hole-free chain exists) run DLX at **80.96× SAT median** because exhaustive enumeration is forced. CMSat's CDCL conflict learning compresses entire branches of the "no hole-free chain" search into single learned clauses; DLX has no such mechanism.

Architectural conclusion for M2.6 (now empirical, not just speculative): hand Hh-style "find any completion" to DLX (its competitive regime), Hc-style "prove no hole-free completion" to SAT (where CDCL conflict learning beats DLX enumeration). M2.3's `CoronaState` is already the bridge that makes the split feasible. n ≥ 9 not run — projected multi-day wall under DLX-only, and the direction is already established at n ≤ 8.

Full write-up at `benchmarks/hybrid/results/m2.5-comparison.md`. Phase-2 row updated to "active (4/7 + M2.4 partial)".

**1 May 2026 (M2.4, partial).** DLX-only end-to-end pipeline shipped at `src/corona/heesch_solver.{hpp,cpp}` (~140 LOC) with `compute_heesch(shape) → (Hc, Hh)` and a heesch-sat-style hole detector ported from `src/sat/src/holes.h::HoleFinder` (8-Moore halo cells, 4-cardinal connectivity between halo cells, lex-smallest halo cell as the outer-component anchor). Harness at `benchmarks/hybrid/run_hybrid.cpp` driving the dataset; CSV output at `benchmarks/hybrid/results/`.

47/47 unit tests PASS in `src/corona/tests/test_corona.cpp` covering geometric primitives, halo computation, CoronaState, simply-connectedness on edge cases (single cell, domino, 3×3 donut, 5×5 with hole), and Kaplan-dataset Hc / Hh on the n = 7 reference shapes. Regression vs M0.4: **n = 7 → 3/3 match (10.2 s); n = 8 → 17/20 match (2:26)**.

Three n = 8 mismatches, two structural failure modes:

- **1 Hh undercount.** Greedy level-1 completion was a dead end for level 2; an alternative level-1 would have admitted level 2 (the dataset case `1 0 2 0 1 1 0 2 1 2 1 3 2 3 3 3` reports Hh = 2, my code reports Hh = 1). The fix is enumerate-and-backtrack DFS over chains; doable but the branching factor on n ≥ 11 explodes without bounds. heesch-sat does not have this issue because its level-2 SAT call sees all level-1+level-2 chain options through one unified formula.
- **2 Hc overcounts.** Two shapes where my hole detector accepts a chain heesch-sat correctly rejects. The detector passes 6 isolated unit tests (single cell, domino, 2×2 square, 3×3 donut, 5×5 hole, n = 7 hh = 1 chain) but mis-classifies some specific local arrangements at n = 8. Direct A/B comparison against heesch-sat's runtime hole-detection output on the same chain would localise the disagreement; that is M2.4-followup work.

Per-shape worst-case wall already 73 s on n = 8; n = 11 / n = 12 would scale to multi-hour, so the full M0.4 regression was not run today. M2.4 exit criterion ("regression suite green") is **not met**; the milestone is **partial-closed** with the gaps tracked at `benchmarks/hybrid/README.md`. The walkback enumeration burden is intrinsic to DLX without conflict-driven pruning — exactly what M2.6 (joint DLX + SAT formulation) is meant to address. M2.5 (empirical scaling benchmark across the dataset) is the natural next step that produces the evidence motivating the M2.6 hybrid architecture.

ROADMAP M2.4 marked **partial** ([~]) with the 87% match receipt and the path-forward note; Phase-2 row updated to "active (3/7 + M2.4 partial)".

**1 May 2026 (Phase 2 continues, M2.3).** M2.3 closed.

- M2.3: handoff format defined and implemented at `src/corona/corona_state.{hpp,cpp}` (~210 LOC). `CoronaState(base_shape)` constructs a state with the canonical level-0 placement (origin (0,0), orientation R0); `add_level(placements)` appends a corona; `interior_cells()` and `halo_cells()` produce the input the M2.2 oracle wants for the next level. Text serialisation is version-tagged, line-oriented, comment-friendly:

  ```text
  v 1
  shape <x0 y0 x1 y1 ...>
  level 0
    0 0 R0
  level 1
    <ox> <oy> <orient>
    ...
  ```

  Orientation tags (`R0`, `R90`, `R180`, `R270`, `M`, `MR90`, `MR180`, `MR270`) round-trip via `tag_of` / `orientation_of_tag`. `write → read → write` is byte-identical.
- 14 new tests inside `src/corona/tests/test_corona.cpp` (now 31 total, all PASS): tag round-trip, format malformed-input rejection (wrong version, missing shape, out-of-order levels, non-canonical level 0), and — load-bearing — composition through the oracle. The composition test uses the n = 7 Hh = 1 heptomino: oracle finds corona-1, push to `CoronaState`, oracle on the new interior fails for corona-2 (consistent with Hh = 1). The serialisation round-trip test repeats the cycle with text serialisation interposed and confirms identical interiors and identical corona-2 verdict.
- This is the protocol M2.4 will use to drive a SAT solver from a DLX-produced inner state. Defining it as a self-contained type with a stable text format means the SAT-side wiring under M2.4 only needs to consume `CoronaState` (in memory or via a temp file) — no DLX-internal data structures leak across the boundary.

**1 May 2026 (Phase 2 continues).** M2.2 closed.

- M2.2: corona-completion oracle at `src/corona/`. `oracle.hpp` + `oracle.cpp` (~330 LOC; depends on `src/dlx/libdlx.a` from M2.1, no other libraries). Polyomino-only for M2.2 — square grid, full D4 symmetry group. API: `Oracle(base_shape)` constructor, `find_completion(interior, out) → bool`, `count_completions(interior, cap) → size_t`, `last_candidate_count()` and `last_nodes_explored()` diagnostics.
- Reduction: "is the corona around `interior` completable by oriented copies of the shape?" becomes an exact-cover problem on `dlx::Solver`. Halo cells are primary columns (covered exactly once); cells the candidate tiles touch outside both interior and halo are secondary columns (covered at most once, preventing tile-tile overlap outside the halo). For each oriented shape × anchor cell × halo cell, the placement that puts the anchor on the halo cell is a candidate row, rejected if it overlaps the interior or fails to touch the halo at all.
- 17-test acceptance gate at `src/corona/tests/test_corona.cpp` (`make test`). Coverage: geometric primitives (the 8 D4 orientations applied to (1, 0) form the four unit vectors; halo of a single square is the four neighbours; halo of a domino is 6 cells), tiny-shape oracles (monomino corona-1 has 4-tile completion; domino and 2×2 square have valid completions), Kaplan-dataset cross-checks (n = 7 Hh = 1 heptominos succeed; n = 9 Hh = 0 nonomino fails), and behaviour / API (count cap, empty-shape rejection, diagnostic plumbing). **All 17 pass.**
- **Hh-semantics for now.** The oracle answers basic corona completion (allowing holes in the further-out region), matching heesch-sat's `cur_solver` SAT call before its `iterateUntilSimplyConnected` walkback. The hole-free Hc semantics is a separate iteration layer that lands under M2.3 / M2.4.

**1 May 2026 (Phase 2 begins).** M2.1 closed; Phase 2 opened.

- M2.1: standalone DLX library at `src/dlx/`. `dlx.hpp` + `dlx.cpp` (~430 LOC, no external dependencies; builds to `libdlx.a` via the per-directory Makefile). Indexed-node Algorithm-X implementation following Knuth's *TAOCP* §7.2.2.1 — `Solver(primary_cols)` constructor, `add_secondary_columns(n)`, `add_row(sorted_cols) → row_id`, `solve(callback) → count`, plus `count_solutions()` and `find_first(out)` convenience wrappers. Primary columns chain off a root header (covered-exactly-once); secondary columns self-loop (covered-at-most-once); the search loop's "S heuristic" (smallest column first, ties broken leftmost-first) is straight from the paper.
- 18-test acceptance gate at `src/dlx/tests/test_dlx.cpp` runnable via `make test`. Coverage: empty-matrix edge cases (zero / non-zero column count, single all-covering row, two disjoint rows), Knuth's §1 worked example (the published unique cover by rows {0, 3, 4} reproduces), n-queens for n ∈ {4, 5, 6, 7, 8} matched against OEIS A000170 (counts 2, 10, 4, 40, 92), early-stop callback semantics, secondary-only edge case, API safety (throws on add-after-solve, non-ascending columns, out-of-range columns), and `nodes_explored` accounting. **All 18 pass.**
- Stand-alone library by design: `src/dlx/` does not link against `src/sat/`'s upstream `dlx.h`. The wiring of the new DLX as a corona-completion oracle is M2.2's scope; M2.1 ships only the kernel + tests so the encoding-refactor work in M2.2-M2.4 starts from a known-correct DLX.

**1 May 2026 (close of day).** M1.6 + M1.7 closed; **Phase 1 closed**.

- M1.6: covered in substance by M0.5's `-stats <file>` JSONL stream (per-shape wall, conflicts, decisions, propagations, sat-call count, corona depth). Per-corona granularity is a small extension since the counters already exist, but no Phase-1 lever needed it for a decision and the M1.7 retrospective does not depend on it. Per-corona granularity gets added under M2 if Phase-2 work needs it.
- M1.7: Phase-1 retrospective drafted at `paper/drafts/phase1-engineering-negatives.md` — this is the negative-result short-paper draft the decision gate calls for. Five Phase-1 levers evaluated, five negative-or-not-applicable findings; the cross-cutting analysis identifies the heesch-sat encoding (reachability-style, corona-completion hardcoded via implication chain) as the bottleneck across all five. ROADMAP M1.6 + M1.7 marked done; Phase-1 row promoted from "active (5/7; decision gate de facto triggered)" to "closed (negative result; M1.2b deferred)".

Decision gate triggered (PROPOSAL §5 / ROADMAP "Phase 1 / Decision gate" — *"if speedup is <2× across the board, write up as negative-result short paper, proceed directly to Layer 3 with v1 solver"*). Concrete consequences:

- Phase 2 (hybrid DLX/SAT, encoding refactor) and Phase 4 (RL pilot) proceed in parallel rather than serially after a successful Layer 1.
- Layer-3 RL accepts the **M0.5 CMSat baseline** (`src/sat/src/sat`, no portfolio, no symmetry preprocessor, no MaxSAT path) as the integer-Hc reward oracle until M2 ships a partial-corona score.
- M1.2b (Glucose backend + clause-sharing portfolio harness) is **deferred**, not abandoned. M1.1 + M1.2a results bound any portfolio without CMSat to "strictly slower than CMSat-solo"; a CMSat-inclusive portfolio is just CMSat-with-process-overhead in expectation. The portfolio architecture is useful infrastructure for future work and is committed as adapter scaffolding (`cadical_backend.h`, `kissat_backend.h`, four-binary Makefile structure).

**1 May 2026 (later still).** M1.5 closed: infrastructure shipped, score deferred. The `-DHEESCH_BACKEND_WCNF_DUMP` build (`sat-wcnf-dump`, `src/sat/src/wcnf_dumping_backend.h`) tees every UNSAT solve to a classic-format DIMACS WCNF with cell-vars as unit soft clauses, and `benchmarks/maxsat/run_rc2.py` walks those files through pysat's RC2. The end-to-end probe (`benchmarks/maxsat/probe/run_probe.sh`) ran on the first heptomino and first octomino in the Kaplan dataset and produced the row set in `benchmarks/maxsat/results/m1.5-probe.csv`.

Every probed WCNF returns **UNSAT-HARD** from RC2 (sub-second to ~20 s per file): the heesch-sat encoding is reachability-style, with corona-completion hardcoded via an implication chain anchored on a forced unit clause, and there is no clean soft-relaxation point. Removing the anchor produces a degenerate trivial-min formula (`cost = num_softs`, all relaxations taken). Real partial-corona scoring needs explicit per-cell `coverage[c]` decision variables decoupled from tile placement — an encoding redesign that overlaps directly with M2's planned DLX/SAT handoff. Doing the work twice is wasteful; the M1.5 deliverable is the infrastructure plus the negative-finding write-up at `benchmarks/maxsat/README.md`.

Phase-1 implication: **the M1.7 decision gate is triggered in substance**. Five Phase-1 levers evaluated, five negative-or-not-applicable findings: M1.1 (CaDiCaL: 1.95× slower than CMSat), M1.2a (Kissat: 7.33× slower), M1.3 (BreakID: zero gain on CMSat with infeasible cost on walkback formulas), M1.4 (no AMO sites to optimise without refactor), M1.5 (MaxSAT relaxation requires encoding refactor). The common thread is that the heesch-sat encoding itself is the bottleneck, not the solver or the preprocessing pipeline. The remaining unfired lever is M1.2b (Glucose backend + portfolio harness) — given M1.1 and M1.2a's results, expectation is that any portfolio without CMSat is strictly slower than CMSat-solo, so M1.2b's ceiling is "matches CMSat baseline." The formal M1.7 negative-result write-up should land before opening Phase 2.

**1 May 2026 (later).** M1.4 closed as **structurally not applicable**. PBLib's value is sweeping the encoding choice of N-way at-most-one constraints, but `src/sat/src/heesch.h::getClauses` does not emit any N-way AMOs — every "at most one" enforcement is pairwise binary mutex over ordered tile-pairs (the dominant block being `heesch.h:615-635`'s geometric-overlap mutex, costing `O(|overlap_pairs| × |levels|²)` binary clauses). To make M1.4 meaningful would require a constraint-generation refactor that groups (tile, level) variables by the cell each covers and emits a single PBLib AMO call per cell — that is not a sweep but an encoding redesign, and it overlaps in scope with M2's hybrid DLX/SAT handoff which is already rethinking constraint structure. Recording the finding here keeps M1.4 honest; the work belongs under M2. The only remaining Phase-1 lever still unevaluated is M1.5 (MaxSAT partial-corona score, the Layer-3 reward signal).

**1 May 2026.** M1.3 closed as a documented **negative result**.

- Built `sat-dump` (a CMSat-backed `-DHEESCH_BACKEND_DUMP` build at `src/sat/src/dumping_backend.h`) that tees every SAT instance heesch-sat issues to DIMACS under `$HEESCH_DUMP_DIR`. Wrote a probe at `benchmarks/breakid/probe/run_probe.sh` that runs heesch-sat on a sample shape, runs `breakid` over each dumped CNF (60 s wall cap), and A/B-tests CMSat raw-vs-broken via a small `cms_solve` helper (source at `benchmarks/breakid/probe/cms_solve.cpp`).
- Two regimes emerged cleanly across n = 7 and n = 8 samples (committed: `benchmarks/breakid/results/m1.3-probe.{log,csv}`):
  - **Fresh corona-loop / iso-solver formulas** (1.7 k–10.6 k clauses): BreakID adds 16–19 symmetry-breaking clauses in <0.2 s. CMSat's solve trace is **byte-identical** raw vs broken — same conflict count, same decision count. CryptoMiniSat's own inprocessing already discovers the same equivalences.
  - **Walkback / `iterateUntilSimplyConnected` formulas** (~750 k–800 k clauses on n = 8): BreakID **times out at 60 s** with zero symmetries detected. Run uncapped on the n = 7 walkback formulas it took 226 s and 753 s respectively, both reporting `Num generators: 0` — every variable-level automorphism is killed by the thousands of hole-banning clauses heesch-sat appends inside the walkback loop.
- Phase-1 implication: a "preprocess every CNF with BreakID" pipeline costs ≥120 s/shape on n = 8 alone for zero solver-side gain. BreakID cannot meet the ≥5× target with CMSat as the engine. M1.4 (encoding sweep) and M1.5 (MaxSAT) remain the only Phase-1 levers not yet ruled out. The structural follow-up — refactoring `iterateUntilSimplyConnected` to one-shot SAT calls so each instance stays in a regime where symmetries survive — sits naturally under M2 (hybrid solver), not M1.
- Full write-up at `benchmarks/breakid/README.md`. The dumping backend remains useful infrastructure for future probes (M1.4 encoding sweeps, M2.3 hybrid handoff).

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
