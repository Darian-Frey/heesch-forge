"""Sat-subprocess H_c oracle for the RL pilot.

`SatOracle` wraps the patched `src/sat/src/sat` binary. Calling it
on a canonical polyomino returns its H_c via a single subprocess.

Performance note: each call costs ~10ms of subprocess overhead plus
the sat solve itself. For the random-policy baseline at small n
(M4.2), one call per trajectory is fine. For the training loop
(M4.3+) this oracle should be replaced with a batched / pooled
version, but that's an optimisation, not a correctness concern.

A `MemoOracle` wrapper memoises by canonical cells; H_c is invariant
under D4 / translation, so once we've seen a shape we never re-solve.
"""

from __future__ import annotations

import re
import subprocess
import threading
from pathlib import Path
from typing import Callable

from .canonical import Cells, canonical


_DEFAULT_SAT_BIN = (
    Path(__file__).resolve().parents[2] / "sat" / "src" / "sat"
)
# `__file__` lives at src/rl/heesch_rl/oracle.py, so parents[2] is
# the repo's `src/` directory. The sat binary sits at src/sat/src/sat.


_SAT_RESPONSE_RE = re.compile(r"^~\s+(\d+)\s+(\d+)")


def _format_shape(cells: Cells) -> str:
    """Heesch-sat input format for a polyomino: `O? <coords>`."""
    parts = ["O?"]
    for x, y in sorted(cells):
        parts.append(str(x))
        parts.append(str(y))
    return " ".join(parts)


class SatOracle:
    """Single-call sat subprocess oracle. Thread-safe.

    Defaults match the M3.x sweep flags (`-isohedral -hh -maxlevel 7`).
    `-isohedral` is essential: without it sat does not run upstream's
    isohedral-tiler check, and tilers come back as `! 0` (inconclusive)
    or hang inside the SAT solve trying to surround a shape that
    doesn't need surrounding.

    `timeout` (seconds, default 30) caps individual sat calls; on
    timeout the call returns 0 (the lowest informative reward). The
    baseline runner reports timeouts via the `timeouts` counter so
    M4.5 can quantify how often this happens.
    """

    def __init__(
        self,
        sat_bin: Path | str | None = None,
        maxlevel: int = 7,
        check_isohedral: bool = True,
        timeout: float = 30.0,
    ) -> None:
        self.sat_bin = Path(sat_bin) if sat_bin else _DEFAULT_SAT_BIN
        if not self.sat_bin.exists():
            raise FileNotFoundError(
                f"sat binary not found at {self.sat_bin}; build it via "
                f"`(cd src/sat/src && make sat)`"
            )
        self.maxlevel = maxlevel
        self.check_isohedral = check_isohedral
        self.timeout = timeout
        self.timeouts = 0
        self._lock = threading.Lock()

    def __call__(self, cells: Cells) -> int:
        cmd = [
            str(self.sat_bin),
            "-hh",
            "-maxlevel",
            str(self.maxlevel),
        ]
        if self.check_isohedral:
            cmd.append("-isohedral")
        shape_line = _format_shape(canonical(cells)) + "\n"
        try:
            with self._lock:
                res = subprocess.run(
                    cmd,
                    input=shape_line,
                    capture_output=True,
                    text=True,
                    timeout=self.timeout,
                )
        except subprocess.TimeoutExpired:
            self.timeouts += 1
            return 0
        if res.returncode != 0:
            raise RuntimeError(
                f"sat exited {res.returncode}; stderr: {res.stderr.strip()!r}"
            )
        # sat writes one classification per shape; parse the H_c.
        # Output formats per src/sat/src/tileio.h:
        #   "~ <Hc> <Hh> ..."   non-tiler with explicit Hc
        #   "I <transitivity>"  isohedral (treat as tiler -> reward 0)
        #   "! <Hc>"            inconclusive (treat as the partial Hc)
        for line in res.stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            if line.startswith("~"):
                m = _SAT_RESPONSE_RE.match(line)
                if not m:
                    raise RuntimeError(f"unparseable sat output: {line!r}")
                return int(m.group(1))
            if line.startswith("I"):
                # Isohedral tilers have effectively unbounded H but the
                # MDP reward is "highest finite Hc you can prove"; for
                # the pilot we treat them as 0 (boring tilers).
                return 0
            if line.startswith("!"):
                parts = line.split()
                if len(parts) >= 2 and parts[1].isdigit():
                    return int(parts[1])
                return self.maxlevel
        raise RuntimeError(f"no classification line in sat output: {res.stdout!r}")


class MemoOracle:
    """Memoise an underlying oracle by canonical cells.

    The MDP env passes canonical cells, but defensive normalisation
    here means the cache also works if a caller forgets.
    """

    def __init__(self, base: Callable[[Cells], int]) -> None:
        self._base = base
        self._cache: dict[Cells, int] = {}
        self._lock = threading.Lock()

    def __call__(self, cells: Cells) -> int:
        key = canonical(cells)
        with self._lock:
            hit = self._cache.get(key)
            if hit is not None:
                return hit
        # Compute outside the lock to allow concurrent solves.
        v = self._base(key)
        with self._lock:
            self._cache.setdefault(key, v)
        return v

    @property
    def cache_size(self) -> int:
        return len(self._cache)


def default_oracle(maxlevel: int = 7) -> MemoOracle:
    """Memoised SatOracle with project defaults. Use this from baselines."""
    return MemoOracle(SatOracle(maxlevel=maxlevel))
