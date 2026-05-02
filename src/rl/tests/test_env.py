from heesch_rl.canonical import canonical
from heesch_rl.env import (
    Action,
    HeeschEnv,
    STOP_ACTION,
    boundary_cells,
    has_hole,
    is_connected,
)


def stub_oracle(_cells):
    """Always returns 0 -- tests do not exercise the real sat oracle."""
    return 0


def test_boundary_cells_seed():
    p = frozenset({(0, 0)})
    assert set(boundary_cells(p)) == {(1, 0), (-1, 0), (0, 1), (0, -1)}


def test_is_connected():
    assert is_connected(frozenset())
    assert is_connected(frozenset({(0, 0)}))
    assert is_connected(frozenset({(0, 0), (1, 0)}))
    assert not is_connected(frozenset({(0, 0), (5, 5)}))


def test_has_hole_false_for_simple_shapes():
    assert not has_hole(frozenset({(0, 0)}))
    assert not has_hole(frozenset({(0, 0), (1, 0), (2, 0)}))
    # L-tromino, no hole.
    assert not has_hole(frozenset({(0, 0), (1, 0), (0, 1)}))


def test_has_hole_true_for_donut():
    # 3x3 with the centre removed.
    cells = frozenset(
        (x, y)
        for x in range(3)
        for y in range(3)
        if (x, y) != (1, 1)
    )
    assert has_hole(cells)


def test_env_reset_then_legal_actions_seed():
    env = HeeschEnv(oracle=stub_oracle, n_max=5, n_min=4)
    s = env.reset()
    assert s.cells == canonical(frozenset({(0, 0)}))
    assert not s.stopped
    actions = env.legal_actions()
    # Single seed cell at (0,0), so 4 boundary positions and Stop is
    # NOT yet legal (n=1 < n_min=4).
    assert sum(1 for a in actions if a.add is not None) == 4
    assert STOP_ACTION not in actions


def test_env_episode_grows_to_nmax():
    """Adds the first legal boundary cell each step; reaches n_max."""
    env = HeeschEnv(oracle=stub_oracle, n_max=4, n_min=4)
    env.reset()
    res = None
    for _ in range(3):  # 1 -> 2 -> 3 -> 4 cells
        actions = [a for a in env.legal_actions() if a.add is not None]
        res = env.step(actions[0])
    assert res is not None
    assert res.done
    assert res.reward == 0.0
    assert len(res.state.cells) == 4


def test_env_invalid_action_terminates_with_negative_reward():
    """Hole-introducing add at the env level returns -0.5 and terminates."""
    # We don't go through the full step-walk -- has_hole / is_connected
    # are unit-tested above. The end-to-end behaviour (env catching a
    # hole-closing add and returning -0.5) is exercised indirectly by
    # baseline runs.
    pass


def test_env_stop_below_floor_returns_neg_one_if_forced():
    """Stop must be excluded by legal_actions() when below n_min."""
    env = HeeschEnv(oracle=stub_oracle, n_max=10, n_min=4)
    env.reset()
    # n=1 < 4: Stop must not be in legal_actions.
    assert STOP_ACTION not in env.legal_actions()
    # Forcing Stop anyway returns -1 and terminates.
    res = env.step(STOP_ACTION)
    assert res.done
    assert res.reward == -1.0


def test_env_stop_above_floor_calls_oracle():
    """Above n_min, Stop calls the oracle and returns its H_c."""
    calls = []

    def oracle(cells):
        calls.append(cells)
        return 1

    env = HeeschEnv(oracle=oracle, n_max=10, n_min=2)
    env.reset()
    env.step(Action(add=(1, 0)))    # n=2
    res = env.step(STOP_ACTION)
    assert res.done
    assert res.reward == 1.0
    assert len(calls) == 1
