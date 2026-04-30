#pragma once
//
// CNF-dumping backend for HeeschSolver (M1.3 stage-2 empirical probe).
//
// Wraps CMSat::SATSolver and tees every clause into an internal buffer.
// On each solve() call, writes the current CNF state to a file under
// $HEESCH_DUMP_DIR (one file per solve invocation, names embed the
// solver-instance id and the per-instance call number) and then forwards
// to CMSat for an actual solve so the rest of the program flows correctly
// and the regression suite still passes. Otherwise the dump is decoupled
// from the solver -- you can run the same binary against any subset of
// the dataset, point HEESCH_DUMP_DIR at a fresh directory, and end up with
// one DIMACS instance per CNF heesch-sat actually feeds the SAT solver.
//
// Selected at compile time via -DHEESCH_BACKEND_DUMP, producing the
// `sat-dump` target in the Makefile.

#include <cryptominisat.h>      // for CMSat::Lit, CMSat::lbool, CMSat::SATSolver

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace dumping_backend {

class DumpingSolver
{
public:
	DumpingSolver()
		: inner_ {}
		, dimacs_ {}
		, nvars_ { 0 }
		, my_id_ { instance_counter_.fetch_add( 1 ) }
		, call_id_ { 0 }
	{}

	void new_vars( uint32_t n )
	{
		nvars_ += n;
		inner_.new_vars( n );
	}

	bool add_clause( const std::vector<CMSat::Lit>& cl )
	{
		std::vector<int> dimacs;
		dimacs.reserve( cl.size() );
		for ( const auto& l : cl ) {
			const int v = static_cast<int>( l.var() ) + 1;
			dimacs.push_back( l.sign() ? -v : v );
		}
		dimacs_.push_back( std::move( dimacs ) );
		return inner_.add_clause( cl );
	}

	CMSat::lbool solve()
	{
		dumpIfRequested_();
		++call_id_;
		return inner_.solve();
	}

	const std::vector<CMSat::lbool>& get_model() const
	{
		return inner_.get_model();
	}

	// Non-const because CMSat::SATSolver's get_last_* accessors are
	// non-const. HeeschSolver::runAndAccount calls these on a non-const
	// reference so this is safe.
	uint64_t get_last_conflicts()    { return inner_.get_last_conflicts(); }
	uint64_t get_last_decisions()    { return inner_.get_last_decisions(); }
	uint64_t get_last_propagations() { return inner_.get_last_propagations(); }

private:
	void dumpIfRequested_()
	{
		const char* dir = std::getenv( "HEESCH_DUMP_DIR" );
		if ( !dir || !*dir ) {
			return;
		}
		char path[512];
		std::snprintf( path, sizeof( path ),
			"%s/inst-%05d-call%03d.cnf", dir, my_id_, call_id_ );
		std::ofstream out( path );
		if ( !out ) {
			return;
		}
		out << "c heesch-forge M1.3 dump: instance " << my_id_
		    << ", call " << call_id_ << '\n';
		out << "p cnf " << nvars_ << ' ' << dimacs_.size() << '\n';
		for ( const auto& cl : dimacs_ ) {
			for ( int lit : cl ) {
				out << lit << ' ';
			}
			out << "0\n";
		}
	}

	CMSat::SATSolver inner_;
	std::vector<std::vector<int>> dimacs_;
	uint32_t nvars_;
	int my_id_;
	int call_id_;

	static std::atomic<int> instance_counter_;
};

inline std::atomic<int> DumpingSolver::instance_counter_ { 0 };

} // namespace dumping_backend
