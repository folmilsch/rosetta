// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/loops/LoopMover_SlidingWindow.cc
/// @brief kinematic loop closure main protocols
/// @author Mike Tyka

//// Unit Headers
#include <protocols/loops/util.hh>
#include <protocols/loops/loops_main.hh>
#include <protocols/loops/LoopMover.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/LoopMover_SlidingWindow.hh>
#include <protocols/loops/SlidingWindowLoopClosure.hh>
#include <protocols/loops/WidthFirstSlidingWindowLoopClosure.hh>
//
//
//// Rosetta Headers
#include <core/chemical/VariantType.hh>

#include <core/conformation/Residue.hh>
#include <core/id/TorsionID.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <basic/options/option.hh>
#include <core/pose/Pose.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/fragment/ConstantLengthFragSet.hh>

#include <basic/Tracer.hh> // tracer output
#include <protocols/jumping/util.hh>

//Utility Headers
#include <numeric/random/random.hh>

// C++ Headers
#include <iostream>
#include <map>
#include <string>

// option key includes

#include <basic/options/keys/loops.OptionKeys.gen.hh>

#include <core/pose/util.hh>
#include <protocols/simple_moves/FragmentMover.fwd.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace loops {

extern basic::Tracer tr;
///////////////////////////////////////////////////////////////////////////////
using namespace core;

static numeric::random::RandomGenerator RG(10003);


LoopMover_SlidingWindow::LoopMover_SlidingWindow(
	protocols::loops::Loops  loops_in
) : IndependentLoopMover( loops_in )
{
	scorefxn_ = get_cen_scorefxn();
	scorefxn_->set_weight( scoring::chainbreak, 1.0*10.0/3.0);

	protocols::moves::Mover::type("LoopMover_SlidingWindow");
	set_default_settings();
}



LoopMover_SlidingWindow::LoopMover_SlidingWindow(
	protocols::loops::Loops  loops_in,
	core::scoring::ScoreFunctionOP  scorefxn
) : IndependentLoopMover( loops_in )
{
	if( scorefxn ){
		scorefxn_ = scorefxn;
	} else {
		scorefxn_ = get_cen_scorefxn();
		scorefxn_->set_weight( scoring::chainbreak, 1.0*10.0/3.0);
	}

	protocols::moves::Mover::type("LoopMover_SlidingWindow");
	set_default_settings();
}


LoopResult LoopMover_SlidingWindow::model_loop(
	core::pose::Pose & pose,
  protocols::loops::Loop const & loop
){
	using namespace kinematics;
	using namespace scoring;
	using namespace protocols::simple_moves;
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace numeric::random;


	// store starting fold tree and cut pose
 	kinematics::FoldTree f_orig=pose.fold_tree();
 	kinematics::FoldTree f_empty;
	f_empty.simple_tree( pose.total_residue() );
	core::pose::Pose closedpose = pose;
	set_single_loop_fold_tree( pose, loop );
	closedpose.fold_tree( f_empty );

	// generate movemap after fold_tree is set
	kinematics::MoveMapOP mm_one_loop = new kinematics::MoveMap();
	set_move_map_for_centroid_loop( loop, *mm_one_loop );

	core::Size const nres =  pose.total_residue();
	// special case ... vrt res at last position
	//bool chainbreak_present =  ( loop.start() != 1 && loop.stop() != nres );
	//chainbreak_present &= (loop.stop() != nres-1 || pose.residue( nres ).aa() != core::chemical::aa_vrt );
	bool chainbreak_present = ( loop.start() != 1 && loop.stop() != nres &&
	                            !pose.residue( loop.start() ).is_lower_terminus() &&
	                            !pose.residue( loop.stop() ).is_upper_terminus() );

	// set loop.cut() variant residue for chainbreak score if chanbreak is present
	if( chainbreak_present ){
		core::pose::add_variant_type_to_pose_residue( pose, chemical::CUTPOINT_LOWER, loop.cut() );
		core::pose::add_variant_type_to_pose_residue( pose, chemical::CUTPOINT_UPPER, loop.cut()+1 );
	}

	// either extend or at least idealize the loop (just in case).
	if( loop.is_extended() ) set_extended_torsions( pose, loop );
	else                     idealize_loop(  pose, loop );

	core::fragment::FragSetOP fragset_small_;

	     if( frag_libs_.size() == 0 ){ utility_exit_with_message("Fragments needed for LoopMover_SlidingWindow"); }
	else if( frag_libs_.size() == 1 ){ fragset_small_ = frag_libs_[0]; }
	else                             { fragset_small_ = frag_libs_[ frag_libs_.size() - 2]; }


	loops::SlidingWindowLoopClosureOP closure_protocol;

	if ( option[ OptionKeys::loops::alternative_closure_protocol ]() ) {
		closure_protocol = new loops::WidthFirstSlidingWindowLoopClosure;
	}else{
		closure_protocol = new loops::SlidingWindowLoopClosure;
	}

	closure_protocol->scored_frag_cycle_ratio( option[ OptionKeys::loops::scored_frag_cycles ]() );
	closure_protocol->short_frag_cycle_ratio( option[ OptionKeys::loops::short_frag_cycles ]() );

	closure_protocol->set_bIdealLoopClosing( false );
	closure_protocol->set_chainbreak_max( option[ OptionKeys::loops::chainbreak_max_accept ]() );

	protocols::jumping::close_chainbreaks(
		  closure_protocol,
		  pose,
		  checkpoints_,
			get_current_tag(),
		  f_empty
	);

	// Check chain break !
	if( chainbreak_present ){
		(*scorefxn_)( pose );
		core::Real chain_break_score = std::max( (float)pose.energies().total_energies()[ scoring::chainbreak ],
																	(float)pose.energies().total_energies()[ scoring::linear_chainbreak ] );

		core::Real chain_break_tol = option[ basic::options::OptionKeys::loops::chain_break_tol ]();
		tr.Info << "Chainbreak: " << chain_break_score << " Max: " << chain_break_tol << std::endl;

		if( chain_break_score > (chain_break_tol*10) ) return ExtendFailure;   // if we have a really bad chain break, go  and extend
		if( chain_break_score > chain_break_tol )      return Failure;         // if we only have a slight chainbreak problem, try again
	}

	// return to original fold tree
	loops::remove_cutpoint_variants( pose );
	pose.fold_tree( f_orig );

	return Success;
}

std::string
LoopMover_SlidingWindow::get_name() const {
	return "LoopMover_SlidingWindow";
}

} // namespace loops
} // namespace protocols
