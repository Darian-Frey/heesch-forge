# M1.5 — MaxSAT partial-corona score

## Verdict

Infrastructure shipped (`sat-wcnf-dump` build, `run_rc2.py` wrapper,
`probe/run_probe.sh` end-to-end pipeline). The MaxSAT score itself is
**not extractable from heesch-sat's existing CNF encoding without an
encoding refactor**, and the refactor belongs under M2 rather than M1.
This is the same architectural finding as M1.4 (PBLib AMO sweep) seen
from a different angle: the heesch-sat encoding is reachability-style,
hardcoding corona-completion via an implication chain anchored on a
forced unit clause, with no clean soft-relaxation point.

## What ships in this commit

- `src/sat/src/wcnf_dumping_backend.h` — wraps a real
  `CMSat::SATSolver`, intercepts `add_clause` to buffer DIMACS-int
  form, and on each UNSAT solve writes a classic-format WCNF to
  `$HEESCH_DUMP_DIR`. Cell-vars (passed in by
  `HeeschSolver::getClauses` under `#ifdef HEESCH_BACKEND_WCNF_DUMP`)
  are emitted as unit soft clauses with weight 1; everything else is
  hard at the top weight.
- `src/sat/src/heesch.h` — adds the
  `HEESCH_BACKEND_WCNF_DUMP` arm to the `SATSolverImpl` typedef and a
  small `#ifdef`-guarded loop in `getClauses` that informs the WCNF
  backend which vars are cell-vars.
- `src/sat/src/Makefile` — `sat-wcnf-dump` build target alongside
  `sat`, `sat-cadical`, `sat-kissat`, `sat-dump`.
- `benchmarks/maxsat/run_rc2.py` — Python wrapper around
  `pysat.examples.rc2.RC2`. Walks a directory of `.wcnf` files, runs
  RC2 on each (60 s wall cap by default), writes a CSV with
  `(path, nvars, nhard, nsoft, cost, score, wall_s, timeout,
  unsat_hard)`. `score = nsoft - cost` is the partial-corona score
  *if* the hard portion is satisfiable.
- `benchmarks/maxsat/probe/run_probe.sh` — wires the two together
  end-to-end. Builds `sat-wcnf-dump` if missing, runs it on a small
  shape set, runs RC2 on each dump, aggregates a per-shape CSV.

## What the probe shows

Default probe (first heptomino + first octomino in the Kaplan
dataset, both Hc = 1):

| shape | instance | hard clauses | softs | RC2 wall | result |
|-------|----------|-------------:|------:|---------:|--------|
| n=7 | inst-00001-call000 |   2,491 |  107 |   0.00 s | UNSAT-HARD |
| n=7 | inst-00002-call000 | 268,323 |  342 |   0.09 s | UNSAT-HARD |
| n=7 | inst-00003-call000 | 390,701 |  355 |   0.11 s | UNSAT-HARD |
| n=8 | inst-00001-call000 |  10,605 |  178 |   0.00 s | UNSAT-HARD |
| n=8 | inst-00002-call000 | 749,355 |  581 |   2.13 s | UNSAT-HARD |
| n=8 | inst-00003-call000 | 790,601 |  581 |  20.02 s | UNSAT-HARD |

(Numbers from `results/m1.5-probe.csv`.)

Every WCNF returns `UNSAT-HARD` — RC2 reports that the hard portion of
the formula is itself infeasible, *regardless* of which soft cell-vars
we ask it to maximise. There is no "partial-corona size" reading to
take from these dumps because the encoding does not separate "tile
placement" from "cell coverage": both are entangled in the same hard
implication chain.

## Why the trivial relaxation does not work either

A reasonable first guess is that the WCNF dumper is treating *too
many* clauses as hard and we should drop the unit clause anchoring
the original shape (`tile 0 at level 0`, the very first clause
`getClauses` emits). The probe was repeated by hand with that one
clause removed:

```
$ awk … inst-00002-call000.wcnf > inst-00002-relaxed.wcnf
$ .venv/bin/python -c "from pysat.examples.rc2 import RC2; …"
nvars=1885 nhard=268322 nsoft=342
cost=342  satisfied_softs=0  wall=0.08s
```

RC2 finishes in 0.08 s and reports **all 342 softs unsatisfied** at
optimum, because the trivial all-false assignment satisfies every
remaining hard implication (each is of the form `tile-var → cell-var`
or `cell-var → some-tile`) and pays the lowest possible cost — zero
satisfied softs. Removing one anchor turns the problem from
"impossible to satisfy" into "trivially over-relaxed."

## What a real partial-corona score requires

The encoding heesch-sat uses today is, in MaxSAT terms, **fully
hard**. Two pieces would have to change for RC2 (or any MaxSAT
solver) to produce a meaningful "how close to a complete corona did
we come" score:

1. Introduce explicit per-cell `coverage[c]` decision variables that
   are *not* forced by tile placement. Each cell currently labelled
   "must be covered at level k via the implication chain" instead
   gets a decision variable.
2. Replace the chain implication `tile placed → halo cell covered`
   (Block D in `getClauses`) with a constraint that *links*
   `tile_placed[t]` to `coverage[c]` only one direction:
   `coverage[c] → some tile covers c at some level` (Block C
   already encodes this), but **not** the reverse. Forcing the
   shape to tile becomes a soft preference: `(coverage[c])` for each
   level-k halo cell c, weight 1.

This is an encoding redesign, not a tweak. It overlaps significantly
with what M2 already plans: PROPOSAL §5 Layer 2 explicitly redefines
the constraint structure so DLX handles cell-covering and SAT handles
non-overlap, which is exactly the separation a MaxSAT formulation
needs. Doing the work twice (under M1.5 and again under M2) is
wasteful.

The infrastructure shipped here lets whoever does that refactor
(under M2 or later) feed real partial-corona WCNFs through RC2
without writing new C++ scaffolding. That is the load-bearing piece;
the negative finding is the rest of the deliverable.

## Phase-1 implications

Combined with M1.1 (CaDiCaL: 1.95× slower than CMSat), M1.2a (Kissat:
7.33× slower), M1.3 (BreakID: zero gain on CMSat with infeasible cost
on walkback formulas), and M1.4 (no AMO sites to optimise), this is
the fifth Phase-1 lever to come back as either negative or
not-applicable-without-refactor.

The M1.7 decision gate (ROADMAP "Phase 1 / Decision gate" — *"If
speedup is <2× across the board, pause Layer 1 work, write up
findings as a negative-result short paper, and proceed directly to
Layer 3 (RL) using v1 solver"*) reads as already triggered in
substance even though M1.7 has not been formally written up yet. The
common thread across all five negatives is that **the heesch-sat
encoding itself is the bottleneck, not the solver or the
preprocessing pipeline**, and the only Phase-1-style intervention
that has a realistic chance of moving the needle is the encoding
redesign that M2 is going to do anyway.

## Reproducing

```bash
# One-time install (PEP 668 venv):
sudo apt install -y python3-venv
python3 -m venv .venv
.venv/bin/pip install --upgrade pip
.venv/bin/pip install 'python-sat[aiger]'

# Build sat-wcnf-dump and probe:
( cd src/sat/src && make sat-wcnf-dump )
bash benchmarks/maxsat/probe/run_probe.sh
# writes:
#   benchmarks/maxsat/results/m1.5-probe.log
#   benchmarks/maxsat/results/m1.5-probe.csv
```

Pass shape coordinate strings as positional arguments to probe
specific shapes, e.g.

```bash
bash benchmarks/maxsat/probe/run_probe.sh \
  "1 0 1 1 0 2 1 2 1 3 2 3 3 3"
```

Hardware for the committed numbers: ThinkPad P15 Gen 2i, Ubuntu 24.04,
g++ 13.3, CryptoMiniSat 5.14, RC2 from python-sat (latest pip).
