#pragma once
//
// Dancing Links / Algorithm X (Knuth, TAOCP §7.2.2.1).
//
// Solves the exact-cover problem: given a 0/1 matrix, find subsets of rows
// whose unions cover each (primary) column exactly once. Optional secondary
// columns may be covered at most once; they give us puzzles whose
// constraints are at-most-one rather than exactly-one (n-queens
// diagonals, polyomino-tiling reflections, etc.).
//
// Implementation note (versus a textbook pointer-laden DLX): nodes live in
// a single contiguous std::vector and refer to one another by integer
// index. Cache locality is better, debugging is far easier, and
// std::vector growth invalidates none of the relationships -- everything
// is index-based, so the underlying buffer can move under us during
// add_row().

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace heesch_forge::dlx {

class Solver
{
public:
	using row_id    = std::uint32_t;
	using col_id    = std::uint32_t;
	using solution  = std::vector<row_id>;
	using callback  = std::function<bool( const solution& )>;

	// Create a solver with `primary_columns` columns that must each be
	// covered exactly once. Add secondary columns with
	// add_secondary_columns(); add rows with add_row(). The matrix must be
	// fully populated before calling solve().
	explicit Solver( col_id primary_columns );

	// Append `count` secondary columns. They are covered at most once
	// rather than exactly once. Must be called before any add_row that
	// references them.
	void add_secondary_columns( col_id count );

	// Add a row whose 1s appear in the given columns. Columns must be
	// strictly ascending and reference indices < total_columns(). Returns
	// the row's auto-assigned id, used in solutions emitted by solve().
	row_id add_row( const std::vector<col_id>& covered_cols );

	// Total column count, primary + secondary.
	col_id total_columns() const { return n_primary_ + n_secondary_; }
	col_id primary_columns() const { return n_primary_; }

	// Run Algorithm X. The callback is invoked for every solution found;
	// return false from the callback to stop the search early. Returns the
	// total number of callback invocations (i.e. solutions emitted).
	std::size_t solve( const callback& cb );

	// Count all solutions without keeping them in memory.
	std::size_t count_solutions();

	// Find one solution. Returns true if at least one solution exists; on
	// success, fills `out` with the row ids of the first solution Algorithm
	// X visits under its standard min-size column heuristic.
	bool find_first( solution& out );

	// Number of "search tree" recursive calls made during the most recent
	// solve(). Useful as a normaliser for performance comparisons.
	std::size_t nodes_explored() const { return last_explored_; }

private:
	// Indexed-node layout, all stored in `nodes_`:
	//   index 0                     : root header
	//   index 1 .. n_primary_       : primary column headers, in a
	//                                 doubly linked list around the root
	//   index n_primary_+1 ..       : secondary column headers, NOT in
	//     n_primary_+n_secondary_    the root's list (so the search loop
	//                                 ignores them when picking a column)
	//   subsequent indices          : one node per matrix 1-cell, organised
	//                                 in column-vertical and row-horizontal
	//                                 toroidal lists
	struct Node {
		int L, R;          // horizontal links (in row, or in header chain)
		int U, D;          // vertical links (in column)
		int col;           // index of this node's column header
		int row;           // row id (-1 for headers)
		int size;          // only used by column headers: count of 1s
	};

	void cover( int c );
	void uncover( int c );
	void search( const callback& cb, bool& keep_going );

	col_id n_primary_;
	col_id n_secondary_;
	row_id next_row_id_;
	std::vector<Node> nodes_;
	solution current_;       // partial solution during search
	std::size_t last_explored_;

	// Debug / safety: which row ids have we already added? Bounds-check
	// future calls so a caller that forgets to add_row in order cannot
	// silently produce a corrupt matrix.
	bool sealed_;            // true after first solve(); add_row forbidden
};

} // namespace heesch_forge::dlx
