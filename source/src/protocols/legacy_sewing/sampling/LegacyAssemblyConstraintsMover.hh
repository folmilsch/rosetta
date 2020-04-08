// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file LegacyAssemblyConstraintsMover.hh
///
/// @brief A simple mover that reads a NativeRotamersFile generated by a an AssemblyMover run and generates the necessary
/// ResidueType constraints to add to the pose
/// @author Tim Jacobs


#ifndef INCLUDED_protocols_legacy_sewing_sampling_LegacyAssemblyConstraintsMover_HH
#define INCLUDED_protocols_legacy_sewing_sampling_LegacyAssemblyConstraintsMover_HH

// Unit Headers
#include <protocols/legacy_sewing/sampling/LegacyAssemblyConstraintsMover.fwd.hh>
#include <protocols/moves/Mover.hh>
#include <core/pack/task/operation/TaskOperation.hh>
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

// Package Headers
#include <protocols/legacy_sewing/util/io.hh>

namespace protocols {
namespace legacy_sewing  {

//////////////LegacyReadNativeRotamersFile/////////////////

class LegacyReadNativeRotamersFile : public core::pack::task::operation::TaskOperation
{
public:
	typedef core::pack::task::operation::TaskOperation parent;

public:

	LegacyReadNativeRotamersFile();
	~LegacyReadNativeRotamersFile() override;

	core::pack::task::operation::TaskOperationOP
	clone() const override;

	void
	apply(
		core::pose::Pose const &,
		core::pack::task::PackerTask &
	) const override;

	void native_rotamers_map(
		NativeRotamersMap const & nat_ro_map
	);

	NativeRotamersMap const &
	native_rotamers_map() const;

	void parse_tag(
		TagCOP, DataMap &
	) override;

	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );
	static std::string keyname() { return "LegacyReadNativeRotamersFile"; }

private:
	NativeRotamersMap nat_ro_map_;
};

//////////////LegacyReadRepeatNativeRotamersFile/////////////////

class LegacyReadRepeatNativeRotamersFile : public LegacyReadNativeRotamersFile
{
public:
	LegacyReadNativeRotamersFile parent;

public:

	LegacyReadRepeatNativeRotamersFile();
	~LegacyReadRepeatNativeRotamersFile() override;

	core::pack::task::operation::TaskOperationOP
	clone() const override;

	void
	apply(
		core::pose::Pose const &,
		core::pack::task::PackerTask &
	) const override;

	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );
	static std::string keyname();
};


//////////////LegacyAssemblyConstraintsMover/////////////////

class LegacyAssemblyConstraintsMover : public protocols::moves::Mover {

public:

	LegacyAssemblyConstraintsMover();

	LegacyAssemblyConstraintsMover(
		NativeRotamersMap const & nat_ro_map,
		core::Real neighbor_cutoff,
		core::Real base_native_bonus,
		core::Real base_native_pro_bonus
	);

	protocols::moves::MoverOP
	clone() const override;

	protocols::moves::MoverOP
	fresh_instance() const override;


	void
	nat_ro_map(
		NativeRotamersMap const & nat_ro_map
	);

	void
	neighbor_cutoff(
		core::Real neighbor_cutoff
	);

	void
	base_native_bonus(
		core::Real base_native_bonus
	);

	void
	apply(
		core::pose::Pose & pose
	) override;

	void
	apply_repeat(
		core::pose::Pose & pose
	);

	void
	parse_my_tag(
		TagCOP tag,
		basic::datacache::DataMap & /*data*/,
		core::pose::Pose const & /*pose*/
	) override;

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:

	NativeRotamersMap nat_ro_map_;

	//Only favor 'natives' that have >= this many neighbors
	core::Real neighbor_cutoff_;

	//Favor all 'natives' by this amount (in REU)
	core::Real base_native_bonus_;

	core::Real base_native_pro_bonus_; // specifically for proline

};

} //legacy_sewing
} //protocols

#endif
