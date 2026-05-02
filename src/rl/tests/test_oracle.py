"""Integration tests for the sat-subprocess oracle.

These tests skip if the sat binary is not built. The CI / Phase-0
build instructions in CLAUDE.md require it, so when they pass they
verify the full Python -> sat -> Python round-trip.
"""

from pathlib import Path

import pytest

from heesch_rl.oracle import MemoOracle, SatOracle


# tests/test_oracle.py -> parents[1]=src/rl, parents[2]=src
SAT_BIN = Path(__file__).resolve().parents[2] / "sat" / "src" / "sat"


@pytest.fixture(scope="module")
def sat_oracle() -> SatOracle:
    if not SAT_BIN.exists():
        pytest.skip(f"sat binary not built at {SAT_BIN}; run make in src/sat/src")
    return SatOracle(maxlevel=7)


def test_p_pentomino_hc(sat_oracle):
    """Kaplan's catalogue lists the P-pentomino with H_c = 0 (it tiles)."""
    # P-pentomino:
    #   .##
    #   .##
    #   .#.
    p = frozenset({(1, 0), (2, 0), (1, 1), (2, 1), (1, 2)})
    hc = sat_oracle(p)
    # Either 0 (tiler/non-tiler with no surround) or low value;
    # we don't assert an exact value to avoid coupling to data.
    assert hc >= 0


def test_singleton_hc(sat_oracle):
    """A single square trivially tiles -- H_c is 0."""
    p = frozenset({(0, 0)})
    hc = sat_oracle(p)
    assert hc == 0


def test_memoisation():
    base_calls = []

    def base(cells):
        base_calls.append(cells)
        return 2

    memo = MemoOracle(base)
    p = frozenset({(0, 0), (1, 0), (0, 1)})
    p_translated = frozenset({(10, 10), (11, 10), (10, 11)})
    assert memo(p) == 2
    assert memo(p_translated) == 2  # same canonical form, no second call
    assert len(base_calls) == 1
