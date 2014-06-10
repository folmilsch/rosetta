// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/generalized_kinematic_closure/filter/GeneralizedKICfilter.cc
/// @brief  Helper class defining success filters for generalized closure of arbitrary segments that could go through side-chains (e.g. disulfides).
/// @author Vikram K. Mulligan (vmullig@uw.edu)

// Unit Headers
#include <protocols/generalized_kinematic_closure/filter/GeneralizedKICfilter.hh>
#include <protocols/generalized_kinematic_closure/util.hh>

#include <core/conformation/Residue.hh>
#include <core/conformation/Conformation.hh>

#include <utility/exit.hh>
#include <utility/string_util.hh>
#include <basic/Tracer.hh>
#include <core/types.hh>
#include <utility/tag/Tag.hh>
#include <core/id/AtomID.hh>

#include <core/pose/util.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <numeric/xyzVector.hh>
#include <numeric/xyzVector.io.hh>
#include <numeric/kinematic_closure/bridgeObjects.hh>
#include <numeric/kinematic_closure/kinematic_closure_helpers.hh>
#include <numeric/conversions.hh>

#include <boost/foreach.hpp>

//Auto Headers
#include <utility/excn/Exceptions.hh>
#include <core/pose/Pose.hh>

using basic::T;
using basic::Error;
using basic::Warning;

namespace protocols {
namespace generalized_kinematic_closure {
namespace filter {

static basic::Tracer TR("protocols.generalized_kinematic_closure.filter.GeneralizedKICfilter");

///@brief Creator for GeneralizedKICfilter.
GeneralizedKICfilter::GeneralizedKICfilter():
		filtertype_(no_filter)
		//utility::pointer::ReferenceCount(),
		//TODO -- make sure above data are copied properly when duplicating this mover.
{}

///@brief Destructor for GeneralizedKICfilter mover.
GeneralizedKICfilter::~GeneralizedKICfilter() {}

///@brief Returns the name of this class ("GeneralizedKICfilter").
std::string GeneralizedKICfilter::get_name() const{
	return "GeneralizedKICfilter";
}

///
/// @brief Given a filter type, return its name.  Returns "unknown_filter" if not recognized.
std::string GeneralizedKICfilter::get_filter_type_name( core::Size const filter_type ) const {
	std::string returnstring = "";
	switch(filter_type) {
		case no_filter:
			returnstring = "no_filter";
			break;
		case loop_bump_check:
			returnstring = "loop_bump_check";
			break;
		case atom_pair_distance:
			returnstring = "atom_pair_distance";
			break;
		default:
			returnstring = "unknown_filter";
			break;
	}
	return returnstring;
}

///
/// @brief Given the name of a filter type, return the filter type enum.  Returns unknown_filter if not recognized.
filter_type GeneralizedKICfilter::get_filter_type_by_name( std::string const &filtername ) const {
	for(core::Size i=1, imax=end_of_filter_list; i<imax; ++i) {
		if(get_filter_type_name(i)==filtername) return (filter_type)i;
	}
	return unknown_filter;
}

///
/// @brief Sets the filter type for this filter.
void GeneralizedKICfilter::set_filter_type( filter_type const &ftype) {
	runtime_assert_string_msg(ftype > 0 && ftype < end_of_filter_list, "Filter type not recognized.  Error in GeneralizedKICfilter::set_filter_type().");
	filtertype_ = ftype;
	filter_params_real_.clear();
	filter_params_size_.clear();
	filter_params_bool_.clear();
	return;
}

///
/// @brief Sets the filter type for this filter by name.
void GeneralizedKICfilter::set_filter_type( std::string const &ftypename) {
	filter_type ftype = get_filter_type_by_name(ftypename);
	runtime_assert_string_msg( ftype < end_of_filter_list, "Filter type " + ftypename + " not recognized.  Error in GeneralizedKICfilter::set_filter_type()." );
	filtertype_ = ftype;
	filter_params_real_.clear();
	filter_params_size_.clear();
	filter_params_bool_.clear();
	return;
}

///
/// @brief Gets the filter type name for THIS filter.
std::string GeneralizedKICfilter::get_this_filter_type_name () const {
	return get_filter_type_name( filtertype_ );
}

///
/// @brief Add a real-valued filter parameter.
void GeneralizedKICfilter::add_filter_param( std::string const &param_name, core::Real const &value )
{
	filter_params_real_.push_back( std::pair< std::string, core::Real >( param_name, value ) );
	return;
}

///
/// @brief Add a integer-valued filter parameter.
void GeneralizedKICfilter::add_filter_param( std::string const &param_name, core::Size const value )
{
	filter_params_size_.push_back( std::pair< std::string, core::Size >( param_name, value ) );
	return;
}

///
/// @brief Add a Boolean-valued filter parameter.
void GeneralizedKICfilter::add_filter_param( std::string const &param_name, bool const value )
{
	filter_params_bool_.push_back( std::pair< std::string, bool >( param_name, value ) );
	return;
}

///
/// @brief Add a string-valued filter parameter.
void GeneralizedKICfilter::add_filter_param( std::string const &param_name, std::string const &value )
{
	filter_params_string_.push_back( std::pair< std::string, std::string >( param_name, value ) );
	return;
}

/// @brief Get a real-valued filter parameter.
/// @details Returns false if the parameter couldn't be found.
bool GeneralizedKICfilter::get_filter_param( std::string const &param_name, core::Real &outvalue ) const
{
	core::Size const listsize = filter_params_real_.size();
	if(!listsize) return false;
	for(core::Size i=1; i<=listsize; ++i) {
		if(filter_params_real_[i].first==param_name) {
			outvalue=filter_params_real_[i].second;
			return true;
		}
	}

	return false;
}

/// @brief Get a integer-valued filter parameter.
/// @details Returns false if the parameter couldn't be found.
bool GeneralizedKICfilter::get_filter_param( std::string const &param_name, core::Size &outvalue ) const
{
	core::Size const listsize = filter_params_size_.size();
	if(!listsize) return false;
	for(core::Size i=1; i<=listsize; ++i) {
		if(filter_params_size_[i].first==param_name) {
			outvalue=filter_params_size_[i].second;
			return true;
		}
	}

	return false;
}

/// @brief Get a Boolean-valued filter parameter.
/// @details Returns false if the parameter couldn't be found.
bool GeneralizedKICfilter::get_filter_param( std::string const &param_name, bool &outvalue ) const
{
	core::Size const listsize = filter_params_bool_.size();
	if(!listsize) return false;
	for(core::Size i=1; i<=listsize; ++i) {
		if(filter_params_bool_[i].first==param_name) {
			outvalue=filter_params_bool_[i].second;
			return true;
		}
	}

	return false;
}

/// @brief Get a string-valued filter parameter.
/// @details Returns false if the parameter couldn't be found.
bool GeneralizedKICfilter::get_filter_param( std::string const &param_name, std::string &outvalue ) const
{
	core::Size const listsize = filter_params_string_.size();
	if(!listsize) return false;
	for(core::Size i=1; i<=listsize; ++i) {
		if(filter_params_string_[i].first==param_name) {
			outvalue=filter_params_string_[i].second;
			return true;
		}
	}

	return false;
}

/// @brief Apply this filter to ONE of the kinematic closure solutions produced by the bridgeObjects function,
///        and return pass or fail.
/// @detailed
/// @param[in] original_pose -- The full, initial pose.
/// @param[in] loop_pose -- A pose consisting of just the loop to be closed.
/// @param[in] residue_map -- The mapping of (residue index in loop_pose, residue index in original_pose).
/// @param[in] atomlist -- A list of atoms making the chain that was closed by bridgeObjects, with residue indices corresponding to loop_pose.
/// @param[in] torsions -- A vector of dihedral angles that the bridgeObjects function spat out.
/// @param[in] bondangles -- A vector of bond angles that the bridgeObjects function spat out.
/// @param[in] bondlengths -- A vector of bond lengths that the bridgeObjects function spat out.
bool GeneralizedKICfilter::apply(
	core::pose::Pose const &original_pose,
	core::pose::Pose const &loop_pose,
	utility::vector1 < std::pair <core::Size, core::Size> > const &residue_map,
	utility::vector1 < std::pair <core::id::AtomID, numeric::xyzVector<core::Real> > > const &atomlist,
	utility::vector1 < core::Real > const &torsions,
	utility::vector1 < core::Real > const &bondangles,
	utility::vector1 < core::Real > const &bondlengths
) const {
	//TR << "Applying GeneralizedKICfilter..." << std::endl; TR.flush(); //DELETE ME

	//utility::vector1 < core::Size > residues_loopindexed; //The list of residues, with indices based on loop_pose.  If necessary, will be generated.
	utility::vector1 < utility::vector1 < core::id::AtomID > > AtomIDs_loopindexed; //The list of lists of AtomIDs, with indices based on loop_pose.  If necessary, will be generated.

	switch(filtertype_) {
		case loop_bump_check:
			return apply_loop_bump_check( original_pose, loop_pose, residue_map, atomlist, torsions, bondangles, bondlengths);
			break;
		case atom_pair_distance:
			return apply_atom_pair_distance( original_pose, loop_pose, residue_map, atomlist, torsions, bondangles, bondlengths);
			break;
		default:
			break;
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////
//          PRIVATE FUNCTIONS                                                 //
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//          PRIVATE APPLY FUNCTIONS FOR EACH FILTER                           //
////////////////////////////////////////////////////////////////////////////////

/// @brief Applies the loop_bump_check filter, which checks for clashes between the atoms in the chain
/// to be closed and the rest of the structure (or for clashes within these atoms).
/// @details Returns "true" for pass and "false" for fail.  This does NOT check for clashes within a
/// residue -- only between residues.  However, if we're doing alpha- or beta- amino acids and the
/// backbone carbonyl carbon is in the chain to be closed, then the carbonyl oxygen will also be
/// checked for clashes.
/// @param[in] original_pose -- The full, initial pose.
/// @param[in] loop_pose -- A pose consisting of just the loop to be closed.
/// @param[in] residue_map -- The mapping of (residue index in loop_pose, residue index in original_pose).
/// @param[in] atomlist -- A list of atoms making the chain that was closed by bridgeObjects, with residue indices corresponding to loop_pose.
/// @param[in] torsions -- A vector of dihedral angles that the bridgeObjects function spat out.
/// @param[in] bondangles -- A vector of bond angles that the bridgeObjects function spat out.
/// @param[in] bondlengths -- A vector of bond lengths that the bridgeObjects function spat out.
bool GeneralizedKICfilter::apply_loop_bump_check(
	core::pose::Pose const &original_pose,
	core::pose::Pose const &loop_pose,
	utility::vector1 < std::pair <core::Size, core::Size> > const &residue_map,
	utility::vector1 < std::pair <core::id::AtomID, numeric::xyzVector<core::Real> > > const &atomlist,
	utility::vector1 < core::Real > const &torsions,
	utility::vector1 < core::Real > const &bondangles,
	utility::vector1 < core::Real > const &bondlengths
) const {
	using namespace protocols::generalized_kinematic_closure;
	using namespace core::id;

	core::Real const multiplier = 0.6; //TEMPORARY -- multiplier to reduce the stringency of the bump check.

	core::Size const nres=original_pose.n_residue(); //Total number of residues in the original pose.

	core::pose::Pose pose(loop_pose); //Make a copy of the loop pose
	set_loop_pose (pose, atomlist, torsions, bondangles, bondlengths);

	//Make a copy of the list of atoms in the chain to be closed:
	utility::vector1 < std::pair <core::id::AtomID, numeric::xyzVector<core::Real> > > atomlist_prime = atomlist;
	//Remove the first three and last three entries in atomlist_prime.  These are atoms outside of the loop used to establish frame:
	for(core::Size i=1; i<=3; ++i) {
		atomlist_prime.erase(atomlist_prime.end()-1);
		atomlist_prime.erase(atomlist_prime.begin());
	}
	//Loop through atomlist_prime and find alpha- or beta-amino acids with carbonyl carbons in the list.  Add the carbonyl oxygens.
	//Also find alpha- or beta-amino acids with CA carbons and one of (N, CM/C, CB).  Add the missing one of (N, CM/C, CB) to the list.
	for(core::Size ia=1, iamax=atomlist_prime.size(); ia<=iamax; ++ia) {
		core::Size const ia_res = atomlist_prime[ia].first.rsd();
		core::Size const ia_atomno = atomlist_prime[ia].first.atomno();
		if(pose.residue(ia_res).type().is_alpha_aa() || pose.residue(ia_res).type().is_beta_aa()) { //if alpha or beta amino acid
			if(pose.residue(ia_res).atom_name(ia_atomno)=="C" && pose.residue(ia_res).has("O")) {
				atomlist_prime.push_back( std::pair<AtomID, numeric::xyzVector<core::Real> > (AtomID( pose.residue(ia_res).atom_index("O"), ia_res), loop_pose.residue(ia_res).xyz("O")  ) );
			} else if(pose.residue(ia_res).atom_name(ia_atomno)=="CA") { //Adding whatever's missing from the (N, CM/C, CB) atoms surrounding a CA.
				utility::vector1 <bool> atsfound;
				atsfound.resize(3,false);
				utility::vector1 <std::string> ats;
				ats.resize(3, "");
				ats[1]="N"; ats[2]=(pose.residue(ia_res).type().is_beta_aa()?"CM":"C"); ats[3]="C";
				for(core::Size i=1; i<=3; ++i) {
					if(ia>1 && atomlist_prime[ia-1].first.rsd()==ia_res && pose.residue(ia_res).atom_name(atomlist_prime[ia-1].first.atomno()) == ats[i]) atsfound[i]=true;
					if(ia<iamax && atomlist_prime[ia+1].first.rsd()==ia_res && pose.residue(ia_res).atom_name(atomlist_prime[ia+1].first.atomno()) == ats[i]) atsfound[i]=true;
				}
				core::Size numfound = 0;
				for(core::Size i=1; i<=3; ++i) { if(atsfound[i]) ++numfound; } //Confirm that two of the three were found
				if(numfound==2) {
					for(core::Size i=1; i<=3; ++i) {
						if(!atsfound[i] && pose.residue(ia_res).has(ats[i])) {
							atomlist_prime.push_back( std::pair<AtomID, numeric::xyzVector<core::Real> > (AtomID( pose.residue(ia_res).atom_index(ats[i]), ia_res ), loop_pose.residue(ia_res).xyz(ats[i])) ); //Add the missing one of the three to the atom list
							TR << "Adding the " << ats[i] << " atom for residue number " << ia_res << " in the chain of residues to be closed to the bump check." << std::endl; //DELETE ME
							break;
						}
					}
				}
			} //else if this is a CA atom
		} //if alpha or beta amino acid
	}
	

	//Loop through atoms in atomlist_prime list:
	for(core::Size ia=1, iamax=atomlist_prime.size(); ia<=iamax; ++ia) {
		core::Size const ia_res=atomlist_prime[ia].first.rsd();
		core::Size const ia_atomno=atomlist_prime[ia].first.atomno();
		core::Real const ia_radius = pose.residue(ia_res).type().atom_type(ia_atomno).lj_radius();

		//First, check internal clashes:
		for(core::Size ja=4; ja<ia-1; ++ja) {
			core::Size const ja_res=atomlist_prime[ja].first.rsd();
			if(ja_res==ia_res || ja_res+1==ia_res || ja_res==ia_res+1) continue; //Don't bother checking for intra-residue clashes, or clashes between adjacent residues.
			core::Size const ja_atomno=atomlist_prime[ja].first.atomno();
			core::Real const ja_radius = pose.residue(ja_res).type().atom_type(ja_atomno).lj_radius();
			if(pose.residue(ja_res).xyz(ja_atomno).distance_squared( pose.residue(ia_res).xyz(ia_atomno) )  < pow((ja_radius+ia_radius)*multiplier, 2) ) {
				TR << "GeneralizedKICfilter::apply_loop_bump_check filter failed due to internal clash within loop that was closed." << std::endl;
				return false;
			}
		}

		//Next, check external clashes:
		for(core::Size jr=1; jr<=nres; ++jr) { //Loop through all residues of the main chain.
			if(original_pose_residue_is_in_residue_map(jr, residue_map)) continue; //If this is a loop residue, we're already considering clashes to it, so skip loop residues.

			//If this residue is connected to the residue containing the current loop atom, skip it.
			core::Size const ia_original_res = get_original_pose_rsd(ia_res, residue_map);
			bool is_connected_to_loop_rsd = false;
			for(core::Size i=1, imax=original_pose.residue(jr).n_residue_connections(); i<=imax; ++i) {
				if( original_pose.residue(jr).connected_residue_at_resconn(i)==ia_original_res ) {
					is_connected_to_loop_rsd=true;
					break;
				}
			}
			if(is_connected_to_loop_rsd) continue;
			
			utility::vector1<core::Size> atoms_to_consider = original_pose.residue(jr).mainchain_atoms();
			if( (original_pose.residue(jr).type().is_alpha_aa() || original_pose.residue(jr).type().is_beta_aa()) && original_pose.residue(jr).has("CB")) {
				atoms_to_consider.push_back( original_pose.residue(jr).atom_index("CB") ); //Also consider clashes with the beta carbon, if present.
			}
			for(core::Size ja=1, jamax=atoms_to_consider.size(); ja<=jamax; ++ja) { //Loop through the mainchain atoms.
				core::Real const ja_radius = original_pose.residue(jr).type().atom_type( atoms_to_consider[ja] ).lj_radius();
				if( original_pose.residue(jr).xyz( atoms_to_consider[ja] ).distance_squared( pose.residue(ia_res).xyz(ia_atomno) ) < pow((ja_radius+ia_radius)*multiplier, 2) ) {
					TR << "GeneralizedKICfilter::apply_loop_bump_check filter failed due to external clash between loop that was closed and the rest of the structure." << std::endl;
					return false;
				}
			}
		}
	}

	TR << "GeneralizedKICfilter::apply_loop_bump_check() filter passed." << std::endl; TR.flush();
	return true;
}

/// @brief Applies the atom_pair_distance filter, checking that the distance between two atoms is less than
/// a given threshhold (or greater than a given threshhold if the user so specifies with the "greater_than"
/// option).
/// @details Returns "true" for pass and "false" for fail.  The user can set the following options:
/// "distance" (real-valued, mandatory)
/// "atom1" (string-valued, mandatory)
/// "atom2" (string-valued, mandatory)
/// "res1" (integer-valued, mandatory, based on original pose numbering)
/// "res2" (integer-valued, mandatory, based on original pose numbering)
/// "greater_than" (boolean, optional, false by default)
/// @param[in] original_pose -- The full, initial pose.
/// @param[in] loop_pose -- A pose consisting of just the loop to be closed.
/// @param[in] residue_map -- The mapping of (residue index in loop_pose, residue index in original_pose).
/// @param[in] atomlist -- A list of atoms making the chain that was closed by bridgeObjects, with residue indices corresponding to loop_pose.
/// @param[in] torsions -- A vector of dihedral angles that the bridgeObjects function spat out.
/// @param[in] bondangles -- A vector of bond angles that the bridgeObjects function spat out.
/// @param[in] bondlengths -- A vector of bond lengths that the bridgeObjects function spat out.
bool GeneralizedKICfilter::apply_atom_pair_distance(
	core::pose::Pose const & original_pose,
	core::pose::Pose const & loop_pose,
	utility::vector1 < std::pair <core::Size, core::Size> > const & residue_map,
	utility::vector1 < std::pair <core::id::AtomID, numeric::xyzVector<core::Real> > > const & atomlist,
	utility::vector1 < core::Real > const & torsions,
	utility::vector1 < core::Real > const & bondangles,
	utility::vector1 < core::Real > const & bondlengths
) const {

	//First, check that all necessary params have been set and store necessary information in local vars:
	std::string at1, at2;
	core::Size res1, res2;
	core::Real dist_cutoff, dist_cutoff_sq;
	bool greaterthan = false;
	runtime_assert_string_msg(get_filter_param("atom1", at1), "In GeneralizedKICfilter::apply_atom_pair_distance(): the atom_pair_distance filter cannot be used without specifying atom1.");
	runtime_assert_string_msg(get_filter_param("atom2", at2), "In GeneralizedKICfilter::apply_atom_pair_distance(): the atom_pair_distance filter cannot be used without specifying atom2.");
	runtime_assert_string_msg(get_filter_param("res1", res1), "In GeneralizedKICfilter::apply_atom_pair_distance(): the atom_pair_distance filter cannot be used without specifying res1.");
	runtime_assert_string_msg(get_filter_param("res2", res2), "In GeneralizedKICfilter::apply_atom_pair_distance(): the atom_pair_distance filter cannot be used without specifying res2.");
	runtime_assert_string_msg(get_filter_param("distance", dist_cutoff), "In GeneralizedKICfilter::apply_atom_pair_distance(): the atom_pair_distance filter cannot be used without specifying a distance cutoff (\"distance\" parameter).");
	get_filter_param("greater_than", greaterthan);
	dist_cutoff_sq = dist_cutoff*dist_cutoff;

	runtime_assert_string_msg(original_pose_residue_is_in_residue_map(res1, residue_map) || original_pose_residue_is_in_residue_map(res2, residue_map),
		"In GeneralizedKICfilter::apply_atom_pair_distance(): at least one of the residues passed to the atom_pair_distance filter must be in the loop to be closed." );

	runtime_assert_string_msg(original_pose.residue(res1).has(at1),
		"In GeneralizedKICfilter::apply_atom_pair_distance(): the residue specified with \"res1\" does not contain the atom specified with \"atom1\"." );
	runtime_assert_string_msg(original_pose.residue(res2).has(at2),
		"In GeneralizedKICfilter::apply_atom_pair_distance(): the residue specified with \"res2\" does not contain the atom specified with \"atom2\"." );


	core::pose::Pose looppose(loop_pose); //Make a copy of the loop pose
	core::pose::Pose fullpose(original_pose); //Make a copy of the full pose
	set_loop_pose (looppose, atomlist, torsions, bondangles, bondlengths); //Set the loop conformation using the torsions, bondangles, and bondlengths vectors.
	copy_loop_pose_to_original( fullpose, looppose, residue_map); //Copy the loop conformation to the full pose.

	if(fullpose.residue(res1).xyz(at1).distance_squared( fullpose.residue(res2).xyz(at2) ) > dist_cutoff_sq) {
		if(!greaterthan) {
			TR << "GeneralizedKICfilter::apply_atom_pair_distance filter() failed." << std::endl; TR.flush();
			return false;
		}
	} else {
		if(greaterthan) {
			TR << "GeneralizedKICfilter::apply_atom_pair_distance filter() failed." << std::endl; TR.flush();
			return false;
		}
	}

	TR << "GeneralizedKICfilter::apply_atom_pair_distance filter() passed." << std::endl; TR.flush();
	return true;
}

} //namespace filter
} //namespace generalized_kinematic_closure
} //namespace protocols
