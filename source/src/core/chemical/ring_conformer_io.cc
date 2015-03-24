// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    core/chemical/ring_conformer_io.cc
/// @brief   Database input/output function definitions for ring-conformer-specific data.
/// @author  Labonte <JWLabonte@jhu.edu>

// Unit header
#include <core/chemical/ring_conformer_io.hh>
#include <core/chemical/RingConformerSet.hh>

// Project headers
#include <core/types.hh>

// Utility headers
#include <utility/exit.hh>
#include <utility/string_util.hh>
#include <utility/file/file_sys_util.hh>
#include <utility/io/izstream.hh>
#include <utility/io/util.hh>

// Basic headers
#include <basic/Tracer.hh>

// C++ headers
#include <sstream>


// Construct tracer.
static thread_local basic::Tracer TR( "core.chemical.ring_conformer_io" );


namespace core {
namespace chemical {

// Return a list of ring conformers, read from a database file.
utility::vector1< RingConformer >
read_conformers_from_database_file_for_ring_size( std::string const & filename, core::Size ring_size )
{
	using namespace std;
	using namespace utility;

	if ( ring_size < 3 ) {
		utility_exit_with_message( "Cannot load database file: An invalid ring size was provided." );
	}

	vector1< string > const lines( io::get_lines_from_file_data( filename ) );
	vector1< RingConformer > conformers;

	Size const n_lines( lines.size() );
	for ( uint i( 1 ); i <= n_lines; ++i ) {
		istringstream line_word_by_word( lines[ i ] );
		RingConformer conformer;
		Real junk;  // a place to throw out unneeded angles from the tables in the database

		// We need 3 less than the number of nu angles to define a ring conformer by Cremer-Pople parameters.
		conformer.CP_parameters.resize( ring_size - 3 );

		// We only need 2 less than the number of nu angles to define a ring conformer by internal angles.
		conformer.nu_angles.resize( ring_size - 2 );

		// We only need 1 less than the ring size for tau angles.
		conformer.tau_angles.resize( ring_size - 1 );

		line_word_by_word >> conformer.specific_name >> conformer.general_name >> conformer.degeneracy;

		for ( uint parameter( 1 ); parameter <= ring_size - 3; ++parameter ) {
			line_word_by_word >> conformer.CP_parameters[ parameter ];
		}

		for ( uint nu( 1 ); nu <= ring_size - 2; ++nu ) {
			line_word_by_word >> conformer.nu_angles[ nu ];
		}

		// Ignore the last two nu angle values.
		line_word_by_word >> junk >> junk;

		for ( uint tau( 1 ); tau <= ring_size - 1; ++tau ) {
			line_word_by_word >> conformer.tau_angles[ tau ];
		}

		// Ignore the last tau angle value.
		line_word_by_word >> junk;

		conformers.push_back( conformer );
	}

	TR.Debug << "Read " << conformers.size() << " " <<
			ring_size << "-membered ring conformers from the carbohydrate database." << endl;

	return conformers;
}

}  // namespace chemical
}  // namespace core
