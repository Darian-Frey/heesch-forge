"""Trajectory storage + D4 replay augmentation per M4.1 §6.

A Trajectory is a sequence of (canonical_state, action, log_prob)
triples plus a terminal reward. Trajectories are stored in canonical
form; at training-batch sample time, an augmenter applies one of
the eight D4 elements to each (state, action) pair.

Note that the action's `add` coordinate is in the post-canonical
frame of its source state. After we apply a D4 element g to that
state's cells, we must also apply g to the action's add coordinate
so the (state, action) pair stays consistent. Reward is D4-invariant
so we don't touch it.
"""

from __future__ import annotations

import random
from dataclasses import dataclass
from typing import Sequence

from .canonical import Cells, _D4_MATRICES, _apply, _translate_to_origin
from .env import Action


@dataclass(frozen=True)
class Step:
    """One transition in a trajectory.

    `state` is the canonical state observed BEFORE this step. `action`
    is the action selected. `log_prob` is the log-probability the
    sampling policy assigned to that action. `reward` is non-zero
    only on the terminal step; intermediate rewards are 0.0 per the
    M4.1 sparse-reward design.
    """
    state: Cells
    action: Action
    log_prob: float
    reward: float


@dataclass(frozen=True)
class Trajectory:
    """Full episode: ordered Steps + terminal reward."""
    steps: tuple[Step, ...]
    terminal_reward: float

    @property
    def length(self) -> int:
        return len(self.steps)


def _apply_to_action(matrix, action: Action) -> Action:
    """Apply a D4 matrix to an action's add-position."""
    if action.add is None:
        return action
    a, b, c, d = matrix
    x, y = action.add
    return Action(add=(a * x + b * y, c * x + d * y))


def augmented_step(step: Step, rng: random.Random) -> Step:
    """Sample one of the eight D4 elements and apply it to (state, action).

    The reward and log_prob are D4-invariant so they pass through.
    The state is re-translated to the lex-smallest cell at origin
    after the rotation/reflection so it stays in a stable frame for
    the GNN encoder.
    """
    matrix = rng.choice(_D4_MATRICES)
    rotated_cells = _translate_to_origin(_apply(matrix, step.state))
    # The action coordinate also needs the rotation, but it is
    # expressed in the *pre-translation* frame. We translate after
    # applying the matrix; the translation offset is computed from
    # the rotated cells. Concretely:
    if step.action.add is not None:
        a, b, c, d = matrix
        ax, ay = step.action.add
        # apply matrix to (ax, ay)
        rx, ry = a * ax + b * ay, c * ax + d * ay
        # compute the same translation offset that _translate_to_origin
        # applied to the cells:
        applied = _apply(matrix, step.state)
        if applied:
            min_x = min(x for x, _ in applied)
            min_y = min(y for x, y in applied if x == min_x)
        else:
            min_x = min_y = 0
        new_action = Action(add=(rx - min_x, ry - min_y))
    else:
        new_action = step.action
    return Step(
        state=rotated_cells,
        action=new_action,
        log_prob=step.log_prob,
        reward=step.reward,
    )


def select_elites(
    trajectories: Sequence[Trajectory],
    elite_fraction: float,
) -> list[Trajectory]:
    """Top-K trajectories by terminal reward, ties broken by length
    (shorter preferred -- discourages padding).
    """
    if not 0.0 < elite_fraction <= 1.0:
        raise ValueError("elite_fraction must be in (0, 1]")
    n = len(trajectories)
    k = max(1, int(round(n * elite_fraction)))
    ranked = sorted(
        trajectories,
        key=lambda t: (-t.terminal_reward, t.length),
    )
    return ranked[:k]
