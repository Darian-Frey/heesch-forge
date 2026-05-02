"""CLI entry point for the M4.3 training loop.

Usage:
    uv run python -m heesch_rl.training_runner \\
        --n-max 10 --batch-size 200 --generations 30 \\
        --elite-fraction 0.1
"""

from __future__ import annotations

import argparse
import json
import sys
from typing import Sequence

from .oracle import default_oracle
from .trainer import train


def _parse_args(argv: Sequence[str] | None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="CEM training loop (M4.3).")
    p.add_argument("--n-max", type=int, default=8)
    p.add_argument("--n-min", type=int, default=4)
    p.add_argument("--batch-size", type=int, default=200)
    p.add_argument("--generations", type=int, default=20)
    p.add_argument("--elite-fraction", type=float, default=0.1)
    p.add_argument("--learning-rate", type=float, default=1e-3)
    p.add_argument("--seed", type=int, default=0)
    return p.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = _parse_args(argv)
    oracle = default_oracle()

    def log(stats):
        print(
            f"  gen {stats.generation:>3d}  "
            f"N={stats.n_trajectories:>4d}  "
            f"elapsed={stats.elapsed_s:>5.1f}s  "
            f"rmean={stats.reward_mean:>6.3f}  rmax={stats.reward_max:>4.1f}  "
            f"elite_mean={stats.elite_reward_mean:>5.3f}  "
            f"loss={stats.loss:>7.4f}  "
            f"unique_terminals={stats.unique_canonical_terminals}",
            file=sys.stderr,
        )

    policy, result = train(
        n_generations=args.generations,
        batch_size=args.batch_size,
        elite_fraction=args.elite_fraction,
        n_max=args.n_max,
        n_min=args.n_min,
        oracle=oracle,
        learning_rate=args.learning_rate,
        seed=args.seed,
        on_generation=log,
    )

    # JSON summary on stdout.
    summary = {
        "n_max": args.n_max,
        "n_min": args.n_min,
        "batch_size": args.batch_size,
        "generations": args.generations,
        "elite_fraction": args.elite_fraction,
        "learning_rate": args.learning_rate,
        "seed": args.seed,
        "oracle_cache_size": getattr(oracle, "cache_size", 0),
        "per_generation": [
            {
                "g": s.generation,
                "elapsed_s": s.elapsed_s,
                "rmean": s.reward_mean,
                "rmax": s.reward_max,
                "elite_mean": s.elite_reward_mean,
                "loss": round(s.loss, 4),
                "unique_terminals": s.unique_canonical_terminals,
            }
            for s in result.generations
        ],
        "discovered_counts": {
            str(k): len(v) for k, v in sorted(result.discovered_high_reward.items())
        },
    }
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
