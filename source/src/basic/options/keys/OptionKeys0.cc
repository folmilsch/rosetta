// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   basic/options/keys/OptionKeys.cc
/// @brief  basic::options::OptionKeys collection
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)

// Unit headers
#include <basic/options/keys/OptionKeys.hh>
#include <basic/options/option.cc.include.gen.hh>

namespace basic {
namespace options {
namespace OptionKeys {


// Include options generated by Python script
#include <basic/options/keys/OptionKeys.cc.gen0.hh>
/// @brief Lookup functors
#include <utility/keys/KeyLookup.functors.ii>


} // namespace OptionKeys
} // namespace options
} // namespace basic
