// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/fold_from_loops/RelaseConstraintFromResidueMoverCreator.hh
/// @brief Remove constraints from the residues specified by a ResidueSelector
/// @author Jaume Bonet (jaume.bonet@gmail.com)

#ifndef INCLUDED_protocols_constraint_generator_AddConstraints_hh
#define INCLUDED_protocols_constraint_generator_AddConstraints_hh

// Unit headers
#include <protocols/fold_from_loops/ReleaseConstraintFromResidueMover.fwd.hh>
#include <protocols/moves/Mover.hh>

// Protocol headers

// Core headers
#include <core/pose/Pose.fwd.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

// Basic/Utility headers
#include <basic/datacache/DataMap.fwd.hh>

namespace protocols {
namespace fold_from_loops {

///@brief Adds constraints generated by ConstraintGenerators to a pose
class ReleaseConstraintFromResidueMover : public protocols::moves::Mover {

public:
	ReleaseConstraintFromResidueMover();
	ReleaseConstraintFromResidueMover( core::select::residue_selector::ResidueSelectorCOP const & selector );

	// destructor (important for properly forward-declaring smart-pointer members)
	~ReleaseConstraintFromResidueMover() override;

	void
	apply( core::pose::Pose & pose ) override;

	/// @brief parse XML tag (to use this Mover in Rosetta Scripts)
	void
	parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data,
		protocols::filters::Filters_map const & filters,
		protocols::moves::Movers_map const & movers,
		core::pose::Pose const & pose ) override;

	/// @brief required in the context of the parser/scripting scheme
	protocols::moves::MoverOP
	fresh_instance() const override;

	/// @brief required in the context of the parser/scripting scheme
	protocols::moves::MoverOP
	clone() const override;

	void
	set_residue_selector( core::select::residue_selector::ResidueSelectorCOP const & selector );

	void
	set_residue_selector( core::select::residue_selector::ResidueSelector const & selector );

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:
	core::select::residue_selector::ResidueSelectorCOP selector_;

};

} //protocols
} //fold_from_loops

#endif //INCLUDED_protocols_constraint_generator_AddConstraints_hh
