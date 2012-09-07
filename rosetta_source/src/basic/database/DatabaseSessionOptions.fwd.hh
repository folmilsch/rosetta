// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   basic/database/DatabaseSessionOptions.fwd.hh
/// @brief  load the database session
/// @author Tim Jacobs

#ifndef INCLUDED_basic_database_DatabaseSessionOptions_fwd_hh
#define INCLUDED_basic_database_DatabaseSessionOptions_fwd_hh

#include <utility/pointer/owning_ptr.hh>


namespace basic {
namespace database {

class DatabaseSessionOptions;
typedef utility::pointer::owning_ptr< DatabaseSessionOptions > DatabaseSessionOptionsOP;
typedef utility::pointer::owning_ptr< DatabaseSessionOptions const > DatabaseSessionOptionsCOP;

} // database
} // basic

#endif // include guard

