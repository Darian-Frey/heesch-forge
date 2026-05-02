#pragma once
//
// DLX-only end-to-end Heesch solver (M2.4).
//
// `compute_heesch(shape)` computes both Heesch numbers of a polyomino
// using only the M2.2 corona-completion oracle and a simply-connected
// walkback. No SAT solver is involved. The intent is to reproduce
// heesch-sat's Hc/Hh values on the M0.4 regression dataset entirely
// from `src/dlx/` + `src/corona/`, validating that the corona-state
// machinery is sound before introducing any DLX -> SAT handoff (which
// would be M2.5b territory once empirical evidence shows where
// DLX-only stops scaling).
//
// Method:
//   Hh: greedily extend coronas by repeatedly calling the M2.2 oracle
//       on the current interior. The largest depth at which a
//       completion exists is Hh.
//   Hc: walk back through the greedy chain. For each level k from
//       Hh down to 1, check whether the union of levels 0..k is
//       simply connected (i.e. the complement in a bounding box is
//       4-connected). The first such k is Hc.
//
// The greedy walkback is sound when the greedy Hh chain happens to be
// simply connected at the right level; for shapes where it is not, a
// future iteration may need to enumerate alternative completions
// (oracle.solve(callback)) and pick a hole-free one. The 174-shape
// M0.4 regression is the empirical correctness gate.

#include "corona_state.hpp"
#include "oracle.hpp"

#include <cstddef>
#include <optional>

namespace heesch_forge::corona {

struct HeeschResult {
	std::size_t hc;          // hole-free Heesch number
	std::size_t hh;          // Heesch number allowing holes
	std::size_t depth_reached;       // = hh + 1, the deepest CoronaState depth
	std::size_t oracle_calls;        // diagnostics
};

// Test whether the union of cells is simply connected: 4-connected
// complement in a bounding box, no enclosed empty cells.
bool is_simply_connected( const cell_set& interior );

// Compute (Hc, Hh) for a polyomino. `max_level` caps the corona search
// depth; defaults to 7 (heesch-sat's `-maxlevel` default), one above
// the all-shape record so legitimate non-tilers cannot run away.
HeeschResult compute_heesch( const shape_cells& shape,
                             std::size_t max_level = 7 );

// M2.6-followup-A.
//
// `compute_heesch_bounded` is a budgeted variant of `compute_heesch`. The
// `node_budget` cap is applied to each DLX call; when any call exhausts
// it, we return early with `complete = false` and `partial_state` set to
// the deepest hole-free CoronaState we'd built so far.
//
// The intended consumer is `benchmarks/hybrid/run_bounded_joint`, which
// hands the partial state to `sat -seed <file>` if `complete == false`.
// On `complete == true` the returned `result` is the same `HeeschResult`
// that `compute_heesch` would return (so callers can use both code
// paths uniformly).
struct BoundedHeeschResult {
	bool                          complete;
	HeeschResult                  result;          // valid iff complete
	std::optional<CoronaState>    partial_state;   // present iff !complete
	std::size_t                   oracle_calls;
	std::size_t                   nodes_explored_total;
};

BoundedHeeschResult compute_heesch_bounded(
	const shape_cells& shape,
	std::size_t max_level = 7,
	std::size_t node_budget = 1'000'000 );

} // namespace heesch_forge::corona
