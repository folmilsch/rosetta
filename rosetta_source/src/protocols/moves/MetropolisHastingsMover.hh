// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file /protocols/moves/MetropolisHastingsMover.hh
/// @brief
/// @author

#ifndef INCLUDED_protocols_moves_MetropolisHastingsMover_hh
#define INCLUDED_protocols_moves_MetropolisHastingsMover_hh

// Unit Headers
#include <protocols/moves/MetropolisHastingsMover.fwd.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/moves/TemperatureController.hh>

// Project Headers
#include <protocols/moves/MonteCarlo.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <numeric/random/WeightedSampler.hh>

// Utility Headers
#include <core/types.hh>
#include <utility/vector1.hh>

#include <protocols/moves/ThermodynamicMover.fwd.hh>
#include <protocols/moves/ThermodynamicObserver.fwd.hh>

#ifdef WIN32
	#include <protocols/moves/ThermodynamicMover.hh>
	#include <protocols/moves/ThermodynamicObserver.hh>
#endif


namespace protocols {
namespace moves {

///@details
class MetropolisHastingsMover : public Mover {

public:

	MetropolisHastingsMover();

	MetropolisHastingsMover(
		MetropolisHastingsMover const & metropolis_hastings_mover
	);

	virtual
	~MetropolisHastingsMover();

	virtual
	void
	apply( core::pose::Pose & pose );

	virtual
	std::string
	get_name() const;

	MoverOP
	clone() const;

	virtual
	MoverOP
	fresh_instance() const;

	virtual
	bool
	reinitialize_for_each_job() const;

	virtual
	bool
	reinitialize_for_new_input() const;

	virtual
	void
	parse_my_tag(
		utility::tag::TagPtr const tag,
		DataMap & data,
		protocols::filters::Filters_map const & filters,
		Movers_map const & movers,
		core::pose::Pose const & pose
	);

	MonteCarloCOP
	monte_carlo() const;

	void
	set_monte_carlo(
		MonteCarloOP monte_carlo
	);

	void
	set_tempering(
		TemperatureControllerOP
	);

	TemperatureControllerCOP
	tempering() const;

	core::Size
	ntrials() const { return ntrials_; }

	void
	set_ntrials(
		core::Size ntrials
	);

	std::string const &
	output_name() const;

	void
	set_output_name(
		std::string const & output_name
	);

	bool
	finished() const;

	ThermodynamicMoverOP
	random_mover();

	virtual void
	add_mover(
		ThermodynamicMoverOP mover,
		core::Real weight
	);

	void
	add_backrub_mover(
		core::Real weight
	);

	void
	add_small_mover(
		core::Real weight
	);

	void
	add_shear_mover(
		core::Real weight
	);

	void
	add_sidechain_mover(
		core::Real weight,
		core::Real prob_uniform,
		core::Real prob_withinrot,
		bool preserve_cbeta
	);

	void
	add_sidechain_mc_mover(
		core::Real weight,
		core::Real prob_uniform,
		core::Real prob_withinrot,
		bool preserve_cbeta,
		core::Size ntrials
	);

	void
	add_observer(
		ThermodynamicObserverOP observer
	);

	ThermodynamicMover const&
	last_move() const;

	bool
	last_accepted() const {
		return last_accepted_;
	}


protected:

	TemperatureControllerOP const&
	tempering() { return tempering_; }

	void wind_down_simulation( core::pose::Pose& pose);
	void prepare_simulation( core::pose::Pose& pose);

	void set_last_accepted( bool setting ) {
		last_accepted_ = setting;
	}

	void set_last_move( ThermodynamicMoverOP setting );

	typedef utility::vector1< ThermodynamicObserverOP > ObserverList;
	ObserverList const& observers() { return observers_; }

	MonteCarlo& nonconst_monte_carlo();
private:

	MonteCarloOP monte_carlo_;
	core::Size ntrials_;
	core::Size trial_;
	utility::vector1< ThermodynamicMoverOP > movers_;
	numeric::random::WeightedSampler weighted_sampler_;
	utility::vector1< ThermodynamicObserverOP > observers_;
	std::string output_name_;
	TemperatureControllerOP tempering_;

	///some status is necessary for the observers
	ThermodynamicMoverOP last_move_;
	bool last_accepted_;

	//internal book keeping
	bool output_name_from_job_distributor_;

}; //end MetropolisHastingsMover

} //namespace moves
} //namespace protocols

#endif //INCLUDED_protocols_moves_MetropolisHastingsMover_HH
