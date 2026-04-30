#pragma once
//
// Kissat adapter for HeeschSolver (M1.2a).
//
// Same shape as cadical_backend.h: presents the CMSat::SATSolver subset that
// HeeschSolver actually uses, lets a single-typedef switch in heesch.h pick
// the backend at compile time. CMSat::Lit / CMSat::lbool remain the project-
// wide literal and truth-value types; the adapter translates at the boundary
// to Kissat's DIMACS-int API.
//
// Important caveat: **Kissat is non-incremental.** The C API exposes no way
// to add clauses after kissat_solve() has been called; re-solving the same
// instance is undefined. heesch::HeeschSolver::iterateUntilSimplyConnected
// (heesch.h ~line 875) breaks that contract by design -- it sits in a
// `while (solver.solve() == l_True)` loop and adds hole-banning clauses
// after every iteration. To make Kissat work without touching heesch.h, the
// adapter **buffers every clause** in a local vector and **rebuilds a fresh
// kissat instance on every solve() call**. The cost is O(num_clauses)
// re-feeding per solve; for the n = 11 / 12 walkback workload that is
// noticeable but bounded, and is the right place to absorb the constraint
// because the rest of the codebase stays backend-agnostic.
//
// Like CaDiCaL 1.7, Kissat 4.x's public API exposes no programmatic per-
// solve counters for conflicts / decisions / propagations -- only a
// `kissat_print_statistics()` call. The adapter therefore returns 0 from
// `get_last_*`; M0.5's wall-time and sat-call counters are still meaningful.

#include <cryptominisat.h>      // for CMSat::Lit, CMSat::lbool

extern "C" {
#include <kissat.h>
}

#include <cstdint>
#include <vector>

namespace kissat_backend {

class KissatSolver
{
public:
	KissatSolver()
		: solver_ { nullptr }
		, nvars_ { 0 }
		, clauses_ {}
		, last_model_ {}
	{}

	~KissatSolver()
	{
		if ( solver_ ) {
			kissat_release( solver_ );
		}
	}

	// Movable but not copyable: Kissat owns process-side state, copying
	// would double-free.
	KissatSolver( const KissatSolver& ) = delete;
	KissatSolver& operator=( const KissatSolver& ) = delete;
	KissatSolver( KissatSolver&& other ) noexcept
		: solver_ { other.solver_ }
		, nvars_ { other.nvars_ }
		, clauses_ { std::move( other.clauses_ ) }
		, last_model_ { std::move( other.last_model_ ) }
	{
		other.solver_ = nullptr;
	}
	KissatSolver& operator=( KissatSolver&& other ) noexcept
	{
		if ( this != &other ) {
			if ( solver_ ) {
				kissat_release( solver_ );
			}
			solver_ = other.solver_;
			nvars_ = other.nvars_;
			clauses_ = std::move( other.clauses_ );
			last_model_ = std::move( other.last_model_ );
			other.solver_ = nullptr;
		}
		return *this;
	}

	void new_vars( uint32_t n )
	{
		nvars_ += n;
		last_model_.assign( nvars_, CMSat::l_Undef );
	}

	// Buffer the clause as DIMACS ints; the actual kissat instance is built
	// at solve() time.
	bool add_clause( const std::vector<CMSat::Lit>& cl )
	{
		std::vector<int> dimacs;
		dimacs.reserve( cl.size() );
		for ( const auto& l : cl ) {
			const int v = static_cast<int>( l.var() ) + 1;
			dimacs.push_back( l.sign() ? -v : v );
		}
		clauses_.push_back( std::move( dimacs ) );
		return true;
	}

	// Always rebuild a fresh kissat instance (it is non-incremental).
	CMSat::lbool solve()
	{
		if ( solver_ ) {
			kissat_release( solver_ );
		}
		solver_ = kissat_init();
		// Suppress Kissat's stderr chatter: harness output mixes with the
		// JSONL log otherwise. -DKISSAT_VERBOSE=1 in the Makefile turns it
		// back on for diagnosis.
		kissat_set_option( solver_, "quiet", 1 );
		if ( nvars_ > 0 ) {
			kissat_reserve( solver_, static_cast<int>( nvars_ ) );
		}
		for ( const auto& cl : clauses_ ) {
			for ( int lit : cl ) {
				kissat_add( solver_, lit );
			}
			kissat_add( solver_, 0 );
		}
		const int r = kissat_solve( solver_ );
		if ( r == 10 ) {
			for ( uint32_t v = 0; v < nvars_; ++v ) {
				const int dimacs_var = static_cast<int>( v ) + 1;
				const int x = kissat_value( solver_, dimacs_var );
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

	// Per-solve counters: not exposed by Kissat's public C API.
	uint64_t get_last_conflicts()    const { return 0; }
	uint64_t get_last_decisions()    const { return 0; }
	uint64_t get_last_propagations() const { return 0; }

private:
	kissat* solver_;
	uint32_t nvars_;
	std::vector<std::vector<int>> clauses_;
	std::vector<CMSat::lbool> last_model_;
};

} // namespace kissat_backend
