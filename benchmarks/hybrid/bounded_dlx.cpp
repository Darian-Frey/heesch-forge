// M2.6-followup-A: bounded-DLX entry point for the joint pipeline.
//
// Reads a Kaplan-2022-format dataset (paired lines: cell coords,
// then `Hc = N Hh = M`), runs `compute_heesch_bounded` per shape, and
// writes a CSV row with the bounded-DLX outcome plus -- on budget
// exhaustion -- the path to a CoronaState file the orchestrator can
// hand to `sat -seed`.
//
// Usage:
//   bounded_dlx <dataset-file> <budget-nodes> <state-dir>
//
// CSV columns: coords,n,hc_dataset,hh_dataset,complete,
//              hc_dlx,hh_dlx,partial_depth,partial_state_path,
//              oracle_calls,nodes_total,wall_ms

#include "../../src/corona/heesch_solver.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

using ::heesch_forge::corona::compute_heesch_bounded;
using ::heesch_forge::corona::BoundedHeeschResult;
using ::heesch_forge::corona::shape_cells;

namespace {

shape_cells parse_coords( const std::string& line )
{
	std::istringstream is { line };
	int x, y;
	shape_cells out;
	while ( is >> x >> y ) {
		out.emplace_back( x, y );
	}
	return out;
}

bool parse_hc_hh( const std::string& line, int& hc, int& hh )
{
	const std::regex re { R"(Hc\s*=\s*(\d+)\s+Hh\s*=\s*(\d+))" };
	std::smatch m;
	if ( !std::regex_search( line, m, re ) ) {
		return false;
	}
	hc = std::atoi( m[1].str().c_str() );
	hh = std::atoi( m[2].str().c_str() );
	return true;
}

} // namespace

int main( int argc, char** argv )
{
	if ( argc < 4 ) {
		std::fprintf( stderr,
			"usage: bounded_dlx <dataset-file> <budget-nodes> "
			"<state-dir>\n" );
		return 2;
	}

	const std::string dataset_path = argv[1];
	const std::size_t budget = static_cast<std::size_t>(
		std::atoll( argv[2] ) );
	const std::filesystem::path state_dir = argv[3];

	std::filesystem::create_directories( state_dir );

	std::ifstream f { dataset_path };
	if ( !f ) {
		std::fprintf( stderr, "cannot open %s\n", dataset_path.c_str() );
		return 2;
	}

	std::cout << "coords,n,hc_dataset,hh_dataset,complete,"
	             "hc_dlx,hh_dlx,partial_depth,partial_state_path,"
	             "oracle_calls,nodes_total,wall_ms\n";

	std::string coords_line, annot_line;
	int total = 0, completed = 0, exhausted = 0, idx = 0;
	while ( std::getline( f, coords_line ) ) {
		if ( !std::getline( f, annot_line ) ) {
			std::fprintf( stderr, "%s: odd line count\n",
				dataset_path.c_str() );
			return 2;
		}
		shape_cells shape = parse_coords( coords_line );
		int ds_hc = -1, ds_hh = -1;
		if ( !parse_hc_hh( annot_line, ds_hc, ds_hh ) ) {
			std::fprintf( stderr, "%s: bad annotation: %s\n",
				dataset_path.c_str(), annot_line.c_str() );
			return 2;
		}
		++idx;

		const auto t0 = std::chrono::steady_clock::now();
		BoundedHeeschResult r;
		try {
			r = compute_heesch_bounded( shape, /*max_level=*/7, budget );
		} catch ( const std::exception& e ) {
			std::fprintf( stderr, "compute_heesch_bounded threw: %s\n",
				e.what() );
			return 2;
		}
		const auto t1 = std::chrono::steady_clock::now();
		const double ms =
			std::chrono::duration<double, std::milli>( t1 - t0 ).count();

		std::string state_path;
		std::size_t partial_depth = 0;
		if ( !r.complete && r.partial_state.has_value() ) {
			std::ostringstream name;
			name << "shape" << std::setw( 5 ) << std::setfill( '0' ) << idx
				<< ".cstate";
			std::filesystem::path full = state_dir / name.str();
			std::ofstream so { full };
			r.partial_state->write( so );
			state_path = full.string();
			partial_depth = r.partial_state->depth();
			++exhausted;
		} else {
			++completed;
		}

		std::cout << '"' << coords_line << "\","
			<< shape.size() << ','
			<< ds_hc << ',' << ds_hh << ','
			<< ( r.complete ? 1 : 0 ) << ','
			<< r.result.hc << ',' << r.result.hh << ','
			<< partial_depth << ','
			<< state_path << ','
			<< r.oracle_calls << ','
			<< r.nodes_explored_total << ','
			<< ms << '\n';

		++total;
	}

	std::cerr << "# bounded_dlx " << dataset_path << " budget=" << budget
		<< ": " << completed << "/" << total << " completed, "
		<< exhausted << " budget-exhausted\n";

	return 0;
}
