// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/conformation/symmetry/SymmDataLoader.fwd.hh
/// @brief  load the SymmData data-structure, which is used to configure symmetric poses.
/// @author Matthew O'Meara (mattjomeara@gmail.com)

#ifndef INCLUDED_core_conformation_symmetry_SymmDataLoader_fwd_hh
#define INCLUDED_core_confomration_symmetry_SymmDataLoader_fwd_hh

#include <utility/pointer/owning_ptr.hh>

namespace core {
namespace conformation {
namespace symmetry {

class SymmDataLoader;
typedef utility::pointer::owning_ptr< SymmDataLoader > SymmDataLoaderOP;
typedef utility::pointer::owning_ptr< SymmDataLoader const > SymmDataLoaderCOP;

} // namespace
} // namespace
} // namespace

#endif // include guard

