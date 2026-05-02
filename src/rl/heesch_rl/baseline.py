"""Random-policy baseline runner.

Per M4.1 §7's validation harness: sample N trajectories under a
uniform-random policy and report the reward distribution. This is
the "random-policy baseline" deliverable of M4.2 -- it tells us
what fraction of randomly-sampled polyominoes hit each H_c value,
which is the floor any trained policy must beat.

Usage:
    python -m heesch_rl.baseline --n-max 8 --trajectories 1000
"""

from __future__ import annotations

import argparse
import json
import random
import sys
import time
from collections import Counter
from typing import Sequence

from .env import HeeschEnv
from .oracle import default_oracle


def run_random_baseline(
    n_max: int,
    n_trajectories: int,
    n_min: int = 4,
    seed: int = 0,
    verbose: bool = True,
) -> dict:
    rng = random.Random(seed)
    oracle = default_oracle()
    env = HeeschEnv(oracle=oracle, n_max=n_max, n_min=n_min, rng=rng)

    rewards: list[float] = []
    lengths: list[int] = []
    t0 = time.perf_counter()
    for i in range(n_trajectories):
        env.reset()
        while True:
            actions = env.legal_actions()
            if not actions:
                break
            action = rng.choice(actions)
            res = env.step(action)
            if res.done:
                rewards.append(res.reward)
                lengths.append(len(res.state.cells))
                break
        if verbose and (i + 1) % max(1, n_trajectories // 10) == 0:
            elapsed = time.perf_counter() - t0
            print(
                f"  [{i+1:>5d}/{n_trajectories}]  elapsed {elapsed:6.1f}s  "
                f"oracle-cache {getattr(oracle, 'cache_size', 0)}",
                file=sys.stderr,
            )

    elapsed = time.perf_counter() - t0
    counter = Counter(rewards)
    summary = {
        "n_max": n_max,
        "n_min": n_min,
        "n_trajectories": n_trajectories,
        "elapsed_s": round(elapsed, 3),
        "trajectories_per_s": round(n_trajectories / max(elapsed, 1e-9), 2),
        "oracle_cache_size": getattr(oracle, "cache_size", 0),
        "reward_distribution": {
            str(k): counter[k] for k in sorted(counter)
        },
        "length_mean": (sum(lengths) / len(lengths)) if lengths else 0.0,
        "length_min": min(lengths) if lengths else 0,
        "length_max": max(lengths) if lengths else 0,
    }
    return summary


def _parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Random-policy MDP baseline.")
    p.add_argument("--n-max", type=int, default=8)
    p.add_argument("--n-min", type=int, default=4)
    p.add_argument("--trajectories", type=int, default=1000)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--quiet", action="store_true")
    return p.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = _parse_args(argv)
    summary = run_random_baseline(
        n_max=args.n_max,
        n_trajectories=args.trajectories,
        n_min=args.n_min,
        seed=args.seed,
        verbose=not args.quiet,
    )
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
