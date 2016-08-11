// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    protocols/grafting/simple_movers/DeleteRegionMover.hh
/// @brief
/// @author  Jared Adolf-Bryfogle


#include <protocols/grafting/simple_movers/DeleteRegionMover.hh>
#include <protocols/grafting/simple_movers/DeleteRegionMoverCreator.hh>


#include <protocols/grafting/util.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/rosetta_scripts/util.hh>

#include <core/conformation/Conformation.hh>
#include <core/pose/util.hh>
#include <core/pose/selection.hh>
#include <core/select/residue_selector/ResidueIndexSelector.hh>
#include <core/select/residue_selector/ResidueRanges.hh>

#include <utility/py/PyAssert.hh>
#include <utility/tag/Tag.hh>
#include <basic/datacache/DataCache.hh>
#include <utility>

namespace protocols {
namespace grafting {
namespace simple_movers {

DeleteRegionMover::DeleteRegionMover():
	protocols::moves::Mover("DeleteRegionMover"),
	selector_(),
	nter_overhang_(0),
	cter_overhang_(0),
	rechain_( false )
{
}

DeleteRegionMover::DeleteRegionMover( core::Size const res_start, core::Size const res_end ):
	protocols::moves::Mover("DeleteRegionMover"),
	selector_(),
	nter_overhang_(0),
	cter_overhang_(0),
	rechain_( false )
{
	std::stringstream start, end;
	start << res_start;
	end << res_end;
	region( start.str(), end.str() );
}

DeleteRegionMover::~DeleteRegionMover(){}

std::string
DeleteRegionMover::get_name() const {
	return "DeleteRegionMover";
}

void
DeleteRegionMover::region( std::string const & res_start, std::string const & res_end )
{
	std::stringstream residues;
	residues << res_start << "-" << res_end;
	selector_ = core::select::residue_selector::ResidueIndexSelectorCOP( new core::select::residue_selector::ResidueIndexSelector( residues.str() ) );
}

void
DeleteRegionMover::set_residue_selector( core::select::residue_selector::ResidueSelectorCOP selector ) {
	selector_ = selector;
}

protocols::moves::MoverOP
DeleteRegionMover::clone() const {
	return protocols::moves::MoverOP( new DeleteRegionMover(*this) );
}

protocols::moves::MoverOP
DeleteRegionMover::fresh_instance() const {
	return protocols::moves::MoverOP( new DeleteRegionMover );
}

protocols::moves::MoverOP
DeleteRegionMoverCreator::create_mover() const {
	return protocols::moves::MoverOP( new DeleteRegionMover );
}

std::string
DeleteRegionMoverCreator::keyname() const {
	return DeleteRegionMoverCreator::mover_name();
}

std::string
DeleteRegionMoverCreator::mover_name(){
	return "DeleteRegionMover";
}

void
DeleteRegionMover::parse_my_tag(
	TagCOP tag,
	basic::datacache::DataMap& data,
	const Filters_map& ,
	const moves::Movers_map& ,
	const Pose& )
{
	selector_ = protocols::rosetta_scripts::parse_residue_selector( tag, data );
	rechain_ = tag->getOption< bool >( "rechain", rechain_ );

	if ( tag->hasOption( "start" ) && tag->hasOption( "end" ) ) {
		std::string const start = tag->getOption< std::string >( "start" );
		std::string const end = tag->getOption< std::string >( "end" );
		region( start, end );
	}

	if ( !selector_ ) {
		throw utility::excn::EXCN_RosettaScriptsOption( "You must specify a start and end residue or a residue selector to DeleteRegionMover.\n" );
	}

	nter_overhang_ = tag->getOption<core::Size>( "nter_overhang", nter_overhang_ );
	cter_overhang_ = tag->getOption<core::Size>( "cter_overhang", cter_overhang_ );
	//std::cout << " N "<<nter_overhang_<< " C " << cter_overhang_<<std::endl;
}

void
DeleteRegionMover::apply( core::pose::Pose& pose )
{
	using core::select::residue_selector::ResidueRanges;

	if ( ! selector_ ) {
		throw utility::excn::EXCN_BadInput( "Selector not set in DeleteRegionMover!" );
	}

	ResidueRanges const ranges( selector_->apply( pose ) );
	for ( ResidueRanges::const_reverse_iterator range=ranges.rbegin(); range!=ranges.rend(); ++range ) {
		PyAssert( range->start() != 0, "Cannot delete region starting with 0 - make sure region is set for DeleteRegionMover" );
		PyAssert( range->stop() != 0, "Cannot delete region ending with 0 - make sure region is set for DeleteRegionMover" );
		PyAssert( range->stop() >= range->start(), "Cannot delete region where end > start" );
		PyAssert( range->stop() <= pose.total_residue(), "Cannot delete region where end is > pose total_residues" );

		core::Size const del_start = nter_overhang_ >= range->start() ? 1 : range->start() - nter_overhang_;
		core::Size const del_stop = range->stop() + cter_overhang_ > pose.total_residue() ? pose.total_residue() : range->stop() + cter_overhang_;

		protocols::grafting::delete_region( pose, del_start, del_stop );

		if ( rechain_ ) {
			add_terminus_variants( pose, del_start );
			pose.conformation().chains_from_termini();
		}
	}

}

/// @brief Adds terminal variants to residues resid and resid-1
/// @param[in,out] pose  Pose to be modified
/// @param[in]     resid Residue number for the residue that would have the lower terminus variant
/// @details Residue resid-1 will have upper_terminus variant, and residue resid will have
///          lower_terminus variant
void
DeleteRegionMover::add_terminus_variants( core::pose::Pose & pose, core::Size const resid ) const
{
	// add terminal variants
	if ( ( resid > 0 ) && ( resid - 1 > 0 ) ) {
		core::pose::add_upper_terminus_type_to_pose_residue( pose, resid - 1 );
	}
	if ( resid <= pose.total_residue() ) {
		core::pose::add_lower_terminus_type_to_pose_residue( pose, resid );
	}
}

}
}
}
