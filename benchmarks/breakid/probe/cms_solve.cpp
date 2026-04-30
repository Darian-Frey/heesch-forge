// Minimal DIMACS-driven CMSat runner used by the M1.3 BreakID probe.
//
// Reads a DIMACS CNF, feeds it to a fresh CryptoMiniSat solver, prints one
// line:
//   <path> result=SAT|UNSAT|UNKNOWN wall_ms=<f> conflicts=<n> decisions=<n>
//
// Used by run_probe.sh to A/B-test CMSat's behaviour on a raw heesch-sat
// CNF dump versus the same CNF after BreakID has appended its symmetry-
// breaking clauses.

#include <cryptominisat.h>

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main( int argc, char** argv )
{
	if ( argc < 2 ) {
		std::cerr << "usage: cms_solve <file.cnf>\n";
		return 2;
	}
	std::ifstream f( argv[1] );
	if ( !f ) {
		std::cerr << "cannot open " << argv[1] << "\n";
		return 2;
	}

	CMSat::SATSolver s;
	std::string line;
	int nvars = 0;
	while ( std::getline( f, line ) ) {
		if ( line.empty() || line[0] == 'c' ) {
			continue;
		}
		if ( line[0] == 'p' ) {
			std::istringstream is( line );
			std::string tok;
			is >> tok >> tok >> nvars;
			s.new_vars( static_cast<uint32_t>( nvars ) );
			continue;
		}
		std::istringstream is( line );
		std::vector<CMSat::Lit> cl;
		int x;
		while ( is >> x ) {
			if ( x == 0 ) break;
			uint32_t v = static_cast<uint32_t>( std::abs( x ) - 1 );
			cl.push_back( CMSat::Lit( v, x < 0 ) );
		}
		if ( !cl.empty() ) {
			s.add_clause( cl );
		}
	}

	auto t0 = std::chrono::steady_clock::now();
	auto r = s.solve();
	auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::steady_clock::now() - t0 ).count();
	const char* label =
		( r == CMSat::l_True )  ? "SAT"
		: ( r == CMSat::l_False ) ? "UNSAT"
		: "UNKNOWN";
	std::cout << argv[1]
		<< " result=" << label
		<< " wall_ms=" << ( ns / 1.0e6 )
		<< " conflicts=" << s.get_last_conflicts()
		<< " decisions=" << s.get_last_decisions()
		<< "\n";
	return 0;
}
