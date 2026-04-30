# M1.3 — BreakID empirical probe (negative result)

## Verdict

**BreakID does not deliver Phase-1 speedup on heesch-sat with CryptoMiniSat
as the SAT engine, and is catastrophically expensive on the formulas
where heesch-sat actually spends time.**

The probe ran each polyomino through `sat-dump` (the `-DHEESCH_BACKEND_DUMP`
build that tees every SAT instance to DIMACS), then ran BreakID 3.1 on
each instance and A/B-tested CMSat on raw vs symmetry-broken CNFs. Two
clean regimes emerged.

| regime | example | clauses in | BreakID wall | breaking clauses | CMSat conflicts (raw → broken) |
|--------|--------:|-----------:|-------------:|-----------------:|-------------------------------:|
| corona-loop / iso solver (fresh) | n=7 inst-00000-call000 |    1,767 |    0.00 s |     16 |  1 → 1 |
| corona-loop / iso solver (fresh) | n=8 inst-00000-call000 |    7,579 |    0.09 s |     19 |  1 → 1 |
| corona-loop / iso solver (fresh) | n=8 inst-00001-call000 |   10,605 |    0.16 s |     19 | 22 → 22 |
| `iterateUntilSimplyConnected`    | n=8 inst-00002-call000 |  749,355 |   60.0 s timeout |  none found  | 1,093 (raw) — broken not run |
| `iterateUntilSimplyConnected`    | n=8 inst-00003-call000 |  790,601 |   60.0 s timeout |  none found  | 1,204 (raw) — broken not run |

(Numbers from `results/m1.3-probe.csv`; full BreakID logs in
`results/m1.3-probe.log`.)

## Why this is conclusive

1. **Fresh-formula symmetries are a wash.** BreakID does find 16–19
   breaking clauses on each fresh formula in well under a second. But
   CMSat's solve trace is **byte-identical** raw versus broken: same
   conflict count, same decision count. CryptoMiniSat's own
   inprocessing already discovers the same equivalences, so the
   appended breaking clauses are redundant. (Wall-time deltas exist
   but are sub-millisecond noise dominated by libcryptominisat5
   startup variance.)
2. **Walkback formulas are the only place the solver actually sweats.**
   The two large CNFs in the table consume 23–29 ms of CMSat wall on
   their own, against sub-millisecond solves for the fresh formulas.
   They are also where heesch-sat spends measurable time per shape.
3. **BreakID cannot help the walkback case.** Each walkback CNF
   contains thousands of hole-banning clauses appended by
   `iterateUntilSimplyConnected`'s `while-solve-then-add-clauses`
   loop. Those clauses break every variable-level automorphism; with
   60 s of preprocessing budget BreakID detects exactly zero
   symmetries on every walkback instance probed. The full-throttle
   run on the n=7 walkback formulas confirmed this without the
   timeout (226 s and 753 s respectively, both reporting
   `Num generators: 0`; see `results/m1.3-probe.log`).

A naive "preprocess every CNF with BreakID before solving" pipeline
therefore costs at least the BreakID time on every instance, with no
corresponding solver-side gain. On the n=8 example above that would
mean **at minimum** an extra 120 s per shape (60 s × 2 walkback solves)
to save zero wall-time. M1.3 cannot meet Phase-1's ≥5× target via
BreakID.

## What about CaDiCaL / Kissat?

The probe used CMSat because it is the M0.5 baseline. A theoretical
case for BreakID with the modern CDCL solvers is that, lacking
CMSat's aggressive inprocessing, they might benefit more from the 19
breaking clauses. M1.1 / M1.2a already established that CaDiCaL is
1.95× slower and Kissat is 7.33× slower than CMSat on the M0.5
workload at every percentile. Even a generous 30 % BreakID-induced
speedup on the modern solvers would still leave them slower than
CMSat alone, so this branch is not worth pursuing as a Phase-1 lift.

## Phase-1 implications

- **M1.3 is closed as a documented negative result.** Phase 1's
  decision gate (ROADMAP §"Phase 1 / Decision gate") explicitly
  accommodates negatives: "If speedup is <2× across the board, pause
  Layer 1 work, write up findings as a negative-result short paper,
  and proceed directly to Layer 3 (RL) using v1 solver." This finding
  is part of that record.
- **The interesting follow-up is structural, not solver-internal.**
  The walkback formulas grow huge precisely because
  `iterateUntilSimplyConnected` keeps appending hole-banning clauses
  to a single SAT instance. A refactor that re-derives the formula at
  a coarser level (e.g. switching to a one-shot SAT call per
  hole-pattern instead of accumulating) might both unlock new
  symmetry-breaking opportunities AND make Kissat's non-incremental
  design (M1.2a) workable. That sits naturally under M2 (hybrid
  solver / DLX) rather than M1.
- **M1.4 (PBLib AMO encoding sweep) and M1.5 (MaxSAT) remain the
  candidate Phase-1 levers** that have not yet been ruled out.

## Reproducing

```bash
bash benchmarks/breakid/probe/run_probe.sh
# writes:
#   benchmarks/breakid/results/m1.3-probe.log
#   benchmarks/breakid/results/m1.3-probe.csv
```

The script auto-builds `sat-dump` and the `cms_solve` helper if
missing. Default shapes are the first heptomino and the first
octomino from the Kaplan dataset; pass shape coordinates as arguments
to probe other shapes.

Hardware for the committed numbers: ThinkPad P15 Gen 2i, Ubuntu 24.04,
g++ 13.3, CryptoMiniSat 5.14, BreakID 3.1.
