// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file RNA de novo fragment assembly
/// @brief protocols that are specific to RNA_DeNovoProtocol
/// @details
/// @author Rhiju Das, Parin Sripakdeevong, Fang-Chieh Chou


// Unit headers
#include <protocols/farna/RNA_DeNovoProtocol.hh>

// Package headers
#include <protocols/toolbox/AllowInsert.hh>
#include <core/pose/rna/RNA_BasePairClassifier.hh>
#include <core/io/rna/RNA_DataReader.hh>
#include <core/io/rna/RNA_DataReader.fwd.hh>
#include <protocols/farna/FullAtomRNA_Fragments.hh>
#include <protocols/farna/RNA_LoopCloser.hh>
#include <protocols/farna/RNA_LoopCloser.fwd.hh>
#include <protocols/farna/RNA_Minimizer.fwd.hh>
#include <protocols/farna/RNA_Minimizer.hh>
#include <protocols/farna/RNA_Relaxer.fwd.hh>
#include <protocols/farna/RNA_Relaxer.hh>
#include <protocols/farna/RNA_StructureParameters.hh>
#include <protocols/farna/RNA_ChunkLibrary.hh>
#include <protocols/farna/RNA_ChunkLibrary.fwd.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/stepwise/modeler/align/util.hh> //move this to toolbox/
#include <protocols/stepwise/modeler/rna/util.hh>
#include <protocols/farna/util.hh>

// Project headers
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/MonteCarlo.fwd.hh>
#include <core/conformation/Residue.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/ScoreType.hh>
#include <core/scoring/rna/RNA_LowResolutionPotential.hh>
#include <core/scoring/rna/RNA_ScoringInfo.hh>
#include <core/chemical/rna/util.hh>
#include <core/scoring/rna/chemical_shift/RNA_ChemicalShiftPotential.hh>
#include <core/id/AtomID_Map.hh>
#include <core/id/AtomID.hh>
#include <core/id/DOF_ID.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/pose/copydofs/util.hh>
#include <basic/database/open.hh>
#include <core/io/silent/RNA_SilentStruct.hh>
#include <core/io/silent/BinarySilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/io/silent/util.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/kinematics/ShortestPathInFoldTree.hh>
#include <core/pose/rna/RNA_BaseDoubletClasses.hh>

#include <core/scoring/Energies.hh>
#include <core/scoring/EnergyMap.hh>

#include <utility/file/file_sys_util.hh>

#include <core/types.hh>
#include <basic/Tracer.hh>

#include <numeric/random/random.hh>

// option key includes
#include <basic/options/option.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <basic/options/keys/rna.OptionKeys.gen.hh>

// External library headers

//C++ headers
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#ifdef WIN32
#include <ctime>
#endif

//Auto Headers
#include <protocols/viewer/GraphicsState.hh>
#include <utility/vector1.hh>
#include <ObjexxFCL/format.hh>

//Auto using namespaces
namespace ObjexxFCL { namespace format { } } using namespace ObjexxFCL::format; // AUTO USING NS
//Auto using namespaces end


using namespace core;

namespace protocols {
namespace farna {


static THREAD_LOCAL basic::Tracer TR( "protocols.rna.RNA_DeNovoProtocol" );

RNA_DeNovoProtocol::RNA_DeNovoProtocol(
	Size const nstruct,
	std::string const silent_file,
	bool const heat_structure /*= true*/,
	bool const minimize_structure /*= false*/,
	bool const relax_structure /*=false*/,
	bool const allow_bulge /*=false*/):
	Mover(),
	nstruct_( nstruct ),
	rounds_( 10 ),
	monte_carlo_cycles_( 0 ), /* will be reset later */
	monte_carlo_cycles_max_default_( 100000 ),
	user_defined_cycles_( false ), /* will change to true if set_monte_carlo_cycles() is called */
	all_rna_fragments_file_( basic::database::full_name("sampling/rna/RICHARDSON_RNA09.torsions") ),
	silent_file_( silent_file ),
	lores_silent_file_( "" ),
	heat_structure_( heat_structure ),
	dump_pdb_( false ), //RHIJU DO NOT CHECK THIS IN AS TRUE!
	minimize_structure_( minimize_structure ),
	relax_structure_( relax_structure ),
	ignore_secstruct_( false ),
	do_close_loops_( false ),
	close_loops_at_end_( true ),
	close_loops_in_last_round_( true ),
	close_loops_after_each_move_( false ),
	simple_rmsd_cutoff_relax_( false ),
	allow_bulge_( allow_bulge ),
	allow_consecutive_bulges_( false ),
	use_chem_shift_data_( basic::options::option[
	basic::options::OptionKeys::
	score::rna_chemical_shift_exp_data].user()),
	m_Temperature_( 2.0 ),
	frag_size_( 3 ),
	rna_params_file_( "" ),
	rna_data_file_( "" ),
	jump_library_file_( basic::database::full_name("sampling/rna/1jj2_RNA_jump_library.dat" ) ),
	rna_structure_parameters_( RNA_StructureParametersOP( new RNA_StructureParameters ) ),
	output_lores_silent_file_( false ),
	filter_lores_base_pairs_( false ),
	filter_lores_base_pairs_early_( false ),
	filter_chain_closure_( true ),
	filter_chain_closure_distance_( 6.0 ), /* in Angstroms. This is pretty loose!*/
	filter_chain_closure_halfway_( true ),
	vary_bond_geometry_( false ),
	binary_rna_output_( false ),
	jump_change_frequency_( 0.1 ),
	lores_scorefxn_( "farna/rna_lores.wts" ),
	chunk_coverage_( 0.0 ),
	staged_constraints_( false ),
	chainbreak_weight_( -1.0 ), /* use farna/rna_lores.wts number unless user specified. -1.0 is never really used. */
	linear_chainbreak_weight_( -1.0 ),  /* use farna/rna_lores.wts number unless user specified. -1.0 is never really used. */
	titrate_stack_bonus_( true ),
	move_first_rigid_body_( false ),
	root_at_first_rigid_body_( false ),
	suppress_bp_constraint_( 1.0 ),
	output_filters_( false ),
	lores_score_early_( false ),
	lores_score_final_( false ),
	autofilter_( false ),
	autofilter_score_quantile_( 0.20 ),
	refine_from_silent_( false ),
	refine_pose_( false ),
	bps_moves_( false )
{
	Mover::type("RNA_DeNovoProtocol");
	rna_loop_closer_ = protocols::farna::RNA_LoopCloserOP( new protocols::farna::RNA_LoopCloser );
	// rna_loop_closer_->fast_scan( true );
	local_rna_low_resolution_potential_.more_precise_base_pair_classification( true );
}

/// @brief Clone this object
protocols::moves::MoverOP RNA_DeNovoProtocol::clone() const {
	return protocols::moves::MoverOP( new RNA_DeNovoProtocol(*this) );
}

//////////////////////////////////////////////////
RNA_DeNovoProtocol::~RNA_DeNovoProtocol() {}

/// @details  Apply the RNA de novo modeling protocol to a pose.
///
void RNA_DeNovoProtocol::apply( core::pose::Pose & pose ) {

	using namespace core::pose;
	using namespace core::scoring;
	using namespace core::io::pdb;
	using namespace core::io::silent;
	using namespace protocols::farna;

	///////////////////////////////////////////////////////////////////////////
	// A bunch of initialization
	///////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////
	// Some useful movers...
	////////////////////////////////////////
	initialize_movers( pose );
	if ( dump_pdb_ ) pose.dump_pdb( "init.pdb" );

	// RNA score function (both low-res and high-res).
	initialize_scorefxn( pose );

	//Keep a copy for resetting after each decoy.
	Pose start_pose = pose;

	monte_carlo_ = protocols::moves::MonteCarloOP( new protocols::moves::MonteCarlo( pose, *denovo_scorefxn_, m_Temperature_ ) );
	setup_monte_carlo_cycles( pose );

	// Some other silent file setup
	initialize_lores_silent_file();
	initialize_tag_is_done();

	Size max_tries( 1 );
	if ( filter_lores_base_pairs_ || filter_chain_closure_ )  max_tries = 10;

	///////////////////////////////////////////////////////////////////////////
	// Main Loop.
	///////////////////////////////////////////////////////////////////////////
	Size refine_pose_id( 1 );
	for ( Size n = 1; n <= nstruct_; n++ ) {

		std::string const out_file_tag = "S_"+lead_zero_string_of( n, 6 );
		if ( tag_is_done_[ out_file_tag ] ) continue;

		for ( Size ntries = 1; ntries <= max_tries; ++ntries ) {
			time_t pdb_start_time = time(NULL);

			if ( ntries > 1 ) TR << TR.Red << "Did not pass filters. Trying the model again: trial " << ntries << " out of " << max_tries << TR.Reset << std::endl;

			if ( !refine_from_silent_ ) {
				pose = start_pose;
				rna_structure_parameters_->setup_fold_tree_and_jumps_and_variants( pose );
			} else {
				pose = *refine_pose_list_[refine_pose_id];
				++refine_pose_id;
				if ( refine_pose_id > refine_pose_list_.size() ) refine_pose_id = 1;
			}
			rna_structure_parameters_->setup_base_pair_constraints( pose ); // needs to happen after setting cutpoint variants, etc.
			initialize_constraints( pose );
			rna_chunk_library_->initialize_random_chunks( pose ); //actually not random if only one chunk in each region.

			if ( refine_pose_ ) core::pose::copydofs::copy_dofs_match_atom_names( pose, start_pose );

			if ( close_loops_after_each_move_ ) {
				rna_loop_closer_->apply( pose );
				do_close_loops_ = true;
			} else {
				do_close_loops_ = false;
			}

			if ( dump_pdb_ ) dump_pdb( pose, "start.pdb" );

			if ( heat_structure_ && !refine_from_silent_ ) do_random_moves( pose );

			if ( dump_pdb_ ) dump_pdb( pose, "random.pdb" );
			monte_carlo_->reset( pose );

			TR << "Beginning main loop... " << std::endl;

			frag_size_ = 3;

			bool found_solution( true );
			for ( Size r = 1; r <= rounds_; r++ ) {

				TR << TR.Blue << "Beginning round " << r << " of " << rounds_ << TR.Reset << std::endl;

				if ( r == rounds_ && close_loops_in_last_round_ ) do_close_loops_ = true;

				//Keep score function coarse for early rounds.
				update_denovo_scorefxn_weights( r );

				monte_carlo_->score_function( *denovo_scorefxn_ );

				pose = monte_carlo_->lowest_score_pose();

				// Introduce constraints in stages.
				update_pose_constraints( r, pose );
				monte_carlo_->reset( pose );

				// Finer and finer fragments
				update_frag_size( r );

				// finer rigid body moves
				setup_rigid_body_mover( pose, r ); // needs to happen after fold_tree is decided...

				//////////////////////
				// This is it ... do the loop.
				//////////////////////
				for ( Size i = 1; i <= monte_carlo_cycles_ / rounds_; ++i ) {
					// Make this generic fragment/jump multimover next?
					RNA_move_trial( pose );
				}

				if ( get_native_pose() ) {
					Real const rmsd = all_atom_rmsd( *get_native_pose(), pose );
					TR << "All atom rmsd: " << rmsd << std::endl;
				}

				monte_carlo_->recover_low( pose );
				monte_carlo_->show_counters();
				monte_carlo_->reset_counters();

				if ( r == 2 || rounds_ == 1 ) { //special 'early' stage
					lores_score_early_ = (*denovo_scorefxn_)( pose );
					if ( filter_lores_base_pairs_early_ ) {
						bool const base_pairs_OK = rna_structure_parameters_->check_base_pairs( pose );
						TR << "Checking base pairs early! Result: " << base_pairs_OK << std::endl;
						if ( !base_pairs_OK ) {
							found_solution = false;
							break;
						}
					}
				}

				if ( r == rounds_ / 2 || rounds_ == 1 ) { // halway point
					if ( filter_chain_closure_halfway_ ) {
						Real const filter_chain_closure_distance_halfway = 2 * filter_chain_closure_distance_;
						bool const rna_loops_OK = rna_loop_closer_->check_closure( pose, filter_chain_closure_distance_halfway );
						TR << "Checking loop closure with tolerance of " << filter_chain_closure_distance_halfway << " Angstroms! Result: " << rna_loops_OK << std::endl;
						if ( !rna_loops_OK ) {
							found_solution = false;
							break;
						}
					}
				}
			}

			time_t pdb_end_time = time(NULL);
			TR << "Finished fragment assembly of " << out_file_tag << " in " << (long)(pdb_end_time - pdb_start_time) << " seconds." << std::endl;

			if ( !found_solution ) { // Just try again if early exit from above
				if ( ntries == max_tries ) pose = monte_carlo_->lowest_score_pose();
				continue;
			}
			pose = monte_carlo_->lowest_score_pose();

			if ( close_loops_at_end_ ) rna_loop_closer_->apply( pose, rna_structure_parameters_->connections() );

			// A bunch of filters
			if ( filter_chain_closure_ ) {
				if ( !rna_loop_closer_->check_closure( pose, filter_chain_closure_distance_ ) ) {
					TR << "Failed chain closure filter." << std::endl;
					continue;
				}
			}

			if ( filter_lores_base_pairs_ ) {
				if ( !rna_structure_parameters_->check_base_pairs( pose ) ) {
					TR << "Failed base pairing filter." << std::endl;
					continue;
				}
			}

			lores_score_final_ = (*denovo_scorefxn_)( pose );
			if ( autofilter_ ) {
				if ( !check_score_filter( lores_score_final_, all_lores_score_final_ ) ) {
					TR << "Failed score filter." << std::endl;
					continue;
				}
			}
			break; //Pass all the filters, early exit
		} // ++ntries <= max_tries

		// Get the full strength constraint back
		update_denovo_scorefxn_weights( rounds_ );
		update_pose_constraints( rounds_, pose );
		denovo_scorefxn_->show( std::cout, pose );
		final_scorefxn_ = denovo_scorefxn_;

		if ( output_lores_silent_file_ ) align_and_output_to_silent_file( pose, lores_silent_file_, out_file_tag );

		if ( minimize_structure_ ) {
			rna_minimizer_->set_allow_insert( rna_structure_parameters_->allow_insert() );
			rna_minimizer_->apply( pose );
			if ( close_loops_at_end_ ) rna_loop_closer_->apply( pose, rna_structure_parameters_->connections() );
			final_scorefxn_ = hires_scorefxn_;
		}

		if ( use_chem_shift_data_ ) apply_chem_shift_data(pose, out_file_tag);

		if ( relax_structure_ ) rna_relaxer_->apply( pose );

		if ( allow_bulge_ ) {
			//Identify and virtual the bulge residues.
			/*Size const num_res_virtualized =*/
			virtualize_bulges(
				pose, allowed_bulge_res_, final_scorefxn_, out_file_tag,
				true /*allow_pre_virtualize*/, allow_consecutive_bulges_,
				true /*verbose*/
			);
		}

		final_score( pose ); // may include rna_chem_map score here.

		std::string const out_file_name = out_file_tag + ".pdb";
		if ( dump_pdb_ ) dump_pdb( pose,  out_file_name );

		align_and_output_to_silent_file( pose, silent_file_, out_file_tag );
	} //nstruct
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
std::string
RNA_DeNovoProtocol::get_name() const {
	return "RNA_DeNovoProtocol";
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::show(std::ostream & output) const
{
	Mover::show(output);
	output <<   "nstruct:                       " << nstruct_  <<
		"\nRounds:                        " << rounds_ <<
		"\nMonte Carlo cycles:            " << monte_carlo_cycles_ <<
		"\nMC cycle max default:          " << monte_carlo_cycles_max_default_ <<
		"\nUser defined MC cycles:        " << (user_defined_cycles_  ? "True" : "False") <<
		"\nAll RNA fragment file:         " << all_rna_fragments_file_ <<
		"\nHeat structure:                " << (heat_structure_ ? "True" : "False") <<
		"\nDump pdb:                      " << (dump_pdb_ ? "True" : "False") <<
		"\nMinimize structure:            " << (minimize_structure_ ? "True" : "False") <<
		"\nRelax structure:               " << (relax_structure_ ? "True" : "False") <<
		"\nIgnore secstruct:              " << (ignore_secstruct_ ? "True" : "False") <<
		"\nDo close loops:                " << (do_close_loops_ ? "True" : "False") <<
		"\nClose loops at end:            " << (close_loops_at_end_ ? "True" : "False") <<
		"\nClose loops in each round:     " << (close_loops_in_last_round_ ? "True" : "False") <<
		"\nClose loops after each move:   " << (close_loops_after_each_move_ ? "True" : "False") <<
		"\nSimple rmsd cutoff relax:      " << (simple_rmsd_cutoff_relax_ ? "True" : "False") <<
		"\nAllow bulges:                  " << (allow_bulge_ ? "True" : "False") <<
		"\nAllow consecutive bulges:      " << (allow_consecutive_bulges_ ? "True" : "False") <<
		"\nUse chem shift data:           " << (use_chem_shift_data_ ? "True" : "False") <<
		"\nDefault temperature for MC:    " << m_Temperature_ <<
		"\nFragment size:                 " << frag_size_ <<
		"\nInput rna params file?:        " << ((rna_params_file_ == "" ) ? "No" : "Yes") <<
		"\nInput rna data file?:          " << ((rna_data_file_ == "" ) ? "No" : "Yes") <<
		"\nJump library file:             " << jump_library_file_ <<
		"\nOutput lores silent file:      " << (output_lores_silent_file_ ? "True" : "False") <<
		"\nFilter lores base pairs:       " << (filter_lores_base_pairs_ ? "True" : "False") <<
		"\nFilter lores base pairs early: " << (filter_lores_base_pairs_early_ ? "True" : "False") <<
		"\nFilter chain closure:          " << (filter_chain_closure_ ? "True" : "False") <<
		"\nFilter chain closure distance: " << filter_chain_closure_distance_ <<
		"\nFilter chain closure halfway:  " << (filter_chain_closure_halfway_ ? "True" : "False") <<
		"\nVary bond geometry:            " << (vary_bond_geometry_ ? "True" : "False") <<
		"\nBinary RNA output:             " << (binary_rna_output_ ? "True" : "False") <<
		"\nJump change frequency:         " << jump_change_frequency_ <<
		"\nChunk coverage:                " << chunk_coverage_ <<
		"\nStaged constraints:            " << (staged_constraints_ ? "True" : "False") <<
		"\nTitrate stack bonus:           " << (titrate_stack_bonus_ ? "True" : "False") <<
		"\nMove first rigid body:         " << (move_first_rigid_body_ ? "True" : "False") <<
		"\nRoot at first rigid body:      " << (root_at_first_rigid_body_ ? "True" : "False") <<
		"\nOutput Filters:                " << (output_filters_ ? "True" : "False") <<
		"\nLores score early:             " << (lores_score_early_ ? "True" : "False") <<
		"\nLores score final:             " << (lores_score_final_ ? "True" : "False") <<
		"\nAutofilter:                    " << (autofilter_ ? "True" : "False") <<
		"\nAutofilter score quantile:     " << autofilter_score_quantile_;

}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_scorefxn( core::pose::Pose & pose ) {

	using namespace core::scoring;
	using namespace basic::options;

	// RNA low-resolution score function.
	denovo_scorefxn_ = ScoreFunctionFactory::create_score_function( lores_scorefxn_ );
	if ( scoring::rna::nonconst_rna_scoring_info_from_pose( pose ).rna_data_info().rna_reactivities().size() > 0 ) {
		denovo_scorefxn_->set_weight( core::scoring::rna_chem_map_lores, option[ OptionKeys::score::rna_chem_map_lores_weight ]() );
	}

	initialize_constraints( pose );

	initial_denovo_scorefxn_ = denovo_scorefxn_->clone();

	if ( chainbreak_weight_ > -1 ) initial_denovo_scorefxn_->set_weight( chainbreak, chainbreak_weight_ );

	if ( linear_chainbreak_weight_ > -1 ) initial_denovo_scorefxn_->set_weight( linear_chainbreak, linear_chainbreak_weight_ );

	// RNA high-resolution score function.
	hires_scorefxn_ = rna_minimizer_->clone_scorefxn();

	// RNA high-resolution score function + rna_chem_shift term
	if ( use_chem_shift_data_ ) {
		Real const CS_weight = 4.0; //hard-coded to 4.0 based on CS-ROSETTA-RNA work (Parin et al. 2012).
		chem_shift_scorefxn_ = core::scoring::ScoreFunctionOP( new ScoreFunction );
		chem_shift_scorefxn_ = hires_scorefxn_->clone();
		chem_shift_scorefxn_->set_weight( rna_chem_shift, CS_weight );
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_constraints( core::pose::Pose & pose ) {

	using namespace core::scoring;

	if ( pose.constraint_set()->has_constraints() ) {
		denovo_scorefxn_->set_weight( atom_pair_constraint, 1.0 );
		denovo_scorefxn_->set_weight( coordinate_constraint, 1.0 ); // now useable in RNA denovo!
		constraint_set_ = pose.constraint_set()->clone();
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_movers( core::pose::Pose & pose ){

	// all jumping, secondary structure, base pair constraint, allow_insertation
	// will be stored in a .prm file.
	rna_structure_parameters_->set_bps_moves( bps_moves_ );
	rna_structure_parameters_->initialize( pose, rna_params_file_, jump_library_file_, ignore_secstruct_ );
	rna_structure_parameters_->set_root_at_first_rigid_body( root_at_first_rigid_body_ );
	rna_structure_parameters_->set_suppress_bp_constraint( suppress_bp_constraint_ );

	// reads in any data on, e.g., exposure of different bases --> saves inside the pose's rna_data_info.
	rna_data_reader_ = core::io::rna::RNA_DataReaderOP( new core::io::rna::RNA_DataReader( rna_data_file_ ) );
	rna_data_reader_->fill_rna_data_info( pose );

	all_rna_fragments_ = protocols::farna::RNA_FragmentsOP( new FullAtomRNA_Fragments( all_rna_fragments_file_ ) );

	rna_chunk_library_ = protocols::farna::RNA_ChunkLibraryOP( new RNA_ChunkLibrary( chunk_pdb_files_, chunk_silent_files_, pose, input_res_ ) );
	if ( bps_moves_ ) rna_chunk_library_->setup_base_pair_step_chunks( pose, rna_structure_parameters_->get_base_pair_steps() );

	chunk_coverage_ = rna_chunk_library_->chunk_coverage();

	rna_structure_parameters_->allow_insert()->and_allow_insert( rna_chunk_library_->allow_insert() );

	std::cout << pose.annotated_sequence() << std::endl;

	rna_structure_parameters_->setup_virtual_phosphate_variants( pose ); // needed to refreeze virtual phosphates!
	// rna_structure_parameters_->allow_insert()->show();

	rna_chunk_library_->set_allow_insert( rna_structure_parameters_->allow_insert() );
	rna_fragment_mover_ = protocols::farna::RNA_FragmentMoverOP( new RNA_FragmentMover( all_rna_fragments_, rna_structure_parameters_->allow_insert() ) );

	rna_minimizer_ = protocols::farna::RNA_MinimizerOP( new RNA_Minimizer );
	rna_minimizer_->set_allow_insert( rna_structure_parameters_->allow_insert() );
	rna_minimizer_->vary_bond_geometry( vary_bond_geometry_ );
	rna_minimizer_->set_extra_minimize_res( extra_minimize_res_ );
	rna_minimizer_->set_extra_minimize_chi_res( extra_minimize_chi_res_ );
	rna_minimizer_->set_move_first_rigid_body( move_first_rigid_body_ );
	rna_minimizer_->use_coordinate_constraints( minimizer_use_coordinate_constraints_ );

	rna_relaxer_ = protocols::farna::RNA_RelaxerOP( new RNA_Relaxer( rna_fragment_mover_, rna_minimizer_) );
	rna_relaxer_->simple_rmsd_cutoff_relax( simple_rmsd_cutoff_relax_ );

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::setup_monte_carlo_cycles( core::pose::Pose const & pose ){

	if ( user_defined_cycles_ ) return;

	// figure out rough number of degrees of freedom.

	// first count up number of residues with allow_insert.
	Size const nres_move = get_moving_res( pose ).size();
	TR << "Number of moving residues: " << nres_move << std::endl;

	Size const nchunks = rna_chunk_library_->num_moving_chunks();
	TR << "Number of moving chunks: " << nchunks << std::endl;

	// then count up rigid bodies that need to be docked.
	Size nbody_move = protocols::farna::get_rigid_body_jumps( pose ).size();
	if ( nbody_move > 1 ) nbody_move--; // first rigid body does not move, by convention.
	if ( nbody_move > 0 ) TR << "Number of moving bodies: " << nbody_move << std::endl;

	monte_carlo_cycles_ = 2000 * ( nres_move + nchunks ) + 20000 * nbody_move;

	if ( monte_carlo_cycles_ > monte_carlo_cycles_max_default_ ) {
		monte_carlo_cycles_ = monte_carlo_cycles_max_default_;
		TR << "Using maximum default Monte Carlo cycles: " <<  monte_carlo_cycles_ << ". Use -cycles option to change this." << std::endl;
	}

	TR << "Using " << monte_carlo_cycles_ << " cycles in de novo modeling." << std::endl;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_tag_is_done()
{
	tag_is_done_ = core::io::silent::initialize_tag_is_done( silent_file_ );
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::setup_rigid_body_mover( pose::Pose const & pose, Size const r ){

	core::kinematics::MoveMap movemap;
	movemap.set_jump( false );

	bool rigid_body_moves = protocols::farna::let_rigid_body_jumps_move( movemap, pose, move_first_rigid_body_ );

	if ( !rigid_body_moves ) return;

	if ( !binary_rna_output_ ) utility_exit_with_message( "Asking for virtual anchor -- need to specify -binary_output" );

	//Keep moves coarse for early rounds. For the last 1/4 of modeling, plateau to the finest moves.
	Real suppress ( 3.0 / 4.0 * r / rounds_ );
	// Real suppress  = ( r - 1.0 ) / ( static_cast<Real>( rounds_ ) * ( 3.0 / 4.0 ) - 1.0 );
	// Real suppress  = (r - 1.0)/( static_cast<Real>(rounds) - 1.0);
	if ( suppress > 1.0 ) suppress = 1.0;

	Real const rot_mag_init( 10.0 ),   rot_mag_final( 0.2 );
	Real const trans_mag_init( 5.0 ), trans_mag_final( 0.1 );
	Real const rot_mag   = rot_mag_init   +  (rot_mag_final - rot_mag_init ) * suppress;
	Real const trans_mag = trans_mag_init +  (trans_mag_final - trans_mag_init ) * suppress;

	rigid_body_mover_ = protocols::rigid::RigidBodyPerturbMoverOP( new protocols::rigid::RigidBodyPerturbMover( pose, movemap, rot_mag, trans_mag, protocols::rigid::partner_upstream /*because virtual anchor should be root*/ ) );
	jump_change_frequency_ = 0.5; /* up from default of 0.1*/

	TR << " rot_mag: " << rot_mag << "    trans_mag: " << trans_mag << std::endl;

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_lores_silent_file() {

	if ( !output_lores_silent_file_ ) return;

	static std::string const new_prefix( "_LORES.out" );

	std::string::size_type pos = silent_file_.find( ".out", 0 );
	if ( pos == std::string::npos ) {
		utility_exit_with_message(  "If you want to output a lores silent file, better use .out suffix ==> " + silent_file_ );
	}
	lores_silent_file_ = silent_file_;
	lores_silent_file_.replace( pos, new_prefix.length(), new_prefix );
}

//////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::calc_rmsds( core::io::silent::SilentStruct & s, core::pose::Pose & pose, std::string const & out_file_tag ) const
{
	using namespace core::scoring;

	Real const rmsd = all_atom_rmsd( *get_native_pose(), pose );
	TR << "All atom rmsd: " << rmsd << " for " << out_file_tag << std::endl;
	s.add_energy( "rms", rmsd );

	Real rmsd_stems = 0.0;
	std::list< Size > stem_residues( rna_structure_parameters_->get_stem_residues( pose ) );

	if ( !stem_residues.empty() ) { //size() > 0 ) {
		rmsd_stems = all_atom_rmsd( *get_native_pose(), pose, stem_residues );
		TR << "All atom rmsd over stems: " << rmsd_stems << " for " << out_file_tag << std::endl;
	}
	s.add_energy( "rms_stem", rmsd_stems );

}

///////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::output_silent_struct(
	core::io::silent::SilentStruct & s, core::io::silent::SilentFileData & silent_file_data,
	std::string const & silent_file, pose::Pose & pose, std::string const out_file_tag,
	bool const score_only /* = false */) const
{

	using namespace core::io::silent;
	using namespace core::scoring;

	if ( get_native_pose() ) calc_rmsds( s, pose, out_file_tag  );

	// TR << "ADD_NUMBER_BASE_PAIRS" << std::endl;
	add_number_base_pairs( pose, s );
	// TR << "ADD_NUMBER_NATIVE_BASE_PAIRS" << std::endl;
	if ( get_native_pose() ) add_number_native_base_pairs( pose, s );

	if ( output_filters_ ) {
		s.add_energy( "lores_early", lores_score_early_ ) ;
		if ( minimize_structure_ ) s.add_energy( "lores_final", lores_score_final_ ) ;
	}

	TR << "Outputting to silent file: " << silent_file << std::endl;
	silent_file_data.write_silent_struct( s, silent_file, score_only );

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::output_to_silent_file(
	core::pose::Pose & pose,
	std::string const & silent_file,
	std::string const & out_file_tag,
	bool const score_only /* = false */) const
{

	using namespace core::io::silent;
	using namespace core::scoring;

	// Silent file setup?
	//static SilentFileData silent_file_data;
	SilentFileData silent_file_data;

	// What is all this rigamarole, making the silent struct data?
	// Why do I need to supply the damn file name? That seems silly.
	TR << "Making silent struct for " << out_file_tag << std::endl;

	if ( binary_rna_output_ ) {
		BinarySilentStruct s( pose, out_file_tag );

		if ( use_chem_shift_data_ ) add_chem_shift_info( s, pose);

		output_silent_struct( s, silent_file_data, silent_file, pose, out_file_tag, score_only );

	} else {
		RNA_SilentStruct s( pose, out_file_tag );

		if ( use_chem_shift_data_ ) add_chem_shift_info( s, pose);

		output_silent_struct( s, silent_file_data, silent_file, pose, out_file_tag, score_only );

	}

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
utility::vector1< Size >
RNA_DeNovoProtocol::get_moving_res( core::pose::Pose const & pose ) const {

	utility::vector1< Size > moving_res;

	protocols::toolbox::AllowInsertOP const & allow_insert = rna_structure_parameters_->allow_insert();

	for ( Size n = 1; n <= pose.total_residue(); n++ ) {
		if ( allow_insert->get( n ) ) {
			moving_res.push_back( n );
		}
	}

	return moving_res;

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::final_score( core::pose::Pose & pose ){
	if ( scoring::rna::nonconst_rna_scoring_info_from_pose( pose ).rna_data_info().rna_reactivities().size() > 0 ) {
		final_scorefxn_->set_weight( core::scoring::rna_chem_map, 1.0 );
	}
	( *final_scorefxn_ )( pose );
	if ( final_scorefxn_->has_nonzero_weight( core::scoring::rna_chem_map ) ) final_scorefxn_->show( pose );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::align_and_output_to_silent_file( core::pose::Pose & pose, std::string const & silent_file, std::string const & out_file_tag ) const
{

	// if input pdbs were specified with -s or -silent, then automatic alignment to first of these input chunks.
	// otherwise, align to native pose, if specified.
	if ( input_res_.size() > 0 ) {

		rna_chunk_library_->superimpose_to_first_chunk( pose );

	} else if ( get_native_pose() ) {
		Pose const & native_pose = *get_native_pose();

		//realign to native for ease of viewing.
		// check for any fixed domains.
		utility::vector1< Size > superimpose_res = get_moving_res( pose );

		// if no fixed domains, just superimpose over all residues.
		if ( superimpose_res.size() == 0 ) {
			for ( Size n = 1; n <= pose.total_residue(); n++ )  superimpose_res.push_back( n );
		}

		id::AtomID_Map< id::AtomID > const & alignment_atom_id_map_native =
			protocols::stepwise::modeler::align::create_alignment_id_map_legacy( pose, native_pose, superimpose_res ); // perhaps this should move to toolbox.

		TR << "Aligning pose to native." << std::endl;

		//pose.dump_pdb( "before_align.pdb");
		//  native_pose.dump_pdb( "native.pdb" );
		core::scoring::superimpose_pose( pose, native_pose, alignment_atom_id_map_native );
		//  pose.dump_pdb( "after_align.pdb");

	}

	output_to_silent_file( pose, silent_file, out_file_tag );
}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::do_random_moves( core::pose::Pose & pose ) {

	rna_chunk_library_->check_fold_tree_OK( pose );
	rna_chunk_library_->initialize_random_chunks( pose );

	if ( dump_pdb_ ) pose.dump_pdb( "add_chunks.pdb" );

	Size const heat_cycles = 3 * pose.total_residue();
	TR << "Heating up... " << std::endl;

	for ( Size i = 1; i <= heat_cycles; i++ ) {
		rna_fragment_mover_->random_fragment_insertion( pose, 1 /*frag_size*/ );
	}

	if ( dump_pdb_ )  pose.dump_pdb( "random_moves1.pdb" );

	rna_chunk_library_->initialize_random_chunks( pose, dump_pdb_ );

	if ( dump_pdb_ )  pose.dump_pdb( "random_moves2.pdb" );

	translate_virtual_anchor_to_first_rigid_body( pose ); //useful for graphics viewing & final output

	if ( dump_pdb_ )  pose.dump_pdb( "random_moves3.pdb" );

	randomize_rigid_body_orientations( pose );

	if ( dump_pdb_ )  pose.dump_pdb( "random_moves4.pdb" );

}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::randomize_rigid_body_orientations( pose::Pose & pose ){

	using namespace protocols::rigid;
	using namespace protocols::farna;
	using namespace kinematics;

	utility::vector1< Size > const rigid_body_jumps = get_rigid_body_jumps( pose );
	Size const found_jumps = rigid_body_jumps.size();
	if ( found_jumps <= 1 )  return; // nothing to rotate/translate relative to another object.

	// translation to first, fixed rigid body.
	Vector first_rigid_body_position = pose.jump( rigid_body_jumps[ 1 ] ).get_translation();

	for ( Size n = 2; n <= rigid_body_jumps.size(); n++ ) {
		Size const i = rigid_body_jumps[ n ];

		// randomize orientation.
		RigidBodyRandomizeMover rigid_body_randomize_mover( pose, i, partner_upstream );
		rigid_body_randomize_mover.apply( pose );

		// randomize translation.
		// how far out should we push this segment?
		// For now, hard-wire a value, but later may want to take into account radius of gyration of the chunk.
		Jump jump = pose.jump( i );
		jump.set_translation( first_rigid_body_position );
		pose.set_jump( i, jump ); // move to 'origin' -- position of first rigid body.

		Real const translation_magnitude( 20.0 );
		RigidBodyPerturbMover rigid_body_perturb_mover( i, 0.0 /*rot_mag_in*/, translation_magnitude, partner_upstream );
		rigid_body_perturb_mover.apply( pose );
	}


}


////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::update_denovo_scorefxn_weights( Size const r )
{
	using namespace core::scoring;

	Real const rna_base_axis_final_weight         = initial_denovo_scorefxn_->get_weight( rna_base_axis );
	Real const rna_base_stagger_final_weight      = initial_denovo_scorefxn_->get_weight( rna_base_stagger );
	Real const rna_base_stack_axis_final_weight   = initial_denovo_scorefxn_->get_weight( rna_base_stack_axis );
	Real const linear_chainbreak_final_weight     = initial_denovo_scorefxn_->get_weight( linear_chainbreak );
	Real const chainbreak_final_weight            = initial_denovo_scorefxn_->get_weight( chainbreak );
	Real const atom_pair_constraint_final_weight  = initial_denovo_scorefxn_->get_weight( atom_pair_constraint );
	Real const coordinate_constraint_final_weight = initial_denovo_scorefxn_->get_weight( coordinate_constraint );
	Real const rna_chem_map_lores_final_weight    = initial_denovo_scorefxn_->get_weight( rna_chem_map_lores );

	//Keep score function coarse for early rounds.
	// Real const suppress  = (r - 1.0) / (rounds - 1.0);
	Real const suppress  = static_cast<Real>( r ) / rounds_;

	denovo_scorefxn_->set_weight( rna_base_axis,      suppress*rna_base_axis_final_weight  );
	denovo_scorefxn_->set_weight( rna_base_stagger,   suppress*rna_base_stagger_final_weight  );
	if ( titrate_stack_bonus_ ) denovo_scorefxn_->set_weight( rna_base_stack_axis,suppress*rna_base_stack_axis_final_weight  );
	denovo_scorefxn_->set_weight( atom_pair_constraint,  suppress*atom_pair_constraint_final_weight  );
	denovo_scorefxn_->set_weight( coordinate_constraint,  suppress*coordinate_constraint_final_weight  );
	denovo_scorefxn_->set_weight( rna_chem_map_lores,   suppress*rna_chem_map_lores_final_weight  );

	// keep chainbreak extra low for early rounds... seems to be important for rigid body modeler.
	//Real suppress_chainbreak  = ( r - ( rounds_/3.0 ) )/ ( static_cast<Real>( rounds_ ) - ( rounds_ / 3.0 ) );
	//Real const suppress_chainbreak_min = 1 / static_cast< Real >( rounds_ );
	//if ( suppress_chainbreak < suppress_chainbreak_min ) suppress_chainbreak = suppress_chainbreak_min;

	denovo_scorefxn_->set_weight( linear_chainbreak,  suppress * linear_chainbreak_final_weight  );
	denovo_scorefxn_->set_weight( chainbreak,  suppress * chainbreak_final_weight  );

}


////////////////////////////////////////////////////////////////////////////////////////
Size
RNA_DeNovoProtocol::figure_out_constraint_separation_cutoff( Size const r, Size const max_dist )
{

	//Keep score function coarse for early rounds.
	Real const suppress  = 5.0 / 3.0 * r / rounds_;

	Size const separation_cutoff ( static_cast<Size>( suppress * max_dist ) + 2 );
	if ( separation_cutoff > max_dist ) return max_dist;

	return separation_cutoff;
}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::update_pose_constraints( Size const r, core::pose::Pose & pose )
{
	using namespace core::scoring::constraints;

	if ( !staged_constraints_ ) return;

	if ( !constraint_set_ ) return;

	static core::kinematics::ShortestPathInFoldTree shortest_path_in_fold_tree( pose.fold_tree() );
	Size const separation_cutoff = figure_out_constraint_separation_cutoff( r, shortest_path_in_fold_tree.max_dist() );
	TR << "ROUND " << r << " out of " << rounds_ << std::endl;
	TR << "FOLD_TREE CURRENT SEPARATION CUTOFF " << separation_cutoff << " out of " << shortest_path_in_fold_tree.max_dist() << std::endl;
	// Fang: apparently separation_cutoff can be smaller than shortest_path_in_fold_tree.dist( i , j )
	// Not sure why but do an early exit hack here
	if ( separation_cutoff >= shortest_path_in_fold_tree.max_dist() ) {
		pose.constraint_set( constraint_set_ );
	} else {
		ConstraintCOPs csts( constraint_set_->get_all_constraints() );
		ConstraintSetOP cst_set_new( new scoring::constraints::ConstraintSet );
		for ( Size n = 1; n <= csts.size(); n++ ) {
			ConstraintCOP const & cst( csts[n] );
			if ( cst->natoms() == 2 )  { // currently only defined for pairwise distance constraints.
				Size const i = cst->atom( 1 ).rsd();
				Size const j = cst->atom( 2 ).rsd();
				Size const dist( shortest_path_in_fold_tree.dist( i , j ) );
				if ( dist  > separation_cutoff ) continue;
			}
			cst_set_new->add_constraint( cst );
		}
		pose.constraint_set( cst_set_new );
	}

	TR << "NUM CONSTRAINTS " << pose.constraint_set()->get_all_constraints().size() << " out of " <<
		constraint_set_->get_all_constraints().size() << std::endl;

}


////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::update_frag_size( Size const r )
{
	Real const frag_size_old = frag_size_;
	frag_size_ = 3;
	if ( r > 1.0 * ( rounds_ / 3.0 ) ) frag_size_ = 2;
	if ( r > 2.0 * ( rounds_ / 3.0 ) ) frag_size_ = 1;
	if ( frag_size_ != frag_size_old ) TR << "Fragment size: " << frag_size_ << std::endl;
}


////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::RNA_move_trial( pose::Pose & pose ) {

	//There are now two kinds of insertions --
	// (1) fragment insertions for, e.g., contiguous 3-mers
	//   and
	// (2) "chunk insertions", which change out whole loops, motifs, or
	//     junctions based on previous models stored in silent files


	if  ( numeric::random::rg().uniform() < jump_change_frequency_ )  {
		//Following returns early if there are no jumps.
		random_jump_trial( pose );
	} else {

		bool did_a_trial( false );
		if ( numeric::random::rg().uniform() < chunk_coverage_ ) {
			did_a_trial = random_chunk_trial( pose );
		}

		if ( !did_a_trial ) {
			random_fragment_trial( pose );
		}
	}


}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::random_jump_trial( pose::Pose & pose ) {

	bool success( false );
	std::string move_type( "" );

	// pose.dump_pdb( "BEFORE.pdb" );
	// std::cout << "BEFORE!" << std::endl;
	// (*denovo_scorefxn_)( pose );
	// denovo_scorefxn_->show( std::cout, pose );

	if ( rigid_body_mover_ &&  numeric::random::rg().uniform() < 0.8 /*totally arbitrary*/ ) {
		rigid_body_mover_->apply( pose );
		success = true; /* rigid body mover is from docking  */
		move_type = "rigid_body";
	} else {
		success = rna_structure_parameters_->random_jump_change( pose );
		move_type = "jump_change";
	}

	if ( !success ) return;

	if ( do_close_loops_ ) rna_loop_closer_->apply( pose );

	// pose.dump_pdb( "AFTER.pdb" );
	// std::cout << "AFTER!" << std::endl;
	// (*denovo_scorefxn_)( pose );
	// denovo_scorefxn_->show( std::cout, pose );
	// exit( 0 );

	monte_carlo_->boltzmann( pose, move_type );

}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::random_fragment_trial( pose::Pose & pose ) {

	rna_fragment_mover_->random_fragment_insertion( pose, frag_size_ );
	if ( do_close_loops_ ) rna_loop_closer_->apply( pose );

	monte_carlo_->boltzmann( pose, "frag" + SS(frag_size_) );

}

////////////////////////////////////////////////////////////////////////////////////////
bool
RNA_DeNovoProtocol::random_chunk_trial( pose::Pose & pose ) {

	// if ( frag_size_ == 2 ) {
	//  pose.dump_pdb( "before_chunk.pdb" );
	//  std::cout << "BEFORE: " << (*denovo_scorefxn_)( pose ) << std::endl;
	//  denovo_scorefxn_->show( std::cout, pose  );
	// }

	bool const did_an_insertion = rna_chunk_library_->random_chunk_insertion( pose );
	if ( !did_an_insertion ) return false;

	// if ( frag_size_ == 2 ) {
	//  pose.dump_pdb( "after_chunk.pdb" );
	//  std::cout << "AFTER: " << (*denovo_scorefxn_)( pose ) << std::endl;
	//  denovo_scorefxn_->show( std::cout, pose );
	//  }

	if ( do_close_loops_ ) rna_loop_closer_->apply( pose );

	monte_carlo_->boltzmann( pose, "chunk" );

	return true /*did an insertion*/;

}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// Following may better fit in a util.cc ,or pose_metrics...
void
RNA_DeNovoProtocol::add_number_base_pairs( pose::Pose const & pose, io::silent::SilentStruct & s ) const
{
	using namespace scoring::rna;
	using namespace pose::rna;
	using namespace conformation;

	// pose::Pose pose = pose_input;
	// (*denovo_scorefxn_)( pose );
	// local_rna_low_resolution_potential_.update_rna_base_pair_list( pose );

	// RNA_ScoringInfo const & rna_scoring_info( rna_scoring_info_from_pose( pose ) );
	// RNA_FilteredBaseBaseInfo const & rna_filtered_base_base_info( rna_scoring_info.rna_filtered_base_base_info() );
	// EnergyBasePairList const & scored_base_pair_list( rna_filtered_base_base_info.scored_base_pair_list() );

	utility::vector1< core::pose::rna::BasePair > base_pair_list;
	utility::vector1< bool > is_bulged;
	core::pose::rna::classify_base_pairs( pose, base_pair_list, is_bulged );

	Size N_WC( 0 ), N_NWC( 0 );

	// for ( EnergyBasePairList::const_iterator it = scored_base_pair_list.begin();
	//    it != scored_base_pair_list.end(); ++it ){
	for ( Size n = 1; n <= base_pair_list.size(); n++ ) {

		BasePair const base_pair = base_pair_list[ n ];

		Size const i = base_pair.res1;
		Size const j = base_pair.res2;

		Size const k = base_pair.edge1;
		Size const m = base_pair.edge2;

		Residue const & rsd_i( pose.residue( i ) );
		Residue const & rsd_j( pose.residue( j ) );

		if ( ( k == core::chemical::rna::WATSON_CRICK && m == core::chemical::rna::WATSON_CRICK
				&& base_pair.orientation == 1 )  &&
				core::chemical::rna::possibly_canonical( rsd_i.aa(), rsd_j.aa() ) )  {
			N_WC++;
		} else {
			N_NWC++;
		}
	}

	s.add_string_value( "N_WC",  ObjexxFCL::format::I( 9, N_WC) );
	s.add_string_value( "N_NWC", ObjexxFCL::format::I( 9, N_NWC ) );
	s.add_string_value( "N_BS",  ObjexxFCL::format::I( 9, core::pose::rna::get_number_base_stacks( pose ) ) );

	//s.add_energy( "N_WC",  N_WC );
	// s.add_energy( "N_NWC", N_NWC );
	// s.add_energy( "N_BS",  core::pose::rna::get_number_base_stacks( pose ) );
}

/////////////////////////////////////////////////////////////////////
bool
check_in_base_pair_list( pose::rna::BasePair const & base_pair /*from native*/,
	utility::vector1< core::pose::rna::BasePair > const & base_pair_list /*for decoy*/)
{
	using namespace pose::rna;

	bool in_list( false );

	for ( Size n = 1; n <= base_pair_list.size(); n++ ) {

		BasePair const base_pair2 = base_pair_list[ n ];

		if ( ( base_pair.res1 == base_pair2.res1 && base_pair.res2 == base_pair2.res2 )  &&
				( base_pair.edge1 == base_pair2.edge1 && base_pair.edge2 == base_pair2.edge2 )  &&
				base_pair.orientation == base_pair2.orientation ) {
			in_list = true;
			break;
		}

		if ( ( base_pair.res2 == base_pair2.res1 && base_pair.res1 == base_pair2.res2 )  &&
				( base_pair.edge2 == base_pair2.edge1 && base_pair.edge1 == base_pair2.edge2 )  &&
				base_pair.orientation == base_pair2.orientation ) {
			in_list = true;
			break;
		}

	}

	return in_list;

}

/////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::add_number_native_base_pairs(pose::Pose & pose, io::silent::SilentStruct & s ) const
{
	if ( !get_native_pose() ) return;

	using namespace scoring::rna;
	using namespace chemical::rna;
	using namespace conformation;

	pose::Pose native_pose = *get_native_pose();

	utility::vector1< core::pose::rna::BasePair > base_pair_list;
	utility::vector1< bool > is_bulged;
	core::pose::rna::classify_base_pairs( pose, base_pair_list, is_bulged );

	utility::vector1< core::pose::rna::BasePair > base_pair_list_native;
	utility::vector1< bool > is_bulged_native;
	core::pose::rna::classify_base_pairs( native_pose, base_pair_list_native, is_bulged_native );


	//(*denovo_scorefxn_)( pose );
	// (*denovo_scorefxn_)( native_pose );
	//local_rna_low_resolution_potential_.update_rna_base_pair_list( native_pose );
	//local_rna_low_resolution_potential_.update_rna_base_pair_list( pose );

	// RNA_ScoringInfo const & rna_scoring_info( rna_scoring_info_from_pose( pose ) );
	// RNA_FilteredBaseBaseInfo const & rna_filtered_base_base_info( rna_scoring_info.rna_filtered_base_base_info() );
	// EnergyBasePairList const & scored_base_pair_list( rna_filtered_base_base_info.scored_base_pair_list() );

	// RNA_ScoringInfo const & rna_scoring_info_native( rna_scoring_info( native_pose ) );
	// RNA_FilteredBaseBaseInfo const & rna_filtered_base_base_info_native( rna_scoring_info_native.rna_filtered_base_base_info() );
	// EnergyBasePairList const & scored_base_pair_list_native( rna_filtered_base_base_info_native.scored_base_pair_list() );

	Size N_WC_NATIVE( 0 ), N_NWC_NATIVE( 0 );
	Size N_WC( 0 ), N_NWC( 0 );

	//std::cout << "BASE PAIR LIST " << std::endl;
	for ( Size n = 1; n <= base_pair_list_native.size(); n++ ) {

		//Real const score = it->first;
		//  Real const SCORE_CUTOFF( -1.0 );
		//  if (score > SCORE_CUTOFF) continue;

		core::pose::rna::BasePair const base_pair = base_pair_list_native[ n ];

		Size const i = base_pair.res1;
		Size const j = base_pair.res2;

		Size const k = base_pair.edge1;
		Size const m = base_pair.edge2;

		Residue const & rsd_i( pose.residue( i ) );
		Residue const & rsd_j( pose.residue( j ) );

		//std::cout << " NATIVE BASE PAIR " << i << " " << j << " " << k << " " << m << " " << it->first << std::endl;

		if ( ( k == core::chemical::rna::WATSON_CRICK && m == core::chemical::rna::WATSON_CRICK
				&& base_pair.orientation == 1 )  &&
				possibly_canonical( rsd_i.aa(), rsd_j.aa() ) )  {
			N_WC_NATIVE++;
			if ( check_in_base_pair_list( base_pair /*from native*/, base_pair_list /*for decoy*/) ) N_WC++;
		} else {
			N_NWC_NATIVE++;
			if ( check_in_base_pair_list( base_pair /*from native*/, base_pair_list /*for decoy*/) ) {
				N_NWC++;
			} else {
				std::cout << "Missing native base pair " << pose.residue( i ).name1() << i << " " << pose.residue(j).name1() << j << "  " << get_edge_from_num( k ) << " " << get_edge_from_num( m ) << " " << std::endl;
			}
		}
	}

	Real f_natWC( 0.0 ), f_natNWC( 0.0 ), f_natBP( 0.0 );
	if ( N_WC_NATIVE > 0 ) f_natWC = ( N_WC / (1.0 * N_WC_NATIVE) );
	if ( N_NWC_NATIVE > 0 ) f_natNWC = ( N_NWC / (1.0 * N_NWC_NATIVE) );
	if ( (N_WC_NATIVE + N_NWC_NATIVE) > 0 ) f_natBP = ( (N_WC+N_NWC) / (1.0 * (N_WC_NATIVE + N_NWC_NATIVE) ));

	s.add_energy( "f_natWC" , f_natWC );
	s.add_energy( "f_natNWC", f_natNWC );
	s.add_energy( "f_natBP" , f_natBP );

}

///////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::set_extra_minimize_res( utility::vector1< core::Size > setting ){
	extra_minimize_res_ = setting;
}

///////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::set_extra_minimize_chi_res( utility::vector1< core::Size > setting ){
	extra_minimize_chi_res_ = setting;
}

///////////////////////////////////////////////////////////////////////////////////////////////
bool
RNA_DeNovoProtocol::check_score_filter( Real const lores_score, std::list< Real > & all_lores_score ){

	all_lores_score.push_back( lores_score );

	all_lores_score.sort(); // nice -- can do this with a list!

	// note that if autofilter_score_quantile_ = 0.20, the first decoy will be 'passed' for free.
	Size const n = all_lores_score.size();
	Size const cutoff_index = static_cast< Size >( n * autofilter_score_quantile_ ) + 1;

	// the one pain with lists -- need to iterate through to find the element corresponding to the quantile score.
	Real all_lores_score_cutoff = all_lores_score.front();
	Size i( 1 );
	for ( std::list< Real >::const_iterator iter = all_lores_score.begin(), end = all_lores_score.end(); iter != end; ++iter, i++ ) {
		if ( i == cutoff_index ) all_lores_score_cutoff = *iter;
	}

	TR << "Comparing current lores score " << lores_score << " to automatically determined cutoff: " << all_lores_score_cutoff << " based on " << autofilter_score_quantile_ << " quantile from "  << n << " models so far" << std::endl;
	return ( lores_score <= all_lores_score_cutoff );
}

///////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::apply_chem_shift_data(core::pose::Pose & pose, std::string const /*out_file_tag*/){

	using namespace core::scoring;
	using namespace core::io::pdb;

	runtime_assert( use_chem_shift_data_ );

	if ( minimize_structure_ ) {
		rna_minimizer_->set_score_function(chem_shift_scorefxn_); //use the chem_shift_scorefxn_
		rna_minimizer_->apply( pose );
		rna_minimizer_->set_score_function(hires_scorefxn_); //set back the original scorefxn.
		if ( close_loops_at_end_ ) rna_loop_closer_->apply( pose, rna_structure_parameters_->connections() );
		final_scorefxn_ = chem_shift_scorefxn_;
	}

	(*chem_shift_scorefxn_)( pose );
}


void
RNA_DeNovoProtocol::add_chem_shift_info(core::io::silent::SilentStruct & silent_struct,
	core::pose::Pose const & const_pose) const {

	using namespace core::scoring;
	using namespace core::pose;

	if ( !use_chem_shift_data_ ) {
		utility_exit_with_message("use_chem_shift_data_ == false!");
	}

	pose::Pose chem_shift_pose=const_pose; //HARD COPY SLOW!

	core::scoring::ScoreFunctionOP temp_scorefxn( new ScoreFunction );

	temp_scorefxn->set_weight( scoring::rna_chem_shift  , 1.00 );

	(*temp_scorefxn)(chem_shift_pose);

	EnergyMap const & energy_map=chem_shift_pose.energies().total_energies();

	Real const rosetta_chem_shift_score= energy_map[ scoring::rna_chem_shift ];

	//This statement should be very fast except possibly the 1st call.
	core::scoring::rna::chemical_shift::RNA_ChemicalShiftPotential const &
		rna_chemical_shift_potential( core::scoring::ScoringManager::
		get_instance()->get_RNA_ChemicalShiftPotential() );

	Size const num_chem_shift_data_points=rna_chemical_shift_potential.get_total_exp_chemical_shift_data_points();

	//rosetta_chem_shift_score --> Sum_square chemical_shift deviation.

	Real const chem_shift_RMSD=sqrt( rosetta_chem_shift_score /
		float(num_chem_shift_data_points) );

	silent_struct.add_energy( "chem_shift_RMSD", chem_shift_RMSD);

	silent_struct.add_energy( "num_chem_shift_data",
		float(num_chem_shift_data_points) );

	if ( silent_struct.has_energy("rna_chem_shift")==false ) {
		//If missing this term, then the rna_chem_shift weight is probably
		//zero in the weight_file.
		silent_struct.add_energy( "rna_chem_shift", 0.0);
	}
}

void
RNA_DeNovoProtocol::set_refine_pose_list( utility::vector1<core::pose::PoseOP> const & setting ) {
	refine_pose_list_ = setting;
	refine_from_silent_ = ( refine_pose_list_.size() != 0 );
	if ( refine_from_silent_ ) rounds_ = 1; // Overide the multi-round behavior
}

std::ostream &operator<< ( std::ostream &os, RNA_DeNovoProtocol const &mover )
{
	mover.show(os);
	return os;
}

} //farna
} //protocols
