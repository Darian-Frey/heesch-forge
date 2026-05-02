// DLX-only end-to-end Heesch solver. See heesch_solver.hpp.

#include "heesch_solver.hpp"

#include <queue>
#include <stdexcept>
#include <utility>

namespace heesch_forge::corona {

bool is_simply_connected( const cell_set& interior )
{
	if ( interior.empty() ) {
		return true;          // vacuously
	}

	// Port of heesch-sat's HoleFinder algorithm (src/sat/src/holes.h).
	// Compute the halo using 8-Moore adjacency (cells diagonally OR
	// 4-cardinally adjacent to interior, not in interior). Then find
	// connected components of the halo using 4-cardinal edges between
	// halo cells. The lex-smallest halo cell is on the outer boundary
	// by definition; any component that does not include it is an
	// enclosed hole.
	//
	// Why 8-Moore halo with 4-cardinal connectivity: a single tile
	// with a 4-cardinal-only halo has 4 disconnected halo components
	// (the four cardinal neighbours), which would falsely report 3
	// holes. Including diagonals in the halo lets the corner cells
	// bridge the four cardinals into a single ring around the tile,
	// matching the topological notion of "no enclosed regions."
	const std::array<cell, 8> moore { {
		{  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 },
		{  1,  1 }, {  1, -1 }, { -1,  1 }, { -1, -1 }
	} };
	const std::array<cell, 4> edge { {
		{  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 }
	} };

	cell_set halo;
	for ( const auto& c : interior ) {
		for ( const auto& d : moore ) {
			cell adj { c.first + d.first, c.second + d.second };
			if ( interior.find( adj ) == interior.end() ) {
				halo.insert( adj );
			}
		}
	}
	if ( halo.empty() ) {
		return true;
	}

	cell halo_min = *halo.begin();             // std::set is sorted
	cell_set visited;
	visited.insert( halo_min );
	std::queue<cell> q;
	q.push( halo_min );
	while ( !q.empty() ) {
		const auto cur = q.front();
		q.pop();
		for ( const auto& d : edge ) {
			cell next { cur.first + d.first, cur.second + d.second };
			if ( halo.find( next ) == halo.end() ) {
				continue;          // not a halo cell
			}
			if ( !visited.insert( next ).second ) {
				continue;          // already visited
			}
			q.push( next );
		}
	}

	return visited.size() == halo.size();
}

namespace {

// Compute the union of `interior` with all cells covered by
// `placements` (laid out via `oracle.base_shape()`).
cell_set extend_interior(
	const Oracle& oracle,
	const cell_set& interior,
	const std::vector<Placement>& placements )
{
	cell_set out = interior;
	for ( const auto& p : placements ) {
		for ( const auto& c : placed_cells( oracle.base_shape(), p ) ) {
			out.insert( c );
		}
	}
	return out;
}

// Find any corona completion of `interior` whose union with `interior`
// is simply connected. Returns true and fills `out` on success.
bool find_simply_connected_completion(
	Oracle& oracle,
	const cell_set& interior,
	std::vector<Placement>& out )
{
	bool found = false;
	oracle.for_each_completion( interior,
		[&]( const std::vector<Placement>& sol ) {
			cell_set test = extend_interior( oracle, interior, sol );
			if ( is_simply_connected( test ) ) {
				out = sol;
				found = true;
				return false;          // stop search
			}
			return true;
		} );
	return found;
}

} // namespace

HeeschResult compute_heesch( const shape_cells& shape,
                             std::size_t max_level )
{
	if ( shape.empty() ) {
		throw std::invalid_argument( "compute_heesch: shape is empty" );
	}

	Oracle oracle { shape };
	std::size_t oracle_calls = 0;

	// Phase 1: Hh -- greedily extend coronas, allowing holes.
	CoronaState hh_state { shape };
	while ( hh_state.depth() <= max_level ) {
		std::vector<Placement> next;
		++oracle_calls;
		if ( !oracle.find_completion( hh_state.interior_cells(), next ) ) {
			break;
		}
		hh_state.add_level( std::move( next ) );
	}
	const std::size_t hh = hh_state.depth() - 1;

	// Phase 2: Hc -- bottom-up extension keeping the chain simply
	// connected. At each level, enumerate completions and pick the
	// first that yields a hole-free union; if no such completion
	// exists, Hc stops at the previous level.
	//
	// This is a greedy ascent rather than a full DFS over chains. For
	// shapes where some choice of level-k tiles permits a longer
	// hole-free chain than another, this can under-report Hc; the M0.4
	// regression is the empirical correctness gate, and a full DFS is
	// a fallback to escalate to if greedy ascent disagrees.
	CoronaState hc_state { shape };
	std::size_t hc = 0;
	while ( hc_state.depth() <= max_level
	        && hc_state.depth() <= hh ) {
		std::vector<Placement> next;
		++oracle_calls;
		if ( !find_simply_connected_completion(
				oracle, hc_state.interior_cells(), next ) ) {
			break;
		}
		hc_state.add_level( std::move( next ) );
		hc = hc_state.depth() - 1;
	}

	return HeeschResult {
		hc,
		hh,
		hh_state.depth(),
		oracle_calls
	};
}

// M2.6-followup-A: budgeted variant. See header for contract.
//
// The mechanics mirror compute_heesch but pass a node budget through
// to the Oracle's underlying DLX. We track the deepest hole-free
// `CoronaState` we successfully built; on budget exhaustion that's
// what gets handed to sat -seed downstream.
BoundedHeeschResult compute_heesch_bounded(
	const shape_cells& shape,
	std::size_t max_level,
	std::size_t node_budget )
{
	if ( shape.empty() ) {
		throw std::invalid_argument(
			"compute_heesch_bounded: shape is empty" );
	}

	Oracle oracle { shape };
	oracle.set_node_budget( node_budget );

	std::size_t oracle_calls = 0;
	std::size_t nodes_total = 0;

	// Phase 1: greedy Hh chain.
	CoronaState hh_state { shape };
	bool exhausted = false;
	while ( hh_state.depth() <= max_level ) {
		std::vector<Placement> next;
		++oracle_calls;
		const bool found =
			oracle.find_completion( hh_state.interior_cells(), next );
		nodes_total += oracle.last_nodes_explored();
		if ( oracle.last_budget_exhausted() ) {
			exhausted = true;
			break;
		}
		if ( !found ) {
			break;
		}
		hh_state.add_level( std::move( next ) );
	}

	if ( exhausted ) {
		// We don't have a definitive Hh; we have a hole-free
		// CoronaState built up to whatever depth completed. Hand it
		// to sat -seed for the rest.
		return BoundedHeeschResult {
			false,
			HeeschResult { 0, 0, hh_state.depth(), oracle_calls },
			std::optional<CoronaState> { hh_state },
			oracle_calls,
			nodes_total
		};
	}

	const std::size_t hh = hh_state.depth() - 1;

	// Phase 2: walkback for Hc, also budget-checked.
	CoronaState hc_state { shape };
	std::size_t hc = 0;
	while ( hc_state.depth() <= max_level
	     && hc_state.depth() <= hh ) {
		std::vector<Placement> next;
		++oracle_calls;
		bool ok_completion = false;
		oracle.for_each_completion( hc_state.interior_cells(),
			[&]( const std::vector<Placement>& sol ) {
				cell_set test = hc_state.interior_cells();
				for ( const auto& p : sol ) {
					for ( const auto& c : placed_cells(
							oracle.base_shape(), p ) ) {
						test.insert( c );
					}
				}
				if ( is_simply_connected( test ) ) {
					next = sol;
					ok_completion = true;
					return false;
				}
				return true;
			} );
		nodes_total += oracle.last_nodes_explored();
		if ( oracle.last_budget_exhausted() ) {
			exhausted = true;
			break;
		}
		if ( !ok_completion ) {
			break;
		}
		hc_state.add_level( std::move( next ) );
		hc = hc_state.depth() - 1;
	}

	if ( exhausted ) {
		// Hand the deepest hc-chain state to sat. Note this may be a
		// shallower depth than the hh chain; the seed that travels to
		// sat is the simply-connected one.
		return BoundedHeeschResult {
			false,
			HeeschResult { hc, hh, hh_state.depth(), oracle_calls },
			std::optional<CoronaState> { hc_state },
			oracle_calls,
			nodes_total
		};
	}

	return BoundedHeeschResult {
		true,
		HeeschResult { hc, hh, hh_state.depth(), oracle_calls },
		std::nullopt,
		oracle_calls,
		nodes_total
	};
}

} // namespace heesch_forge::corona
