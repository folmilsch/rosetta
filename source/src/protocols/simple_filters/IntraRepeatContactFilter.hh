// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.
//
/// @file protocols/simple_filters/IntraRepeatContactFilter
/// @brief filter structures by IntraRepeatContacts
/// @details
/// @author TJ Brunette

#ifndef INCLUDED_protocols_simple_filters_IntraRepeatContactFilter_hh
#define INCLUDED_protocols_simple_filters_IntraRepeatContactFilter_hh

// Unit Headers
#include <protocols/simple_filters/IntraRepeatContactFilter.fwd.hh>

// Package Headers
#include <protocols/filters/Filter.hh>

// Project Headers
#include <core/pose/Pose.fwd.hh>

// Utility headers

// Parser headers
#include <basic/datacache/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <protocols/filters/Filter.fwd.hh>
#include <utility/tag/Tag.fwd.hh>

#include <utility/vector1.hh>

//// C++ headers

namespace protocols {
namespace simple_filters {

class IntraRepeatContactFilter : public protocols::filters::Filter {
public:

	typedef protocols::filters::Filter Super;
	typedef protocols::filters::Filter Filter;
	typedef protocols::filters::FilterOP FilterOP;
	typedef core::Size Size;
	typedef core::Real Real;
	typedef core::pose::Pose Pose;
	typedef std::string String;

	typedef utility::tag::TagCOP TagCOP;
	typedef basic::datacache::DataMap DataMap;


public:// constructor/destructor


	// @brief default constructor
	IntraRepeatContactFilter();

	// @brief copy constructor
	IntraRepeatContactFilter( IntraRepeatContactFilter const & rval );

	~IntraRepeatContactFilter() override= default;


public:// virtual constructor


	// @brief make clone
	filters::FilterOP clone() const override {
		return utility::pointer::make_shared< IntraRepeatContactFilter >( *this );
	}

	// @brief make fresh instance
	filters::FilterOP fresh_instance() const override {
		return utility::pointer::make_shared< IntraRepeatContactFilter >();
	}


public:// mutator


	// @brief
	void filtered_value( Real const & value );


public:// accessor


	// @brief get name of this filter


public:// parser

	void parse_my_tag( TagCOP tag,
		basic::datacache::DataMap &,
		Pose const & ) override;


public:// virtual main operation


	// @brief returns true if the given pose passes the filter, false otherwise.
	// In this case, the test is whether the give pose is the topology we want.
	bool apply( Pose const & pose ) const override;

	/// @brief
	Real report_sm( Pose const & pose ) const override;

	/// @brief used to report score
	void report( std::ostream & out, Pose const & pose ) const override;

	/// @brief
	Real compute( Pose const & pose ) const;

	std::string
	name() const override;

	static
	std::string
	class_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );



private:

	Real filtered_value_;
	core::Size numbRepeats_;
	core::Size sequenceSep_;
	Real distThresh_;
};

} // filters
} // protocols

#endif

