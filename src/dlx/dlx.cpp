// Dancing Links / Algorithm X implementation.
// See dlx.hpp for API and design notes.

#include "dlx.hpp"

#include <stdexcept>
#include <string>

namespace heesch_forge::dlx {

namespace {
constexpr int ROOT = 0;
}

Solver::Solver( col_id primary_columns )
	: n_primary_ { primary_columns }
	, n_secondary_ { 0 }
	, next_row_id_ { 0 }
	, nodes_ {}
	, current_ {}
	, last_explored_ { 0 }
	, node_budget_ { 0 }
	, last_budget_exhausted_ { false }
	, sealed_ { false }
{
	// Allocate root + primary column headers up front.
	nodes_.resize( 1 + primary_columns );

	// Root: linked into its own list initially.
	nodes_[ROOT] = Node { ROOT, ROOT, ROOT, ROOT, ROOT, -1, 0 };

	// Primary headers: chained horizontally through the root and vertically
	// to themselves (an empty column).
	int prev = ROOT;
	for ( col_id c = 0; c < primary_columns; ++c ) {
		const int idx = static_cast<int>( c ) + 1;
		nodes_[idx] = Node { prev, ROOT, idx, idx, idx, -1, 0 };
		nodes_[prev].R = idx;
		nodes_[ROOT].L = idx;
		prev = idx;
	}
}

void Solver::add_secondary_columns( col_id count )
{
	if ( sealed_ ) {
		throw std::logic_error( "add_secondary_columns after solve" );
	}
	const col_id base = total_columns();
	nodes_.resize( nodes_.size() + count );
	for ( col_id c = 0; c < count; ++c ) {
		const int idx = static_cast<int>( base + c ) + 1;
		// Secondary headers point at themselves: not in the root's list,
		// so the search loop's "if root.R == root, solution" termination
		// fires correctly. They still hold vertical links so cover/uncover
		// work normally.
		nodes_[idx] = Node { idx, idx, idx, idx, idx, -1, 0 };
	}
	n_secondary_ += count;
}

Solver::row_id Solver::add_row( const std::vector<col_id>& covered_cols )
{
	if ( sealed_ ) {
		throw std::logic_error( "add_row after solve" );
	}
	if ( covered_cols.empty() ) {
		throw std::invalid_argument( "add_row: empty row" );
	}
	for ( std::size_t i = 0; i < covered_cols.size(); ++i ) {
		if ( covered_cols[i] >= total_columns() ) {
			throw std::out_of_range(
				"add_row: column " + std::to_string( covered_cols[i] )
				+ " >= total_columns()" );
		}
		if ( i > 0 && covered_cols[i] <= covered_cols[i - 1] ) {
			throw std::invalid_argument(
				"add_row: covered_cols must be strictly ascending" );
		}
	}

	const row_id rid = next_row_id_++;
	const int first = static_cast<int>( nodes_.size() );

	for ( std::size_t k = 0; k < covered_cols.size(); ++k ) {
		const int my = first + static_cast<int>( k );
		const int col_header = static_cast<int>( covered_cols[k] ) + 1;

		// Horizontal links: row torus
		const int prev_in_row = ( k == 0 )
			? ( first + static_cast<int>( covered_cols.size() ) - 1 )
			: ( my - 1 );
		const int next_in_row = ( k + 1 == covered_cols.size() )
			? first
			: ( my + 1 );

		// Vertical links: insert at the bottom of the column's list, just
		// above the column header (which closes the cycle).
		const int up_in_col = nodes_[col_header].U;
		const int down_in_col = col_header;

		nodes_.push_back( Node {
			prev_in_row, next_in_row,
			up_in_col,   down_in_col,
			col_header,
			static_cast<int>( rid ),
			0
		} );

		nodes_[up_in_col].D = my;
		nodes_[col_header].U = my;
		nodes_[col_header].size += 1;
	}

	return rid;
}

void Solver::cover( int c )
{
	nodes_[nodes_[c].R].L = nodes_[c].L;
	nodes_[nodes_[c].L].R = nodes_[c].R;
	for ( int i = nodes_[c].D; i != c; i = nodes_[i].D ) {
		for ( int j = nodes_[i].R; j != i; j = nodes_[j].R ) {
			nodes_[nodes_[j].D].U = nodes_[j].U;
			nodes_[nodes_[j].U].D = nodes_[j].D;
			nodes_[nodes_[j].col].size -= 1;
		}
	}
}

void Solver::uncover( int c )
{
	for ( int i = nodes_[c].U; i != c; i = nodes_[i].U ) {
		for ( int j = nodes_[i].L; j != i; j = nodes_[j].L ) {
			nodes_[nodes_[j].col].size += 1;
			nodes_[nodes_[j].D].U = j;
			nodes_[nodes_[j].U].D = j;
		}
	}
	nodes_[nodes_[c].R].L = c;
	nodes_[nodes_[c].L].R = c;
}

void Solver::search( const callback& cb, bool& keep_going )
{
	++last_explored_;
	if ( !keep_going ) {
		return;
	}
	// M2.6-followup-A: enforce node-count budget. Flips keep_going so
	// every recursive frame above us also unwinds promptly.
	if ( node_budget_ != 0 && last_explored_ > node_budget_ ) {
		last_budget_exhausted_ = true;
		keep_going = false;
		return;
	}
	if ( nodes_[ROOT].R == ROOT ) {
		keep_going = cb( current_ );
		return;
	}

	// S heuristic: pick the (primary) column with the smallest size, ties
	// broken by leftmost-first (the order they were inserted).
	int c = nodes_[ROOT].R;
	for ( int j = nodes_[c].R; j != ROOT; j = nodes_[j].R ) {
		if ( nodes_[j].size < nodes_[c].size ) {
			c = j;
		}
	}

	// If the chosen column is empty, this branch fails; backtrack
	// immediately rather than entering an empty inner loop.
	if ( nodes_[c].size == 0 ) {
		return;
	}

	cover( c );
	for ( int r = nodes_[c].D; r != c && keep_going; r = nodes_[r].D ) {
		current_.push_back( static_cast<row_id>( nodes_[r].row ) );
		for ( int j = nodes_[r].R; j != r; j = nodes_[j].R ) {
			cover( nodes_[j].col );
		}
		search( cb, keep_going );
		// Undo in REVERSE order: walk leftward.
		for ( int j = nodes_[r].L; j != r; j = nodes_[j].L ) {
			uncover( nodes_[j].col );
		}
		current_.pop_back();
	}
	uncover( c );
}

std::size_t Solver::solve( const callback& cb )
{
	sealed_ = true;
	last_explored_ = 0;
	last_budget_exhausted_ = false;
	current_.clear();

	std::size_t count = 0;
	bool keep_going = true;
	auto wrapped = [&]( const solution& s ) {
		++count;
		return cb( s );
	};
	search( wrapped, keep_going );
	return count;
}

std::size_t Solver::count_solutions()
{
	return solve( []( const solution& ) { return true; } );
}

bool Solver::find_first( solution& out )
{
	bool found = false;
	solve( [&]( const solution& s ) {
		out = s;
		found = true;
		return false;          // stop after the first
	} );
	return found;
}

} // namespace heesch_forge::dlx
