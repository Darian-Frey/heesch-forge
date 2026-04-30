// M2.1 unit tests for src/dlx/dlx.{hpp,cpp}.
//
// Coverage:
//   - Empty / trivial matrices.
//   - Knuth's §1 worked example from the "Dancing Links" paper
//     (Knuth, "Dancing links", Millennial Perspectives in Computer
//     Science, 2000; arXiv:cs/0011047, §1).
//   - n-queens for n in {4, 5, 6, 7, 8} matched against the
//     well-known solution counts (OEIS A000170: 1, 0, 0, 2, 10, 4,
//     40, 92, 352, 724, ...).
//   - Early-stop callback semantics.
//   - Secondary columns rejected from the search header chain
//     (covered-at-most-once behaviour vs covered-exactly-once).
//
// Each TEST(name) registers itself; main() runs them all and reports
// pass / fail counts. Non-zero exit on any failure.

#include "dlx.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ::heesch_forge::dlx::Solver;

struct TestCase {
	const char* name;
	std::function<bool()> fn;       // returns true on pass
};

std::vector<TestCase>& registry()
{
	static std::vector<TestCase> r;
	return r;
}

struct Register {
	Register( const char* name, std::function<bool()> fn )
	{
		registry().push_back( { name, std::move( fn ) } );
	}
};

#define TEST(name)                                                     \
	static bool name##_impl();                                         \
	static ::Register name##_reg( #name, name##_impl );                \
	static bool name##_impl()

#define REQUIRE(expr)                                                  \
	do {                                                               \
		if ( !( expr ) ) {                                             \
			std::fprintf( stderr,                                      \
				"  FAIL: %s:%d: %s\n",                                 \
				__FILE__, __LINE__, #expr );                           \
			return false;                                              \
		}                                                              \
	} while ( 0 )

#define REQUIRE_EQ(a, b)                                               \
	do {                                                               \
		auto _av = ( a );                                              \
		auto _bv = ( b );                                              \
		if ( !( _av == _bv ) ) {                                       \
			std::fprintf( stderr,                                      \
				"  FAIL: %s:%d: %s != %s (got %lld vs %lld)\n",        \
				__FILE__, __LINE__, #a, #b,                            \
				static_cast<long long>( _av ),                         \
				static_cast<long long>( _bv ) );                       \
			return false;                                              \
		}                                                              \
	} while ( 0 )

// ============================================================
// 1. Trivial / edge cases
// ============================================================

TEST( empty_matrix_no_rows_zero_cols )
{
	// Zero columns means the empty set covers the (empty) constraint set.
	// Algorithm X recognises this immediately as one solution: pick no
	// rows.
	Solver s( 0 );
	REQUIRE_EQ( s.count_solutions(), 1u );
	return true;
}

TEST( empty_matrix_no_rows_some_cols )
{
	// With primary columns and no rows, no solution exists.
	Solver s( 3 );
	REQUIRE_EQ( s.count_solutions(), 0u );
	return true;
}

TEST( single_row_covers_everything )
{
	Solver s( 4 );
	const auto r = s.add_row( { 0, 1, 2, 3 } );
	(void)r;
	REQUIRE_EQ( s.count_solutions(), 1u );
	return true;
}

TEST( two_disjoint_rows_unique_solution )
{
	Solver s( 4 );
	s.add_row( { 0, 1 } );
	s.add_row( { 2, 3 } );
	REQUIRE_EQ( s.count_solutions(), 1u );
	return true;
}

TEST( find_first_returns_correct_rows )
{
	Solver s( 4 );
	const auto r0 = s.add_row( { 0, 1 } );
	const auto r1 = s.add_row( { 2, 3 } );
	Solver::solution out;
	REQUIRE( s.find_first( out ) );
	std::sort( out.begin(), out.end() );
	REQUIRE_EQ( out.size(), 2u );
	REQUIRE_EQ( out[0], r0 );
	REQUIRE_EQ( out[1], r1 );
	return true;
}

// ============================================================
// 2. Knuth's §1 example from "Dancing Links"
// ============================================================
//
// The matrix from §1 of the Knuth paper is 6 rows by 7 columns. Rows are
// (0-indexed):
//     row 0: { 2, 4, 5 }
//     row 1: { 0, 3, 6 }
//     row 2: { 1, 2, 5 }
//     row 3: { 0, 3 }
//     row 4: { 1, 6 }
//     row 5: { 3, 4, 6 }
// The unique exact cover is {row 0, row 3, row 4} (Knuth §1).
//
// Verification: the union covers each column exactly once:
//     row 0 {2,4,5} ∪ row 3 {0,3} ∪ row 4 {1,6} = {0,1,2,3,4,5,6}. ✓

TEST( knuth_section_1_matrix )
{
	Solver s( 7 );
	const auto r0 = s.add_row( { 2, 4, 5 } );
	const auto r1 = s.add_row( { 0, 3, 6 } );
	const auto r2 = s.add_row( { 1, 2, 5 } );
	const auto r3 = s.add_row( { 0, 3 } );
	const auto r4 = s.add_row( { 1, 6 } );
	const auto r5 = s.add_row( { 3, 4, 6 } );
	(void)r1; (void)r2; (void)r5;

	REQUIRE_EQ( s.count_solutions(), 1u );

	Solver::solution sol;
	REQUIRE( s.find_first( sol ) );
	std::sort( sol.begin(), sol.end() );
	REQUIRE_EQ( sol.size(), 3u );
	REQUIRE_EQ( sol[0], r0 );
	REQUIRE_EQ( sol[1], r3 );
	REQUIRE_EQ( sol[2], r4 );
	return true;
}

// ============================================================
// 3. n-queens
// ============================================================
//
// Encoding (Knuth's TAOCP §7.2.2.1, exercise 17):
//     primary columns: 2n total
//         columns [0, n)        --- one per rank (each must hold a queen)
//         columns [n, 2n)       --- one per file (each must hold a queen)
//     secondary columns: 2*(2n-1)
//         columns [2n, 2n+2n-1) --- one per "/" diagonal (rank+file)
//         columns [2n+2n-1, 2n+2*(2n-1)) --- one per "\" diagonal
//                                            (rank-file+(n-1))
// Each row corresponds to placing a queen at (rank, file): four 1s.

std::size_t n_queens_count( int n )
{
	const int diag_pos = 2 * n - 1;       // count of "/" diagonals
	const int diag_neg = 2 * n - 1;       // count of "\" diagonals
	Solver s( static_cast<Solver::col_id>( 2 * n ) );
	s.add_secondary_columns(
		static_cast<Solver::col_id>( diag_pos + diag_neg ) );
	for ( int r = 0; r < n; ++r ) {
		for ( int f = 0; f < n; ++f ) {
			std::vector<Solver::col_id> cols;
			cols.push_back( static_cast<Solver::col_id>( r ) );           // rank
			cols.push_back( static_cast<Solver::col_id>( n + f ) );       // file
			const int dpos = r + f;                                       // 0..2n-2
			const int dneg = r - f + ( n - 1 );                           // 0..2n-2
			cols.push_back( static_cast<Solver::col_id>( 2 * n + dpos ) );
			cols.push_back( static_cast<Solver::col_id>(
				2 * n + diag_pos + dneg ) );
			std::sort( cols.begin(), cols.end() );
			s.add_row( cols );
		}
	}
	return s.count_solutions();
}

// OEIS A000170 (number of arrangements of n non-attacking queens):
//   n  count
//   1  1
//   2  0
//   3  0
//   4  2
//   5  10
//   6  4
//   7  40
//   8  92
//   9  352
//  10  724

TEST( queens_4 ) { REQUIRE_EQ( n_queens_count( 4 ), 2u );  return true; }
TEST( queens_5 ) { REQUIRE_EQ( n_queens_count( 5 ), 10u ); return true; }
TEST( queens_6 ) { REQUIRE_EQ( n_queens_count( 6 ), 4u );  return true; }
TEST( queens_7 ) { REQUIRE_EQ( n_queens_count( 7 ), 40u ); return true; }
TEST( queens_8 ) { REQUIRE_EQ( n_queens_count( 8 ), 92u ); return true; }

// Sanity: as a one-off, find_first on n=8 must return exactly 8 rows.
TEST( queens_8_find_first_has_eight_queens )
{
	const int n = 8;
	const int diag_pos = 2 * n - 1;
	const int diag_neg = 2 * n - 1;
	Solver s( static_cast<Solver::col_id>( 2 * n ) );
	s.add_secondary_columns(
		static_cast<Solver::col_id>( diag_pos + diag_neg ) );
	for ( int r = 0; r < n; ++r ) {
		for ( int f = 0; f < n; ++f ) {
			std::vector<Solver::col_id> cols;
			cols.push_back( static_cast<Solver::col_id>( r ) );
			cols.push_back( static_cast<Solver::col_id>( n + f ) );
			const int dpos = r + f;
			const int dneg = r - f + ( n - 1 );
			cols.push_back( static_cast<Solver::col_id>( 2 * n + dpos ) );
			cols.push_back( static_cast<Solver::col_id>(
				2 * n + diag_pos + dneg ) );
			std::sort( cols.begin(), cols.end() );
			s.add_row( cols );
		}
	}
	Solver::solution sol;
	REQUIRE( s.find_first( sol ) );
	REQUIRE_EQ( sol.size(), static_cast<std::size_t>( n ) );

	// Each rank exactly once, each file exactly once.
	std::set<int> ranks, files;
	for ( const auto rid : sol ) {
		const int r = static_cast<int>( rid ) / n;
		const int f = static_cast<int>( rid ) % n;
		ranks.insert( r );
		files.insert( f );
	}
	REQUIRE_EQ( ranks.size(), static_cast<std::size_t>( n ) );
	REQUIRE_EQ( files.size(), static_cast<std::size_t>( n ) );
	return true;
}

// ============================================================
// 4. Search semantics
// ============================================================

TEST( early_stop_callback_visits_one_solution )
{
	// n=8 has 92 total solutions. A callback that returns false on the
	// first one must yield exactly one solve() return.
	const int n = 8;
	const int diag_pos = 2 * n - 1;
	const int diag_neg = 2 * n - 1;
	Solver s( static_cast<Solver::col_id>( 2 * n ) );
	s.add_secondary_columns(
		static_cast<Solver::col_id>( diag_pos + diag_neg ) );
	for ( int r = 0; r < n; ++r ) {
		for ( int f = 0; f < n; ++f ) {
			std::vector<Solver::col_id> cols;
			cols.push_back( static_cast<Solver::col_id>( r ) );
			cols.push_back( static_cast<Solver::col_id>( n + f ) );
			const int dpos = r + f;
			const int dneg = r - f + ( n - 1 );
			cols.push_back( static_cast<Solver::col_id>( 2 * n + dpos ) );
			cols.push_back( static_cast<Solver::col_id>(
				2 * n + diag_pos + dneg ) );
			std::sort( cols.begin(), cols.end() );
			s.add_row( cols );
		}
	}

	std::size_t visited = s.solve( []( const Solver::solution& ) {
		return false;
	} );
	REQUIRE_EQ( visited, 1u );
	return true;
}

TEST( secondary_only_no_primary_means_zero_or_one_solution )
{
	// Two secondary columns and no primary columns: the empty selection of
	// rows is itself a solution (every primary column -- there are none --
	// is covered exactly once, every secondary -- at most once).
	Solver s( 0 );
	s.add_secondary_columns( 2 );
	REQUIRE_EQ( s.count_solutions(), 1u );
	return true;
}

TEST( add_row_after_solve_throws )
{
	Solver s( 2 );
	s.add_row( { 0, 1 } );
	s.count_solutions();
	bool threw = false;
	try {
		s.add_row( { 0, 1 } );
	} catch ( const std::logic_error& ) {
		threw = true;
	}
	REQUIRE( threw );
	return true;
}

TEST( add_row_columns_must_be_ascending )
{
	Solver s( 5 );
	bool threw = false;
	try {
		s.add_row( { 2, 1, 3 } );      // not sorted
	} catch ( const std::invalid_argument& ) {
		threw = true;
	}
	REQUIRE( threw );
	return true;
}

TEST( add_row_columns_must_be_in_range )
{
	Solver s( 3 );
	bool threw = false;
	try {
		s.add_row( { 0, 5 } );          // 5 >= 3
	} catch ( const std::out_of_range& ) {
		threw = true;
	}
	REQUIRE( threw );
	return true;
}

// ============================================================
// 5. Internal accounting
// ============================================================

TEST( nodes_explored_is_nonzero_after_solve )
{
	// Ensure the public statistic plumbing actually updates.
	Solver s( 4 );
	s.add_row( { 0, 1 } );
	s.add_row( { 2, 3 } );
	s.count_solutions();
	REQUIRE( s.nodes_explored() > 0 );
	return true;
}

} // anonymous namespace

int main()
{
	int passed = 0, failed = 0;
	std::printf( "# heesch-forge DLX unit tests\n" );
	std::printf( "# %zu test cases registered\n\n", registry().size() );
	for ( const auto& tc : registry() ) {
		std::printf( "  %-50s ", tc.name );
		std::fflush( stdout );
		const bool ok = tc.fn();
		std::printf( "%s\n", ok ? "PASS" : "FAIL" );
		if ( ok ) ++passed; else ++failed;
	}
	std::printf( "\n# %d passed, %d failed\n", passed, failed );
	return failed == 0 ? 0 : 1;
}
