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
