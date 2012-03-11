// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer,email:license@u.washington.edu.

/// @author Jianqing Xu ( xubest@gmail.com )

#include <protocols/jobdist/JobDistributors.hh> // SJF Keep first for mpi

#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueSelector.hh>

#include <core/chemical/VariantType.hh>
#include <core/fragment/FragData.hh>
#include <core/fragment/FragID.hh>
#include <core/fragment/FragSet.hh>
#include <core/fragment/Frame.hh>
#include <core/fragment/FrameIterator.hh>
#include <core/fragment/FrameList.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/io/silent/SilentStruct.hh>
#include <core/io/silent/SilentStructFactory.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/antibody.OptionKeys.gen.hh>
//#include <basic/options/keys/antibody2.OptionKeys.gen.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/docking.OptionKeys.gen.hh>
#include <basic/prof.hh>
#include <core/pack/rotamer_set/UnboundRotamersOperation.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/NoRepackDisulfides.hh>
#include <core/pack/task/operation/OperateOnCertainResidues.hh>
#include <core/pack/task/operation/OptH.hh>
#include <core/pack/task/operation/ResFilters.hh>
#include <core/pack/task/operation/ResLvlTaskOperations.hh>
#include <protocols/toolbox/task_operations/RestrictToInterface.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <core/pose/util.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreType.hh>
#include <core/scoring/rms_util.tmpl.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/constraints/ConstraintFactory.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <core/pack/dunbrack/RotamerConstraint.hh>
#include <basic/Tracer.hh>
#include <basic/datacache/BasicDataCache.hh>
#include <basic/datacache/DiagnosticData.hh>

#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
using namespace ObjexxFCL::fmt;

#include <protocols/jd2/ScoreMap.hh>
#include <protocols/simple_moves/FragmentMover.hh>
#include <protocols/antibody2/Ab_Info.hh>
#include <protocols/antibody2/Ab_ModelCDRH3.hh>
#include <protocols/docking/SidechainMinMover.hh>
#include <protocols/rigid/RB_geometry.hh>
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/Job.hh>
#include <protocols/jd2/JobOutputter.hh>
#include <protocols/loops/loops_main.hh>
#include <protocols/loops/Loop.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/antibody2/CDRH3Modeler2.hh>

#include <protocols/simple_moves/ConstraintSetMover.hh>
#include <protocols/moves/JumpOutMover.hh>
#include <protocols/simple_moves/MinMover.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/moves/MoverContainer.hh>
#include <protocols/simple_moves/PackRotamersMover.hh>
#include <protocols/moves/PyMolMover.hh>
#include <protocols/moves/RepeatMover.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/simple_moves/RotamerTrialsMover.hh>
#include <protocols/simple_moves/RotamerTrialsMinMover.hh>
#include <protocols/moves/TrialMover.hh>

//Auto Headers
#include <core/import_pose/import_pose.hh>
#include <core/pose/util.hh>

#include <protocols/antibody2/Ab_LH_SnugFit_Mover.hh>





using basic::T;
using basic::Error;
using basic::Warning;

static basic::Tracer TR("protocols.antibody2.Ab_ModelCDRH3");
using namespace core;

namespace protocols {
namespace antibody2 {

// default constructor
Ab_ModelCDRH3::Ab_ModelCDRH3() : Mover() {
	user_defined_ = false;
	init();
}

// default destructor
Ab_ModelCDRH3::~Ab_ModelCDRH3() {}

//clone
protocols::moves::MoverOP
Ab_ModelCDRH3::clone() const {
	return( new Ab_ModelCDRH3() );
}

    
    
    
    
    
    
    
void Ab_ModelCDRH3::init() {
	Mover::type( "Ab_ModelCDRH3" );

	// setup all the booleans with default values
	// they will get overwritten by the options and/or passed values
    
	set_default();
    
	register_options();
    
	init_from_options();

    
//	if ( ab_model_score() == NULL ) { //<- use this if we want to pass in score functions
		// score functions
		scorefxn_ = core::scoring::ScoreFunctionFactory::create_score_function( "standard", "score12" );
		scorefxn_->set_weight( core::scoring::chainbreak, 1.0 );
		scorefxn_->set_weight( core::scoring::overlap_chainbreak, 10./3. );
		scorefxn_->set_weight( core::scoring::atom_pair_constraint, 1.00 );
//	}

	setup_objects();

}

    
    
    
    
    
    
void
Ab_ModelCDRH3::set_default()
{
	TR <<  "Setting up default settings to all FALSE" << std::endl;
	model_h3_  = false;
	snugfit_   = false;
	graft_l1_  = false;
	graft_l2_  = false;
	graft_l3_  = false;
	graft_h1_  = false;
	graft_h2_  = false;
	graft_h3_  = false;
	benchmark_ = false;
	camelid_   = false;
	camelid_constraints_ = false;

}

    
    
    
    
    
    
void
Ab_ModelCDRH3::register_options()
{
	using namespace basic::options;

	option.add_relevant( OptionKeys::antibody::model_h3 );
	option.add_relevant( OptionKeys::antibody::snugfit );
	option.add_relevant( OptionKeys::antibody::camelid );
	option.add_relevant( OptionKeys::antibody::camelid_constraints );
	option.add_relevant( OptionKeys::antibody::graft_l1 );
	option.add_relevant( OptionKeys::antibody::graft_l2 );
	option.add_relevant( OptionKeys::antibody::graft_l3 );
	option.add_relevant( OptionKeys::antibody::graft_h1 );
	option.add_relevant( OptionKeys::antibody::graft_h2 );
	option.add_relevant( OptionKeys::antibody::graft_h3 );
	option.add_relevant( OptionKeys::constraints::cst_weight );
	option.add_relevant( OptionKeys::run::benchmark );
	option.add_relevant( OptionKeys::in::file::native );
}

    
    
    
    
void
Ab_ModelCDRH3::init_from_options() {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	TR <<  "Reading Options" << std::endl;
	if ( option[ OptionKeys::antibody::model_h3 ].user() )
                set_model_h3( option[ OptionKeys::antibody::model_h3 ]() );
	if ( option[ OptionKeys::antibody::snugfit ].user() )
                set_snugfit( option[ OptionKeys::antibody::snugfit ]() );
	if ( option[ OptionKeys::antibody::graft_l1 ].user() )
                set_graft_l1( option[ OptionKeys::antibody::graft_l1 ]() );
	if ( option[ OptionKeys::antibody::graft_l2 ].user() )
                set_graft_l2( option[ OptionKeys::antibody::graft_l2 ]() );
	if ( option[ OptionKeys::antibody::graft_l3 ].user() )
                set_graft_l3( option[ OptionKeys::antibody::graft_l3 ]() );
	if ( option[ OptionKeys::antibody::graft_h1 ].user() )
                set_graft_h1( option[ OptionKeys::antibody::graft_h1 ]() );
	if ( option[ OptionKeys::antibody::graft_h2 ].user() )
                set_graft_h2( option[ OptionKeys::antibody::graft_h2 ]() );
	if ( option[ OptionKeys::antibody::graft_h3 ].user() )
                set_graft_h3( option[ OptionKeys::antibody::graft_h3 ]() );
	if ( option[ OptionKeys::antibody::camelid ].user() )
                set_camelid( option[ OptionKeys::antibody::camelid ]() );
	if ( option[ OptionKeys::antibody::camelid_constraints ].user() )
                set_camelid_constraints( option[ OptionKeys::antibody::camelid_constraints ]() );
	if ( option[ OptionKeys::run::benchmark ].user() )
                set_benchmark( option[ OptionKeys::run::benchmark ]() );

	//set native pose if asked for
	if ( option[ OptionKeys::in::file::native ].user() ) {
		core::pose::PoseOP native_pose = new core::pose::Pose();
		core::import_pose::pose_from_pdb( *native_pose, option[ OptionKeys::in::file::native ]() );
		set_native_pose( native_pose );
	}
	else{
		set_native_pose(NULL);
	}
    
    
	cst_weight_ = option[ OptionKeys::constraints::cst_weight ]();
    
	if( camelid_ ) {
		graft_l1_ = false;
		graft_l2_ = false;
		graft_l3_ = false;
		snugfit_ = false;
	}
    
	if( camelid_constraints_ )
		model_h3_ = false;

}

    
    
    
    
    
    
    
    
void
Ab_ModelCDRH3::setup_objects() {
    
	sync_objects_with_flags();
    
    
    
}
    
void Ab_ModelCDRH3::sync_objects_with_flags() {

	using namespace protocols::moves;

	// add movers to sequence mover depending on the flags that were set



	if ( model_h3_ ){model_cdrh3_ = new CDRH3Modeler2( model_h3_, true, true, camelid_, benchmark_ );}
	else            {model_cdrh3_ = NULL;}

    
    

    
    
    
    
	flags_and_objects_are_in_sync_ = true;
	first_apply_with_current_setup_ = true;
}


    
    
    
    
    
    

    

void
Ab_ModelCDRH3::finalize_setup( pose::Pose & frame_pose ) {

    

	TR<<"AAAAAAAA     model_h3: "<<model_h3_<<std::endl;
	TR<<"AAAAAAAA     cst_weight: "<<cst_weight_<<std::endl;
	if( model_h3_ && ( cst_weight_ != 0.00 ) ) {
		simple_moves::ConstraintSetMoverOP cdr_constraint = new simple_moves::ConstraintSetMover();
		cdr_constraint->apply( frame_pose );
	}

	// check for native and input pose
	if ( !get_input_pose() ) {
		pose::PoseOP input_pose = new pose::Pose(frame_pose);  //JQX: QUESTION: why owning pointer here
		set_input_pose( input_pose );   // JQX: pass the input_pose to the mover.input_pose_
	}


	pose::PoseOP native_pose;
	if ( !get_native_pose() ) {
		TR << "Danger Will Robinson! Native is an impostor!" << std::endl;
        TR << "   'native_pose' is just a copy of the 'input_pose'    " << std::endl;
        TR << "    since you didn't sepcifiy the native pdb name"<<std::endl;
		native_pose = new pose::Pose(frame_pose);
	} else {
		native_pose = new pose::Pose( *get_native_pose() );
	}

	pose::set_ss_from_phipsi( *native_pose ); // JQX: this is the secondary structure from the native pose

	set_native_pose( native_pose ); // pass the native pose to the mover.native_pose_

	ab_info_.setup_CDR_loops( frame_pose, camelid_ );
            TR<< " Check ab_info object !!!!!    "<<std::endl;
            TR<<ab_info_<<std::endl;


	if( model_h3_ ) {
		// Read standard Rosetta fragments file
		exit(-1);
		read_and_store_fragments( frame_pose );
        model_cdrh3_->set_offset_frags( offset_frags_ );


        
		model_cdrh3_->set_native_pose( get_native_pose() );
	}
}



//APPLY



void Ab_ModelCDRH3::apply( pose::Pose & frame_pose ) {

    using namespace chemical;
    using namespace id;
    using namespace fragment;
    using namespace scoring;
    using namespace core::scoring::constraints;
    using namespace protocols::moves;

//  I assume the pose is from the job distributor, which can take the -s flag to get the pose
    // the below test proves that the inital secstruct is all "L"   !!!!!!!!!!!!!
/*    TR<<"JQX:    this is the 1st time that the 'pose' is used in the code: "<<std::endl;
    TR<<pose<<std::endl;
    for ( Size i = 1; i <= pose.total_residue(); ++i ) {
            TR<<"JQX:   residue: "<<i<<"       secstruct: "<<pose.secstruct(i)<<std::endl;
    }
    exit(-1);   */
    
    
    
    protocols::moves::PyMolMover pymol;
    if ( !flags_and_objects_are_in_sync_ ){ 
       sync_objects_with_flags(); 
    }
    
    if ( first_apply_with_current_setup_ ){ 
        finalize_setup(frame_pose);  
        first_apply_with_current_setup_=false; 
    }



	basic::prof_reset();
	protocols::jd2::JobOP job( protocols::jd2::JobDistributor::get_instance()->current_job() );
	// utility::exit( EXIT_FAILURE, __FILE__, __LINE__);

	pose::set_ss_from_phipsi( frame_pose );
    
	pymol.apply( frame_pose );
	pymol.send_energy( frame_pose );

	// display constraints and return
	if( camelid_constraints_ ) {
		display_constraint_residues( frame_pose );
		return;
	}




	// graft loops

//	graft_move_->apply( frame_pose );

	pymol.apply( frame_pose );
	pymol.send_energy( frame_pose );

	exit(-1);

	// model h3
	if ( model_h3_ ) {
		// centroid
		// high res
		model_cdrh3_->apply( frame_pose );
		pymol.apply( frame_pose );
		pymol.send_energy( frame_pose );
		// if not snugfit, relax cdr
		if ( !snugfit_ ) relax_cdrs( frame_pose );
		pymol.apply( frame_pose );
		pymol.send_energy( frame_pose );
	}

	// snugfit
	if ( !camelid_ && snugfit_ ) {
		all_cdr_VL_VH_fold_tree( frame_pose, ab_info_.all_cdr_loops_ );
        
		relax_cdrs( frame_pose );
		pymol.apply( frame_pose );
		pymol.send_energy( frame_pose );
        
		repulsive_ramp ( frame_pose, ab_info_.all_cdr_loops_ );
		pymol.apply( frame_pose );
		pymol.send_energy( frame_pose );
        
        
        //############################################################################
        Ab_LH_SnugFit_Mover ab_lh_snugfit_mover(ab_info_.all_cdr_loops_); 
        // TODO: JQX: should just pass the pointer of ab_info_ into the SnugFit class
        ab_lh_snugfit_mover.apply(frame_pose);
		//snugfit_mcm_protocol ( frame_pose, ab_info_.all_cdr_loops_ );
		//pymol.apply( frame_pose );
		//pymol.send_energy( frame_pose );
        //############################################################################

        
        

		// align pose to native pose
		pose::Pose native_pose = *get_native_pose();
		antibody2::Ab_Info native_ab( native_pose, camelid_ );
//		ab_info_.align_to_native( pose, native_ab, native_pose );
	}
	pymol.apply( frame_pose );
	pymol.send_energy( frame_pose );

	// remove cutpoints variants for all cdrs
	// "true" forces removal of variants even from non-cutpoints
	loops::remove_cutpoint_variants( frame_pose, true );

	// Define CDR H3 loop
	Size frag_size   = (ab_info_.get_CDR_loop("h3")->stop()  - ab_info_.get_CDR_loop("h3")->start()) + 3;
	Size cutpoint    =  ab_info_.get_CDR_loop("h3")->start() + int( frag_size / 2 );
	loops::Loop cdr_h3( ab_info_.get_CDR_loop("h3")->start(), ab_info_.get_CDR_loop("h3")->stop(), cutpoint, 0, false );

	// Fold Tree
	antibody2::simple_one_loop_fold_tree( frame_pose, cdr_h3 );

	// Redefining CDR H3 cutpoint variants
	loops::add_single_cutpoint_variant( frame_pose, cdr_h3 );

	// add scores to map for outputting constraint score
	( *scorefxn_ )( frame_pose );

	Real constraint_score = frame_pose.energies().total_energies()[ core::scoring::atom_pair_constraint ];

	// removing constraint score
	scorefxn_->set_weight( core::scoring::atom_pair_constraint, 0.00 );
	// add scores to map for output
	( *scorefxn_ )( frame_pose );

	job->add_string_real_pair("AA_H3", global_loop_rmsd( frame_pose, *get_native_pose(), "h3" ));
	job->add_string_real_pair("AB_H2", global_loop_rmsd( frame_pose, *get_native_pose(), "h2" ));
	job->add_string_real_pair("AC_H1", global_loop_rmsd( frame_pose, *get_native_pose(), "h1" ));
	if( !camelid_ ) {
		job->add_string_real_pair("AC_L3", global_loop_rmsd( frame_pose, *get_native_pose(), "l3" ));
		job->add_string_real_pair("AD_L2", global_loop_rmsd( frame_pose, *get_native_pose(), "l2" ));
		job->add_string_real_pair("AE_L1", global_loop_rmsd( frame_pose, *get_native_pose(), "l1" ));
	}
	job->add_string_real_pair("AF_constraint", constraint_score);

	set_last_move_status( protocols::moves::MS_SUCCESS );   

	basic::prof_show();





}// end apply








std::string
Ab_ModelCDRH3::get_name() const {
	return "Ab_ModelCDRH3";
}






	void
	Ab_ModelCDRH3::read_and_store_fragments( core::pose::Pose & pose ) {
		using namespace chemical;
		using namespace id;
		using namespace fragment;
		using namespace core::scoring;

		if ( !model_h3_ )
			return;

		// fragment initialization
		utility::vector1< FragSetOP > frag_libs;

		protocols::loops::read_loop_fragments( frag_libs );

		Size frag_size = (ab_info_.get_CDR_loop("h3")->stop()-ab_info_.get_CDR_loop("h3")->start()) + 3;
		Size cutpoint =  ab_info_.get_CDR_loop("h3")->start() + int( frag_size / 2 );
		setup_simple_fold_tree(  ab_info_.get_CDR_loop("h3")->start() - 1, cutpoint,
														 ab_info_.get_CDR_loop("h3")->stop() + 1,
														 pose.total_residue(),
														 pose );

		FragSetOP offset_3mer_frags;
		// a fragset of same type should be able to handle everything
		offset_3mer_frags = frag_libs[2]->empty_clone();
		FrameList loop_3mer_frames;
		Size offset = 0;
		frag_libs[2]->region_simple( 1, frag_size, loop_3mer_frames );
		for ( FrameList::const_iterator it = loop_3mer_frames.begin(),
						eit = loop_3mer_frames.end(); it!=eit; ++it ) {
			FrameOP short_frame = (*it)->clone_with_frags();
			offset++;
			short_frame->shift_to( ( ab_info_.get_CDR_loop("h3")->start() - 2 ) + offset  );
			offset_3mer_frags->add( short_frame );
		}

		FragSetOP offset_9mer_frags;
		// a fragset of same type should be able to handle everything
		offset_9mer_frags = frag_libs[1]->empty_clone();
		FrameList loop_9mer_frames;
		offset = 0;
		frag_libs[1]->region_simple( 1, frag_size, loop_9mer_frames );
		for ( FrameList::const_iterator it = loop_9mer_frames.begin(),
						eit = loop_9mer_frames.end(); it!=eit; ++it ) {
			FrameOP short_frame = (*it)->clone_with_frags();
			offset++;
			short_frame->shift_to( ( ab_info_.get_CDR_loop("h3")->start() - 2 ) + offset  );
			offset_9mer_frags->add( short_frame );
		}

		offset_frags_.push_back( offset_9mer_frags );
		offset_frags_.push_back( offset_3mer_frags );

		return;
	} // read_and_store_fragments









	void
	Ab_ModelCDRH3::setup_simple_fold_tree(
		Size jumppoint1,
		Size cutpoint,
		Size jumppoint2,
		Size nres,
		pose::Pose & pose_in ) {

		using namespace kinematics;

		TR << "ABM Setting up simple fold tree" << std::endl;

		FoldTree f;
		f.clear();

		f.add_edge( 1, jumppoint1, Edge::PEPTIDE );
		f.add_edge( jumppoint1, cutpoint, Edge::PEPTIDE );
		f.add_edge( cutpoint + 1, jumppoint2, Edge::PEPTIDE );
		f.add_edge( jumppoint2, nres, Edge::PEPTIDE );
		f.add_edge( jumppoint1, jumppoint2, 1 );
		f.reorder( 1 );

		pose_in.fold_tree( f );

		TR << "ABM Done: Setting up simple fold tree" << std::endl;

	} // setup_simple_fold_tree

	///////////////////////////////////////////////////////////////////////////
	/// @begin relax_cdrs
	///
	/// @brief relaxes all cdrs simultaneously
	///
	/// @detailed based on the all_cdrs loop definiton, minimizes only those
	///           regions. A standard dfpmin is utilized with score12 and chain
	///           -break and chain-overlap set. The allow_bb/chi arrays are
	///           changed accordingly but then are reset to their initial
	///           states before exiting the routine. Similarly the fold tree
	///           and jump movements are restored to their initial states
	///
	/// @param[out]
	///
	/// @global_read
	///
	/// @global_write
	///
	/// @remarks
	///
	/// @references
	///
	/// @authors Aroop 02/15/2010
	///
	/// @last_modified 02/15/2010
	///////////////////////////////////////////////////////////////////////////
	void
	Ab_ModelCDRH3::relax_cdrs( core::pose::Pose & pose )
	{
		using namespace pack;
		using namespace pack::task;
		using namespace pack::task::operation;
		using namespace protocols;
		using namespace protocols::toolbox::task_operations;
		using namespace protocols::moves;
		// Storing initial fold tree
		kinematics::FoldTree const input_tree( pose.fold_tree() );

		// changing to all cdr fold tree
		ab_info_.all_cdr_fold_tree( pose );

		// adding cutpoint variants for chainbreak score computation
		loops::add_cutpoint_variants( pose );

		Size const nres = pose.total_residue();

		//setting MoveMap
		kinematics::MoveMapOP allcdr_map;
		allcdr_map = new kinematics::MoveMap();
		allcdr_map->clear();
		allcdr_map->set_chi( false );
		allcdr_map->set_bb( false );
		utility::vector1< bool> is_flexible( nres, false );
		bool include_neighbors( false );
		select_loop_residues( pose, ab_info_.all_cdr_loops_, include_neighbors, is_flexible );
		allcdr_map->set_bb( is_flexible );
		include_neighbors = true;
		select_loop_residues( pose, ab_info_.all_cdr_loops_, include_neighbors, is_flexible );
		allcdr_map->set_chi( is_flexible );
		for( Size ii = 1; ii <= ab_info_.all_cdr_loops_.num_loop(); ii++ )
			allcdr_map->set_jump( ii, false );

		// score functions
		core::scoring::ScoreFunctionOP scorefxn;
		scorefxn = core::scoring::ScoreFunctionFactory::
			create_score_function( "standard", "score12" );
		scorefxn->set_weight( core::scoring::chainbreak, 10. / 3. );
		scorefxn->set_weight( core::scoring::overlap_chainbreak, 10. / 3. );

		Real min_tolerance = 0.1;
		if( benchmark_ ) min_tolerance = 1.0;
		std::string min_type = std::string( "dfpmin_armijo_nonmonotone" );
		bool nb_list = true;
        simple_moves::MinMoverOP all_cdr_min_moves = new simple_moves::MinMover( allcdr_map,
			scorefxn, min_type, min_tolerance, nb_list );
		all_cdr_min_moves->apply( pose );

		if( !benchmark_ ) {
            simple_moves::PackRotamersMoverOP repack=new simple_moves::PackRotamersMover( scorefxn );
			setup_packer_task( pose );
			( *scorefxn )( pose );
			tf_->push_back( new RestrictToInterface( is_flexible ) );
			repack->task_factory( tf_ );
			repack->apply( pose );

            simple_moves::RotamerTrialsMinMoverOP rtmin = new simple_moves::RotamerTrialsMinMover( scorefxn, tf_ );
			rtmin->apply( pose );
		}

		// Restoring pose fold tree
		pose.fold_tree( input_tree );
	} // relax_cdrs

	///////////////////////////////////////////////////////////////////////////
	/// @begin all_cdr_VL_VH_fold_tree
	///
	/// @brief change to all CDR and VL-VH dock fold tree
	///
	/// @detailed
	///
	/// @param[out]
	///
	/// @global_read
	///
	/// @global_write
	///
	/// @remarks
	///
	/// @references
	///
	/// @authors Aroop 07/13/2010
	///
	/// @last_modified 07/13/2010
	///////////////////////////////////////////////////////////////////////////
	void
	Ab_ModelCDRH3::all_cdr_VL_VH_fold_tree( pose::Pose & pose_in, const loops::Loops & loops_in ) {

		using namespace kinematics;

		Size nres = pose_in.total_residue();
		core::pose::PDBInfoCOP pdb_info = pose_in.pdb_info();
		char second_chain = 'H';
		Size rb_cutpoint(0);

		for ( Size i = 1; i <= nres; ++i ) {
			if( pdb_info->chain( i ) == second_chain) {
				rb_cutpoint = i-1;
				break;
			}
		}

		Size jump_pos1 ( geometry::residue_center_of_mass( pose_in, 1, rb_cutpoint ) );
		Size jump_pos2 ( geometry::residue_center_of_mass( pose_in,rb_cutpoint+1, nres ) );

		// make sure rb jumps do not reside in the loop region
		for( loops::Loops::const_iterator it= loops_in.begin(), it_end = loops_in.end(); it != it_end; ++it ) {
			if ( jump_pos1 >= ( it->start() - 1 ) &&
					 jump_pos1 <= ( it->stop() + 1) )
				jump_pos1 = it->stop() + 2;
			if ( jump_pos2 >= ( it->start() - 1 ) &&
					 jump_pos2 <= ( it->stop() + 1) )
				jump_pos2 = it->start() - 2;
		}

		// make a simple rigid-body jump first
		setup_simple_fold_tree(jump_pos1,rb_cutpoint,jump_pos2,nres, pose_in );

		// add the loop jump into the current tree,
		// delete some old edge accordingly
		FoldTree f( pose_in.fold_tree() );

		for( loops::Loops::const_iterator it=loops_in.begin(),
					 it_end=loops_in.end(); it != it_end; ++it ) {
			Size const loop_start ( it->start() );
			Size const loop_stop ( it->stop() );
			Size const loop_cutpoint ( it->cut() );
			Size edge_start(0), edge_stop(0);
			bool edge_found = false;
			const FoldTree & f_const = f;
			Size const num_jump = f_const.num_jump();
			for( FoldTree::const_iterator it2=f_const.begin(),
						 it2_end=f_const.end(); it2 !=it2_end; ++it2 ) {
				edge_start = std::min( it2->start(), it2->stop() );
				edge_stop = std::max( it2->start(), it2->stop() );
				if ( ! it2->is_jump() && loop_start > edge_start
						 && loop_stop < edge_stop ) {
					edge_found = true;
					break;
				}
			}

			f.delete_unordered_edge( edge_start, edge_stop, Edge::PEPTIDE);
			f.add_edge( loop_start-1, loop_stop+1, num_jump+1 );
			f.add_edge( edge_start, loop_start-1, Edge::PEPTIDE );
			f.add_edge( loop_start-1, loop_cutpoint, Edge::PEPTIDE );
			f.add_edge( loop_cutpoint+1, loop_stop+1, Edge::PEPTIDE );
			f.add_edge( loop_stop+1, edge_stop, Edge::PEPTIDE );
		}

		f.reorder(1);
		pose_in.fold_tree(f);
	} // all_cdr_VL_VH_fold_tree

	///////////////////////////////////////////////////////////////////////////
	/// @begin repulsive_ramp
	///
	/// @brief ramping up the fullatom repulsive weight slowly to allow the
	///        partners to relieve clashes and make way for each other
	///
	/// @detailed This routine is specially targetted to the coupled
	///           optimization of docking partners and the loop region.  The
	///           loop modelling & all previous  steps  involve mainly
	///           centroid  mode .On switching  on fullatom mode, one is bound
	///           to end up with clashes.To relieve the clashes, it is
	///           essential to slowly  dial up the  repulsive weight instead of
	///           turning it on to the maximum  value in one single step
	///
	/// @param[in] input pose which is assumed to have a docking fold tree
	///
	/// @global_read fa_rep : fullatom repulsive weight
	///
	/// @global_write fa_rep ( It is reset to the original value at the end )
	///
	/// @remarks A particular portion is  commented out,which can be
	///          uncommented if one  uses a  low  resolution  homology  model.
	///          Check details in the beginning of the commented out region
	///
	/// @references
	///
	/// @authors Aroop 07/13/2010
	///
	/// @last_modified 07/13/2010
	///////////////////////////////////////////////////////////////////////////
	void
	Ab_ModelCDRH3::repulsive_ramp(
		pose::Pose & pose_in,
		loops::Loops loops_in ) {

		Size nres = pose_in.total_residue();

		//setting MoveMap
		kinematics::MoveMapOP cdr_dock_map;
		cdr_dock_map = new kinematics::MoveMap();
		cdr_dock_map->clear();
		cdr_dock_map->set_chi( false );
		cdr_dock_map->set_bb( false );
		utility::vector1< bool> is_flexible( nres, false );
		bool include_neighbors( false );
		select_loop_residues( pose_in, loops_in, include_neighbors, is_flexible);
		cdr_dock_map->set_bb( is_flexible );
		include_neighbors = true;
		select_loop_residues( pose_in, loops_in, include_neighbors, is_flexible);
		cdr_dock_map->set_chi( is_flexible );
		cdr_dock_map->set_jump( 1, true );
		for( Size ii = 2; ii <= loops_in.num_loop() + 1; ii++ )
			cdr_dock_map->set_jump( ii, false );


		// score functions
		core::scoring::ScoreFunctionOP scorefxn;
		scorefxn = core::scoring::ScoreFunctionFactory::
			create_score_function( "docking", "docking_min" );
		scorefxn->set_weight( core::scoring::chainbreak, 1.0 );
		scorefxn->set_weight( core::scoring::overlap_chainbreak, 10./3. );

		// score functions
		core::scoring::ScoreFunctionOP pack_scorefxn;
		pack_scorefxn = core::scoring::ScoreFunctionFactory::
			create_score_function( "standard" );

		// remove cutpoints variants for all cdrs
		// "true" forces removal of variants even from non-cutpoints
		loops::remove_cutpoint_variants( pose_in, true );

		using namespace core::chemical;
		for ( loops::Loops::const_iterator it = loops_in.begin(),
						it_end = loops_in.end();	it != it_end; ++it ) {
			core::pose::add_variant_type_to_pose_residue( pose_in, CUTPOINT_LOWER, it->cut() );
			core::pose::add_variant_type_to_pose_residue( pose_in, CUTPOINT_UPPER,it->cut()+1);
		}
		// add scores to map
		( *scorefxn )( pose_in );

		// dampen fa_rep weight
		Real rep_weight_max = scorefxn->get_weight( core::scoring::fa_rep );
		Size rep_ramp_cycles(3);
		Size cycles(4);
		Real minimization_threshold(15.0);
		Real func_tol(1.0);
		//mjo commenting out 'nb_list' because it is unused and causes a warning
		//bool nb_list( true );
		if( benchmark_ ) {
			rep_ramp_cycles = 1;
			cycles = 1;
			minimization_threshold = 150.0;
			func_tol = 10.0;
		}

		Real rep_ramp_step = (rep_weight_max - 0.02) / Real(rep_ramp_cycles-1);
		for ( Size i = 1; i <= rep_ramp_cycles; i++ ) {
			Real rep_weight = 0.02 + rep_ramp_step * Real(i-1);
			scorefxn->set_weight( core::scoring::fa_rep, rep_weight );
			snugfit_MC_min ( pose_in, cdr_dock_map, cycles, minimization_threshold,
											 scorefxn, pack_scorefxn, is_flexible);
		}

		return;
	} // repulsive_ramp

	void
	Ab_ModelCDRH3::snugfit_MC_min (
		pose::Pose & pose_in,
		kinematics::MoveMapOP cdr_dock_map,
		Size cycles,
		Real minimization_threshold,
		core::scoring::ScoreFunctionOP scorefxn,
		core::scoring::ScoreFunctionOP pack_scorefxn,
		utility::vector1< bool> is_flexible ) {

		using namespace moves;
		bool nb_list = true;
		Size nres = pose_in.total_residue();

        simple_moves::MinMoverOP min_mover = new simple_moves::MinMover( cdr_dock_map, scorefxn,
					"dfpmin_armijo_nonmonotone", minimization_threshold, nb_list );

		//set up rigid body movers
        rigid::RigidBodyPerturbMoverOP rb_perturb=new rigid::RigidBodyPerturbMover(pose_in,
			*cdr_dock_map, 2.0, 0.1 , rigid::partner_downstream, true );

		setup_packer_task( pose_in );
		//set up sidechain movers for rigid body jump and loop & neighbors
		utility::vector1_size rb_jump;
		rb_jump.push_back( 1 );
		using namespace core::pack::task;
		using namespace core::pack::task::operation;
		// selecting movable c-terminal residues
		ObjexxFCL::FArray1D_bool loop_residues( nres, false );
		for( Size i = 1; i <= nres; i++ ) loop_residues( i ) = is_flexible[ i ]; // check mapping
		using namespace protocols::toolbox::task_operations;
		tf_->push_back( new RestrictToInterface( rb_jump, loop_residues ) );

        simple_moves::RotamerTrialsMoverOP pack_rottrial = new simple_moves::RotamerTrialsMover( pack_scorefxn, tf_ );
		SequenceMoverOP rb_mover = new SequenceMover;
		rb_mover->add_mover( rb_perturb );
		rb_mover->add_mover( pack_rottrial );
		JumpOutMoverOP rb_mover_min = new JumpOutMover( rb_mover, min_mover, scorefxn,	minimization_threshold );

		Real temperature = 0.8;
		MonteCarloOP mc = new MonteCarlo( pose_in, *scorefxn, temperature );
		TrialMoverOP rb_mover_min_trial = new TrialMover( rb_mover_min, mc);
		RepeatMoverOP first_mcm_cycles = new RepeatMover( rb_mover_min_trial, cycles );
		first_mcm_cycles->apply( pose_in );

		return;

	} // snugfit_MC_min


    
    
    
    
    
    // delete the snugfit_mcm_protocol
    
    
    
    

	void
	Ab_ModelCDRH3::setup_packer_task(
		pose::Pose & pose_in ) {
		using namespace pack::task;
		using namespace pack::task::operation;

		if( init_task_factory_ ) {
			tf_ = new TaskFactory( *init_task_factory_ );
			TR << "AbModeler Reinitializing Packer Task" << std::endl;
			return;
		}
		else
			tf_ = new TaskFactory;

		TR << "AbModeler Setting Up Packer Task" << std::endl;

		tf_->push_back( new OperateOnCertainResidues( new PreventRepackingRLT, new ResidueLacksProperty("PROTEIN") ) );
		tf_->push_back( new InitializeFromCommandline );
		tf_->push_back( new IncludeCurrent );
		tf_->push_back( new RestrictToRepacking );
		tf_->push_back( new NoRepackDisulfides );

		// incorporating Ian's UnboundRotamer operation.
		// note that nothing happens if unboundrot option is inactive!
		pack::rotamer_set::UnboundRotamersOperationOP unboundrot = new pack::rotamer_set::UnboundRotamersOperation();
		unboundrot->initialize_from_command_line();
		operation::AppendRotamerSetOP unboundrot_operation = new operation::AppendRotamerSet( unboundrot );
		tf_->push_back( unboundrot_operation );
		// adds scoring bonuses for the "unbound" rotamers, if any
		core::pack::dunbrack::load_unboundrot( pose_in );

		init_task_factory_ = tf_;

		TR << "AbModeler Done: Setting Up Packer Task" << std::endl;

	} // setup_packer_task

	Real
	Ab_ModelCDRH3::global_loop_rmsd (
	  const pose::Pose & pose_in,
		const pose::Pose & native_pose,
		std::string cdr_type ) {

		using namespace scoring;

		loops::LoopOP current_loop = ab_info_.get_CDR_loop( cdr_type );
		Size loop_start = current_loop->start();
		Size loop_end = current_loop->stop();

		using ObjexxFCL::FArray1D_bool;
		FArray1D_bool superpos_partner ( pose_in.total_residue(), false );

		for ( Size i = loop_start; i <= loop_end; ++i ) superpos_partner(i) = true;

		using namespace core::scoring;
		Real rmsG = rmsd_no_super_subset( native_pose, pose_in, superpos_partner, is_protein_CA );
		return ( rmsG );
	} // global_loop_rmsd

	void
	Ab_ModelCDRH3::display_constraint_residues( core::pose::Pose & pose ) {

		// Detecting di-sulfide bond

		Size H1_Cys(0), H3_Cys(0);

		if(      pose.residue( pose.pdb_info()->pdb2pose('H',32 ) ).name3() == "CYS" )
			H1_Cys = pose.pdb_info()->pdb2pose( 'H', 32 );
		else if( pose.residue( pose.pdb_info()->pdb2pose('H',33 ) ).name3() == "CYS" )
			H1_Cys = pose.pdb_info()->pdb2pose( 'H', 33 );

		for( Size ii = ab_info_.get_CDR_loop("h3")->start(); ii <= ab_info_.get_CDR_loop("h3")->stop(); ii++ )
			if( pose.residue(ii).name3() == "CYS" ) H3_Cys = ii;

		if( ( H1_Cys != 0 ) && ( H3_Cys != 0 ) )
			TR << "CONSTRAINTS: "<< "AtomPair CA " << H1_Cys << " CA " << H3_Cys
				 << " BOUNDED 4.0 6.1 0.6 BOND; mean 5.6 sd 0.6" << std::endl;

		// Specifying extended kink

		Size hfr_46(0), h3_closest(0);
		hfr_46 = pose.pdb_info()->pdb2pose( 'H', 46 );
		if( ab_info_.is_extended() ) h3_closest = ab_info_.get_CDR_loop("h3")->stop() - 5;
		if( h3_closest != 0 )
			TR << "CONSTRAINTS: " << "AtomPair CA " << hfr_46 << " CA " << h3_closest
				 << " BOUNDED 6.5 9.1 0.7 DISTANCE; mean 8.0 sd 0.7" << std::endl;

		return;
	} // display_constraint_residues

    
    
    
    
    
    
    
    
    
    
/// @details  Show the complete setup of the docking protocol
void
Ab_ModelCDRH3::show( std::ostream & out ) {
    if ( !flags_and_objects_are_in_sync_ ){
        sync_objects_with_flags();
    }
    out << *this;
}
    
std::ostream & operator<<(std::ostream& out, const Ab_ModelCDRH3 & ab_m_2 )
{
    using namespace ObjexxFCL::fmt;
        
    // All output will be 80 characters - 80 is a nice number, don't you think?
    std::string line_marker = "///";
    out << "////////////////////////////////////////////////////////////////////////////////" << std::endl;
    out << line_marker << A( 47, "Rosetta 3 Antibody Modeler" ) << space( 27 ) << line_marker << std::endl;
    out << line_marker << space( 74 ) << line_marker << std::endl;
    // Display the movable jumps that will be used in docking
    out << line_marker << " Dockable Jumps: ";
        
        
        
    // Display the state of the low resolution docking protocol that will be used
    out << line_marker << " Graft_l1:  " << ab_m_2.graft_l1_<<std::endl;
    out << line_marker << " Graft_l2:  " << ab_m_2.graft_l2_<<std::endl;
    out << line_marker << " Graft_l3:  " << ab_m_2.graft_l3_<<std::endl;
    out << line_marker << " Graft_h1:  " << ab_m_2.graft_h1_<<std::endl;
    out << line_marker << " Graft_h2:  " << ab_m_2.graft_h2_<<std::endl;
    out << line_marker << " Graft_h3:  " << ab_m_2.graft_h3_<<std::endl;
    out << line_marker << "  camelid:  " << ab_m_2.camelid_ <<std::endl;
    out << line_marker << " model_h3:  " << ab_m_2.model_h3_<<std::endl;
    out << line_marker << "  snugfit:  " << ab_m_2.snugfit_ <<std::endl;
        
        
    // Display the state of the low resolution docking protocol that will be used
        
        
    // Close the box I have drawn
    out << "////////////////////////////////////////////////////////////////////////////////" << std::endl;
    return out;
}
    

    
    
    
    
    
    
    
    
    
    

} // end antibody2
} // end protocols

