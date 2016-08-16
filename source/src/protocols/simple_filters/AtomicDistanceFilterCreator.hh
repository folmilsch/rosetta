// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/simple_filters/AtomicDistanceFilterCreator.hh
/// @brief  FilterCreator for the AtomicDistanceFilter
/// @author Rocco Moretti (rmoretti@uw.edu)

#ifndef INCLUDED_protocols_simple_filters_AtomicDistanceFilterCreator_hh
#define INCLUDED_protocols_simple_filters_AtomicDistanceFilterCreator_hh


// Package Headers
#include <protocols/filters/FilterCreator.hh>

// Utility Headers
#include <utility/pointer/ReferenceCount.hh>

// c++ headers
#include <string>

namespace protocols {
namespace simple_filters {

class AtomicDistanceFilterCreator : public protocols::filters::FilterCreator
{
public:
	virtual filters::FilterOP create_filter() const;
	virtual std::string keyname() const;
};


} //namespace filters
} //namespace protocols

#endif
