"""Deep cross-entropy training loop (Wagner 2021).

Per M4.1 + M4.2, this module implements the M4.3 deliverable:

    1. Sample N trajectories from the current policy.
    2. Score by terminal reward.
    3. Take the top-K elites (ties broken by shorter length).
    4. Train the policy to imitate elite (state, action) pairs.
    5. Repeat for `n_generations`.

The implementation deliberately follows Wagner (arXiv:2104.14516) §3
in spirit -- each generation is a self-contained supervised-learning
phase. We do NOT do PPO / actor-critic / value-learning; the M4.3
contract is plain CEM, with the policy network's add-logit + stop-
logit output the only learned quantity.

Throughput note (cf. M4.2 results): at n_max = 12 the random-policy
runner was 8 traj/s on this hardware, dominated by sat subprocess
overhead. With a learned policy that adds GNN forward-pass cost,
expect ~5-7 traj/s. M4.3's pilot run sizes (n_generations * batch_size)
must be chosen with that budget in mind.
"""

from __future__ import annotations

import random
import time
from dataclasses import dataclass
from typing import Callable, Sequence

import torch
import torch.nn.functional as F
from torch import Tensor, optim

from .canonical import Cells, canonical
from .env import HeeschEnv, RewardOracle, STOP_ACTION
from .policy import HeeschPolicy, Observation, encode_observation
from .trajectory import Step, Trajectory, augmented_step, select_elites


def _action_logits(
    obs: Observation, policy: HeeschPolicy, stop_legal: bool
) -> Tensor:
    """Run the policy and return logits over (boundaries + Stop).

    If `stop_legal` is False, the Stop logit is suppressed via
    -inf masking.
    """
    add_logits, stop_logit = policy(obs)
    stop_view = stop_logit.unsqueeze(0)
    if not stop_legal:
        stop_view = torch.full_like(stop_view, float("-inf"))
    return torch.cat([add_logits, stop_view], dim=0)


def sample_trajectory(
    env: HeeschEnv,
    policy: HeeschPolicy,
    rng: random.Random | None = None,  # accepted for API symmetry; unused
) -> Trajectory:
    """Run one episode under `policy`, returning the trajectory."""
    del rng  # action sampling uses torch.multinomial; env handles its own rng.
    env.reset()
    steps: list[Step] = []
    final_reward = 0.0
    while True:
        legal = env.legal_actions()
        if not legal:
            break
        state = env.state.cells
        # Detect whether Stop is legal (the env exposes it as the
        # last entry conventionally; check by membership instead).
        stop_legal = STOP_ACTION in legal
        adds = [a for a in legal if a.add is not None]
        boundaries = tuple(a.add for a in adds)  # type: ignore[misc]

        obs = encode_observation(state, n_max=env.n_max)
        # Force the observation's boundaries to match the env's --
        # the env produces a sorted list; encode_observation does too,
        # so this should be identical, but assert defensively.
        assert obs.boundaries == boundaries, (
            f"boundary mismatch: env {boundaries!r} vs obs {obs.boundaries!r}"
        )

        with torch.no_grad():
            logits = _action_logits(obs, policy, stop_legal=stop_legal)
            probs = F.softmax(logits, dim=0)
            idx = int(torch.multinomial(probs, num_samples=1).item())
            log_prob_t = F.log_softmax(logits, dim=0)[idx]
            log_prob = float(log_prob_t.item())

        if idx == len(boundaries):
            action = STOP_ACTION
        else:
            action = adds[idx]

        res = env.step(action)
        steps.append(Step(state=state, action=action, log_prob=log_prob,
                          reward=res.reward if res.done else 0.0))
        if res.done:
            final_reward = res.reward
            break

    return Trajectory(steps=tuple(steps), terminal_reward=final_reward)


@dataclass
class GenerationStats:
    generation: int
    n_trajectories: int
    elapsed_s: float
    reward_mean: float
    reward_max: float
    elite_threshold: float
    elite_reward_mean: float
    loss: float
    unique_canonical_terminals: int


@dataclass
class TrainingResult:
    generations: list[GenerationStats]
    discovered_high_reward: dict[float, list[Cells]]   # reward -> shapes seen


def _imitation_loss(
    elites: Sequence[Trajectory],
    policy: HeeschPolicy,
    n_max: int,
    rng: random.Random,
) -> Tensor:
    """Sum of negative-log-likelihood of elite actions under the
    *current* policy. D4-augmented at sample time per M4.1 §6."""
    losses: list[Tensor] = []
    for traj in elites:
        for step in traj.steps:
            aug = augmented_step(step, rng)
            obs = encode_observation(aug.state, n_max=n_max)
            stop_legal = len(aug.state) >= 1
            # Re-align the augmented action against this observation's
            # boundaries. Find the index of aug.action in obs.boundaries
            # (or the Stop slot at the end).
            if aug.action.add is None:
                target_idx = len(obs.boundaries)
            else:
                if aug.action.add not in obs.boundaries:
                    # Augmentation produced an action not in the
                    # current boundary list -- shouldn't happen if
                    # canonicalisation is consistent, but skip
                    # defensively.
                    continue
                target_idx = obs.boundaries.index(aug.action.add)
            logits = _action_logits(obs, policy, stop_legal=stop_legal)
            log_probs = F.log_softmax(logits, dim=0)
            losses.append(-log_probs[target_idx])
    if not losses:
        # Edge case: zero training signal this generation.
        return torch.zeros(1, requires_grad=True).sum()
    return torch.stack(losses).mean()


def train(
    *,
    n_generations: int,
    batch_size: int,
    elite_fraction: float,
    n_max: int,
    n_min: int,
    oracle: RewardOracle,
    policy: HeeschPolicy | None = None,
    learning_rate: float = 1e-3,
    seed: int = 0,
    on_generation: Callable[[GenerationStats], None] | None = None,
) -> tuple[HeeschPolicy, TrainingResult]:
    """Run the CEM loop. Returns the trained policy and a summary."""
    if policy is None:
        policy = HeeschPolicy(hidden_dim=64, num_layers=3)
    rng = random.Random(seed)
    torch.manual_seed(seed)
    optimiser = optim.Adam(policy.parameters(), lr=learning_rate)
    env = HeeschEnv(oracle=oracle, n_max=n_max, n_min=n_min, rng=rng)

    gens: list[GenerationStats] = []
    discovered: dict[float, list[Cells]] = {}

    for g in range(n_generations):
        t0 = time.perf_counter()
        trajectories = [
            sample_trajectory(env, policy, rng) for _ in range(batch_size)
        ]
        rewards = [t.terminal_reward for t in trajectories]
        rmax = max(rewards) if rewards else 0.0
        rmean = sum(rewards) / max(len(rewards), 1)

        for traj in trajectories:
            r = traj.terminal_reward
            if r >= 1.0:
                discovered.setdefault(r, []).append(canonical(traj.steps[-1].state |
                                                              ({traj.steps[-1].action.add}
                                                               if traj.steps[-1].action.add is not None
                                                               else set())))

        elites = select_elites(trajectories, elite_fraction)
        elite_rewards = [t.terminal_reward for t in elites]
        elite_threshold = min(elite_rewards) if elite_rewards else 0.0
        elite_mean = sum(elite_rewards) / max(len(elite_rewards), 1)

        loss = _imitation_loss(elites, policy, n_max=n_max, rng=rng)
        optimiser.zero_grad()
        loss.backward()
        optimiser.step()

        terminals = {canonical(t.steps[-1].state) for t in trajectories}
        elapsed = time.perf_counter() - t0
        stats = GenerationStats(
            generation=g,
            n_trajectories=len(trajectories),
            elapsed_s=round(elapsed, 3),
            reward_mean=round(rmean, 4),
            reward_max=rmax,
            elite_threshold=elite_threshold,
            elite_reward_mean=round(elite_mean, 4),
            loss=float(loss.item()),
            unique_canonical_terminals=len(terminals),
        )
        gens.append(stats)
        if on_generation is not None:
            on_generation(stats)

    return policy, TrainingResult(generations=gens, discovered_high_reward=discovered)
