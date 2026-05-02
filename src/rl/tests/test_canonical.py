from heesch_rl.canonical import canonical, d4_orbit


def test_singleton_canonical():
    p = frozenset({(5, 7)})
    assert canonical(p) == frozenset({(0, 0)})


def test_horizontal_domino_collapses_with_vertical():
    horiz = frozenset({(0, 0), (1, 0)})
    vert = frozenset({(0, 0), (0, 1)})
    assert canonical(horiz) == canonical(vert)


def test_translation_invariance():
    p = frozenset({(0, 0), (1, 0), (1, 1), (2, 1)})  # a Z-tetromino
    p_translated = frozenset({(x + 100, y - 50) for x, y in p})
    assert canonical(p) == canonical(p_translated)


def test_canonical_is_idempotent():
    p = frozenset({(0, 0), (1, 0), (2, 0), (2, 1), (1, 2)})
    once = canonical(p)
    twice = canonical(once)
    assert once == twice


def test_d4_orbit_size_atmost_eight():
    # A symmetric square has D4-stabiliser of size 8 -> orbit collapses.
    sq = frozenset({(0, 0), (1, 0), (0, 1), (1, 1)})
    orbit = set(d4_orbit(sq))
    assert len(orbit) == 1
    # An L-tromino has trivial stabiliser -> 8 distinct images.
    L = frozenset({(0, 0), (1, 0), (0, 1)})
    orbit_L = set(d4_orbit(L))
    assert len(orbit_L) == 4  # L has a reflection symmetry
