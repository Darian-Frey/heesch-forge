"""D4 canonicalisation for polyominoes.

A polyomino is a frozenset of (x, y) integer coordinates. Two
polyominoes are D4-equivalent if one is the image of the other
under a translation composed with an element of the dihedral group
D4 (rotations by 0/90/180/270 degrees, plus four reflections).

`canonical(cells)` returns a frozenset that is the lex-smallest
representative across the eight D4 orbits, after translating its
lex-smallest cell to (0, 0).

`d4_orbit(cells)` returns the eight D4-related polyominoes (each
in its own translated-to-origin form). Useful for replay
augmentation in the RL training loop.

Both functions canonicalise the input first; calling
`canonical(canonical(p)) == canonical(p)` is a no-op.
"""

from __future__ import annotations

Cells = frozenset[tuple[int, int]]


# The eight D4 elements as 2x2 integer matrices applied to (x, y):
#   (a, b, c, d) means (x, y) -> (a*x + b*y, c*x + d*y).
# Order matters only for tie-breaking in canonical(); we list the
# identity first.
_D4_MATRICES: tuple[tuple[int, int, int, int], ...] = (
    (1, 0, 0, 1),     # identity
    (0, -1, 1, 0),    # rot 90
    (-1, 0, 0, -1),   # rot 180
    (0, 1, -1, 0),    # rot 270
    (-1, 0, 0, 1),    # reflect across y-axis
    (1, 0, 0, -1),    # reflect across x-axis
    (0, 1, 1, 0),     # reflect across y = x
    (0, -1, -1, 0),   # reflect across y = -x
)


def _translate_to_origin(cells: Cells) -> Cells:
    """Translate so the lex-smallest cell is at (0, 0)."""
    if not cells:
        return cells
    min_x = min(x for x, _ in cells)
    min_y = min(y for x, y in cells if x == min_x)
    return frozenset((x - min_x, y - min_y) for x, y in cells)


def _apply(matrix: tuple[int, int, int, int], cells: Cells) -> Cells:
    a, b, c, d = matrix
    return frozenset((a * x + b * y, c * x + d * y) for x, y in cells)


def d4_orbit(cells: Cells) -> tuple[Cells, ...]:
    """Return the eight D4 images of `cells`, each translated to origin."""
    return tuple(_translate_to_origin(_apply(m, cells)) for m in _D4_MATRICES)


def canonical(cells: Cells) -> Cells:
    """Return the lex-smallest D4-orbit representative."""
    if not cells:
        return cells
    # Sort each orbit by sorted-tuple-of-cells; pick the lex-smallest.
    best: tuple[tuple[int, int], ...] | None = None
    for image in d4_orbit(cells):
        key = tuple(sorted(image))
        if best is None or key < best:
            best = key
    assert best is not None
    return frozenset(best)
