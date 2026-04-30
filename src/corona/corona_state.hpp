#pragma once
//
// Corona handoff format (M2.3).
//
// `CoronaState` records the placements of a polyomino S at each corona
// level it has been completed for so far: level 0 (always the original
// shape, fixed at the origin in orientation R0), level 1 (the placements
// produced by the M2.2 oracle for corona-1 around level 0), level 2
// (placements around the union of levels 0 and 1), and so on.
//
// This is the bridge between the DLX-based corona-completion oracle in
// `oracle.hpp` and a downstream SAT solver that takes over for outer
// coronas. Both sides agree on the same `CoronaState` layout and on the
// same text serialisation, so a level-(k-1) corona that DLX has produced
// can be handed to SAT via either an in-memory struct or a temp file.
//
// Format (version 1):
//
//   v 1
//   shape <x0 y0 x1 y1 ... xN yN>
//   level 0
//     0 0 R0
//   level 1
//     <ox0> <oy0> <orientation_tag>
//     <ox1> <oy1> <orientation_tag>
//     ...
//   level 2
//     ...
//
// Orientation tags: "R0" "R90" "R180" "R270" "M" "MR90" "MR180" "MR270".
// Lines starting with `#` are comments and are ignored on read. Each
// "level k" block is optional after level 0 if the corona at depth k
// has not yet been computed; the top-level "level k" line must precede
// the placements within that block.
//
// The format is intentionally simple text rather than JSON / protobuf:
// debuggability and grep-ability matter more than terseness here, and
// the sizes involved (a polyomino n=20 with corona depth 3 is at most a
// few hundred placements) are not large enough for an efficient binary
// format to be worth the cost.

#include "oracle.hpp"

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

namespace heesch_forge::corona {

class CoronaState
{
public:
	// Construct an empty state with only the level-0 placement (the
	// original shape at origin, orientation R0).
	explicit CoronaState( shape_cells base_shape );

	const shape_cells& base_shape() const { return base_shape_; }

	// Number of corona levels for which we have placements (1 means
	// level 0 only; 2 means levels 0 and 1; ...).
	std::size_t depth() const { return levels_.size(); }

	// Placements at level k. level(0) returns a one-element vector with
	// the canonical {origin (0,0), R0} entry; level(k) for k > 0 returns
	// the placements added by add_level().
	const std::vector<Placement>& level( std::size_t k ) const
	{ return levels_.at( k ); }

	// Append a new corona level. The placements should not overlap one
	// another or any prior level (the CoronaState class does not check
	// this; the caller -- typically the M2.2 oracle's output -- is
	// responsible). Returns the new depth().
	std::size_t add_level( std::vector<Placement> placements );

	// Union of all cells covered by every placement at every level.
	// Useful as the `interior` argument when asking the M2.2 oracle for
	// the next corona level.
	cell_set interior_cells() const;

	// Halo of interior_cells() (the cells that the next-level corona
	// must cover).
	cell_set halo_cells() const;

	// Text serialisation (see file header for format). write() emits the
	// canonical form; read() parses it. read() throws std::runtime_error
	// on malformed input. Both round-trip exactly: write -> read -> write
	// produces byte-identical output.
	void write( std::ostream& os ) const;
	static CoronaState read( std::istream& is );

private:
	shape_cells base_shape_;
	std::vector<std::vector<Placement>> levels_;
};

// Free helper: convert an Orientation enum value to its string tag and
// back. tag_of() is total; orientation_of_tag() throws on unknown tags.
const char* tag_of( Orientation o );
Orientation orientation_of_tag( const std::string& tag );

} // namespace heesch_forge::corona
