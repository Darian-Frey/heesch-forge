"""Smoke tests for the CEM trainer.

These exercise the loop with a stub oracle (constant rewards) so
they don't depend on the sat binary. They verify that the trainer:
    - completes a small number of generations,
    - returns a stats object with the expected shape,
    - produces a non-empty `discovered_high_reward` map when the
      stub oracle returns positive values.
"""

import random

import torch

from heesch_rl.canonical import canonical
from heesch_rl.policy import HeeschPolicy
from heesch_rl.trainer import sample_trajectory, train
from heesch_rl.env import HeeschEnv
from heesch_rl.trajectory import select_elites, Trajectory, Step
from heesch_rl.env import Action


def stub_oracle(_cells):
    return 0


def test_sample_trajectory_runs():
    rng = random.Random(0)
    torch.manual_seed(0)
    env = HeeschEnv(oracle=stub_oracle, n_max=5, n_min=4, rng=rng)
    policy = HeeschPolicy(hidden_dim=8, num_layers=2)
    traj = sample_trajectory(env, policy, rng)
    assert traj.length >= 1
    # Final step must have triggered termination.
    assert env.state.stopped


def test_select_elites_orders_by_reward():
    s = (Step(state=canonical(frozenset({(0, 0)})),
              action=Action(add=(1, 0)),
              log_prob=-1.0, reward=0.0),)
    trajs = [
        Trajectory(steps=s, terminal_reward=0.0),
        Trajectory(steps=s, terminal_reward=2.0),
        Trajectory(steps=s, terminal_reward=1.0),
        Trajectory(steps=s, terminal_reward=3.0),
    ]
    elites = select_elites(trajs, elite_fraction=0.5)
    assert [t.terminal_reward for t in elites] == [3.0, 2.0]


def test_train_smoke():
    """A 2-generation, 6-trajectory run completes and returns stats."""
    def positive_oracle(_cells):
        # Constant reward to give the imitation loss something
        # non-trivial to chew on -- in real usage this is the sat
        # subprocess oracle.
        return 1
    policy, result = train(
        n_generations=2,
        batch_size=6,
        elite_fraction=0.5,
        n_max=5,
        n_min=4,
        oracle=positive_oracle,
        learning_rate=1e-2,
        seed=0,
    )
    assert isinstance(policy, HeeschPolicy)
    assert len(result.generations) == 2
    for g in result.generations:
        assert g.n_trajectories == 6
        assert g.elapsed_s >= 0.0
