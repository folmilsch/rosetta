// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file src/protocols/denovo_design/filters/SSPredictionFilter.hh
/// @brief header file for filter to determine agreement with psipred for secondary structure prediction
/// @details
/// @author Tom Linsky (tlinsky@uw.edu)

#ifndef INCLUDED_protocols_denovo_design_filters_sspredictionfilter_hh
#define INCLUDED_protocols_denovo_design_filters_sspredictionfilter_hh

// Unit Headers
#include <protocols/denovo_design/filters/SSPredictionFilter.fwd.hh>

// Package headers

// Project headers
#include <core/conformation/Residue.hh>
#include <core/io/external/PsiPredInterface.fwd.hh>
#include <protocols/filters/Filter.hh>
#include <protocols/filters/FilterCreator.hh>
#include <protocols/parser/BluePrint.fwd.hh>
#include <protocols/ss_prediction/SS_predictor.fwd.hh>

namespace protocols {
namespace denovo_design {
namespace filters {

// main SSPredictionFilter class
class SSPredictionFilter : public protocols::filters::Filter {
public:
	SSPredictionFilter();
	SSPredictionFilter( core::Real const threshold,
		std::string const & cmd,
		std::string const & blueprint_filename,
		bool const use_probability,
		bool const mismatch_probability,
		bool const use_scratch_dir = false );
	~SSPredictionFilter() override;
	bool apply( core::pose::Pose const & pose ) const override;
	protocols::filters::FilterOP clone() const override;
	protocols::filters::FilterOP fresh_instance() const override;
	void report( std::ostream & out, core::pose::Pose const & pose ) const override;
	core::Real report_sm( core::pose::Pose const & pose ) const override;
	core::Real compute( core::pose::Pose const & pose ) const;

	void parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data_map,
		core::pose::Pose const & pose ) override;

	std::string
	name() const override;

	static
	std::string
	class_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );
	void set_secstruct(std::string const secstruct);

private:
	/// @brief computes the weighted boltzmann sum of the passed vector
	core::Real compute_boltz_sum( utility::vector1< core::Real > const & probabilities ) const;

private:
	/// @brief computes one minus the geometric mean of the passed vector
	core::Real compute_mismatch_prob( utility::vector1< core::Real > const & probabilities ) const;

	/// @brief set the scratch dir to out::path::scratch or error if that's not set
	void set_scratch_dir();


private:
	core::Real threshold_;
	std::string cmd_;
	protocols::parser::BluePrintOP blueprint_;
	// tells whether to just count residues that don't match, or use the probability generated by psipred
	bool use_probability_;
	// assumes use_probability, tells whether to use probabilities directly or in a (weird) weighted way
	bool mismatch_probability_;
	/// @brief tells whether to use the psipred pass2 confidence values -- overrrides use_probability.
	bool use_confidence_;
	/// @brief should we use svm to estimate secondary structure?
	bool use_svm_;
	/// @brief what temperature should we be using doing a boltzmann sum?
	core::Real temp_;
	/// @brief the object which predicts the secondary structure
	protocols::ss_prediction::SS_predictorOP ss_predictor_;
	/// @brief the object which communicates with psipred and interprets its output
	core::io::external::PsiPredInterfaceOP psipred_interface_;
	std::string secstruct_;
	/// @brief the scratch folder to use with psipred (or "")
	std::string scratch_dir_;
};  //SSPredictionFilter

//namespaces
}
}
}

#endif
