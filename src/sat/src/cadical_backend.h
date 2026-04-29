#pragma once
//
// CaDiCaL adapter for HeeschSolver (M1.1).
//
// Mimics the subset of CMSat::SATSolver that heesch.h actually uses, so the
// rest of the solver code can stay backend-agnostic via a typedef:
//
//   #ifdef HEESCH_BACKEND_CADICAL
//     using SATSolverImpl = cadical_backend::CadicalSolver;
//   #else
//     using SATSolverImpl = CMSat::SATSolver;
//   #endif
//
// The adapter uses CMSat::Lit / CMSat::lbool as the project-wide literal and
// truth-value types (defined in sat.h via cryptominisat.h). Both backend
// builds therefore link libcryptominisat5 for those small POD types. The
// CaDiCaL build additionally links libcadical for the actual SAT engine.
//
// CaDiCaL 1.7's public C++ API does not expose programmatic per-solve
// counters for conflicts / decisions / propagations -- only a `statistics()`
// call that prints to stdout. The adapter therefore returns 0 from the
// `get_last_*` accessors; M0.5's wall-time + sat-call counters still work.
// If a future CaDiCaL release adds the accessors, only this file changes.

#include <cryptominisat.h>      // for CMSat::Lit, CMSat::lbool
#include <cadical.hpp>

#include <cstdint>
#include <vector>

namespace cadical_backend {

class CadicalSolver
{
public:
	CadicalSolver()
		: solver_ {}
		, nvars_ { 0 }
		, last_model_ {}
	{}

	// Reserve variable space. CMSat treats this as creating new vars; CaDiCaL
	// creates vars on demand when added, so we only need to remember the
	// count for sizing the post-solve model vector.
	void new_vars( uint32_t n )
	{
		nvars_ += n;
		last_model_.assign( nvars_, CMSat::l_Undef );
		// reserve() bumps CaDiCaL's internal max-var hint; harmless if smaller.
		solver_.reserve( static_cast<int>( nvars_ ) );
	}

	// Add a clause expressed as a vector of CMSat::Lit. Translation:
	//   CMSat var v (0-based) -> DIMACS lit (v + 1), negated if l.sign().
	bool add_clause( const std::vector<CMSat::Lit>& cl )
	{
		for ( const auto& l : cl ) {
			int dimacs_var = static_cast<int>( l.var() ) + 1;
			solver_.add( l.sign() ? -dimacs_var : dimacs_var );
		}
		solver_.add( 0 );  // CaDiCaL clause terminator
		return true;
	}

	// Solve the current formula and (if SAT) populate the model. Returns the
	// CMSat-style lbool so callers can keep their existing comparisons
	// against CMSat::l_True / l_False / l_Undef.
	CMSat::lbool solve()
	{
		const int r = solver_.solve();
		if ( r == 10 ) {
			for ( uint32_t v = 0; v < nvars_; ++v ) {
				const int dimacs_var = static_cast<int>( v ) + 1;
				const int x = solver_.val( dimacs_var );
				last_model_[v] = ( x > 0 ) ? CMSat::l_True
				              : ( x < 0 ) ? CMSat::l_False
				                          : CMSat::l_Undef;
			}
			return CMSat::l_True;
		}
		if ( r == 20 ) {
			return CMSat::l_False;
		}
		return CMSat::l_Undef;
	}

	const std::vector<CMSat::lbool>& get_model() const
	{
		return last_model_;
	}

	// Per-solve counters. CaDiCaL 1.7 does not expose these via its public
	// API; returning 0 is a deliberate gap recorded in
	// benchmarks/baseline/README.md and src/sat/UPSTREAM.md.
	uint64_t get_last_conflicts()    const { return 0; }
	uint64_t get_last_decisions()    const { return 0; }
	uint64_t get_last_propagations() const { return 0; }

private:
	CaDiCaL::Solver solver_;
	uint32_t nvars_;
	std::vector<CMSat::lbool> last_model_;
};

} // namespace cadical_backend
