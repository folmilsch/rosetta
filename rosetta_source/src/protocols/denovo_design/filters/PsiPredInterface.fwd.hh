// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/protocols/denovo_design/filters/PsiPredInterface.fwd.hh
/// @brief header file for class which interfaces with psipred
/// @detailed
/// @author Tom Linsky (tlinsky@uw.edu)

#ifndef INCLUDED_protocols_denovo_design_filters_psipredinterface_fwd_hh
#define INCLUDED_protocols_denovo_design_filters_psipredinterface_fwd_hh

// Unit Headers

// Package headers

// Project headers
#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace denovo_design {
namespace filters {

// Forward
class PsiPredInterface;

// Types
typedef utility::pointer::owning_ptr< PsiPredInterface > PsiPredInterfaceOP;
typedef utility::pointer::owning_ptr< PsiPredInterface const > PsiPredInterfaceCOP;

}
}
}
#endif
