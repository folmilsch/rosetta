// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 sw=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file protocols/hotspot_hashing/movers/PlaceSurfaceProbe.hh
/// @brief
/// @author Alex Ford fordas@uw.edu

#ifndef INCLUDED_protocols_hotspot_hashing_movers_PlaceSurfaceProbe_hh
#define INCLUDED_protocols_hotspot_hashing_movers_PlaceSurfaceProbe_hh


// Project Headers
#include <string>

#include <protocols/moves/Mover.hh>
#include <core/pose/Pose.fwd.hh>
#include <utility/tag/Tag.fwd.hh>
#include <protocols/moves/DataMap.fwd.hh>

#include <protocols/hotspot_hashing/movers/PlaceSurfaceProbe.fwd.hh>
#include <protocols/hotspot_hashing/movers/PlaceProbeMover.hh>

// Unit headers

namespace protocols
{
namespace hotspot_hashing
{
namespace movers
{

class PlaceSurfaceProbe : public protocols::hotspot_hashing::movers::PlaceProbeMover, virtual public protocols::moves::Mover
{
  public:
    PlaceSurfaceProbe();

    PlaceSurfaceProbe(
			std::string residue_name,
			core::Real search_density,
			core::Real angle_sampling,
			core::Real translocation_sampling,
			core::Real max_radius,
			core::Real distance_sampling,
			core::Real max_distance,
			core::conformation::ResidueCOP target_residue,
			core::pack::task::TaskFactoryOP surface_selection = NULL,
      core::Size search_partition = 1,
      core::Size total_search_partition = 1);

    virtual std::string get_name() const { return "PlaceSurfaceProbe"; }

		virtual protocols::moves::MoverOP clone() const;

    void parse_my_tag(
         utility::tag::TagPtr const tag,
         protocols::moves::DataMap &,
         protocols::filters::Filters_map const &,
         protocols::moves::Movers_map const &,
         core::pose::Pose const &);

    virtual bool reinitialize_for_new_input() const { return false; }

  protected:
    virtual SearchPatternOP create_search_pattern(core::pose::Pose const & target_pose);

  private:

		core::Real search_density_;
		core::pack::task::TaskFactoryOP surface_selection_;

		core::Real angle_sampling_;
		core::Real translocation_sampling_;
		core::Real max_radius_;
		core::Real distance_sampling_;
		core::Real max_distance_;
};

}
}
}

#endif 
