# CLAUDE.md — heesch-forge Handoff

This file is the single point of context for any LLM (Claude Code in Antigravity, web Claude, audit LLMs) continuing work on heesch-forge. Read this first.

---

## What this project is

heesch-forge is a research project on the polyomino Heesch number problem. The Heesch number of a non-tiling shape is the maximum number of complete coronas of congruent copies that surround it. The polyomino case currently caps at H=3 (Kaplan 2022, n=17). The all-shape record is H=6 (Bašić 2020, non-polyform). We want to know whether polyominoes can do better than H=3, and to develop better tooling and theory along the way.

Live at [github.com/Darian-Frey/heesch-forge](https://github.com/Darian-Frey/heesch-forge). Master document: `PROPOSAL.md`. Phase status: `ROADMAP.md`. Annotated bibliography: `LITERATURE.md`.

---

## For Claude Code specifically

Read this section in full before doing anything. The rest of CLAUDE.md is shared with other LLMs and humans; this section is the operating contract for an autonomous coding agent.

### Reading order

1. This section.
2. `ROADMAP.md` — only the phase currently marked 🟢/🟡 in the status table at the top. Do not read all phases.
3. The relevant Layer in `PROPOSAL.md` §5 for the current milestone.
4. `LITERATURE.md` only when a specific reference is needed.

Do not read the entire repo before starting. Over-loaded context produces worse code.

### Session scoping

One milestone per session unless Shane explicitly extends. If a milestone has more than three sub-tasks, do the first one and ask before continuing. Better to deliver M0.2 cleanly than M0.2+M0.4+M1.1 half-done.

If a session would touch more than ~10 files or ~500 lines net, stop and propose the plan first.

### What's manual vs delegated

| Action | Who |
|--------|-----|
| `sudo apt install`, `nix-env`, system package management | Shane |
| Editing source code, Makefiles, scripts inside the repo | Claude Code |
| Running builds (`make`), running solver/benchmarks under ~5 min | Claude Code |
| Long benchmarks (>5 min wall time) or anything cloud-billable | Stop and ask |
| Downloading datasets via curl/wget | Claude Code |
| `git push` to `origin` | Claude Code, see PR policy below |
| Anything that incurs cost (Lambda, Vast.ai, paid APIs) | Stop and ask |

### Build & test

For the upstream `heesch-sat` once vendored at `src/sat/`:

```bash
# System deps (Shane installs these once):
#   build-essential, libcairo2-dev, plus CryptoMiniSat from source
#   (https://github.com/msoos/cryptominisat — there's no Ubuntu package recent enough)

cd src/sat
# Edit Makefile so LIBS points at your CryptoMiniSat install
make                  # builds gen, sat, viz, surrounds, report
./sat -h              # smoke check
```

There is no upstream test suite. Verification is via the M0.4 regression check against Kaplan's published values.

For Python ML work later (Layer 3): use `uv` for env management; pin all versions in `pyproject.toml`. Do not commit `requirements.txt`.

### How to vendor `heesch-sat` into `src/sat/`

Use a **plain copy via `git clone --depth 1`** stripped of its `.git`, not a GitHub fork and not a submodule. Reasons: (a) we will diverge substantially from upstream; (b) submodules are fragile; (c) a real GitHub fork pollutes the user's repo list. Concrete sequence:

```bash
mkdir -p src
git clone --depth 1 https://github.com/isohedral/heesch-sat.git /tmp/heesch-sat
rm -rf /tmp/heesch-sat/.git
mv /tmp/heesch-sat src/sat
git add src/sat
git commit -m "Vendor isohedral/heesch-sat at <commit-sha> as src/sat/"
```

Record the upstream commit SHA in the commit message and in `src/sat/UPSTREAM.md` so the provenance is recoverable. Upstream is BSD-3-Clause, which is compatible with our work; preserve `src/sat/LICENSE` unchanged.

### License

Repo-level `LICENSE` should be **BSD-3-Clause** to stay compatible with vendored upstream. Datasets and writeups under `data/` and `paper/` are CC-BY 4.0 (separate `LICENSE` file in those subdirs when populated). Do not introduce GPL anywhere — it would be incompatible with the BSD vendor.

### Commits and pushes

- **Default to PRs**, not direct push to `main`. Branch naming: `m0.2-vendor-upstream`, `m1.3-breakid`, etc. — the milestone code prefix is the convention.
- **Conventional Commits** style: `feat(sat):`, `fix(dlx):`, `docs(roadmap):`, `chore:`. One logical change per commit.
- **British English** in commit messages, code comments, and docs (e.g. "behaviour", "factorise").
- **Never force-push** to `main` or any branch with an open PR.

### Epistemic discipline

- Tag any unverified claim in code comments or docs with `[VERIFY]`. Tag known-correct ones with `[EST]`.
- If a search produces no result, write "no result found in [scope]" rather than fabricating one.
- If a benchmark number is reported, also commit the raw log it came from.
- If asked to summarise a paper, paraphrase and cite section numbers; do not invent quotes.

### When to stop and ask

- Any tool failure repeated more than twice with the same error.
- Any change to the layered architecture in `PROPOSAL.md` §5.
- Any addition or removal of a planned file from the `ROADMAP.md` "Cross-cutting artefacts" table.
- Any external network call beyond `git`, package mirrors, and the explicit dataset URL.
- Anything that touches Shane's other repos (CODE-GEO, CODA, NEPTUNE-NTT, etc.).

---

## How Shane works

- **Independent researcher** based in Kings Langley, England.
- **Hardware.** ThinkPad P15 Gen 2i, Linux. Cloud GPU only when needed (Lambda / Vast.ai).
- **Workflow.** Multi-LLM. Tasks are dispatched to several LLMs in parallel; Claude is the *primary auditor and formal document generator*. Other models (GPT-class, Gemini-class) handle exploration and code drafting.
- **Stack.** C++17/20 for solvers, Python for ML, LaTeX for papers, Qt 6 for any GUI work, GitHub for version control.
- **Conventions.** British English. ROADMAP/CLAUDE/LITERATURE files in every project. CODA-style versioning (v0.1, v0.2...). Honest epistemic tagging when uncertain.

---

## What has been done so far

**29 April 2026.** Project scaffolded and named. The following files exist:

- `README.md` — entry point.
- `PROPOSAL.md` — full research proposal v0.1.
- `ROADMAP.md` — phased plan, currently in Phase 0 (Setup).
- `LITERATURE.md` — annotated bibliography.
- `CLAUDE.md` — this file.

**Repository live at github.com/Darian-Frey/heesch-forge** (public, currently empty). M0.1 effectively complete pending push of the .md files. **No code yet.**

---

## What to do next

The next concrete action is to push the five .md files to the live repo and confirm CI is configured (GitHub Actions sufficient — basic markdown lint will do for now).

After that, **M0.2**: vendor `isohedral/heesch-sat` into `src/sat/` (see "For Claude Code specifically" below for the exact method). Build dependencies upstream are **CryptoMiniSat** (the SAT backend), Cairo (only for the optional `viz` PDF tool), and a C++17 compiler. Layer 1 will later swap in CaDiCaL/Kissat/Glucose as a portfolio — but the upstream baseline must build with CryptoMiniSat first so the regression suite has something to compare against.

Build process is `make` in `src/` after editing `src/Makefile` to point at local library paths. There is no test suite upstream — verification is by reproducing Kaplan's published H values, which is what M0.4 covers. Executables produced: `gen`, `sat`, `viz`, `surrounds`, `report`.

Then **M0.4**: pull Kaplan's published H≥1 polyomino dataset from `cs.uwaterloo.ca/~csk/heesch/`, parse it into the regression suite format defined in `benchmarks/`, and verify that unmodified `heesch-sat` reproduces the listed H values for n ≤ 12. This locks the baseline.

---

## Known design decisions

- **Layered architecture.** Five layers: engineering → hybrid solver → RL search → lateral grids → theory. Each independently publishable. Later layers depend on earlier layers' tooling but not on their results. See PROPOSAL §5.
- **Reward signal for RL.** Layer 3 will use the MaxSAT partial-corona score from Layer 1, *not* just the integer Heesch number. This avoids a flat reward landscape.
- **Verification discipline.** Any high-Heesch candidate from RL will be re-verified by the deterministic SAT solver before publication. MaxSAT is reward-shaping, not proof.
- **Negative results count.** If Layer 1 yields <2× speedup, write it up. If RL fails to recover known H=3 shapes, write it up. Negatives constrain the problem.
- **Phase 6 (theory) runs continuously.** Even if no breakthrough comes, formalising corona-completion as equivariant lifting is itself a contribution.

---

## Likely traps

- **Don't fabricate citations.** If a referenced result cannot be found, mark `[VERIFY]` and flag for next pass. Default Shane policy: never cite what hasn't been read.
- **Don't conflate Hc and Hh.** Kaplan's 2022 dataset reports both. Some older literature uses unrestricted Heesch number; the simply-connected version (Hc) is what most modern papers report. heesch-forge uses Hc as primary.
- **Don't assume `heesch-sat` builds on first try.** Upstream is a research codebase. Phase 0 may need patches.
- **Don't treat polyhex/polyiamond H=4 as a polyomino result.** They are different grids. The polyomino record stands at H=3. (Wikipedia and casual sources sometimes blur this.)
- **Be careful with Bašić's 2020 figure.** It is *not* a polyform. Its H=6 status is the all-shape record but not directly comparable to polyomino Hc records.

---

## Open questions to watch

- Is there a polyomino with Hc ≥ 4? (RQ1.)
- Can RL recover Kaplan's H=3 catalogue without exhaustive enumeration? (Phase 4 exit criterion.)
- Does the bevelhex (4.6.12) grid support higher Heesch numbers than polyhexes? (RQ5.)
- Is there an obstruction stronger than Ammann's parity? (RQ4 / Phase 6.)

These are tracked in `OPEN_QUESTIONS.md`, which will be created when the first side-question accumulates. See `ROADMAP.md` "Cross-cutting artefacts" for the full list of planned files and their trigger conditions.

---

## Communication conventions

When auditing a result:

- **EST** = established (peer-reviewed and reproduced)
- **SPEC** = speculative (proposed, not verified)
- **OPEN** = open question
- **SEALED** = closed by Shane's decision

Apply these tags in all heesch-forge documents and reasoning. They are inherited from CODA practice.

---

## File this project alongside

heesch-forge is in the same family as:

- **CODE-GEO / The-Euclid-Sentinel** — modified gravity and SPARC fitting.
- **CODA** — Krylov complexity coupled to Einstein-Hilbert gravity.
- **NEPTUNE-NTT** — AVX2 NTT for ML-KEM.

These are separate repos. heesch-forge does not depend on any of them. The methodological similarity (multi-track research with engineering, empirical, and theoretical components running in parallel) is intentional.

---

*End of CLAUDE.md v0.1.*
