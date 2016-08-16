// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   --path--/--class--.hh
/// @brief  Creator for --class--
/// @author --name-- (--email--)

#ifndef INCLUDED_--path_underscore--_--class--Creator_HH
#define INCLUDED_--path_underscore--_--class--Creator_HH

// Package headers
#include <core/select/residue_selector/ResidueSelectorCreator.hh>

// Utility headers
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

--namespace--

class --class--Creator : public core::select::residue_selector::ResidueSelectorCreator {
public:
	virtual core::select::residue_selector::ResidueSelectorOP create_residue_selector() const;

	virtual std::string keyname() const;

	virtual void provide_xml_schema( utility::tag::XMLSchemaDefinition & ) const;
};

--end_namespace--

#endif //INCLUDED_--path--_--class--.hh
