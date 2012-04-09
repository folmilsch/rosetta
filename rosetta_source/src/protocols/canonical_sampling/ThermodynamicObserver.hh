// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file /protocols/canonical_sampling/ThermodynamicObserver.hh
/// @brief
/// @author

#ifndef INCLUDED_protocols_canonical_sampling_ThermodynamicObserver_hh
#define INCLUDED_protocols_canonical_sampling_ThermodynamicObserver_hh

// Unit Headers
#include <protocols/canonical_sampling/ThermodynamicObserver.fwd.hh>

// Project Headers
#include <protocols/canonical_sampling/MetropolisHastingsMover.fwd.hh>
// AUTO-REMOVED #include <core/id/DOF_ID_Range.hh>
// AUTO-REMOVED #include <core/id/TorsionID_Range.hh>
#include <core/pose/Pose.fwd.hh>
#include <protocols/moves/Mover.hh>

// Utility Headers
#include <core/types.hh>

#include <utility/vector1.hh>


namespace protocols {
namespace canonical_sampling {

///@details
class ThermodynamicObserver : public protocols::moves::Mover {

public:

	///@brief
	ThermodynamicObserver();

	virtual
	~ThermodynamicObserver();

	virtual
	void apply( core::pose::Pose& ) {};
	/// @brief callback executed before any Monte Carlo trials

	virtual
	bool
	restart_simulation(
		core::pose::Pose &,
		protocols::canonical_sampling::MetropolisHastingsMover&,
		core::Size&,
		core::Size&,
		core::Real& 
	) { return false; }


	virtual
	void
	initialize_simulation(
		core::pose::Pose &,
		protocols::canonical_sampling::MetropolisHastingsMover const &,
		core::Size //non-zero if trajectory is restarted
	) {};

	/// @brief callback executed after the Metropolis criterion is evaluated
	virtual
	void
	observe_after_metropolis(
		protocols::canonical_sampling::MetropolisHastingsMover const & metropolis_hastings_mover
	) = 0;

	/// @brief callback executed after all Monte Carlo trials
	virtual
	void
	finalize_simulation(
		core::pose::Pose &,
		protocols::canonical_sampling::MetropolisHastingsMover const &
	) {};

	/// @brief return false here if a valid pose is not required for "observe"
	/// i.e. a trialcounter
	virtual
	bool
	requires_pose() { return true; };

private:

}; //end ThermodynamicObserver

} //namespace canonical_sampling
} //namespace protocols

#endif // INCLUDED_protocols_canonical_sampling_ThermodynamicObserver_HH
