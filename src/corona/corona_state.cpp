// CoronaState implementation. See corona_state.hpp.

#include "corona_state.hpp"

#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace heesch_forge::corona {

namespace {

constexpr int kFormatVersion = 1;

} // namespace

const char* tag_of( Orientation o )
{
	switch ( o ) {
	case Orientation::R0:    return "R0";
	case Orientation::R90:   return "R90";
	case Orientation::R180:  return "R180";
	case Orientation::R270:  return "R270";
	case Orientation::M:     return "M";
	case Orientation::MR90:  return "MR90";
	case Orientation::MR180: return "MR180";
	case Orientation::MR270: return "MR270";
	}
	return "?";
}

Orientation orientation_of_tag( const std::string& tag )
{
	if ( tag == "R0" )    return Orientation::R0;
	if ( tag == "R90" )   return Orientation::R90;
	if ( tag == "R180" )  return Orientation::R180;
	if ( tag == "R270" )  return Orientation::R270;
	if ( tag == "M" )     return Orientation::M;
	if ( tag == "MR90" )  return Orientation::MR90;
	if ( tag == "MR180" ) return Orientation::MR180;
	if ( tag == "MR270" ) return Orientation::MR270;
	throw std::runtime_error( "unknown orientation tag: " + tag );
}

CoronaState::CoronaState( shape_cells base_shape )
	: base_shape_ { std::move( base_shape ) }
	, levels_ {}
{
	if ( base_shape_.empty() ) {
		throw std::invalid_argument(
			"CoronaState: base_shape must be non-empty" );
	}
	// Level 0 is canonical: the shape itself at origin, orientation R0.
	levels_.push_back( { Placement { { 0, 0 }, Orientation::R0 } } );
}

std::size_t CoronaState::add_level( std::vector<Placement> placements )
{
	levels_.push_back( std::move( placements ) );
	return levels_.size();
}

cell_set CoronaState::interior_cells() const
{
	cell_set out;
	for ( const auto& level : levels_ ) {
		for ( const auto& p : level ) {
			for ( const auto& c : placed_cells( base_shape_, p ) ) {
				out.insert( c );
			}
		}
	}
	return out;
}

cell_set CoronaState::halo_cells() const
{
	return halo_of( interior_cells() );
}

void CoronaState::write( std::ostream& os ) const
{
	os << "v " << kFormatVersion << '\n';
	os << "shape";
	for ( const auto& c : base_shape_ ) {
		os << ' ' << c.first << ' ' << c.second;
	}
	os << '\n';
	for ( std::size_t k = 0; k < levels_.size(); ++k ) {
		os << "level " << k << '\n';
		for ( const auto& p : levels_[k] ) {
			os << "  " << p.origin.first
			   << ' '  << p.origin.second
			   << ' '  << tag_of( p.orient )
			   << '\n';
		}
	}
}

CoronaState CoronaState::read( std::istream& is )
{
	std::string line;
	int version = 0;
	shape_cells base_shape;
	std::vector<std::vector<Placement>> levels;

	auto trim_left = []( std::string& s ) {
		std::size_t i = 0;
		while ( i < s.size() && ( s[i] == ' ' || s[i] == '\t' ) ) {
			++i;
		}
		s.erase( 0, i );
	};

	int current_level = -1;          // -1 means "no level header seen yet"

	while ( std::getline( is, line ) ) {
		// Strip CRLF if present.
		if ( !line.empty() && line.back() == '\r' ) {
			line.pop_back();
		}
		std::string stripped = line;
		trim_left( stripped );
		if ( stripped.empty() || stripped[0] == '#' ) {
			continue;
		}
		std::istringstream iss( stripped );
		std::string head;
		iss >> head;
		if ( head == "v" ) {
			iss >> version;
			if ( version != kFormatVersion ) {
				throw std::runtime_error(
					"CoronaState: unsupported version "
					+ std::to_string( version ) );
			}
			continue;
		}
		if ( head == "shape" ) {
			int x, y;
			while ( iss >> x >> y ) {
				base_shape.emplace_back( x, y );
			}
			if ( base_shape.empty() ) {
				throw std::runtime_error(
					"CoronaState: shape line had no cells" );
			}
			continue;
		}
		if ( head == "level" ) {
			int k;
			if ( !( iss >> k ) ) {
				throw std::runtime_error(
					"CoronaState: malformed level header: " + line );
			}
			if ( k != static_cast<int>( levels.size() ) ) {
				throw std::runtime_error(
					"CoronaState: level " + std::to_string( k )
					+ " out of order (expected level "
					+ std::to_string( levels.size() ) + ")" );
			}
			levels.emplace_back();
			current_level = k;
			continue;
		}
		// Otherwise: must be a placement line for the current level.
		if ( current_level < 0 ) {
			throw std::runtime_error(
				"CoronaState: placement before any level header: " + line );
		}
		std::istringstream pl( stripped );
		int ox, oy;
		std::string orient_tag;
		if ( !( pl >> ox >> oy >> orient_tag ) ) {
			throw std::runtime_error(
				"CoronaState: malformed placement: " + line );
		}
		levels.back().push_back( Placement {
			{ ox, oy },
			orientation_of_tag( orient_tag )
		} );
	}

	if ( base_shape.empty() ) {
		throw std::runtime_error(
			"CoronaState: missing shape line" );
	}
	if ( levels.empty()
	     || levels[0].size() != 1
	     || levels[0][0].origin != cell { 0, 0 }
	     || levels[0][0].orient != Orientation::R0 ) {
		throw std::runtime_error(
			"CoronaState: level 0 must be the canonical "
			"{(0, 0), R0} entry" );
	}

	CoronaState s { std::move( base_shape ) };
	// Replace the auto-initialised level 0 -- which already matches --
	// with the parsed levels (skipping the parsed level 0, since the
	// constructor already populated it identically).
	for ( std::size_t k = 1; k < levels.size(); ++k ) {
		s.add_level( std::move( levels[k] ) );
	}
	return s;
}

} // namespace heesch_forge::corona
