# heesch-forge — Literature

Annotated bibliography. Organised by topic, not chronology. Each entry has a one-paragraph annotation explaining what the work contributes and how it relates to heesch-forge.

---

## Bibliography metadata and verification

- **BibTeX records** for every entry below: [`paper/lit/bibtex.bib`](./paper/lit/bibtex.bib). Citation keys are `<Surname><SectionID>`, e.g. `KaplanA8` for the 2022 polyforms paper.
- **PDF filing** is local-only; PDFs are gitignored. Run [`paper/lit/fetch_pdfs.sh`](./paper/lit/fetch_pdfs.sh) to populate the open-access subset (arXiv + CDM); paywalled items need institutional access. See [`paper/lit/README.md`](./paper/lit/README.md) for the convention.
- **Verification status.** Per CLAUDE.md's "never cite what hasn't been read" rule, each entry below carries an implicit `[VERIFY]` until a maintainer reads it and a future commit promotes it to `[EST]`. The currently-promoted entries are:

| Section | Entry | Why [EST] |
|---------|-------|-----------|
| A.8 | Kaplan 2022, *Heesch Numbers of Unmarked Polyforms* | Hc/Hh values reproduced from this paper's dataset by `benchmarks/kaplan/` (M0.4); reading at section level. |
| F.1 | Kaplan, *heesch-sat* | Vendored at SHA `1adb3720` and built locally (M0.2/M0.3). |
| F.3 | Kaplan, *Heesch dataset* | Downloaded, hashed, parsed, regression-tested (M0.4). |

Entries flagged with a `[VERIFY]` `note:` field in `bibtex.bib` need their identifier or attribution confirmed before they can be cited in any heesch-forge writeup, regardless of reading status. The most suspicious right now: `HyperbolicMonotileE2` (LITERATURE.md gives `arXiv:2603.27827`, which encodes a 2026-03 submission stamp — confirm before citing).

---

## A. Heesch problem — primary literature

### A.1 Heesch, H. (1968). *Reguläres Parkettierungsproblem.* Westdeutscher Verlag.

The original monograph that posed the problem. Heesch exhibited a tile with H=1 (the union of a square, an equilateral triangle, and a 30-60-90 triangle) and asked which positive integers can occur as Heesch numbers. The foundational reference for the entire field.

### A.2 Fontaine, A. (1991). *An Infinite Number of Plane Figures with Heesch Number Two.* J. Combin. Theory A 57, 151–156.

First systematic construction giving infinitely many polyominoes with H=2. Important historically as the first parametric family in the Heesch literature; conceptually the precursor to Mann's hexapillars. Worth re-examining for any structural pattern that could generalise to H=3 polyominoes (a source of construction-side ideas for Layer 5).

### A.3 Mann, C. (2004). *Heesch's Tiling Problem.* American Mathematical Monthly 111(6), 509–517.

The most accessible published account of the n-hexapillar and n-polypillar families. Theorem 1: 2- and 3-hexapillars have H=4, 5-hexapillar has H=5. Critical reading because it shows that *parametric family analysis* is the main competitor to exhaustive enumeration for record-setting. Mann's belief, recorded in secondary sources, that "more rounded polyominoes" might do better than the long skinny pillars he used, is a direct prompt for Layer 3 RL.

### A.4 Mann, C. & Thomas, B. C. (2016). *Heesch Numbers of Edge-Marked Polyforms.* Experimental Mathematics 25(3).

The brute-force computer search that pushed the marked-polyform record to H=5. Documents the limitation that motivates SAT: the algorithm tries to surround in a fixed order and frequently times out. Establishes the empirical baseline against which `heesch-sat` measured itself.

### A.5 Bašić, B. (2015). *The Heesch number for multiple prototiles is unbounded.* C. R. Math. Acad. Sci. Paris 353, 665–667.

Critical adjacent result: with a finite *protoset* of multiple prototiles, every positive integer is a Heesch number. Proves unboundedness in the multi-tile setting. The mechanism Bašić uses defers obstructions across coronas. Whether that mechanism admits a single-tile encoding (especially on richer grids like bevelhex) is one of the most interesting open questions and motivates Layer 4.

### A.6 Bašić, B. & Slivková, A. (2020). *Asymptotical Unboundedness of the Heesch Number in E^d for d → ∞.* Discrete & Computational Geometry.

Companion result: in dimension d, Heesch numbers are unbounded as d → ∞. Strengthens the case that the obstruction in E² is dimensional rather than fundamental. Suggests E² polyomino bounds (if any) reflect Z² rigidity specifically.

### A.7 Bašić, B. (2021). *A Figure with Heesch Number 6: Pushing a Two-Decade-Old Boundary.* Mathematical Intelligencer.

The current monotile record-holder. The figure is a hand construction, not a polyform. The detailed construction is in a forthcoming article promised in this note. Points to inspect: which boundary features force the obstruction at corona 7? Can analogues exist in the polyomino regime?

### A.8 Kaplan, C. S. (2022). *Heesch Numbers of Unmarked Polyforms.* Contributions to Discrete Mathematics 17(2), 150–171. arXiv:2105.09438.

The current state of the art for unmarked polyforms. SAT-based exhaustive computation up to 19-ominoes, 17-hexes, 24-iamonds. Highest polyomino H=3 (two 17-ominoes). Highest polyhex/iamond H=4 (multiple). Critical concluding remark: extending further requires significant performance improvements Kaplan has not made. heesch-forge's Layers 1 and 2 directly address this.

### A.9 Kaplan, C. S. (2024). *Detecting Isohedral Polyforms with a SAT Solver.* GASCom 2024 Abstracts. arXiv:2406.16407.

Adds isohedral-tiling detection to the same SAT framework. Important because it shows the SAT approach generalises to richer tiling questions; suggests Heesch-related quantities (e.g., approximate-tiling defect) might admit SAT formulations too.

---

## B. Aperiodic monotile — for technique transfer

### B.1 Smith, D., Myers, J. S., Kaplan, C. S., Goodman-Strauss, C. (2024). *An aperiodic monotile.* Combinatorial Theory 4(1), #6. arXiv:2303.10798.

The hat tile and its aperiodicity proof. Two proof techniques: (a) substitution rules through metatiles, (b) combinatorial computer-assisted argument. Central reference for Layer 5: a *decreasing* substitution structure failing at corona k would give a constructive H=k bound. The combinatorial computer-assisted half also demonstrates that SAT-style tooling extends to substitution verification.

### B.2 Smith, Myers, Kaplan, Goodman-Strauss (2024). *A chiral aperiodic monotile.* Companion paper.

The spectre tile. Same proof apparatus, single chirality. Of secondary relevance to heesch-forge but worth knowing for the mechanism.

---

## C. Combinatorial-search techniques — for Layer 1 / 2

### C.1 Knuth, D. E. (2000). *Dancing Links.* In *Millennial Perspectives in Computer Science.*

The canonical reference for Algorithm X / DLX. Direct application to corona enumeration is the basis of Layer 2.

### C.2 Biere, A. et al. *CaDiCaL, Kissat.* SAT competition documentation.

Modern CDCL SAT solvers — competitive at SAT Competition, both descended from Lingeling. Layer 1 replaces upstream's single CryptoMiniSat backend with a portfolio (CaDiCaL + Kissat + Glucose) running first-to-finish with clause sharing. Reference implementations are in `github.com/arminbiere/`.

### C.3 Devriendt, J., Bogaerts, B., Bruynooghe, M. *BreakID.* Automorphism-based symmetry breaking for CNF.

Mature symmetry-breaking tool. Detects graph automorphisms of the formula and emits breaking clauses. Plug-in compatible with any CDCL solver. Layer 1 milestone M1.3.

### C.4 PBLib documentation. Cardinality-encoding library.

Encodes pseudo-Boolean and at-most-k constraints. Multiple AMO encodings (sequential, commander, product, bimander) with different size/propagation tradeoffs. Layer 1 milestone M1.4.

### C.5 Ignatiev, A. et al. *RC2 MaxSAT solver.* Berg et al. *EvalMaxSAT.*

Modern MaxSAT solvers. Layer 1 milestone M1.5: replace boolean SAT yes/no with MaxSAT score for partial coronas.

### C.6 Schiendorfer, A. et al. *Mallob.* Distributed SAT.

Mallob is a state-of-the-art parallel SAT solver with clause-sharing. The clause-sharing wrapper used here may serve as a model for Layer 1 portfolio integration even on a single machine.

---

## D. Machine learning for combinatorial discovery — for Layer 3

### D.1 Wagner, A. Z. (2021). *Constructions in combinatorics via neural networks.* arXiv:2104.14516.

Direct template for Layer 3. Uses the deep cross-entropy method (a reinforcement learning algorithm) to find counterexamples to several open conjectures in extremal combinatorics. The recipe — define a sequential construction MDP, define a reward, train a small policy network with cross-entropy elite selection — transfers to polyomino Heesch search nearly verbatim.

### D.2 Truter, M. (2024). *Deep Reinforcement Learning for Fano Hypersurfaces.* arXiv preprint.

More recent demonstration of the same paradigm on a genuinely high-dimensional discrete-geometry problem (lattice integer points satisfying complex constraints). Found thousands of new examples, hundreds inaccessible to existing search methods. Strongest empirical evidence available that this method suits high-dimensional discrete-geometry discovery problems.

### D.3 Liu, H. et al. (2020). *TilinGNN: Learning to Tile with Self-Supervised Graph Neural Network.* ACM TOG 39(4).

Graph neural network trained to predict tile-placement decisions in a tiling formulation different from Heesch's, but with directly relevant ideas: tile-overlap graph encoding, learned heuristic over candidate placements. Layer 3 architecture inspiration; could also feed into MCTS-with-priors as a fallback if the cross-entropy approach falters.

### D.4 Chen, X., Tian, Y. (2020). *Learning to Perform Local Rewriting for Combinatorial Optimization.* NeurIPS.

Local-rewriting methods (mutate-and-test) are an alternative to constructive RL. Worth noting as Phase-4 fallback (M4.7 decision gate) — local search over polyomino space with mutation operators (add/remove/move a square) and SAT-derived reward.

### D.5 Selsam, D., Bjørner, N. (2019). *Guiding High-Performance SAT Solvers with Unsat-Core Predictions.* SAT Workshop.

Tangentially relevant: ML-guided SAT itself. If Layer 1+2 SAT calls remain a bottleneck inside the Layer 3 reward loop, this line of work suggests learned variable-selection heuristics could shave per-call cost.

---

## E. Theoretical / topological obstructions — for Layer 5

### E.1 Akhmedov, A. *Cayley graphs with an infinite Heesch number.* arXiv:1412.0358.

Shows that on certain Cayley graphs, finite subsets can have arbitrary finite Heesch numbers. A discrete-graph analogue of the Heesch problem. Potentially the right algebraic setting for Layer 5: corona-completion as an action of a finitely generated group on a graph.

### E.2 *Unboundedness of the Heesch Number for Hyperbolic Convex Monotiles.* arXiv:2603.27827.

Hyperbolic-plane analogue. Different geometry, but the layer-by-layer construction technique and the obstruction language are directly transferable.

### E.3 Goodman-Strauss, C. (2000). *Open Questions in Tiling.* `strauss.hosted.uark.edu/papers/survey.pdf`.

Survey of open questions in tiling theory including connections between Heesch's problem and the undecidability of the tiling domino problem. Frames the broader landscape Layer 5 sits in.

---

## F. Software and datasets

### F.1 Kaplan, C. S. *heesch-sat.* `github.com/isohedral/heesch-sat`.

The codebase Layer 1 vendors and modernises. C++17, CryptoMiniSat backend, BSD-3-Clause licensed.

### F.2 Myers, J. *Polyform tiling software.* `polyomino.org.uk/mathematics/polyform-tiling/`.

Joseph Myers' suite for testing whether a polyform tiles. Used by `heesch-sat` as a fast pre-filter (sub-millisecond per shape). heesch-forge continues to depend on it.

### F.3 Kaplan, C. S. *Heesch dataset.* `cs.uwaterloo.ca/~csk/heesch/`.

Text files and PDFs of all polyforms with non-trivial Heesch numbers from Kaplan (2022). The heesch-forge regression baseline in Phase 0.

### F.4 Mann, C. *Edge-Marked Polyform Database.* (Older URL `math.uttyler.edu/polyformDB/`; current location to be verified.)

Reference dataset for marked-polyform Heesch numbers. Useful as cross-validation set if heesch-forge extends to marked polyforms.

---

## G. Reading priority

For someone joining the project cold, read in this order:

1. **A.8** Kaplan (2022) — current state of the art.
2. **A.3** Mann (2004) — clearest exposition of parametric families.
3. **A.7** Bašić (2021) — current record holder construction.
4. **PROPOSAL.md** in this repo — what we're doing.
5. **D.1** Wagner (2021) — the method underlying Layer 3.
6. **A.5, A.6** Bašić unboundedness results — context.
7. **B.1** Smith et al. (2024) — for Layer 5 technique transfer.

Everything else can be deferred until the relevant phase.
