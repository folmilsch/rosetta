// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/constraint_generator/RemoveConstraints.cc
/// @brief Removes constraints generated by a ConstraintGenerator
/// @author Tom Linsky (tlinsky@uw.edu)

// Unit headers
#include <protocols/constraint_generator/RemoveConstraints.hh>
#include <protocols/constraint_generator/RemoveConstraintsCreator.hh>

// Protocol headers
#include <protocols/constraint_generator/ConstraintGenerator.hh>
#include <protocols/constraint_generator/ConstraintsManager.hh>
#include <protocols/constraint_generator/util.hh>

// Core headers
#include <core/pose/Pose.hh>

// Basic/Utility headers
#include <basic/datacache/DataMap.hh>
#include <basic/Tracer.hh>
#include <utility/string_util.hh>
#include <utility/tag/Tag.hh>

static THREAD_LOCAL basic::Tracer TR( "protocols.constraint_generator.RemoveConstraints" );

namespace protocols {
namespace constraint_generator {

RemoveConstraints::RemoveConstraints():
	protocols::moves::Mover( RemoveConstraints::class_name() ),
	generators_()
{
}

RemoveConstraints::RemoveConstraints( ConstraintGeneratorCOPs const & generators ):
	protocols::moves::Mover( RemoveConstraints::class_name() ),
	generators_( generators )
{
}

RemoveConstraints::~RemoveConstraints()
{
}

void
RemoveConstraints::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & ,
	protocols::moves::Movers_map const & ,
	core::pose::Pose const & )
{
	std::string const generators_str = tag->getOption< std::string >( "constraint_generators", "" );
	ConstraintGeneratorCOPs generators = parse_constraint_generators( tag, data );
	if ( generators.empty() ) {
		std::stringstream msg;
		msg << "RemoveConstraints: You must specify 'constraint_generators' -- a comma-separated list of names of constraint generators defined in an AddConstraints mover." << std::endl;
		throw utility::excn::EXCN_RosettaScriptsOption( msg.str() );
	}
}

protocols::moves::MoverOP
RemoveConstraints::clone() const{
	return protocols::moves::MoverOP( new RemoveConstraints( *this ) );
}

moves::MoverOP
RemoveConstraints::fresh_instance() const
{
	return protocols::moves::MoverOP( new RemoveConstraints );
}

std::string
RemoveConstraints::get_name() const
{
	return "RemoveConstraints";
}

void
RemoveConstraints::apply( core::pose::Pose & pose )
{
	using core::scoring::constraints::ConstraintCOPs;

	ConstraintsManager const & manager = *ConstraintsManager::get_instance();
	for ( ConstraintGeneratorCOPs::const_iterator cg=generators_.begin(); cg!=generators_.end(); ++cg ) {
		debug_assert( *cg );
		ConstraintCOPs const & csts = manager.retrieve_constraints( pose, (*cg)->id() );

		TR << "Removing " << csts.size() << " constraints from pose generated by " << (*cg)->id() << std::endl;

		if ( !csts.empty() ) {
			if ( ! pose.remove_constraints( csts, false ) ) {
				if ( ! pose.remove_constraints( csts, true ) ) {
					throw EXCN_RemoveCstsFailed();
				}
			}
		}
		manager.remove_constraints( pose, (*cg)->id() );
	}
}

void
RemoveConstraints::add_generator( ConstraintGeneratorCOP generator )
{
	generators_.push_back( generator );
}

/////////////// Creator ///////////////

protocols::moves::MoverOP
RemoveConstraintsCreator::create_mover() const
{
	return protocols::moves::MoverOP( new RemoveConstraints );
}

std::string
RemoveConstraintsCreator::keyname() const
{
	return RemoveConstraints::class_name();
}

} //protocols
} //constraint_generator


