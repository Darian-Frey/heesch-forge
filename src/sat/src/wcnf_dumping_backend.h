#pragma once
//
// WCNF-dumping backend for HeeschSolver (M1.5 partial-corona scoring).
//
// Same shape as dumping_backend.h: wraps a real CMSat::SATSolver, intercepts
// every add_clause to buffer DIMACS-int form, runs CMSat for the actual
// solve so the program flows correctly. The new piece is `add_soft_cell_var`
// -- HeeschSolver::getClauses (under #ifdef HEESCH_BACKEND_WCNF_DUMP) marks
// every cell-var as a soft-candidate -- and a WCNF emitter that fires only
// when the inner solve returns UNSAT.
//
// Why only on UNSAT: a complete-corona SAT call that returns SAT is not
// interesting for partial-corona scoring (the full corona exists, score is
// trivially the total cell count). UNSAT is exactly when "how close did we
// come?" is the meaningful question, and it is also the only case where
// MaxSAT's optimal cost differs from zero.
//
// Output format: one .wcnf file per UNSAT solve, written under
// $HEESCH_DUMP_DIR (must be set, otherwise nothing is dumped). The format is
// the classic DIMACS WCNF the pysat library reads:
//
//   c heesch-forge M1.5 wcnf dump: instance N, call M
//   p wcnf <nvars> <nclauses+nsofts> <top>
//   <top> <hard literals...> 0
//   ...
//   1 <cell_var> 0
//   ...
//
// Selected via -DHEESCH_BACKEND_WCNF_DUMP, producing the `sat-wcnf-dump`
// build target.

#include <cryptominisat.h>      // for CMSat::Lit, CMSat::lbool, CMSat::SATSolver

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unordered_set>
#include <vector>

namespace wcnf_dumping_backend {

class WcnfDumpingSolver
{
public:
	WcnfDumpingSolver()
		: inner_ {}
		, dimacs_ {}
		, soft_cell_vars_ {}
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

	// HeeschSolver calls this from getClauses (under
	// #ifdef HEESCH_BACKEND_WCNF_DUMP) for every cell-var. var is the
	// 0-based CMSat index; we translate to 1-based DIMACS at dump time.
	void add_soft_cell_var( uint32_t var )
	{
		soft_cell_vars_.insert( var );
	}

	CMSat::lbool solve()
	{
		auto r = inner_.solve();
		if ( r == CMSat::l_False ) {
			dumpIfRequested_();
		}
		++call_id_;
		return r;
	}

	const std::vector<CMSat::lbool>& get_model() const
	{
		return inner_.get_model();
	}

	// Non-const because CMSat::SATSolver's get_last_* are non-const.
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
			"%s/inst-%05d-call%03d.wcnf", dir, my_id_, call_id_ );
		std::ofstream out( path );
		if ( !out ) {
			return;
		}
		const std::size_t nhard = dimacs_.size();
		const std::size_t nsoft = soft_cell_vars_.size();
		// Top weight in classic WCNF must exceed sum of all soft weights.
		// Every soft is unit weight, so top = nsoft + 1 suffices.
		const std::size_t top = nsoft + 1;

		out << "c heesch-forge M1.5 wcnf dump: instance " << my_id_
		    << ", call " << call_id_ << '\n';
		out << "c hard clauses: " << nhard
		    << "  soft (cell-var) clauses: " << nsoft
		    << "  top weight: " << top << '\n';
		out << "p wcnf " << nvars_ << ' ' << ( nhard + nsoft )
		    << ' ' << top << '\n';
		for ( const auto& cl : dimacs_ ) {
			out << top;
			for ( int lit : cl ) {
				out << ' ' << lit;
			}
			out << " 0\n";
		}
		for ( uint32_t v : soft_cell_vars_ ) {
			// Soft unit clause requesting cell-var v == true.
			// CMSat is 0-based, DIMACS is 1-based.
			out << "1 " << ( static_cast<int>( v ) + 1 ) << " 0\n";
		}
	}

	CMSat::SATSolver inner_;
	std::vector<std::vector<int>> dimacs_;
	std::unordered_set<uint32_t> soft_cell_vars_;
	uint32_t nvars_;
	int my_id_;
	int call_id_;

	static std::atomic<int> instance_counter_;
};

inline std::atomic<int> WcnfDumpingSolver::instance_counter_ { 0 };

} // namespace wcnf_dumping_backend
