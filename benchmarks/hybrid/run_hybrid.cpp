// M2.4 hybrid-pipeline harness.
//
// Reads a Kaplan-2022 polyomino dataset file (paired lines: cell
// coordinates, then `Hc = N Hh = M`), runs the DLX-only end-to-end
// `compute_heesch` over each shape, and writes a CSV row per shape
// to stdout:
//
//   coords,n,hc_dataset,hh_dataset,hc_computed,hh_computed,ok,oracle_calls,wall_ms
//
// `ok` is 1 iff (hc_computed, hh_computed) == (hc_dataset, hh_dataset).
// A non-zero exit code means at least one shape disagreed; the
// summary line at the end of stdout repeats the count of mismatches.
//
// Usage:
//   run_hybrid <dataset-file>
//
// The command is one-input-file-per-process so the regression script
// (run_regression.sh) can iterate file-by-file.

#include "../../src/corona/heesch_solver.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

using ::heesch_forge::corona::compute_heesch;
using ::heesch_forge::corona::HeeschResult;
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
	if ( argc < 2 ) {
		std::fprintf( stderr, "usage: run_hybrid <dataset-file>\n" );
		return 2;
	}

	std::ifstream f { argv[1] };
	if ( !f ) {
		std::fprintf( stderr, "cannot open %s\n", argv[1] );
		return 2;
	}

	std::cout << "coords,n,hc_dataset,hh_dataset,hc_computed,"
	             "hh_computed,ok,oracle_calls,wall_ms\n";

	std::string coords_line, annot_line;
	int total = 0, matches = 0, mismatches = 0;
	while ( std::getline( f, coords_line ) ) {
		if ( !std::getline( f, annot_line ) ) {
			std::fprintf( stderr, "%s: odd line count\n", argv[1] );
			return 2;
		}
		shape_cells shape = parse_coords( coords_line );
		int ds_hc = -1, ds_hh = -1;
		if ( !parse_hc_hh( annot_line, ds_hc, ds_hh ) ) {
			std::fprintf( stderr, "%s: bad annotation: %s\n",
				argv[1], annot_line.c_str() );
			return 2;
		}

		const auto t0 = std::chrono::steady_clock::now();
		HeeschResult r;
		try {
			r = compute_heesch( shape );
		} catch ( const std::exception& e ) {
			std::fprintf( stderr, "compute_heesch threw: %s\n", e.what() );
			return 2;
		}
		const auto t1 = std::chrono::steady_clock::now();
		const double ms =
			std::chrono::duration<double, std::milli>( t1 - t0 ).count();

		const bool ok = ( static_cast<int>( r.hc ) == ds_hc )
		             && ( static_cast<int>( r.hh ) == ds_hh );

		std::cout << '"' << coords_line << "\","
			<< shape.size() << ','
			<< ds_hc << ',' << ds_hh << ','
			<< r.hc  << ',' << r.hh  << ','
			<< ( ok ? 1 : 0 ) << ','
			<< r.oracle_calls << ','
			<< ms
			<< '\n';

		++total;
		if ( ok ) ++matches; else ++mismatches;
	}

	std::cerr << "# " << argv[1] << ": "
	          << matches << '/' << total << " match, "
	          << mismatches << " mismatch\n";

	return mismatches == 0 ? 0 : 1;
}
