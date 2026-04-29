# heesch-forge

> *"…aus kongruenten Bereichen."* — Heesch (1968)

A research project on the polyomino Heesch problem. Successor to Kaplan's `heesch-sat`, with a broader algorithmic and theoretical agenda.

## What this is

The Heesch number of a non-tiling shape is the maximum number of complete coronas of congruent copies that can surround it. It measures how *close to tiling* a non-tiler comes. Whether every positive integer can be a Heesch number is open since 1968.

Among polyominoes (shapes built from unit squares), the highest known Heesch number is **3**, established by Kaplan (2022) via SAT-based exhaustive enumeration up to n=19. The all-shape record is **6** (Bašić 2020, hand-constructed non-polyform). The gap is unexplained.

heesch-forge attacks the polyomino Heesch problem on five fronts simultaneously:

1. **Engineering** — modernise Kaplan's `heesch-sat`: portfolio SAT, MaxSAT, BreakID, instrumentation.
2. **Algorithm** — hybrid Algorithm-X / SAT solver exploiting the structure DLX wants and the structure SAT wants.
3. **Search** — Wagner-style RL over the polyomino space, treating Heesch number as the reward signal.
4. **Lateral** — first published Heesch numbers for polyforms on the bevelhex (4.6.12 Archimedean) and octasquare grids.
5. **Theory** — new upper-bound obstruction techniques beyond Ammann's projection-vs-indentation parity argument.

The full proposal, with gap analysis, methodology, risks, and references, is in [`PROPOSAL.md`](./PROPOSAL.md).

## Repository layout (target)

```
heesch-forge/
├── README.md           — this file
├── PROPOSAL.md         — full research proposal (master document)
├── ROADMAP.md          — phased milestones, current status
├── LITERATURE.md       — annotated bibliography
├── CLAUDE.md           — handoff doc for Claude Code / multi-LLM continuation
├── src/
│   ├── sat/            — fork of Kaplan's heesch-sat, modernised (Layer 1)
│   ├── dlx/            — Algorithm X / Dancing Links implementation
│   ├── rl/             — cross-entropy / RL agent over shape space
│   └── theory/         — formalisation experiments (Lean / SageMath)
├── data/
│   ├── kaplan-2022/    — published H≥1 polyomino dataset (regression baseline)
│   ├── bevelhex/       — new bevelhex polyform datasets
│   └── results/        — heesch-forge-discovered shapes
├── notebooks/          — exploratory analysis (Jupyter)
├── paper/              — LaTeX manuscripts (engineering, RL, lateral, theory)
├── benchmarks/         — solver benchmark suite + results
└── docs/               — supporting notes, derivations, design docs
```

## Status

**v0.1, 29 April 2026.** Proposal drafted. Repository scaffolded. No code yet.

## How to engage

- For background and the full plan: [`PROPOSAL.md`](./PROPOSAL.md).
- For current phase and milestones: [`ROADMAP.md`](./ROADMAP.md).
- For continuing a session in another LLM or coding agent: [`CLAUDE.md`](./CLAUDE.md).

## License

**BSD-3-Clause** for code (matches vendored `heesch-sat` upstream). **CC-BY 4.0** for datasets and writeups under `data/` and `paper/`. See `LICENSE` once that file lands (per `ROADMAP.md` planned-files table).
