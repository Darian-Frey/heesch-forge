// Corona-completion oracle implementation. See oracle.hpp.

#include "oracle.hpp"

#include "../dlx/dlx.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

namespace heesch_forge::corona {

cell apply_orientation( Orientation o, cell c )
{
	const int x = c.first;
	const int y = c.second;
	switch ( o ) {
	case Orientation::R0:    return { x, y };
	case Orientation::R90:   return { -y, x };
	case Orientation::R180:  return { -x, -y };
	case Orientation::R270:  return { y, -x };
	case Orientation::M:     return { x, -y };
	case Orientation::MR90:  return { y, x };
	case Orientation::MR180: return { -x, y };
	case Orientation::MR270: return { -y, -x };
	}
	return c;
}

shape_cells placed_cells( const shape_cells& shape, const Placement& p )
{
	shape_cells out;
	out.reserve( shape.size() );
	for ( const auto& c : shape ) {
		const auto rotated = apply_orientation( p.orient, c );
		out.emplace_back(
			rotated.first  + p.origin.first,
			rotated.second + p.origin.second );
	}
	return out;
}

cell_set halo_of( const cell_set& cells )
{
	cell_set h;
	const std::array<cell, 4> nbrs { {
		{  1,  0 }, { -1,  0 }, {  0,  1 }, {  0, -1 }
	} };
	for ( const auto& c : cells ) {
		for ( const auto& n : nbrs ) {
			cell adj { c.first + n.first, c.second + n.second };
			if ( cells.find( adj ) == cells.end() ) {
				h.insert( adj );
			}
		}
	}
	return h;
}

Oracle::Oracle( shape_cells base_shape )
	: base_shape_ { std::move( base_shape ) }
	, oriented_ {}
{
	if ( base_shape_.empty() ) {
		throw std::invalid_argument( "Oracle: base_shape must be non-empty" );
	}
	for ( std::size_t i = 0; i < all_orientations.size(); ++i ) {
		const Orientation o = all_orientations[i];
		shape_cells& dst = oriented_[i];
		dst.reserve( base_shape_.size() );
		for ( const auto& c : base_shape_ ) {
			dst.push_back( apply_orientation( o, c ) );
		}
	}
}

namespace {

// One candidate placement and the columns it touches in a built matrix.
struct Candidate {
	Placement p;
	std::vector<dlx::Solver::col_id> cols;        // sorted ascending: primary
	                                               // halo cols then secondary
	                                               // outside cols
};

// Output of matrix planning. Owned by the calling helper while it
// instantiates a Solver and feeds rows.
struct Matrix {
	dlx::Solver::col_id n_primary;
	dlx::Solver::col_id n_secondary;
	std::vector<Candidate> candidates;
};

// Plan a DLX matrix that asks: "complete the corona around `interior`
// using oriented copies of `base_shape`/`oriented`, with no overlap with
// the interior and no overlap among the placed copies." If no halo
// exists (the interior is unbounded) returns std::nullopt.
//
// Returns a Matrix struct describing the column layout and a list of
// candidate placements, each annotated with the sorted ascending
// columns it covers (halo primary cols first, then outside secondary
// cols). The caller plugs this into a freshly-constructed dlx::Solver.
[[nodiscard]] bool plan_matrix(
	const shape_cells& base_shape,
	const std::array<shape_cells, 8>& oriented,
	const cell_set& interior,
	Matrix& out )
{
	cell_set halo = halo_of( interior );
	if ( halo.empty() ) {
		return false;
	}

	// Index halo cells -> primary col id.
	std::map<cell, dlx::Solver::col_id> halo_index;
	{
		dlx::Solver::col_id i = 0;
		for ( const auto& c : halo ) {
			halo_index.emplace( c, i++ );
		}
	}

	// First pass: enumerate candidate placements that touch at least one
	// halo cell and do not overlap the interior. Collect the cells they
	// cover that lie outside both the interior and the halo (these become
	// secondary columns).
	struct Raw {
		Placement p;
		std::vector<dlx::Solver::col_id> halo_cols;
		std::vector<cell> outside;
	};
	std::vector<Raw> raws;
	cell_set outside_cells;

	for ( std::size_t oi = 0; oi < oriented.size(); ++oi ) {
		const Orientation orient = all_orientations[oi];
		const shape_cells& shape = oriented[oi];
		for ( const auto& anchor : shape ) {
			for ( const auto& h : halo ) {
				const cell origin {
					h.first  - anchor.first,
					h.second - anchor.second
				};
				Placement p { origin, orient };
				const shape_cells covered = placed_cells( base_shape, p );

				bool overlap_interior = false;
				std::vector<dlx::Solver::col_id> halo_cols;
				std::vector<cell> outside;
				for ( const auto& c : covered ) {
					if ( interior.find( c ) != interior.end() ) {
						overlap_interior = true;
						break;
					}
					auto it = halo_index.find( c );
					if ( it != halo_index.end() ) {
						halo_cols.push_back( it->second );
					} else {
						outside.push_back( c );
					}
				}
				if ( overlap_interior || halo_cols.empty() ) {
					continue;
				}
				std::sort( halo_cols.begin(), halo_cols.end() );
				halo_cols.erase(
					std::unique( halo_cols.begin(), halo_cols.end() ),
					halo_cols.end() );

				for ( const auto& c : outside ) {
					outside_cells.insert( c );
				}
				raws.push_back( Raw {
					std::move( p ),
					std::move( halo_cols ),
					std::move( outside )
				} );
			}
		}
	}

	if ( raws.empty() ) {
		return false;
	}

	// Index outside cells -> secondary col id, starting from n_primary.
	std::map<cell, dlx::Solver::col_id> outside_index;
	{
		dlx::Solver::col_id i = static_cast<dlx::Solver::col_id>( halo.size() );
		for ( const auto& c : outside_cells ) {
			outside_index.emplace( c, i++ );
		}
	}

	// Second pass: build the final candidate column lists.
	out.n_primary   = static_cast<dlx::Solver::col_id>( halo.size() );
	out.n_secondary = static_cast<dlx::Solver::col_id>( outside_cells.size() );
	out.candidates.clear();
	out.candidates.reserve( raws.size() );
	for ( auto& r : raws ) {
		std::vector<dlx::Solver::col_id> cols;
		cols.reserve( r.halo_cols.size() + r.outside.size() );
		cols.insert( cols.end(), r.halo_cols.begin(), r.halo_cols.end() );
		for ( const auto& c : r.outside ) {
			cols.push_back( outside_index.at( c ) );
		}
		std::sort( cols.begin(), cols.end() );
		out.candidates.push_back( Candidate { std::move( r.p ),
		                                       std::move( cols ) } );
	}
	return true;
}

dlx::Solver build_solver( const Matrix& m )
{
	dlx::Solver solver { m.n_primary };
	solver.add_secondary_columns( m.n_secondary );
	for ( const auto& cand : m.candidates ) {
		solver.add_row( cand.cols );
	}
	return solver;
}

} // namespace

bool Oracle::find_completion( const cell_set& interior,
                              std::vector<Placement>& out )
{
	Matrix matrix;
	if ( !plan_matrix( base_shape_, oriented_, interior, matrix ) ) {
		out.clear();
		last_candidates_ = matrix.candidates.size();
		last_explored_   = 0;
		return false;
	}
	last_candidates_ = matrix.candidates.size();
	dlx::Solver solver = build_solver( matrix );
	dlx::Solver::solution sol;
	const bool ok = solver.find_first( sol );
	last_explored_ = solver.nodes_explored();
	if ( !ok ) {
		out.clear();
		return false;
	}
	out.clear();
	out.reserve( sol.size() );
	for ( const auto rid : sol ) {
		out.push_back( matrix.candidates[rid].p );
	}
	return true;
}

std::size_t Oracle::count_completions( const cell_set& interior,
                                       std::size_t cap )
{
	Matrix matrix;
	if ( !plan_matrix( base_shape_, oriented_, interior, matrix ) ) {
		last_candidates_ = matrix.candidates.size();
		last_explored_   = 0;
		return 0;
	}
	last_candidates_ = matrix.candidates.size();
	dlx::Solver solver = build_solver( matrix );
	std::size_t n = 0;
	solver.solve( [&]( const dlx::Solver::solution& ) {
		++n;
		if ( cap != 0 && n >= cap ) return false;
		return true;
	} );
	last_explored_ = solver.nodes_explored();
	return n;
}

std::size_t Oracle::for_each_completion( const cell_set& interior,
                                          const completion_callback& cb )
{
	Matrix matrix;
	if ( !plan_matrix( base_shape_, oriented_, interior, matrix ) ) {
		last_candidates_ = matrix.candidates.size();
		last_explored_   = 0;
		return 0;
	}
	last_candidates_ = matrix.candidates.size();
	dlx::Solver solver = build_solver( matrix );
	std::size_t n = 0;
	bool keep_going = true;
	solver.solve( [&]( const dlx::Solver::solution& sol ) {
		if ( !keep_going ) return false;
		std::vector<Placement> placements;
		placements.reserve( sol.size() );
		for ( const auto rid : sol ) {
			placements.push_back( matrix.candidates[rid].p );
		}
		++n;
		keep_going = cb( placements );
		return keep_going;
	} );
	last_explored_ = solver.nodes_explored();
	return n;
}

} // namespace heesch_forge::corona
