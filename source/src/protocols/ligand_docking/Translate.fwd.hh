// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/ligand_docking/Translate.hh
///
/// @brief
/// @author Ian W. Davis

#ifndef INCLUDED_protocols_ligand_docking_Translate_fwd_hh
#define INCLUDED_protocols_ligand_docking_Translate_fwd_hh

#include <utility/pointer/owning_ptr.hh>
#include <utility/vector1.hh>

namespace protocols {
namespace ligand_docking {

class Translate; // fwd declaration
typedef utility::pointer::shared_ptr< Translate > TranslateOP;
typedef utility::pointer::shared_ptr< Translate const > TranslateCOP;
typedef utility::vector1<TranslateOP> TranslateOPs;
typedef utility::vector1<TranslateCOP> TranslateCOPs;

} // namespace ligand_docking
} // namespace protocols

#endif // INCLUDED_protocols_ligand_docking_Translate_HH
