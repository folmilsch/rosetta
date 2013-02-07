// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/loophash/LoopHashLibraryOptions.fwd.hh
/// @brief Load the Loop Hash library using the resource manager
/// @author Tim Jacobs

#ifndef INCLUDED_core_conformation_symmetry_LoopHashLibraryOptions_fwd_hh
#define INCLUDED_core_conformation_symmetry_LoopHashLibraryOptions_fwd_hh

#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace loophash {

class LoopHashLibraryOptions;
typedef utility::pointer::owning_ptr< LoopHashLibraryOptions > LoopHashLibraryOptionsOP;
typedef utility::pointer::owning_ptr< LoopHashLibraryOptions const > LoopHashLibraryOptionsCOP;

} // namespace
} // namespace

#endif // include guard
