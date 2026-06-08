# CLAUDE.md — heesch-forge Handoff

> **Status:** Active
> **Provenance:** Claude (primary auditor & document generator); multi-LLM hub-and-spoke (GPT-/Gemini-class spokes for exploration & code drafting)
> **Last reviewed:** 2026-06-08
> **Why this status:** Phases 0–3 closed with retrospectives shipped; Phase 4 (RL pilot) landed through M4.3, surfacing the expected reward-signal failure. One load-bearing open milestone — **M2.6-followup-B** (MaxSAT coverage-variable encoding refactor) — gates the entire Phase-4 dense-reward chain. Resumption condition: session 2 on M2.6-followup-B via the CNF-diff approach (see "What to do next").

---

This file is the single point of context for any LLM (Claude Code in Antigravity, web Claude, hub-and-spoke audit/exploration models) continuing work on heesch-forge. **Read this first.**

This is **v0.2**. The previous v0.1 (29 April 2026) described a repo with "no code yet"; that is long obsolete. Phases 0–3 are closed and Phase 4 is mid-flight. The state below is current as of the last commit (`0dd1ce3`, 2 May 2026).

---

## What this project is

heesch-forge is a research project on the **polyomino Heesch number** problem. The Heesch number of a non-tiling shape is the maximum number of complete coronas of congruent copies that surround it without gaps or overlaps. Among polyominoes the record is **Hc = 3** (Kaplan 2022, SAT-based exhaustive computation of Heesch numbers up to 19-ominoes). The two smallest Hc = 3 polyominoes have **17 cells** (Kaplan 2022, Fig. 3). The all-shape record is **6** (Bašić 2020, a hand-constructed non-polyform). Whether a polyomino can reach Hc ≥ 4 is open, as is whether every positive integer is some shape's Heesch number (open since Heesch 1968).

> `[EST]` Both figures above were reconfirmed against Kaplan (2022) in the 2026-06-08 audit: exhaustive Heesch-number computation runs to **19-ominoes** (also 17-hexes, 24-iamonds — do not conflate the 17-hex range with the 17-cell smallest-Hc=3 polyomino), and the smallest Hc = 3 polyominoes are **17-ominoes** (paper Fig. 3). Note the dataset enumerates *all* polyominoes only through n ≈ 10; above that it applies Heesch-threshold cuts rather than full enumeration. Source: arXiv:2105.09438 / cs.uwaterloo.ca/~csk/heesch/.

Live at [github.com/Darian-Frey/heesch-forge](https://github.com/Darian-Frey/heesch-forge). Master document: `PROPOSAL.md`. Phase status: `ROADMAP.md` (status notes are latest-first at the bottom). Annotated bibliography: `LITERATURE.md`.

The project attacks the problem on five layers, each independently publishable: **L1 engineering** (modernise Kaplan's `heesch-sat`), **L2 hybrid solver** (Algorithm-X/DLX × SAT), **L3 RL search** (Wagner-style cross-entropy over shape space), **L4 lateral grids** (Heesch numbers on bevelhex / octasquare), **L5 theory** (obstructions beyond Ammann parity). See `PROPOSAL.md` §5.

---

## For Claude Code specifically

Read this section in full before doing anything. The rest is shared with humans and hub-and-spoke models.

### Reading order

1. This section.
2. `ROADMAP.md` — the **Status notes (latest first)** block at the bottom, plus only the phase currently active. Do not read all six phases.
3. The relevant Layer in `PROPOSAL.md` §5 for the current milestone.
4. For the current blocker specifically: `benchmarks/maxsat/M2.6-followup-B-attempt.md` — the full post-mortem of session 1.
5. `LITERATURE.md` only when a specific reference is needed.

Do not read the entire repo before starting. Over-loaded context produces worse code.

### Session scoping

One milestone per session unless Shane explicitly extends. If a milestone has more than three sub-tasks, do the first and ask. If a session would touch more than ~10 files or ~500 lines net, stop and propose the plan first.

### What's manual vs delegated

| Action | Who |
| --- | --- |
| `sudo apt install`, system package management, CryptoMiniSat/CaDiCaL/Kissat installs | Shane |
| Editing source, Makefiles, scripts inside the repo | Claude Code |
| Builds (`make`), solver/benchmark runs **under ~5 min wall** | Claude Code |
| Long benchmarks (>5 min) or anything cloud-billable | Stop and ask |
| Downloading datasets via curl/wget (from the recorded URLs only) | Claude Code |
| `git push` to `origin` | Claude Code, PR policy below |
| Anything incurring cost (Lambda, Vast.ai, paid APIs) | Stop and ask |

### Build & test (current — `src/sat` is vendored and builds)

`heesch-sat` is already vendored at `src/sat/` (provenance in `src/sat/UPSTREAM.md`). It is **not** a submodule — it is a plain copy we diverge from. Do not re-vendor.

```bash
# System deps (Shane installs once): build-essential, libcairo2-dev,
# CryptoMiniSat from source, plus CaDiCaL and Kissat for the portfolio.

# 1) DLX library + 18-test gate
cd src/dlx && make && ./test_dlx

# 2) Corona-completion oracle (depends on libdlx)
cd ../corona && make && ./test_corona

# 3) The SAT engine + portfolio + dump binaries. Edit src/sat/src/Makefile
#    LIBS to point at local CryptoMiniSat first.
cd ../sat/src && make
#   targets: sat viz gen surrounds report sat-cadical sat-kissat sat-dump sat-wcnf-dump
#   sat links libcorona.a (M2.6-followup-A) for CoronaState seed support.

# 4) Regression gate — THE correctness contract. No upstream unit tests;
#    verification is reproducing Kaplan's published (Hc, Hh) values.
cd ../../../benchmarks/kaplan && python3 run_regression.py        # sizes 7,8,11,12 (174 shapes, ~tens of s)
#   Full n<=12 (3524 shapes, multi-hour): python3 run_regression.py --sizes 7,8,9,10,11,12

# 5) RL layer (Python, uv-managed, CPU-only torch). 20 pytest tests.
cd ../../src/rl && uv run pytest
```

**The 174-shape M0.4 regression (`match=174 mismatch=0`) is the invariant.** Any encoding change must keep it green. Session 1 of M2.6-followup-B broke it to `match=22 mismatch=152` — that is the canary.

### Commits and pushes

- **Default to PRs**, not direct push to `main`. Branch = milestone prefix: `m2.6b-encoding-refactor`, `m4.4-dense-reward`, etc.
- **Conventional Commits**: `feat(sat):`, `fix(dlx):`, `docs(roadmap):`, `chore:`. One logical change per commit.
- **British English** in messages, comments, docs ("behaviour", "factorise").
- Commit the raw log/CSV alongside any reported benchmark number.
- **Never force-push** to `main` or any branch with an open PR.

### Epistemic discipline

- Tag unverified claims `[VERIFY]`, estimates `[EST]`. Tag results `EST` / `SPEC` / `OPEN` / `SEALED` (inherited from CODA practice).
- "No result found in [scope]" rather than a fabricated one. Never invent citations or quotes; paraphrase and cite section numbers.

### When to stop and ask

- Any tool failure repeated more than twice with the same error.
- Any change to the layered architecture (`PROPOSAL.md` §5).
- Any addition/removal of a planned file in the `ROADMAP.md` cross-cutting-artefacts table.
- Any network call beyond `git`, package mirrors, and recorded dataset URLs.
- Anything touching Shane's other repos (CODA, Euclid-Sentinel, NEPTUNE-NTT, …).

---

## What has been done so far

State as of `0dd1ce3` (2 May 2026). Full detail in `ROADMAP.md` status notes.

- **Phase 0 — Setup.** ✅ done (6/6).
- **Phase 1 — Engineering (L1).** ✅ closed as a **negative result**. Portfolio SAT (CaDiCaL, Kissat), MaxSAT/WCNF infrastructure, BreakID probe (M1.3, no win). M1.5 surfaced the architectural finding that drives the current blocker: the reachability-style CNF cannot expose a partial-corona score for MaxSAT. Retrospective: `paper/drafts/phase1-engineering-negatives.md`.
- **Phase 2 — Hybrid solver (L2).** ✅ closed (6/7). Standalone DLX library (`src/dlx/`, 18 tests), corona-completion oracle (`src/corona/`), DLX×SAT hybrid. **M2.6-followup-A** landed a bounded-DLX + SAT partial-state seed: **53× speedup** vs the naïve joint. Retrospective: `paper/drafts/phase2-engineering-hybrid.md`.
- **Phase 3 — Lateral grids (L4).** ✅ closed (5/5). **First publishable non-Kaplan-grid Heesch data**: Hc = 2 on bevelhex (n = 9, 10, 11) and 20 Hc = 2 on octasquare (s+o = 7, 9, 10). No Hc ≥ 3 found on either grid yet. Needed an upstream `bitgrid<128>`→`<256>` bump for bevelhex n ≥ 11. Retrospective: `paper/drafts/phase3-lateral-grids.md`.
- **Phase 4 — RL pilot (L3).** 🟡 in progress (table currently mislabels this "not started" — **stale, fix it**). M4.1 (MDP design), M4.2 (GraphSAGE policy + random baseline), M4.3 (CEM trainer) all landed. **M4.3 surfaced the reward-signal failure**: vanilla CEM cannot beat the random floor because Hc ≥ 2 fires < 1× per generation in expectation. This is itself a publishable negative — and it routes cleanly to M4.4 (dense reward), which is gated on the blocker below.

**Dependency chain:** `M4.5 (recall Kaplan's Hc≥2) → M4.4 (MaxSAT dense reward) → M2.6-followup-B (encoding refactor)`. Everything Phase-4-substantive is stuck behind one encoding change.

### Two stale docs to fix on the next docs pass (independent of any milestone)

1. `README.md` `## Status` still says "v0.1 … No code yet" — wildly wrong; update to reflect Phases 0–3 closed / Phase 4 mid-flight, and add Shane's four-field status header (the README currently lacks it).
2. `ROADMAP.md` Phase-status table marks Phase 4 "⚪ not started", contradicting the status note directly above it (M4.1–M4.3 landed). Set it to 🟡.

---

## What to do next — M2.6-followup-B, session 2

**Goal.** Decouple cell-coverage from tile-placement so RC2 MaxSAT can read a meaningful partial-corona score off a near-miss corona. This is the dense-reward source for RL (M4.4) and an independently useful solver capability.

**What happened in session 1 (reverted, nothing on `main`).** The per-cell-per-level `coverage[c,k]` rewrite of Blocks B/C/D in `src/sat/src/heesch.h` broke Hh detection on **152/174** regression shapes. Diagnosis in `benchmarks/maxsat/M2.6-followup-B-attempt.md`: the rewritten Block C (`coverage[c,k] → ∨_t tile[t,k]`, same-level) is **too tight** versus the original (`cell_var[c] → ∨_t ∨_{any level} tile[t,k]`, any-level), and the failure shows up specifically on the **culled-adjacency** Hh path (`extendLevelWithTransforms(level_-1, cloud_.adjacent_culled_)` then a fresh `allow_holes=true` solve).

**Smallest reproducer.** n = 8 shape `1 0 2 0 1 1 0 2 1 2 1 3 2 3 3 3` — dataset `Hc=1 Hh=2`; original encoding returns `~ 1 2 0`, refactor returns `~ 1 1 0`.

**Code landmarks** (all in `src/sat/src/heesch.h` unless noted):

- `getClauses(SATSolverImpl&, bool allow_holes)` — encoding entry, ~line 584. Blocks B/C/D are the cell-coverage logic.
- `extendLevelWithTransforms(...)` — ~line 523; `cloud_.adjacent_` vs `cloud_.adjacent_culled_` is the crux (culled path is the one that breaks).
- `add_soft_cell_var(...)` ~line 609 — existing soft-clause hook for WCNF.
- Seed support (M2.6-followup-A, leave alone): `addSeedPlacement` / `solveFromSeed`; `-seed` flag in `sat.cpp`.

**Recommended approach — option (3), CNF-diff.** The tooling already exists: build the original and refactored encodings and dump both with the **`sat-dump`** / **`sat-wcnf-dump`** targets, then diff the two CNFs on the n = 8 reproducer to isolate the exact missing/extra clause. Half-day estimate; mechanical. Do this **before** committing to either fallback, so the same bug isn't baked in under a new name:

- Option (1): keep `cell_var[c]` as-is, ADD parallel `coverage[c,k]` softs linked one-way (`coverage[c,k] → cell_var[c]` hard). Safest, doubles cell vars.
- Option (2): keep Block C global (any-level), rewrite only Block D per-level. Preserves the original solution space while exposing per-level softs.

**Exit criterion for the session.** M0.4 regression back to `match=174 mismatch=0` with the new encoding present, AND `sat-wcnf-dump` emitting per-level coverage softs that RC2 can score. If only the diagnosis lands (clause identified, no fix yet), that is still a clean session — record it the same way session 1 was recorded.

---

## Hub-and-spoke dispatch (this is how the research continues from here)

Claude is the **hub**: auditor, synthesiser, and formal-document generator. Spokes (GPT-/Gemini-class models) get scoped, independent, adversarial sub-tasks; the hub reconciles. Suggested decomposition for M2.6-followup-B so spokes work without colliding:

- **Spoke A — CNF semantics.** Given the two encodings' dumped CNFs on the reproducer, identify the precise clause-set difference and explain which solution the refactor wrongly excludes. Deliverable: the missing-clause hypothesis, no code.
- **Spoke B — minimal patch.** Independently propose the smallest edit to `getClauses` that exposes per-level coverage softs while keeping the original any-level solution space (favour option 2). Deliverable: a diff against `heesch.h`.
- **Spoke C — adversarial regression.** Try to break any proposed patch: hand-pick shapes beyond the 174 (especially holey n = 11–12 with Hh > Hc) likely to expose same-level-vs-any-level divergence. Deliverable: a candidate shape list + expected `(Hc, Hh)`.

Hub reconciles A's diagnosis against B's patch, runs C's shapes through the M0.4 gate, and only then writes the status note. **Verification stays deterministic**: MaxSAT is reward-shaping; any Hc claim is reconfirmed by the plain SAT solver before it enters a paper. Cross-model agreement is evidence, not proof — the regression gate is the arbiter.

---

## Known design decisions (carried forward)

- **Layered architecture** (PROPOSAL §5): five independently-publishable layers; later layers use earlier tooling, not earlier results.
- **RL reward** is the MaxSAT partial-corona score, **not** the bare integer Hc — precisely to avoid the flat landscape M4.3 has now empirically confirmed (`EST`: vanilla CEM stalls at the random floor at our reward sparsity).
- **Negative results count.** Phase 1 (no portfolio win), Phase 4 (CEM-fails) are written up. Negatives constrain the problem.
- **Phase 6 (theory)** runs continuously; formalising corona-completion as equivariant lifting is a contribution even without a record.

---

## Likely traps

- **Don't conflate Hc and Hh.** Kaplan's dataset reports both; heesch-forge uses **Hc** (simply-connected) as primary. The current blocker is *specifically* an Hh-path failure — Hc on the reproducer is unaffected.
- **Don't treat polyhex/polyiamond Hc = 4 as a polyomino result.** Different grids. The polyomino record is **3**. (Wikipedia and casual sources blur this.)
- **Bašić's 2020 H = 6 is not a polyform** — all-shape record, not comparable to the polyomino Hc record.
- **The per-level Block C trap** (new, from session 1): a "tighter" same-level coverage encoding is *not* equivalent to the original any-level one under the culled-adjacency Hh path. Prove equivalence on the reproducer before trusting it.
- **Don't re-vendor `src/sat`.** It is a tracked plain copy we have diverged from (bitgrid bump, seed support, WCNF dump). Build artefacts (`sat`, `sat-cadical`, …) are gitignored — that's expected, not a missing checkout.

---

## Communication conventions

`EST` established (peer-reviewed & reproduced) · `SPEC` speculative · `OPEN` open question · `SEALED` closed by Shane's decision. Plus inline `[VERIFY]` / `[EST]` on individual claims. Apply in all heesch-forge docs and reasoning.

---

## Open questions to watch

- Is there a polyomino with Hc ≥ 4? (RQ1, the crown jewel.)
- Can RL recover Kaplan's Hc ≥ 2 catalogue without exhaustive enumeration? (Phase-4 exit criterion — currently blocked on dense reward → M2.6-followup-B.)
- Do bevelhex / octasquare support Hc ≥ 3? (RQ5; bounded-compute followups defined: bevelhex n = 12 ≈ 4–6 h; the 49 octasquare (5,5) inconclusives at deeper maxlevel.)
- An obstruction stronger than Ammann parity? (RQ4 / Phase 6.)

---

*End of CLAUDE.md v0.2 — 2026-06-08.*
