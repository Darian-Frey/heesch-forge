# heesch-forge

## A Research Proposal on the Polyomino Heesch Problem

**Repository.** [github.com/Darian-Frey/heesch-forge](https://github.com/Darian-Frey/heesch-forge)

**Status.** v0.1 — initial proposal.
**Author.** Shane Hartley (independent researcher).
**Date.** 29 April 2026.

---

## 0. Executive Summary

The Heesch number of a non-tiling shape is the maximum number of complete coronas of congruent copies of itself that can surround it. The Heesch problem asks which positive integers can occur as Heesch numbers; despite sixty years of attention, this is open. The current record, by Bašić (2020), is H=6, achieved by a hand-constructed non-polyform tile. For polyominoes specifically — the natural combinatorial setting — the highest known Heesch number is H=3, found by Kaplan (2022) at n=17 via exhaustive SAT-based enumeration up to n=19.

The field has been worked by remarkably few hands and remarkably few techniques: backtracking, fixed-order surround heuristics, SAT, and a small number of clever hand constructions. Modern combinatorial-search machinery — Algorithm X / Dancing Links, MaxSAT, portfolio SAT, Answer Set Programming, GNN-guided search, reinforcement learning over the shape space — has not been brought to bear on the polyomino Heesch problem. The principal author of the current state of the art (Kaplan) has explicitly stated that further progress requires performance improvements he has not made.

heesch-forge proposes a five-layer attack on the polyomino Heesch problem, each layer self-contained and independently publishable. The layers compose: engineering hardening of existing tooling enables a hybrid solver, which in turn enables an RL search over shape space, which provides experimental signal for a new theoretical-obstruction line of work. The lateral move into Archimedean-grid polyforms (which have never been searched) is a parallel low-cost track.

---

## 1. Problem Statement

### 1.1 Definitions

Let *P* be a closed topological disk in the Euclidean plane.

- The **0-th corona** *C₀* of *P* is *P* itself.
- The **k-th corona** *C_k* is the set of all congruent copies of *P*, distinct from *P*, that share at least one boundary point with *C_{k-1}* and have pairwise disjoint interiors with *C_{k-1}* and with each other.
- The **Heesch number** *H(P)* is the supremum of *k* such that *C_k* exists.
- *P* is a **tiler** if and only if *H(P) = ∞* (König's lemma).
- Two refinements are standard: *H_c(P)* requires each corona to be simply connected (no holes); *H_h(P)* permits holes in the outermost corona.

The **Heesch problem** asks: *What is the set {H(P) : P a closed topological disk, H(P) < ∞}?* Equivalently, is every positive integer attained?

### 1.2 The polyomino specialisation

A **polyomino** is a closed connected union of unit squares from the integer grid Z². The polyomino Heesch problem restricts the question to polyomino prototiles. The grid constraint reduces a continuous problem to a combinatorial one and admits exhaustive enumeration: there are roughly *Klarner_constant^n ≈ 4.06^n* fixed n-ominoes.

Polyominoes are not just a tractable special case. They lie in a particular regime: the prototile has dihedral symmetry compatible with Z², the boundary is rectilinear, and every corona placement reduces to a discrete cell-covering problem. This is *more* constrained than general topological disks but *less* constrained than edge-marked polyforms. The polyomino regime currently sits at H=3, which is a striking gap from the all-shape record of H=6 and from Mann's edge-marked polyform record of H=5. Whether this gap reflects a genuine geometric obstruction or only the limit of current search is the central open question heesch-forge addresses.

---

## 2. State of the Art

### 2.1 Historical record

| Year | Author | Result | Method |
|------|--------|--------|--------|
| 1928 | Lietzmann | First H=1 example | Hand |
| 1968 | Heesch | Problem posed; H=1 example | Hand |
| 1991 | Fontaine | Infinitely many polyominoes with H=2 | Construction |
| ~1990s | Ammann | H=3 (or H=4 by Day's reckoning) | Hand; projection-vs-indentation argument |
| 2001 | Mann (PhD) | H=5 via parametric "5-hexapillar" family | Systematic family analysis |
| 2016 | Mann & Thomas | Marked polyforms up to H=5 | Brute-force fixed-order surround |
| 2020 | Bašić | H=6 (general tile, not polyform) | Hand construction |
| 2022 | Kaplan | Polyominoes up to H=3 (n≤19); polyhexes/iamonds up to H=4 | SAT solver, exhaustive |

### 2.2 Algorithmic state

The current standard tool is Kaplan's `heesch-sat` (C++, public repository `github.com/isohedral/heesch-sat`). It encodes corona-completion as a Boolean satisfiability instance and dispatches to CryptoMiniSat. Per-shape cost is approximately 0.15 seconds average, capped near a minute. Joseph Myers' polyform-tiling tester acts as a sub-millisecond pre-filter to discard tilers.

Kaplan extended his code in 2023–2024 to handle additional grids — polyhexes, polyiamonds, octasquare, and the (4.6.12) Archimedean "bevelhex" grid — though Heesch numbers for the latter two have not been published.

### 2.3 Theoretical state

The only published upper-bound proof technique is Ammann's projection-vs-indentation count, generalised by Mann to derive Heesch numbers of his hexapillar family. This is essentially a Z₂ invariant on boundary edge labels.

Two unboundedness results exist in adjacent settings:

- **Bašić (2015)**: The Heesch number is unbounded for finite protosets of multiple prototiles in the Euclidean plane.
- **Bašić & Slivková (2020)**: The Heesch number is asymptotically unbounded in *E^d* as *d → ∞*.

For monotiles in *E²* the boundedness question is wide open.

---

## 3. Gap Analysis

We organise the unexplored territory into three categories.

### 3.1 Algorithmic gaps

1. **Algorithm X / Dancing Links** has not been applied to corona enumeration. This is anomalous: DLX is the canonical exact-cover algorithm and corona-completion is an exact-cover problem (each boundary cell covered exactly once by one placed copy). Knuth's DLX has decades of polish, beats SAT on certain tile-placement formulations, and is complementary to SAT (fast on small instances, no learning).
2. **MaxSAT** has not been used. Vanilla SAT yields a yes/no signal per corona depth; MaxSAT yields a *largest partial corona* score, which is a real-valued reward suitable for downstream learning.
3. **Portfolio SAT** (Kissat + CaDiCaL + Glucose with clause sharing via Mallob) is competition-standard, and absent from `heesch-sat`.
4. **Symmetry breaking** via BreakID is not applied to the encoding; Heesch instances have rich automorphism structure.
5. **Cardinality-encoding choice** has not been swept; PBLib offers many at-most-one variants giving 2–10× swings on similar formulae.
6. **Answer Set Programming** (clingo, DLV) routinely outperforms SAT on combinatorial-design problems with rich symmetry. No ASP formulation exists.
7. **GPU-accelerated SAT** (ParaFROST, etc.) is unattempted. Heesch is a per-shape problem, and a GPU pipeline could process millions of shapes in parallel.

### 3.2 Search-strategy gaps

8. **Reinforcement learning over the shape space.** Wagner (2021) introduced the deep cross-entropy method for combinatorial counterexample search; Truter (2024) used deep RL to discover thousands of previously unknown Fano hypersurfaces inaccessible to existing search. The recipe transfers directly: state = partial polyomino, action = add square, reward = computed Heesch number. This bypasses exhaustive enumeration entirely and searches over the right object — *shapes likely to have high H* — rather than the wrong one — *all shapes of size n*.
9. **GNN-guided MCTS over corona placements.** TilinGNN (Liu et al. 2020) showed graph neural networks can learn tiling decisions with runtime linear in candidate placements. A GNN trained to predict corona depth could serve as a heuristic in tree search inside a single shape's evaluation.
10. **Hybrid DLX / SAT layering.** Inner coronas (1, 2) have small search spaces where DLX wins; outer coronas (3+) benefit from CDCL conflict learning. A hybrid solver with the boundary as the handoff has not been built.

### 3.3 Theoretical / mathematical gaps

11. **Higher invariants beyond Ammann's parity.** The corona problem has substantial algebraic structure: the boundary edge sequence is a word in the free group of grid moves, the placements form a groupoid action, the corona-completion question can be stated as an equivariant lifting problem. Discrete fundamental groups of the boundary edge graph, character theory of the boundary symmetry, and Čech-style local-to-global obstructions of the corona are unexplored. Ammann's projection-count is the only published obstruction; it is almost certainly not the strongest.
12. **Substitution-rule machinery imported from the einstein proof.** The 2024 hat aperiodicity proof (Smith–Myers–Kaplan–Goodman-Strauss) uses metatiles and substitution systems. A *decreasing* substitution structure that fails at exactly the k-th iteration would simultaneously prove H=k and provide a constructive recipe.
13. **Monotile encoding of Bašić's multi-prototile mechanism.** Bašić's 2015 unboundedness proof for finite protosets uses some mechanism to defer obstructions across coronas. Whether this mechanism admits a single-tile encoding — possibly only on richer grids such as bevelhex — is not established.
14. **Bevelhex / octasquare polyforms** have never been searched for Heesch numbers despite the code existing. The (4.6.12) Archimedean grid has three-cell-type boundary geometry that locally resembles a multi-prototile setup.

---

## 4. Research Questions

- **RQ1 (Existence).** Does there exist a polyomino *P* with *H_c(P) ≥ 4*?
- **RQ2 (Algorithm).** Can the per-shape cost of polyomino Heesch-number computation be reduced by an order of magnitude over `heesch-sat` v1?
- **RQ3 (Search).** Can a learned generator find polyominoes with high *H_c* without exhaustive enumeration, achieving Kaplan's H=3 catalogue at *n=17* with substantially less compute?
- **RQ4 (Theory).** Is there a discrete-topological or representation-theoretic invariant of a polyomino that yields an upper bound on *H_c* strictly stronger than Ammann's projection-vs-indentation count?
- **RQ5 (Lateral).** What are the Heesch numbers of small polyforms on the bevelhex and octasquare grids?

---

## 5. Methodology

A five-layer plan. Each layer is independently meaningful and produces its own deliverables; later layers depend on earlier ones for tooling but not for completion.

### Layer 1 — Engineering hardening (tooling)

**Goal.** A `heesch-sat` v2 that is faster, more general, and instrumented.

**Tasks.**
- Fork `heesch-sat`. Establish a regression test suite over Kaplan's published H≥1 polyomino dataset; lock current performance.
- Replace single-solver SAT call with a portfolio (CaDiCaL + Kissat + Glucose) using a Mallob-style clause-sharing wrapper.
- Integrate BreakID symmetry breaking; benchmark with and without across the dataset.
- Sweep PBLib at-most-one and at-most-k encoding choices; record per-corona-depth optima.
- Replace SAT with MaxSAT (RC2 / EvalMaxSAT) to expose a partial-corona score.
- Add structured logging: per-shape per-corona solve time, conflict counts, decision counts.

**Expected gain.** 5–20× per-shape speedup on hard instances. This brings n=20 polyomino enumeration into reach on a single workstation.

**Deliverable.** `src/sat/` solver; benchmark report.

### Layer 2 — Hybrid solver (algorithm)

**Goal.** A solver that uses the right tool at the right corona depth.

**Tasks.**
- Implement Algorithm X / Dancing Links for inner-corona placement (depths 1, 2). Reuse Knuth's standard DLX implementation.
- Define a clean handoff: when DLX produces a complete (k-1)-corona, hand the resulting boundary configuration to the SAT solver for the k-th corona.
- Calibrate the crossover depth empirically.
- Investigate joint DLX+SAT formulations where DLX handles cell-covering and SAT handles non-overlap globally.

**Expected gain.** Order-of-magnitude on inner coronas. Independently publishable as a solver paper.

**Deliverable.** Hybrid-solver paper; integrated tool.

### Layer 3 — RL search over the shape space

**Goal.** Find high-H polyominoes without exhaustive enumeration.

**Tasks.**
- Cast polyomino construction as a sequential decision problem: state = current polyomino, action = "add a square at boundary position p" or "stop".
- Use the Layer-1+2 solver as the reward oracle; reward = *H_c* (or MaxSAT-derived score).
- Implement Wagner-style deep cross-entropy with a small policy network (a GNN over the polyomino's adjacency structure is the natural choice).
- Validate by rediscovering Kaplan's H=3 polyominoes at n=17 with substantially less compute than exhaustive enumeration.
- Scale to n=20–25 where exhaustion is infeasible.

**Expected gain.** This is the most likely route to RQ1. If H≥4 polyominoes exist at modest n, RL will find one before brute force can.

**Deliverable.** RL-discovery paper; trained models; any new high-H shapes found.

### Layer 4 — Archimedean-grid exploration (lateral)

**Goal.** Search the bevelhex and octasquare grids that Kaplan added but never published Heesch results for.

**Tasks.**
- Run Layer-1+2 tooling on bevelhex polyforms up to feasible n.
- Run on octasquare polyforms.
- Tabulate analogues of Kaplan's polyhex/polyiamond H=4 catalogue.

**Expected gain.** Cheap potential record. Bevelhex's three-cell-type geometry is the most promising untouched venue.

**Deliverable.** Short empirical paper analogous to Kaplan (2022) for the new grids.

### Layer 5 — Theoretical obstruction theory

**Goal.** Develop upper-bound proof techniques stronger than Ammann's parity argument.

**Tasks.**
- Formalise corona-completion as an equivariant lifting problem in the free groupoid generated by Z²-translations and dihedral grid symmetries.
- Investigate the boundary edge-word as an element of the corresponding group; identify cohomological obstructions to extension.
- Connect to the substitution-rule machinery from the hat einstein proof: build candidate decreasing substitutions whose failure at step k bounds H ≤ k.
- Test obstructions against known H-values from Layers 1–4; publish either positive bounds or the failure modes that suggest H is unbounded for polyominoes.

**Expected gain.** High-risk, high-reward. Resolution of the polyomino Heesch problem in either direction would be a major result.

**Deliverable.** Theory paper.

---

## 6. Risks and Mitigations

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Layer 1 yields only modest speedup (<3×) | Low | Low | Layer 2/3 still proceed; engineering report still publishable |
| Layer 2 hybrid is no faster than pure SAT | Medium | Low | Negative result still publishable; clarifies the SAT-vs-DLX boundary |
| Layer 3 RL fails to recover known H=3 shapes | Medium | Medium | Pivot to MCTS-with-priors or imitation learning from Kaplan's dataset |
| H≥4 polyominoes simply do not exist | Medium | Low (changes outcome, not validity) | The non-existence becomes the result; theoretical line in Layer 5 strengthens |
| Layer 5 produces no new obstruction | High | Low | Engineering and empirical papers stand alone |
| Compute insufficient on ThinkPad P15 Gen 2i for Layer 3 scaling | High | Medium | Lambda Labs / Vast.ai for training runs; CPU-only inference for evaluation |
| Kaplan or another group publishes overlapping work | Low | Medium | Multi-track design means partial scoop on one layer leaves others intact |

---

## 7. Expected Outputs

1. **`src/sat/` and `src/dlx/`** — open-source modernised solver with portfolio SAT, MaxSAT, BreakID, hybrid DLX path, and instrumentation. (Layer 1+2.)
2. **Engineering paper** — solver-level improvements, benchmarks. (Layer 1+2.)
3. **RL-discovery paper** — methodology and any new high-H polyominoes. (Layer 3.)
4. **Bevelhex/octasquare paper** — first published Heesch numbers for these grids. (Layer 4.)
5. **Theory paper** — new obstructions or formal failure modes. (Layer 5.)
6. **Datasets** — full computed Heesch numbers for n ≤ 22 polyominoes on Hugging Face / Zenodo.
7. **Reproducibility artefacts** — Docker images, seeded experiments, full logs.

---

## 8. Indicative Timeline

Single-researcher cadence; multi-LLM auditing workflow; ThinkPad P15 + occasional cloud compute.

| Phase | Duration | Layers | Key milestone |
|-------|----------|--------|---------------|
| 0 — Setup | 2 weeks | — | Repo scaffolded, regression suite green |
| 1 — Engineering | 4–6 weeks | L1 | v2 solver with ≥5× speedup verified |
| 2 — Hybrid | 6–8 weeks | L2 | DLX/SAT crossover characterised |
| 3 — Lateral | parallel from Phase 2 | L4 | Bevelhex/octasquare results |
| 4 — RL | 8–12 weeks | L3 | Recover H=3 catalogue with <10% of exhaustive compute |
| 5 — Scale | 8–12 weeks | L3 | Push to n=22+ polyominoes |
| 6 — Theory | continuous from Phase 1 | L5 | Submission target ~12 months |

Phases 1, 3, and 6 can run concurrently. Phase 2 gates Phase 4. Phase 5 may yield the H≥4 result (if it exists) at any point.

---

## 9. References

**Primary literature**

- Bašić, B. (2021). *A Figure with Heesch Number 6: Pushing a Two-Decade-Old Boundary.* Mathematical Intelligencer.
- Bašić, B. (2015). *The Heesch number for multiple prototiles is unbounded.* C. R. Math. Acad. Sci. Paris 353, 665–667.
- Bašić, B. & Slivková, A. (2020). *Asymptotical Unboundedness of the Heesch Number in E^d for d → ∞.* Discrete & Computational Geometry.
- Fontaine, A. (1991). *An Infinite Number of Plane Figures with Heesch Number Two.* J. Combin. Theory A 57, 151–156.
- Heesch, H. (1968). *Reguläres Parkettierungsproblem.* Westdeutscher Verlag.
- Kaplan, C. S. (2022). *Heesch Numbers of Unmarked Polyforms.* Contributions to Discrete Mathematics 17(2), 150–171. arXiv:2105.09438.
- Kaplan, C. S. (2024). *Detecting Isohedral Polyforms with a SAT Solver.* GASCom 2024 Abstracts, 118–122. arXiv:2406.16407.
- Mann, C. (2004). *Heesch's Tiling Problem.* American Mathematical Monthly 111(6), 509–517.
- Mann, C. & Thomas, B. C. (2016). *Heesch Numbers of Edge-Marked Polyforms.* Experimental Mathematics 25(3).
- Smith, D., Myers, J. S., Kaplan, C. S., Goodman-Strauss, C. (2024). *An aperiodic monotile.* Combinatorial Theory 4(1), #6. arXiv:2303.10798.

**Methodology references**

- Knuth, D. E. (2000). *Dancing Links.* In *Millennial Perspectives in Computer Science.*
- Liu, H. et al. (2020). *TilinGNN: Learning to Tile with Self-Supervised Graph Neural Network.* ACM TOG 39(4).
- Truter, M. (2024). *Deep Reinforcement Learning for Fano Hypersurfaces.* arXiv preprint.
- Wagner, A. Z. (2021). *Constructions in combinatorics via neural networks.* arXiv:2104.14516.

**Software**

- Kaplan, C. S. *heesch-sat.* `github.com/isohedral/heesch-sat`.
- Myers, J. *Polyform tiling software.* `polyomino.org.uk`.
- Soos, M. *CryptoMiniSat.* Biere, A. *CaDiCaL, Kissat.*
- Devriendt, J. et al. *BreakID.*

---

*End of PROPOSAL.md v0.1.*
