// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/medal/MedalMover.cc
/// @author Christopher Miles (cmiles@uw.edu)

// Unit headers
#include <protocols/medal/MedalMover.hh>

// C/C++ headers
#include <iostream>
#include <string>

// External headers
#include <boost/unordered/unordered_map.hpp>

// Utility headers
#include <basic/Tracer.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/OptionKeys.hh>
#include <basic/options/keys/abinitio.OptionKeys.gen.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>
#include <basic/options/keys/jumps.OptionKeys.gen.hh>
#include <basic/options/keys/rigid.OptionKeys.gen.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <utility/vector1.hh>

// Project headers
#include <core/types.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/VariantType.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/Jump.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/constraints/util.hh>
#include <core/scoring/methods/EnergyMethodOptions.hh>
#include <core/sequence/SequenceAlignment.hh>
#include <core/util/SwitchResidueTypeSet.hh>
#include <protocols/abinitio/MaxSeqSepConstraintSet.hh>
#include <protocols/comparative_modeling/util.hh>
#include <protocols/jd2/InnerJob.hh>
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/jd2/ThreadingJob.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/util.hh>
#include <protocols/moves/RationalMonteCarlo.hh>
#include <protocols/moves/RigidBodyMotionMover.hh>
#include <protocols/nonlocal/StarTreeBuilder.hh>

namespace protocols {
namespace medal {

typedef boost::unordered_map<int, core::kinematics::Jump> Jumps;

static basic::Tracer TR("protocols.medal.MedalMover");

void MedalMover::apply(core::pose::Pose& pose) {
  using namespace basic::options;
  using namespace basic::options::OptionKeys;
  using core::scoring::ScoreFunctionOP;
  using core::sequence::SequenceAlignment;
  using protocols::jd2::ThreadingJob;
  using protocols::loops::Loops;
  using protocols::moves::MoverOP;
  using protocols::moves::RationalMonteCarlo;
  using protocols::moves::RigidBodyMotionMover;
  using protocols::nonlocal::StarTreeBuilder;
  using std::endl;

  // Always operate in centroid mode
  core::util::switch_to_residue_type_set(pose, core::chemical::CENTROID);

  // Retrieve the current job from jd2, identify aligned regions
  ThreadingJob const * const job = current_job();
  const SequenceAlignment& alignment = job->alignment();
  Loops unaligned = protocols::comparative_modeling::loops_from_alignment(pose.total_residue(), alignment, 0);
  Loops aligned = unaligned.invert(pose.total_residue());

  TR << "Alignment: " << alignment.alignment_id() << endl;
  TR << "Aligned regions: " << aligned << endl;
  TR << "Unaligned regions: " << unaligned << endl;

  // Star fold tree construction
  StarTreeBuilder builder;
  builder.set_up(aligned, &pose);

  // Score function setup
  ScoreFunctionOP score = score_function(&pose);
  score->show(TR, pose);
  TR.flush_all_channels();

  Jumps jumps;
  jumps_from_pose(pose, &jumps);
  add_cutpoint_variants(&pose);

  MoverOP mover = new RationalMonteCarlo(
        new RigidBodyMotionMover(aligned, jumps),
        score,
        option[OptionKeys::rigid::cycles](),
        option[OptionKeys::rigid::temperature](),
        true);

  mover->apply(pose);

  // Remove virtual residue placed during star fold tree construction
  remove_cutpoint_variants(&pose);
  builder.tear_down(&pose);
}

void MedalMover::jumps_from_pose(const core::pose::Pose& pose, Jumps* jumps) const {
  assert(jumps);
  for (core::Size i = 1; i <= pose.num_jump(); ++i) {
    (*jumps)[i] = pose.jump(i);
    TR.Debug << "Added jump_num " << i << std::endl;
  }
}

core::scoring::ScoreFunctionOP MedalMover::score_function(core::pose::Pose* pose) const {
  using namespace basic::options;
  using namespace basic::options::OptionKeys;
  using core::scoring::ScoreFunctionFactory;
  using core::scoring::ScoreFunctionOP;
  using core::scoring::methods::EnergyMethodOptions;
  assert(pose);

  ScoreFunctionOP score = ScoreFunctionFactory::create_score_function("score0");

  // Allowable sequence separation
  EnergyMethodOptions options(score->energy_method_options());
  options.cst_max_seq_sep(option[OptionKeys::rigid::sequence_separation]());
  score->set_energy_method_options(options);

  // Enable specific energy terms
  score->set_weight(core::scoring::atom_pair_constraint, option[OptionKeys::constraints::cst_weight]());
  score->set_weight(core::scoring::hbond_lr_bb, 1);
  score->set_weight(core::scoring::hbond_sr_bb, 1);
  score->set_weight(core::scoring::linear_chainbreak, option[OptionKeys::jumps::increase_chainbreak]());
  score->set_weight(core::scoring::rama, 0.1);
  score->set_weight(core::scoring::rg, 1.5 * option[OptionKeys::abinitio::rg_reweight]());
  score->set_weight(core::scoring::sheet, 1);

  /// ... more here

  core::scoring::constraints::add_constraints_from_cmdline(*pose, *score);
  return score;
}

// TODO(cmiles) deduplicate w/ protocols/nonlocal/NonlocalAbinitio
protocols::jd2::ThreadingJob const * const MedalMover::current_job() const {
  using protocols::jd2::InnerJobCOP;
  using protocols::jd2::JobDistributor;
  using protocols::jd2::ThreadingJob;

  JobDistributor* jd2 = JobDistributor::get_instance();
  InnerJobCOP inner = jd2->current_job()->inner_job();
  return (ThreadingJob const * const) inner();
}

std::string MedalMover::get_name() const {
  return "MedalMover";
}

protocols::moves::MoverOP MedalMover::clone() const {
  return new MedalMover(*this);
}

protocols::moves::MoverOP MedalMover::fresh_instance() const {
  return new MedalMover();
}

// TODO(cmiles) deduplicate w/ protocols/nonlocal/BrokenFold
void MedalMover::add_cutpoint_variants(core::pose::Pose* pose) const {
  const core::kinematics::FoldTree& tree(pose->fold_tree());
  for (core::Size i = 1; i <= pose->total_residue(); ++i) {
    if (!tree.is_cutpoint(i) || i >= (pose->total_residue() - 1))
      continue;

    core::pose::add_variant_type_to_pose_residue(*pose, core::chemical::CUTPOINT_LOWER, i);
    core::pose::add_variant_type_to_pose_residue(*pose, core::chemical::CUTPOINT_UPPER, i+1);
  }
}

// TODO(cmiles) deduplicate w/ protocols/nonlocal/BrokenFold
void MedalMover::remove_cutpoint_variants(core::pose::Pose* pose) const {
  const core::kinematics::FoldTree& tree(pose->fold_tree());
  for (core::Size i = 1; i <= pose->total_residue(); ++i) {
    if (!tree.is_cutpoint(i) || i >= (pose->total_residue() - 1))
      continue;

    core::pose::remove_variant_type_from_pose_residue(*pose, core::chemical::CUTPOINT_LOWER, i);
    core::pose::remove_variant_type_from_pose_residue(*pose, core::chemical::CUTPOINT_UPPER, i+1);
  }
}

}  // namespace medal
}  // namespace protocols
