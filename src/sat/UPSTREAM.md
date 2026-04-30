# Upstream provenance

This directory is a **plain-copy vendor** of `isohedral/heesch-sat`. It is not a
git submodule and not a GitHub fork. Divergence from upstream is expected and
will not be merged back; see `CLAUDE.md` and `PROPOSAL.md` §5 (Layer 1) for the
modernisation plan.

| Field | Value |
|-------|-------|
| Upstream URL | <https://github.com/isohedral/heesch-sat> |
| Upstream commit SHA | `1adb37204013b96b62b954a616265e49d7cf21ad` |
| Upstream commit date | 2026-02-17 |
| Vendored on | 2026-04-29 |
| Vendor method | `git clone --depth 1` then `rm -rf .git`; see `CLAUDE.md` |
| Upstream license | BSD-3-Clause (Kaplan 2021), preserved as `src/sat/LICENSE` |
| heesch-forge license | BSD-3-Clause (compatible) |

## Known upstream-vs-Linux divergence

Issues found while making upstream build on Ubuntu 24.04 with g++ 13.3 under
**M0.3 (local build patch)**. The fixes live in their own commit, not in the
vendor import.

### Makefile (macOS/MacPorts assumptions)

- `CXX = clang++` with `-stdlib=libc++` — switched to `g++` and dropped the
  flag (libstdc++ is default).
- `-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk` — removed.
- `INCLUDES` referenced `/opt/local/include` (MacPorts). Replaced with
  `pkg-config --cflags cairo` (Ubuntu `libcairo2-dev`); CryptoMiniSat headers
  stay at `/usr/local/include/cryptominisat5/` since CryptoMiniSat 5.14 is
  built from source and installed there.
- `LIBS = -L/usr/local/lib -rpath /usr/local/lib …` — the bare `-rpath` is a
  clang-on-mac convenience; rewritten as `-Wl,-rpath,/usr/local/lib` for GNU
  ld. CryptoMiniSat does not ship a `.pc` file, so its flags remain
  hard-coded; cairo flags use `pkg-config`.
- Link order: upstream wrote `$(CXX) $(LIBS) -o sat sat.o`; GNU ld is strict
  about putting object files **before** their libraries. Reordered to
  `$(CXX) -o sat sat.o $(LIBS)`.
- `clean: rm ${OBJECTS} ${DEPENDS}` — added `-f` and the binaries to the
  removal list so a fresh tree can `make clean` without errors.

### Genuine upstream bugs (would also bite on macOS with a stricter compiler)

- **`-std=c++17` is wrong.** `tileio.h:392` uses **templated lambdas**
  (`[]<typename Grid>(...) { … }`), a C++20 feature. Bumped to
  `-std=c++20`. Upstream presumably built with a clang that silently accepted
  the construct; g++ 13 in C++17 mode rejects it.
- **`OBJECTS` missing `isohedral.o`.** Upstream's `OBJECTS = sat.o viz.o
  surrounds.o gen.o report.o` never compiles `isohedral.cpp`, even though
  `sat.cpp` and `surrounds.cpp` (via `heesch.h`) call
  `IsohedralChecker::tilesIsohedrally`, which is *defined* there. With GNU
  ld this is an undefined-reference link error. Added `isohedral.o` to
  `OBJECTS` and to the `sat` and `surrounds` link lines.
- **`isohedral.cpp` missing `#include <map>`.** The file uses
  `map<Factor, vector<…>>` but only includes `<iostream>`, `<functional>`,
  `<iterator>`, `<set>`. libc++ pulls `<map>` in transitively from one of
  those; libstdc++ does not. Added an explicit `#include <map>`.

### heesch-forge additions (M0.5 instrumentation)

These are not bug fixes; they are new functionality bolted onto upstream so
Phase-1 speedup claims have something to be measured against. If a future
upstream sync is attempted, these need to be re-applied (or replaced if
upstream adds its own stats hooks).

- **`HeeschSolver` stats counters and `runAndAccount` helper.**
  `src/sat/src/heesch.h` gains four `uint64_t` members
  (`cum_conflicts_`, `cum_decisions_`, `cum_propagations_`, `num_sat_calls_`)
  and a private `runAndAccount(CMSat::SATSolver&)` method that calls
  `s.solve()` and adds CMS's per-call counters
  (`get_last_conflicts/decisions/propagations`) into the cumulative members.
  Every `solve()` site reachable from `HeeschSolver::solve()` — five sites
  in total, in `solve()` itself, in `iterateUntilSimplyConnected`, and in
  `checkIsohedralTiling` — was rewritten to call `runAndAccount` instead of
  bare `solve()`. Public getters expose the counters for instrumentation.
  The failsafe / `-old` path (`hasCorona`, `allCoronas`) is **not**
  instrumented; baseline runs use the default path.
- **`-stats <file>` CLI flag in `src/sat/src/sat.cpp`.** When set,
  `computeHeesch` wraps `HeeschSolver::solve` in a
  `std::chrono::steady_clock` timer and emits one JSON object per shape to
  the named file. Schema is documented in `benchmarks/baseline/README.md`.
  Without the flag, sat behaviour is byte-identical to the M0.3 build —
  the M0.4 regression confirms this on every commit that touches `src/sat/`.

### heesch-forge additions (M1.1 SAT-backend abstraction)

- **`SATSolverImpl` typedef + `cadical_backend.h` adapter.**
  `src/sat/src/heesch.h` introduces a single project-wide alias
  `using SATSolverImpl = ...;` chosen by an `#ifdef HEESCH_BACKEND_CADICAL`
  switch: the default keeps `CMSat::SATSolver`, the macro switches to
  `cadical_backend::CadicalSolver`. The adapter mimics the subset of
  CryptoMiniSat's API that `HeeschSolver` actually calls (`new_vars`,
  `add_clause(vector<CMSat::Lit>)`, `solve()` returning `CMSat::lbool`,
  `get_model()` returning `const vector<CMSat::lbool>&`, plus the three
  `get_last_*` counter accessors). `CMSat::Lit` and `CMSat::lbool` remain
  the project-wide literal and truth-value types in both builds, so the
  rest of `heesch.h` is backend-agnostic.
- **Two binaries, not a runtime flag.** `src/sat/src/Makefile` builds
  both `sat` (CryptoMiniSat) and `sat-cadical` (CaDiCaL) from the same
  sources. Compile-time selection avoids virtual dispatch in the SAT
  hot path and keeps each binary single-backend; M1.2 (portfolio with
  clause sharing) is the proper home for runtime multiplexing.
- **CaDiCaL stats gap.** CaDiCaL 1.7's public C++ API does not expose
  programmatic per-solve counters for conflicts / decisions /
  propagations (only a `statistics()` call that prints to stdout). The
  adapter therefore returns 0 from those getters, and the JSONL records
  written by `sat-cadical` carry zeros in those fields. M0.5's wall-time
  and `sat_calls` counters still work. If a future CaDiCaL release adds
  the accessors, only `cadical_backend.h` changes.

### heesch-forge additions (M1.2a Kissat backend)

- **`kissat_backend.h` adapter.** Same pattern as `cadical_backend.h`:
  presents the CMSat::SATSolver subset HeeschSolver uses, lets
  `-DHEESCH_BACKEND_KISSAT` switch the typedef in `heesch.h`. Wraps
  Kissat's C API (`kissat_init` / `kissat_add` / `kissat_solve` /
  `kissat_value` / `kissat_release`) and translates between
  CMSat::Lit/lbool and Kissat's DIMACS-int form. Sets
  `kissat_set_option(s, "quiet", 1)` to suppress Kissat's stderr
  chatter so JSONL log capture stays clean.
- **Non-incremental constraint.** Kissat does not support adding
  clauses after `kissat_solve()` returns. `HeeschSolver::iterate-
  UntilSimplyConnected` adds clauses inside a `while (solver.solve()
  == l_True)` loop, which would be undefined on a Kissat instance.
  The adapter therefore buffers every `add_clause` in a local
  `vector<vector<int>>` and rebuilds a fresh `kissat*` on every
  `solve()` call — feeding **all** prior clauses each time. Cost is
  O(num_clauses) per solve. For non-incremental call sites
  (`cur_solver`, `iso_solver`, the two `final_solver`s) the rebuild
  happens at most once per solver lifetime, so the overhead is
  negligible. For the n = 11 walkback loop the cost compounds and
  becomes infeasible (>14 min per shape observed); the M1.2a baseline
  therefore restricts to sizes 7, 8, 12. Full architectural rationale
  in `benchmarks/baseline/results/m1.2a-comparison.md`.
- **Kissat stats gap.** Same as CaDiCaL: Kissat's public C API
  (`kissat_print_statistics()` only) exposes no programmatic
  per-solve counters. `sat-kissat` writes zeros into the
  conflict / decision / propagation fields of the JSONL stats stream.
- **Two-way movability.** `KissatSolver` deletes copy and defines
  noexcept move so it can live in `std::vector` /
  `std::unique_ptr` containers (`HeeschSolver::solve()` keeps a
  `vector<unique_ptr<SATSolverImpl>> past_solvers` for walkback;
  same lifetime model as CMSat / CaDiCaL).

### heesch-forge additions (M1.3 dumping backend)

- **`dumping_backend.h`.** Wraps a real `CMSat::SATSolver`, intercepts
  every `add_clause` to buffer DIMACS-int form, and on each `solve()`
  writes the current CNF to a file under `$HEESCH_DUMP_DIR` before
  forwarding to CMSat for the actual solve. Filenames embed an atomic
  per-process instance id and a per-instance call counter so every
  SAT call heesch-sat issues becomes one DIMACS file. Selected via
  `-DHEESCH_BACKEND_DUMP`, producing the `sat-dump` binary. The
  regression suite still passes against `sat-dump` because the
  underlying CMSat solver is the one actually deciding satisfiability.
- **Used only by the M1.3 BreakID probe.** Documented in
  `benchmarks/breakid/README.md`. The probe exposed a clean
  bimodal regime (fresh formulas: tiny gain, no Phase-1 lift;
  walkback formulas: 60 s+ BreakID timeouts with no symmetries
  found) that closed M1.3 as a negative result. The dumping backend
  remains useful infrastructure for any future probe that wants
  per-call CNFs (encoding sweeps under M1.4, hybrid handoff under
  M2.3, etc.).

## Build dependencies (upstream targets `gen sat viz surrounds report`)

- C++20 compiler (g++ ≥ 10 or clang++ ≥ 13). Upstream Makefile says C++17
  but `tileio.h` uses templated lambdas — see "Genuine upstream bugs" below.
- **CryptoMiniSat 5** — must be built from source from
  <https://github.com/msoos/cryptominisat> ; no Ubuntu package is recent enough.
- **Cairo** — `libcairo2-dev` on Ubuntu; only required for the optional `viz`
  PDF-rendering tool. The other four binaries do not need it.
- GNU make.

## Re-syncing with upstream (future, optional)

If a future upstream change is worth pulling:

```bash
git clone --depth 1 https://github.com/isohedral/heesch-sat.git /tmp/heesch-sat-new
NEW_SHA=$(git -C /tmp/heesch-sat-new rev-parse HEAD)
diff -ruN src/sat /tmp/heesch-sat-new   # review divergence first
# Cherry-pick what's needed; do NOT blanket-overwrite, our patches will be lost.
```

Update this file's SHA / date row in the same commit.
