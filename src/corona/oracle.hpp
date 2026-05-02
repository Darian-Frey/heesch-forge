#pragma once
//
// Corona-completion oracle (M2.2).
//
// Given a polyomino S (a set of unit squares, expressed as integer (x, y)
// cells) and an "interior" set of cells already covered by S placed at
// the origin plus any tiles in coronas 0 .. k-1, decide whether the
// k-corona can be completed and produce one such completion if so.
//
// "Complete corona" means: every halo cell of the interior (i.e. every
// cell adjacent to but not in the interior) is covered by exactly one
// placed copy of S, no placed copy overlaps the interior, and no two
// placed copies overlap.
//
// The oracle reduces this to an exact-cover problem and uses src/dlx/'s
// Solver. Halo cells are primary columns (covered exactly once); the
// further-out cells the candidate tiles touch are secondary columns
// (covered at most once, to prevent tile-tile overlap outside the halo).
//
// This module is intentionally polyomino-specific for M2.2. The grid
// abstraction over hex/iamond/etc. that upstream heesch-sat carries is
// not pulled in; that generalisation is a later milestone.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <set>
#include <utility>
#include <vector>

namespace heesch_forge::corona {

using cell        = std::pair<int, int>;
using shape_cells = std::vector<cell>;
using cell_set    = std::set<cell>;

// Square-grid symmetry group (D4): 4 rotations × {identity, reflection}.
// The reflection convention here is "flip the y axis" so M = (x, -y).
enum class Orientation : std::uint8_t {
	R0 = 0, R90 = 1, R180 = 2, R270 = 3,
	M = 4, MR90 = 5, MR180 = 6, MR270 = 7,
};

constexpr std::array<Orientation, 8> all_orientations {
	Orientation::R0,  Orientation::R90,  Orientation::R180, Orientation::R270,
	Orientation::M,   Orientation::MR90, Orientation::MR180, Orientation::MR270,
};

// Apply an orientation to a cell relative to (0, 0).
cell apply_orientation( Orientation o, cell c );

// A tile placement: an oriented copy of the shape, translated so its
// "anchor" (the first cell of the shape after orientation) sits at
// `origin`. To compare placements as sets of cells, use placed_cells().
struct Placement {
	cell        origin;
	Orientation orient;

	bool operator==( const Placement& other ) const = default;
	bool operator<( const Placement& other ) const
	{
		if ( origin != other.origin ) return origin < other.origin;
		return static_cast<std::uint8_t>( orient )
		     < static_cast<std::uint8_t>( other.orient );
	}
};

// Cells covered by a placement: each cell of the shape, oriented and
// translated by origin.
shape_cells placed_cells( const shape_cells& shape, const Placement& p );

// 4-connected halo of a set of cells: cells adjacent (N/S/E/W) to some
// member but not themselves members.
cell_set halo_of( const cell_set& cells );

class Oracle
{
public:
	explicit Oracle( shape_cells base_shape );

	// Read-only access to the shape and its 8 oriented copies (each
	// oriented shape is the original cells passed through the matching
	// Orientation, with no further normalisation).
	const shape_cells& base_shape() const { return base_shape_; }
	const std::array<shape_cells, 8>& oriented_shapes() const
	{ return oriented_; }

	// Try to find one corona completion. The interior must be a non-empty
	// 4-connected set of cells (the original shape at corona 0, plus any
	// tiles already placed in coronas 1 .. k-1). Returns true if a
	// completion exists; on success, fills `out` with one Placement per
	// tile in the completion. The order of placements is the order DLX's
	// search loop emits them and is deterministic for a given input.
	bool find_completion( const cell_set& interior,
	                      std::vector<Placement>& out );

	// Count completions, capped at `cap` (cap = 0 means unlimited). Useful
	// for sanity-checking unique-completion shapes and for benchmarking.
	std::size_t count_completions( const cell_set& interior,
	                               std::size_t cap = 0 );

	// Enumerate completions, invoking `cb` once per solution with the
	// list of placements. Returning false from the callback stops the
	// search early. Returns the number of callback invocations made.
	// Used by the M2.4 walkback to pick a simply-connected chain when
	// the first completion DLX visits has enclosed holes.
	using completion_callback =
		std::function<bool( const std::vector<Placement>& )>;
	std::size_t for_each_completion( const cell_set& interior,
	                                  const completion_callback& cb );

	// Diagnostic: number of candidate placements built for the most
	// recent find_completion / count_completions call. (Equal to the
	// number of rows in the DLX matrix.)
	std::size_t last_candidate_count() const { return last_candidates_; }

	// Diagnostic: DLX search-tree node count for the most recent solve.
	std::size_t last_nodes_explored() const { return last_explored_; }

	// M2.6-followup-A: per-call DLX node-count budget. Sticky across
	// calls. 0 = unlimited (default). After a call, check
	// last_budget_exhausted() to know whether the result is incomplete.
	void set_node_budget( std::size_t budget ) { node_budget_ = budget; }
	std::size_t node_budget() const { return node_budget_; }
	bool last_budget_exhausted() const { return last_budget_exhausted_; }

private:
	shape_cells base_shape_;
	std::array<shape_cells, 8> oriented_;

	std::size_t last_candidates_ = 0;
	std::size_t last_explored_   = 0;
	std::size_t node_budget_     = 0;
	bool last_budget_exhausted_  = false;
};

} // namespace heesch_forge::corona
