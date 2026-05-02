# src/rl — Phase-4 RL pilot

Implementation of the MDP defined in
[`benchmarks/rl/M4.1-mdp.md`](../../benchmarks/rl/M4.1-mdp.md).

## Layout

```text
heesch_rl/
    canonical.py    D4 canonicalisation of polyominoes.
    env.py          MDP environment (state, action, step, termination).
    oracle.py       sat-subprocess H_c oracle (memoised).
    policy.py       GraphSAGE-based GNN policy network.
    baseline.py     Random-policy baseline runner (M4.2 deliverable).
tests/
    test_canonical.py
    test_env.py
    test_oracle.py  (skips if src/sat/src/sat is not built)
```

## Setup

```bash
cd src/rl
uv sync          # installs torch, torch-geometric, pytest
```

## Running the random-policy baseline

```bash
uv run python -m heesch_rl.baseline --n-max 8 --trajectories 1000
```

Output is a JSON summary with per-reward counts, oracle-cache size,
and trajectory throughput. This is the floor any trained policy must
beat in subsequent milestones.

## Running tests

```bash
uv run pytest -q
```

`test_oracle.py` skips if the upstream sat binary is not built.
Build it via:

```bash
cd ../sat/src && make sat
```

## Status

- M4.1 (MDP design) — closed; spec at `benchmarks/rl/M4.1-mdp.md`.
- M4.2 (this directory) — GNN scaffolded; random baseline runs.
- M4.3 (training loop) — pending.
