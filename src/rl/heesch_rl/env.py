"""MDP environment per benchmarks/rl/M4.1-mdp.md.

State: D4-canonical polyomino + `stopped` flag.
Action: Add(x, y) for a 4-cardinal boundary cell, or Stop.
Reward: sparse, terminal, -1/-0.5/H_c (oracle-driven; oracle
        injection is the caller's responsibility -- see baseline.py).
Termination: Stop, |cells| == n_max, or invalid (hole / disconnect).
"""

from __future__ import annotations

import random
from collections import deque
from dataclasses import dataclass
from typing import Callable

from .canonical import Cells, canonical


# A reward oracle takes a canonical polyomino and returns its H_c.
# The pilot wires this to a sat subprocess (see oracle.py); tests
# inject pure-Python stubs.
RewardOracle = Callable[[Cells], int]


_DIRS_4 = ((1, 0), (-1, 0), (0, 1), (0, -1))


def boundary_cells(cells: Cells) -> tuple[tuple[int, int], ...]:
    """Empty 4-cardinal neighbours of any occupied cell."""
    if not cells:
        return ((0, 0),)  # the empty board permits placing a single seed
    seen: set[tuple[int, int]] = set()
    out: list[tuple[int, int]] = []
    for x, y in cells:
        for dx, dy in _DIRS_4:
            n = (x + dx, y + dy)
            if n not in cells and n not in seen:
                seen.add(n)
                out.append(n)
    out.sort()
    return tuple(out)


def is_connected(cells: Cells) -> bool:
    if len(cells) <= 1:
        return True
    start = next(iter(cells))
    seen: set[tuple[int, int]] = {start}
    queue: deque[tuple[int, int]] = deque([start])
    while queue:
        x, y = queue.popleft()
        for dx, dy in _DIRS_4:
            n = (x + dx, y + dy)
            if n in cells and n not in seen:
                seen.add(n)
                queue.append(n)
    return len(seen) == len(cells)


def has_hole(cells: Cells) -> bool:
    """True iff the bounding-box exterior cannot reach every empty
    cell adjacent to `cells` via 4-cardinal moves through empty
    cells.

    Concrete recipe: flood-fill from outside the bounding box (via
    a ring one step beyond it); any empty cell adjacent to `cells`
    not reached by the flood is a hole.
    """
    if not cells:
        return False
    xs = [x for x, _ in cells]
    ys = [y for _, y in cells]
    x_min, x_max = min(xs) - 1, max(xs) + 1
    y_min, y_max = min(ys) - 1, max(ys) + 1

    # Flood-fill through empty cells, starting from the outer ring.
    seen: set[tuple[int, int]] = set()
    queue: deque[tuple[int, int]] = deque()
    for x in range(x_min, x_max + 1):
        for y in (y_min, y_max):
            seen.add((x, y))
            queue.append((x, y))
    for y in range(y_min + 1, y_max):
        for x in (x_min, x_max):
            seen.add((x, y))
            queue.append((x, y))
    while queue:
        x, y = queue.popleft()
        for dx, dy in _DIRS_4:
            n = (x + dx, y + dy)
            if (
                x_min <= n[0] <= x_max
                and y_min <= n[1] <= y_max
                and n not in cells
                and n not in seen
            ):
                seen.add(n)
                queue.append(n)

    # Any empty cell inside the bounding box not in `seen` is a hole.
    for x in range(x_min + 1, x_max):
        for y in range(y_min + 1, y_max):
            if (x, y) not in cells and (x, y) not in seen:
                return True
    return False


@dataclass(frozen=True)
class MDPState:
    cells: Cells          # always D4-canonical
    stopped: bool = False


@dataclass(frozen=True)
class StepResult:
    state: MDPState
    reward: float         # 0 unless terminal; terminal reward as in M4.1 §4
    done: bool


@dataclass(frozen=True)
class Action:
    """Either an add at a boundary position, or stop."""
    add: tuple[int, int] | None  # None -> Stop


STOP_ACTION = Action(add=None)


# Reward constants per M4.1 §4.1.
_REWARD_INVALID = -0.5
_REWARD_UNDER_FLOOR = -1.0


class HeeschEnv:
    """The single-polyomino MDP. One environment per RL trajectory.

    Construction parameters per M4.1 §5:
        n_max:    upper bound on |cells|; episode ends if reached.
        n_min:    floor below which Stop returns -1.
        oracle:   function from canonical Cells to H_c; injected so
                  tests can stub it without spawning sat subprocesses.

    Convention: positions in `Action.add` are in the *post-canonical*
    coordinate frame returned by the previous step's observation.
    Internally the env re-canonicalises after each Add.
    """

    def __init__(
        self,
        oracle: RewardOracle,
        n_max: int = 12,
        n_min: int = 4,
        rng: random.Random | None = None,
    ) -> None:
        if n_min < 1:
            raise ValueError("n_min must be >= 1")
        if n_max < n_min:
            raise ValueError("n_max must be >= n_min")
        self.oracle = oracle
        self.n_max = n_max
        self.n_min = n_min
        self.rng = rng or random.Random()
        self._state: MDPState | None = None

    @property
    def state(self) -> MDPState:
        if self._state is None:
            raise RuntimeError("call reset() before accessing state")
        return self._state

    def reset(self) -> MDPState:
        """Start with a single seed cell at (0, 0). Always canonical."""
        cells = frozenset({(0, 0)})
        self._state = MDPState(cells=canonical(cells), stopped=False)
        return self._state

    def legal_actions(self) -> tuple[Action, ...]:
        """Per M4.1 §2.3 / §3.1: boundary adds, plus Stop if above floor."""
        s = self.state
        if s.stopped:
            return ()
        out: list[Action] = [Action(add=p) for p in boundary_cells(s.cells)]
        if len(s.cells) >= self.n_min:
            out.append(STOP_ACTION)
        return tuple(out)

    def step(self, action: Action) -> StepResult:
        s = self.state
        if s.stopped:
            raise RuntimeError("episode already terminated")

        # --- Stop action: terminal, compute reward.
        if action.add is None:
            if len(s.cells) < self.n_min:
                # Disallowed by legal_actions(), but defensive: also
                # punish if a buggy policy bypasses it.
                term = MDPState(cells=s.cells, stopped=True)
                self._state = term
                return StepResult(term, _REWARD_UNDER_FLOOR, True)
            return self._terminal(s.cells)

        # --- Add action.
        new_cells_raw = s.cells | {action.add}
        if has_hole(new_cells_raw) or not is_connected(new_cells_raw):
            term = MDPState(cells=s.cells, stopped=True)
            self._state = term
            return StepResult(term, _REWARD_INVALID, True)

        new_cells = canonical(new_cells_raw)
        if len(new_cells) >= self.n_max:
            return self._terminal(new_cells)

        ns = MDPState(cells=new_cells, stopped=False)
        self._state = ns
        return StepResult(ns, 0.0, False)

    def _terminal(self, cells: Cells) -> StepResult:
        canon = canonical(cells)
        # By construction (we only enter here from `step`) cells is
        # connected and hole-free. The oracle returns H_c for it.
        hc = self.oracle(canon)
        term = MDPState(cells=canon, stopped=True)
        self._state = term
        return StepResult(term, float(hc), True)
