// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/loophash/LoopHashLibraryLoader.hh
/// @brief Load the Loop Hash library using the resource manager
/// @author Tim Jacobs

#ifndef INCLUDED_protocols_loophash_LoopHashLibraryLoader_hh
#define INCLUDED_protocols_loophash_LoopHashLibraryLoader_hh

//unit headers
#include <protocols/loophash/LoopHashLibraryLoader.fwd.hh>
#include <protocols/loophash/LoopHashLibraryOptions.hh>
#include <basic/resource_manager/ResourceLoader.hh>

//package headers
#include <basic/resource_manager/ResourceOptions.hh>
#include <basic/resource_manager/types.hh>

//utility headers
#include <utility/pointer/ReferenceCount.hh>
#include <utility/tag/Tag.fwd.hh>

//C++ headers
#include <istream>

namespace protocols {
namespace loophash {

class LoopHashLibraryLoader : public basic::resource_manager::ResourceLoader
{
public:
	virtual ~LoopHashLibraryLoader() {}

	virtual
	basic::resource_manager::ResourceOP
	create_resource(
		basic::resource_manager::ResourceOptions const & options,
		basic::resource_manager::LocatorID const &,
		std::istream &
	) const;

	virtual
	basic::resource_manager::ResourceOptionsOP
	default_options() const { return new LoopHashLibraryOptions();}
};

} // namespace
} // namespace

#endif
