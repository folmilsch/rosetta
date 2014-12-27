// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/protocols/abinitio/abscript/AbscriptLoopCloserCM.cc
/// @author Justin Porter

// Unit Headers
#include <protocols/abinitio/abscript/AbscriptLoopCloserCM.hh>
#include <protocols/abinitio/abscript/AbscriptLoopCloserCMCreator.hh>

// Package headers
#include <protocols/environment/claims/TorsionClaim.hh>

#include <protocols/environment/DofUnlock.hh>
#include <protocols/environment/EnvExcn.hh>

#include <core/environment/DofPassport.hh>

// Project headers
#include <core/fragment/FragmentIO.hh>
#include <core/fragment/FragSet.hh>

#include <core/kinematics/MoveMap.hh>

#include <core/scoring/ScoreFunctionFactory.hh>

#include <core/pack/task/residue_selector/TrueResidueSelector.hh>

#include <protocols/loops/loop_closure/ccd/WidthFirstSlidingWindowLoopClosure.hh>
#include <protocols/loops/Exceptions.hh>

#include <protocols/checkpoint/CheckPointer.hh>

#include <protocols/jumping/util.hh>

#include <protocols/idealize/IdealizeMover.hh>

#include <protocols/rosetta_scripts/util.hh>

#ifdef WIN32
  #include <basic/datacache/WriteableCacheableMap.hh>
#endif

//Utility Headers

#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/abinitio.OptionKeys.gen.hh>
#include <basic/options/keys/frags.OptionKeys.gen.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>
#include <basic/options/keys/jumps.OptionKeys.gen.hh>

#include <utility/excn/Exceptions.hh>
#include <utility/tag/Tag.hh>

#include <boost/functional/hash.hpp>
#include <boost/foreach.hpp>

// tracer
#include <basic/Tracer.hh>

// C++ Headers

// ObjexxFCL Headers

static thread_local basic::Tracer tr( "protocols.abinitio.abscript.AbscriptLoopCloserCM", basic::t_info );

namespace protocols {
namespace abinitio {
namespace abscript {

using namespace core::environment;
using namespace protocols::environment;

// creator
std::string
AbscriptLoopCloserCMCreator::keyname() const {
  return AbscriptLoopCloserCMCreator::mover_name();
}

protocols::moves::MoverOP
AbscriptLoopCloserCMCreator::create_mover() const {
  return protocols::moves::MoverOP( new AbscriptLoopCloserCM );
}

std::string
AbscriptLoopCloserCMCreator::mover_name() {
  return "AbscriptLoopCloserCM";
}

AbscriptLoopCloserCM::AbscriptLoopCloserCM():
  Parent(),
  fragset_(),
  scorefxn_(),
  selector_( new core::pack::task::residue_selector::TrueResidueSelector() )
{}

AbscriptLoopCloserCM::AbscriptLoopCloserCM( core::fragment::FragSetCOP fragset,
                                            core::scoring::ScoreFunctionOP scorefxn ):
  Parent(),
  fragset_( fragset ),
  scorefxn_( scorefxn ),
  selector_( new core::pack::task::residue_selector::TrueResidueSelector() ),
  bUpdateMM_( true )
{}

claims::EnvClaims AbscriptLoopCloserCM::yield_claims( core::pose::Pose const&,
                                                      basic::datacache::WriteableCacheableMapOP ){
  claims::EnvClaims claims;

  // We want to control everything that will be relevant to the output pose (which should be the same as the input.
  claims::TorsionClaimOP claim( new claims::TorsionClaim( utility::pointer::static_pointer_cast< ClaimingMover > ( get_self_ptr() ), selector() ) );
  claim->strength( claims::CAN_CONTROL, claims::DOES_NOT_CONTROL );

  claims.push_back( claim );

  return claims;
}

void AbscriptLoopCloserCM::apply( core::pose::Pose& in_pose ){

  assert( passport() );
  assert( final_ft_ );

  //Produce unprotected pose
  core::pose::Pose pose( in_pose );
  core::conformation::ConformationOP deprotected( new core::conformation::Conformation( in_pose.conformation() ) );
  pose.set_new_conformation( deprotected );

  tr.Debug << "Closing loops with current fold tree: " << in_pose.fold_tree();
  tr.Debug << "Using final fold tree: " << *final_ft_ << std::endl;

  // Close Loops
  bool success;

  attempt_idealize( pose );

  success = attempt_ccd( pose );

  if( success ){
    attempt_idealize( pose );
  } else {
    // We could implement "don't fail unclosed" here, but I don't think we need to?
    return;
  }

  // Copy result into protected conformation in in_pose
  DofUnlock unlock( in_pose.conformation(), passport() );

  for ( Size i = 1; i <= in_pose.total_residue(); ++i ) {
    tr.Debug << "Movemap for " << in_pose.residue( i ).name3() << i
             << ": bb(" << ( movemap_->get_bb( i ) ? "T" : "F" )
             << ") chi(" << ( movemap_->get_chi( i ) ? "T" : "F" ) << ")" << std::endl;
    try {
      if( pose.omega( i ) != in_pose.omega( i ) ){
        in_pose.set_omega( i, pose.omega( i ) );
      }
      if( pose.phi( i ) != in_pose.phi( i ) ){
        in_pose.set_phi( i, pose.phi( i ) );
      }
      if( pose.psi( i ) != in_pose.psi( i ) ) {
        in_pose.set_psi( i, pose.psi( i ) );
      }
    } catch ( EXCN_Env_Security_Exception& ){
      tr.Error << "[ERROR] Unauthorized changes occurred during loop closure (attempt to write to resid "
               << i << ")." << std::endl;
      throw;
    }
  }
}

bool AbscriptLoopCloserCM::attempt_ccd( core::pose::Pose& pose ){
  using namespace loops::loop_closure::ccd;

  try {
    checkpoint::CheckPointer checkpointer( "AbscriptLoopCloserCM" );
    SlidingWindowLoopClosureOP closing_protocol;
    closing_protocol = SlidingWindowLoopClosureOP( new WidthFirstSlidingWindowLoopClosure( fragset_,
                                                               scorefxn_,
                                                               movemap_ ) );

    jumping::close_chainbreaks( closing_protocol,
                               pose,
                               checkpointer,
                               get_current_tag(),
                               *final_ft_ );
  } catch ( loops::EXCN_Loop_not_closed& excn ) {
    set_last_move_status( moves::FAIL_DO_NOT_RETRY );
    set_current_tag( "C_"+get_current_tag().substr(std::min(2,(int)get_current_tag().size())) );
    return false;
  }

  return true;
}

void AbscriptLoopCloserCM::parse_my_tag( utility::tag::TagCOP tag,
                                         basic::datacache::DataMap & datamap,
                                         protocols::filters::Filters_map const&,
                                         protocols::moves::Movers_map const&,
                                         core::pose::Pose const& ) {

  using namespace basic::options::OptionKeys;
  using namespace basic::options;

	if( tag->hasOption( "selector" ) ) {
		set_selector( datamap.get_ptr< core::pack::task::residue_selector::ResidueSelector const >( "ResidueSelector", tag->getOption<std::string>( "selector" ) ) );
	} else {
		set_selector( core::pack::task::residue_selector::ResidueSelectorCOP( new core::pack::task::residue_selector::TrueResidueSelector() ) );
	}

  std::string fragfile;
  if(tag->hasOption("fragments"))
    fragfile = tag->getOption<std::string>("fragments");
  else if(!option[OptionKeys::in::file::frag3].user()){
    tr.Error <<"enter frags with either fragments= or -in:file:frag3"  << std::endl;
    throw utility::excn::EXCN_RosettaScriptsOption( "fragment file wrong type" );
  }
  else
    fragfile = option[ OptionKeys::in::file::frag3 ]();

  core::fragment::FragmentIO frag_io( option[ OptionKeys::abinitio::number_3mer_frags ](), 1,
                                      option[ OptionKeys::frags::annotate ]() );

  fragset_ = frag_io.read_data( fragfile );

  if( tag->hasOption( "scorefxn" ) ){
    try {
      scorefxn_ = protocols::rosetta_scripts::parse_score_function( tag, datamap );
    } catch ( ... ) {
      tr.Error << "AbscriptLoopCloserCM failed to find the score function '"
               << tag->getOption< std::string >( "scorefxn" ) << std::endl;
      throw;
    }
  } else {
    tr.Warning << "Configuring AbscriptLoopCloserCM '" << tag->getName() << "' with default abinitio loop closure score function." << std::endl;

    option[ OptionKeys::abinitio::stage4_patch ].activate();

    scorefxn_ = core::scoring::ScoreFunctionFactory::create_score_function( "score3", option[ OptionKeys::abinitio::stage4_patch ]() );
    scorefxn_->set_weight( core::scoring::linear_chainbreak, option[ jumps::chainbreak_weight_stage4 ]() );
    scorefxn_->set_weight( core::scoring::atom_pair_constraint, option[ OptionKeys::constraints::cst_weight ] );
  }
}

void AbscriptLoopCloserCM::attempt_idealize( core::pose::Pose& pose ) {
  idealize::IdealizeMoverOP idealizer( new idealize::IdealizeMover );
  idealizer->fast( false );

  utility::vector1< core::Size > pos_list;
  for( core::Size i = 1; i <= pose.total_residue(); ++i ){
    if( movemap_->get_bb( i ) ){
      pos_list.push_back( i );
    }
    if( tr.Trace.visible() ){
      for( core::Size i = 1; i <= pose.total_residue(); ++i ){
        tr.Trace << ( movemap_->get_bb(i) ? "T" : "F" );
      }
      tr.Trace << std::endl;
    }
  }

  if( pos_list.size() == 0 &&
      pose.fold_tree().num_cutpoint() - final_ft_->num_cutpoint() > 0 ){
    std::ostringstream ss;
    ss << this->get_name() << " tried to start closing the fold tree by idealization ("
       << pose.fold_tree().num_cutpoint() - final_ft_->num_cutpoint()
       << " cuts are to be closed) but was not given any movable positions." << std::endl;
    throw utility::excn::EXCN_BadInput( ss.str() );
  }

  idealizer->set_pos_list( pos_list );

  core::kinematics::FoldTree fold_tree( pose.fold_tree() );
  pose.fold_tree( *final_ft_ );
  if( pos_list.size() > 0 ) // req'd because an empty pos_list is treated as all
    idealizer->apply( pose );
  pose.fold_tree( fold_tree );
  (*scorefxn_)( pose );
  //jd2::output_intermediate_pose( pose, "loops_closed_preprocessed" );
}

void AbscriptLoopCloserCM::passport_updated() {
  if( has_passport() ){
    movemap_ = passport()->render_movemap();
  } else {
    movemap_ = core::kinematics::MoveMapOP( new core::kinematics::MoveMap() );
    movemap_->set_bb( true );
  }
}


void AbscriptLoopCloserCM::broking_finished( EnvClaimBroker::BrokerResult const& broker ) {
  //set the target fold tree for the closer.
  final_ft_ = broker.closer_ft;
  assert( final_ft_ );
}

std::string AbscriptLoopCloserCM::get_name() const {
  return "AbscriptLoopCloserCM";
}

moves::MoverOP AbscriptLoopCloserCM::clone() const {
  return moves::MoverOP( new AbscriptLoopCloserCM( *this ) );
}

} // abscript
} // abinitio
} // protocols
