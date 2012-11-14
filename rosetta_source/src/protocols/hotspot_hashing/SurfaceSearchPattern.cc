// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file
/// @brief
/// @author 

#include <utility/vector1.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/pointer/owning_ptr.hh>

#include <numeric/xyzVector.hh>
#include <numeric/xyzMatrix.hh>
#include <numeric/xyz.functions.hh>

#include <core/types.hh>

#include <core/pose/Pose.hh>

#include <core/pack/task/TaskFactory.hh>

#include <core/conformation/Residue.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/ResidueFactory.hh>

#include <core/scoring/sc/MolecularSurfaceCalculator.hh>

#include <protocols/hotspot_hashing/SearchPattern.hh>
#include <protocols/hotspot_hashing/SurfaceSearchPattern.hh>

#include <protocols/hotspot_hashing/PlaceMinimizeSearch.hh>

namespace protocols {
namespace hotspot_hashing {

static basic::Tracer TR( "protocols.hotspot_hashing.SurfaceSearchPattern" ); 

SurfaceSearchPattern::SurfaceSearchPattern(
	core::pose::Pose const & source_pose,
	core::pack::task::TaskFactoryOP surface_selection_,
	core::Real surface_density,
	core::Real angle_sampling,
	core::Real translocation_sampling,
	core::Real max_radius,
	core::Real distance_sampling,
	core::Real max_distance) : 
	surface_vectors_(),
	surface_density_(surface_density),
	angle_sampling_(angle_sampling),
	translocation_sampling_(translocation_sampling),
	max_radius_(max_radius),
	distance_sampling_(distance_sampling),
	max_distance_(max_distance)
{
	using namespace core::scoring::sc;

	core::pose::Pose pose(source_pose);

	// Create list of target residues using taskoperation
  core::pack::task::PackerTaskOP task = core::pack::task::TaskFactory::create_packer_task( pose );
  if ( surface_selection_ != 0 )
	{
    task = surface_selection_->create_task_and_apply_taskoperations( pose );
		TR.Debug << "Initializing from packer task." << std::endl;
  }
	else
	{
		TR.Debug << "No packer task specified, using default task." << std::endl;
  }

	TR.Debug << "Initializing SurfaceSearchPattern. Density: " << surface_density << std::endl;

	// Perform SC calculation, surface dots will be extracted for partner 1
	MolecularSurfaceCalculator calculator;
	calculator.settings.density = surface_density_;
	calculator.Init();
	calculator.Calc(pose);

	std::vector<DOT> surface_dots = calculator.GetDots(0);

	TR.Debug << "Generated surface dots: " << surface_dots.size() << std::endl;

	core::Size zeronormal_dots = 0;
	core::Size skipped_dots = 0;
	core::Size selected_dots = 0;

	for (core::Size i = 0; i < surface_dots.size(); i++) 
	{
		if (surface_dots[i].outnml.length() == 0)
		{
			zeronormal_dots++;
			continue;
		}
		else if (task->pack_residue(surface_dots[i].atom->nresidue))
		{
			selected_dots++;
			surface_vectors_.push_back(VectorPair(surface_dots[i].coor, surface_dots[i].outnml));
		}
		else
		{
			skipped_dots++;
		}
	}

	TR.Debug << "Zero-length normal dots: " << zeronormal_dots << std::endl;
	TR.Debug << "Skipped dots: " << skipped_dots << std::endl;
	TR.Debug << "Accepted dots: " << selected_dots << std::endl;

	TR.Debug << "Generated surface vectors: " << surface_vectors_.size() << std::endl;

	if ( TR.Trace.visible() )
	{
		TR.Trace << "Raw surface dots:" << "\n";
		TR.Trace << "id atomid residueid residue atom area x y z nx ny nz " << "\n";

		for (core::Size i = 0; i < surface_dots.size(); i++) 
		{
			if (task->pack_residue(surface_dots[i].atom->nresidue))
			{
				TR.Trace <<	i << " " <<
										surface_dots[i].atom->nresidue << " " <<
										surface_dots[i].atom->natom << " " <<
										surface_dots[i].atom->residue << " " <<
										surface_dots[i].atom->atom << " " <<
										surface_dots[i].area << " " <<
										surface_dots[i].coor.x() << " " << 
										surface_dots[i].coor.y() << " " <<
										surface_dots[i].coor.z() << " " <<
										surface_dots[i].outnml.x() << " " <<
										surface_dots[i].outnml.y() << " " <<
										surface_dots[i].outnml.z() << " " <<
										"\n";
			}
		}

		TR.Trace << std::endl;
	}
}

utility::vector1<RT> SurfaceSearchPattern::Searchpoints()
{
	utility::vector1<RT> searchpoints;

	for (core::Size i = 1; i <= surface_vectors_.size(); i++)
	{
		VectorPair search_vector = surface_vectors_[i];

		Vector xunit = Vector(0, 0, 1);
		Matrix normal_rotation = numeric::rotation_matrix( search_vector.direction.cross(xunit), angle_of(search_vector.direction, xunit));
		
		for (core::Real x = -max_radius_; x <= max_radius_; x += translocation_sampling_)
		{
			for (core::Real y = -max_radius_; y <= max_radius_; y += translocation_sampling_)
			{
				if(sqrt(x*x + y*y) <= max_radius_)
				{
					for (core::Real z = 0; z <= max_distance_; z += distance_sampling_)
					{
						for (core::Real angle = 0; angle < 360; angle += angle_sampling_)
						{
							Vector translation = Vector(x, y, z);
							Matrix rotation = numeric::x_rotation_matrix_degrees(angle);

							RT tp(
									rotation * normal_rotation,
									translation + search_vector.position);
							searchpoints.push_back(tp);
						}
					}
				}
			}
		}
	}

	return searchpoints;
}

}
}
