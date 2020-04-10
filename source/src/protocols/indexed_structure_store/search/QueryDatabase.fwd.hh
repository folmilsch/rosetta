// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file ./src/protocols/indexed_structure_store/search/QueryDatabase.fwd.hh
/// @brief Database for generic alignment-based queries over structure stores.
/// @author Alex Ford (fordas@uw.edu)
//

#ifndef INCLUDED_protocols_indexed_structure_store_search_QueryDatabase_FWD_HH
#define INCLUDED_protocols_indexed_structure_store_search_QueryDatabase_FWD_HH

#include <utility/pointer/owning_ptr.hh>
#include <utility/pointer/access_ptr.hh>

namespace protocols { namespace indexed_structure_store { namespace search {

class StructureDatabase;

typedef utility::pointer::shared_ptr<StructureDatabase> StructureDatabaseOP;
typedef utility::pointer::shared_ptr<StructureDatabase const> StructureDatabaseCOP;

} } }

#endif
