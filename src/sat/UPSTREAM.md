# Upstream provenance

This directory is a **plain-copy vendor** of `isohedral/heesch-sat`. It is not a
git submodule and not a GitHub fork. Divergence from upstream is expected and
will not be merged back; see `CLAUDE.md` and `PROPOSAL.md` §5 (Layer 1) for the
modernisation plan.

| Field | Value |
|-------|-------|
| Upstream URL | https://github.com/isohedral/heesch-sat |
| Upstream commit SHA | `1adb37204013b96b62b954a616265e49d7cf21ad` |
| Upstream commit date | 2026-02-17 |
| Vendored on | 2026-04-29 |
| Vendor method | `git clone --depth 1` then `rm -rf .git`; see `CLAUDE.md` |
| Upstream license | BSD-3-Clause (Kaplan 2021), preserved as `src/sat/LICENSE` |
| heesch-forge license | BSD-3-Clause (compatible) |

## Known upstream-vs-Linux divergence

The upstream `src/sat/src/Makefile` is written for macOS + MacPorts and will
not build on Linux without patching. Issues, all to be addressed under **M0.3
(local build patch)**, not as a vendor change:

- `CXX = clang++` with `-stdlib=libc++` — drop `-stdlib` on Linux (use the
  default libstdc++) or install libc++ explicitly.
- `-isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk` — macOS only,
  must be removed on Linux.
- `INCLUDES` points at `/opt/local/include` (MacPorts) and
  `/usr/local/include/cryptominisat5`; on Ubuntu these become
  `/usr/include/cryptominisat5` (if installed from source to default prefix)
  and `/usr/include/cairo` (Ubuntu `libcairo2-dev`).
- `LIBS = -L/usr/local/lib -rpath /usr/local/lib -lcryptominisat5` — the bare
  `-rpath` flag is a clang-on-mac convenience; with GNU ld it must be
  `-Wl,-rpath,/usr/local/lib`.

The Linux-side patch will live in a separate commit (`m0.3-…`) so the vendor
import here remains a clean snapshot.

## Build dependencies (upstream targets `gen sat viz surrounds report`)

- C++17 compiler (g++ ≥ 9 or clang++ ≥ 10).
- **CryptoMiniSat 5** — must be built from source from
  https://github.com/msoos/cryptominisat ; no Ubuntu package is recent enough.
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
