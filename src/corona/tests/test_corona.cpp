// M2.2 unit tests for src/corona/oracle.{hpp,cpp}.
//
// Layered on top of the M2.1 DLX gate: exercises the corona-completion
// oracle on small shapes whose answers we can derive by hand or look up
// in the Kaplan dataset (paper/lit/bibtex.bib KaplanA8).

#include "../oracle.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ::heesch_forge::corona::all_orientations;
using ::heesch_forge::corona::apply_orientation;
using ::heesch_forge::corona::cell;
using ::heesch_forge::corona::cell_set;
using ::heesch_forge::corona::halo_of;
using ::heesch_forge::corona::Oracle;
using ::heesch_forge::corona::Orientation;
using ::heesch_forge::corona::Placement;
using ::heesch_forge::corona::placed_cells;
using ::heesch_forge::corona::shape_cells;

struct TestCase {
	const char* name;
	std::function<bool()> fn;
};

std::vector<TestCase>& registry()
{
	static std::vector<TestCase> r;
	return r;
}

struct Register {
	Register( const char* n, std::function<bool()> f )
	{ registry().push_back( { n, std::move( f ) } ); }
};

#define TEST(name)                                                \
	static bool name##_impl();                                    \
	static ::Register name##_reg( #name, name##_impl );           \
	static bool name##_impl()

#define REQUIRE(expr)                                             \
	do {                                                          \
		if ( !( expr ) ) {                                        \
			std::fprintf( stderr,                                 \
				"  FAIL: %s:%d: %s\n",                            \
				__FILE__, __LINE__, #expr );                      \
			return false;                                         \
		}                                                         \
	} while ( 0 )

#define REQUIRE_EQ(a, b)                                          \
	do {                                                          \
		auto _av = ( a );                                         \
		auto _bv = ( b );                                         \
		if ( !( _av == _bv ) ) {                                  \
			std::fprintf( stderr,                                 \
				"  FAIL: %s:%d: %s != %s (got %lld vs %lld)\n",   \
				__FILE__, __LINE__, #a, #b,                       \
				static_cast<long long>( _av ),                    \
				static_cast<long long>( _bv ) );                  \
			return false;                                         \
		}                                                         \
	} while ( 0 )

shape_cells parse_shape( const std::string& s )
{
	// "x0 y0 x1 y1 ..." -- the Kaplan-dataset coordinate format.
	shape_cells cells;
	int x = 0, y = 0;
	bool have_x = false;
	for ( std::size_t i = 0; i <= s.size(); ++i ) {
		if ( i == s.size() || s[i] == ' ' || s[i] == '\t' ) {
			// Token boundary
			continue;
		}
	}
	// Manual parse: split on whitespace.
	std::string token;
	std::vector<int> ints;
	for ( char ch : s ) {
		if ( ch == ' ' || ch == '\t' ) {
			if ( !token.empty() ) {
				ints.push_back( std::atoi( token.c_str() ) );
				token.clear();
			}
		} else {
			token.push_back( ch );
		}
	}
	if ( !token.empty() ) {
		ints.push_back( std::atoi( token.c_str() ) );
	}
	for ( std::size_t i = 0; i + 1 < ints.size(); i += 2 ) {
		cells.emplace_back( ints[i], ints[i + 1] );
	}
	(void)x; (void)y; (void)have_x;
	return cells;
}

cell_set as_set( const shape_cells& v )
{
	return cell_set { v.begin(), v.end() };
}

// Verify that the placements produced by find_completion form a valid
// corona-completion: each halo cell covered exactly once, no overlap with
// the interior, no overlap among the placements.
bool placements_cover_halo( const Oracle& oracle,
                            const cell_set& interior,
                            const std::vector<Placement>& pls )
{
	const cell_set halo = halo_of( interior );
	cell_set covered;          // cells covered by placements
	for ( const auto& p : pls ) {
		const auto cells = placed_cells( oracle.base_shape(), p );
		for ( const auto& c : cells ) {
			if ( interior.find( c ) != interior.end() ) {
				return false;          // overlap with interior
			}
			if ( !covered.insert( c ).second ) {
				return false;          // overlap with another placement
			}
		}
	}
	for ( const auto& h : halo ) {
		if ( covered.find( h ) == covered.end() ) {
			return false;              // halo cell uncovered
		}
	}
	return true;
}

// ============================================================
// 1. Geometric primitives
// ============================================================

TEST( apply_orientation_R0_is_identity )
{
	REQUIRE_EQ( apply_orientation( Orientation::R0, { 3, 5 } ).first,  3 );
	REQUIRE_EQ( apply_orientation( Orientation::R0, { 3, 5 } ).second, 5 );
	return true;
}

TEST( apply_orientation_R90_rotates_quarter )
{
	// (x, y) -> (-y, x)
	const auto r = apply_orientation( Orientation::R90, { 3, 0 } );
	REQUIRE_EQ( r.first,  0 );
	REQUIRE_EQ( r.second, 3 );
	return true;
}

TEST( apply_orientation_R180_negates )
{
	const auto r = apply_orientation( Orientation::R180, { 3, 5 } );
	REQUIRE_EQ( r.first,  -3 );
	REQUIRE_EQ( r.second, -5 );
	return true;
}

TEST( apply_orientation_M_flips_y )
{
	// (x, y) -> (x, -y)
	const auto r = apply_orientation( Orientation::M, { 3, 5 } );
	REQUIRE_EQ( r.first,   3 );
	REQUIRE_EQ( r.second, -5 );
	return true;
}

TEST( orientations_form_a_group )
{
	// Applying every orientation to (1, 0) must produce 4 distinct cells:
	// the four unit vectors rotated. (Reflections of (1, 0) coincide with
	// rotations, since y = 0.)
	cell_set seen;
	for ( const auto o : all_orientations ) {
		seen.insert( apply_orientation( o, { 1, 0 } ) );
	}
	REQUIRE_EQ( seen.size(), 4u );
	REQUIRE( seen.contains( {  1,  0 } ) );
	REQUIRE( seen.contains( { -1,  0 } ) );
	REQUIRE( seen.contains( {  0,  1 } ) );
	REQUIRE( seen.contains( {  0, -1 } ) );
	return true;
}

TEST( halo_of_single_square )
{
	cell_set s { { 0, 0 } };
	const auto h = halo_of( s );
	REQUIRE_EQ( h.size(), 4u );
	REQUIRE( h.contains( {  1,  0 } ) );
	REQUIRE( h.contains( { -1,  0 } ) );
	REQUIRE( h.contains( {  0,  1 } ) );
	REQUIRE( h.contains( {  0, -1 } ) );
	return true;
}

TEST( halo_of_horizontal_domino )
{
	cell_set s { { 0, 0 }, { 1, 0 } };
	const auto h = halo_of( s );
	// Halo: 2 above + 2 below + 1 left + 1 right = 6 cells.
	REQUIRE_EQ( h.size(), 6u );
	return true;
}

// ============================================================
// 2. Tiny shape oracles
// ============================================================

TEST( monomino_corona_1_succeeds_and_is_valid )
{
	shape_cells shape { { 0, 0 } };
	Oracle oracle( shape );
	cell_set interior { { 0, 0 } };
	std::vector<Placement> out;
	REQUIRE( oracle.find_completion( interior, out ) );
	// Halo has 4 cells; each monomino covers exactly 1 cell, so we need
	// exactly 4 placements.
	REQUIRE_EQ( out.size(), 4u );
	REQUIRE( placements_cover_halo( oracle, interior, out ) );
	return true;
}

TEST( horizontal_domino_corona_1_succeeds_and_is_valid )
{
	shape_cells shape { { 0, 0 }, { 1, 0 } };
	Oracle oracle( shape );
	cell_set interior { { 0, 0 }, { 1, 0 } };
	std::vector<Placement> out;
	REQUIRE( oracle.find_completion( interior, out ) );
	REQUIRE( placements_cover_halo( oracle, interior, out ) );
	return true;
}

TEST( two_by_two_square_corona_1_succeeds )
{
	shape_cells shape { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	Oracle oracle( shape );
	cell_set interior { { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 } };
	std::vector<Placement> out;
	REQUIRE( oracle.find_completion( interior, out ) );
	REQUIRE( placements_cover_halo( oracle, interior, out ) );
	return true;
}

// ============================================================
// 3. Cross-check against Kaplan dataset
// ============================================================
//
// The M2.2 oracle answers "can the halo be tiled by congruent copies of S
// without overlapping the interior?" -- i.e., basic corona completion
// allowing holes in the further-out region. In the heesch-sat
// vocabulary that matches the *Hh*-with-extended-adjacency check, not the
// hole-free *Hc*. Hole-detection is a separate layer (M2's
// iterateUntilSimplyConnected analogue) and is not in M2.2's scope.
//
// Test references:
//   data/kaplan-2022/omino/07omino_0up.txt line 1
//     1 0 1 1 0 2 1 2 1 3 2 3 3 3                  Hc = 1, Hh = 1
//   data/kaplan-2022/omino/07omino_0up.txt line 5
//     2 0 2 1 0 2 1 2 2 2 3 2 2 3                  Hc = 0, Hh = 1
//   data/kaplan-2022/omino/09omino_0up.txt
//     0 0 1 0 3 0 4 0 0 1 1 1 2 1 3 1 4 1          Hc = 0, Hh = 0
//
// The Hh = 1 cases must succeed; the Hh = 0 case must fail. (The Hc =
// 0 / Hh = 1 case succeeds at the M2.2 layer because it has a corona
// completion if we permit further-out holes -- the hole-free check is
// what makes Hc = 0 in the dataset.)

TEST( kaplan_heptomino_hc1_hh1_has_corona_completion )
{
	const shape_cells shape =
		parse_shape( "1 0 1 1 0 2 1 2 1 3 2 3 3 3" );
	REQUIRE_EQ( shape.size(), 7u );
	Oracle oracle( shape );
	cell_set interior = as_set( shape );
	std::vector<Placement> out;
	REQUIRE( oracle.find_completion( interior, out ) );
	REQUIRE( placements_cover_halo( oracle, interior, out ) );
	return true;
}

TEST( kaplan_heptomino_hc0_hh1_still_has_corona_at_M22_layer )
{
	// Hc = 0 means hole-free corona doesn't exist; Hh = 1 means a corona
	// allowing holes in the further-out region does. M2.2 ships the
	// allowing-holes oracle, so this shape must succeed at our layer.
	// The hole-free distinction is M2.x's responsibility.
	const shape_cells shape =
		parse_shape( "2 0 2 1 0 2 1 2 2 2 3 2 2 3" );
	REQUIRE_EQ( shape.size(), 7u );
	Oracle oracle( shape );
	cell_set interior = as_set( shape );
	std::vector<Placement> out;
	REQUIRE( oracle.find_completion( interior, out ) );
	REQUIRE( placements_cover_halo( oracle, interior, out ) );
	return true;
}

TEST( kaplan_nonomino_hh0_has_no_corona_completion )
{
	// The first Hh = 0 polyomino in the dataset (n = 9). 5x2 rectangle
	// with the middle-bottom cell removed:
	//   row y=1:  X X X X X
	//   row y=0:  X X . X X
	// No congruent copy fits adjacent to the interior without overlap;
	// our oracle must return no completion.
	const shape_cells shape =
		parse_shape( "0 0 1 0 3 0 4 0 0 1 1 1 2 1 3 1 4 1" );
	REQUIRE_EQ( shape.size(), 9u );
	Oracle oracle( shape );
	cell_set interior = as_set( shape );
	std::vector<Placement> out;
	const bool ok = oracle.find_completion( interior, out );
	REQUIRE( !ok );
	REQUIRE_EQ( out.size(), 0u );
	return true;
}

// ============================================================
// 4. Behavioural / API
// ============================================================

TEST( count_completions_caps_correctly )
{
	// Monomino has many corona-1 completions (every choice of orientation
	// for each of the 4 halo monominoes). With cap=3, count must return 3.
	shape_cells shape { { 0, 0 } };
	Oracle oracle( shape );
	cell_set interior { { 0, 0 } };
	REQUIRE_EQ( oracle.count_completions( interior, 3 ), 3u );
	return true;
}

TEST( count_completions_zero_for_hh0_shape )
{
	// Same Hh = 0 nonomino as the find_completion failure case.
	const shape_cells shape =
		parse_shape( "0 0 1 0 3 0 4 0 0 1 1 1 2 1 3 1 4 1" );
	Oracle oracle( shape );
	cell_set interior = as_set( shape );
	REQUIRE_EQ( oracle.count_completions( interior, 0 ), 0u );
	return true;
}

TEST( oracle_rejects_empty_shape )
{
	bool threw = false;
	try {
		Oracle oracle( shape_cells {} );
	} catch ( const std::invalid_argument& ) {
		threw = true;
	}
	REQUIRE( threw );
	return true;
}

TEST( find_completion_records_diagnostics )
{
	shape_cells shape { { 0, 0 } };
	Oracle oracle( shape );
	cell_set interior { { 0, 0 } };
	std::vector<Placement> out;
	REQUIRE( oracle.find_completion( interior, out ) );
	REQUIRE( oracle.last_candidate_count() > 0u );
	REQUIRE( oracle.last_nodes_explored() > 0u );
	return true;
}

} // anonymous namespace

int main()
{
	int passed = 0, failed = 0;
	std::printf( "# heesch-forge corona oracle unit tests\n" );
	std::printf( "# %zu test cases registered\n\n", registry().size() );
	for ( const auto& tc : registry() ) {
		std::printf( "  %-58s ", tc.name );
		std::fflush( stdout );
		const bool ok = tc.fn();
		std::printf( "%s\n", ok ? "PASS" : "FAIL" );
		if ( ok ) ++passed; else ++failed;
	}
	std::printf( "\n# %d passed, %d failed\n", passed, failed );
	return failed == 0 ? 0 : 1;
}
