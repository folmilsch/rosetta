// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/fold_from_loops/NubInitioLoopClosureMover.hh
/// @brief Applies CCD closure to a NubInitioMover results being aware of the protocol's restrictions through labels.
/// @author Jaume Bonet (jaume.bonet@gmail.com)

#ifndef INCLUDED_fold_from_loops_movers_NubInitioLoopClosureMover_hh
#define INCLUDED_fold_from_loops_movers_NubInitioLoopClosureMover_hh

// Unit headers
#include <protocols/fold_from_loops/movers/NubInitioLoopClosureMover.fwd.hh>
#include <protocols/moves/Mover.hh>

// Protocol headers

// Core headers
#include <core/types.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/fragment/FragSet.fwd.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/select/residue_selector/ResidueSelector.hh>
#include <core/select/residue_selector/ReturnResidueSubsetSelector.hh>

// Basic/Utility headers
#include <basic/datacache/DataMap.fwd.hh>

#include <set>

namespace protocols {
namespace fold_from_loops {
namespace movers {

///@brief Adds constraints generated by ConstraintGenerators to a pose
class NubInitioLoopClosureMover : public protocols::moves::Mover {

public:
	NubInitioLoopClosureMover();
	~NubInitioLoopClosureMover() override;

	void
	apply( core::pose::Pose & pose ) override;

	/// @brief centroid_score is provided to the CCDclosure to close the loop.
	core::scoring::ScoreFunctionOP centroid_scorefxn() const;
	void centroid_scorefxn( core::scoring::ScoreFunctionOP const & scorefxn );
	/// @brief fullatom_score is used to pack sidechains after the closure
	core::scoring::ScoreFunctionOP fullatom_scorefxn() const;
	void fullatom_scorefxn( core::scoring::ScoreFunctionOP const & scorefxn );
	/// @brief break_side defines how many residues in each side of the chainbreak
	/// are allowed to move (as long as they are not !FLEXIBLE+MOTIF labeled residues)
	core::Size break_side() const;
	void break_side( core::Real value );
	/// @brief when set to true, in each unsuccesfull closure try, it allows one more
	/// residue to move
	bool break_side_ramp() const;
	void break_side_ramp( bool pick );
	/// @brief set how many times we try to close the loops before accepting we cannot
	void max_trials( core::Size choice );
	core::Size max_trials() const;
	/// @brief id of the fragments loaded into the DataMap
	std::string fragments_id() const;
	void fragments_id( std::string const & name );
	/// @brief fragments
	core::fragment::FragSetOP fragments() const;
	void fragments( core::fragment::FragSetOP frags);
	/// @brief if true, trust the residue variants to locate chain breaks; otherwise use the FoldTree
	bool trust() const;
	void trust( bool pick );
	/// @brief if true, label the resiudes that have moved or been repacked during the Mover.
	bool label() const;
	void label( bool pick );
	/// @brief if true, assume input is centroid, and so should be the output.
	bool centroid() const;
	void centroid( bool pick );
	/// @brief if true, a part from repacking, also redesign the residues of the loop and around
	/// following FFL rules.
	bool design() const;
	void design( bool pick );


	/// @brief parse XML tag (to use this Mover in Rosetta Scripts)
	void
	parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data,
		core::pose::Pose const & reference_pose ) override;

	/// @brief required in the context of the parser/scripting scheme
	protocols::moves::MoverOP
	fresh_instance() const override;

	/// @brief required in the context of the parser/scripting scheme
	protocols::moves::MoverOP clone() const override;
	std::string get_name() const override;
	static std::string mover_name();
	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

private:
	/// @brief Repack sidechains without alter bb
	void repack( core::pose::Pose & pose );
	/// @brief Evaluate if the supossed cutpoint has already been fixed.
	/// It is assumed that one previously check if the residue is cutpoint.
	bool is_cutpoint_close( core::pose::Pose const & pose, core::Size const & residue, bool by_variant=false ) const;
	/// @brief Generates the expected final FoldTree that the Pose should have
	core::kinematics::FoldTree make_final_tree( core::pose::Pose const & pose ) const;
	/// @brief Generates the movemap for loop_closure
	core::kinematics::MoveMapOP loop_closure_movemap( core::pose::Pose const & pose );
	/// @brief Selector to pick residues labeled TEMPLATE or FLEXIBLE; the only ones
	/// that can be allowed to move.
	core::select::residue_selector::ResidueSelectorOP movable_residues() const;
	/// @brief This mover works only when there are residues labeled TEMPLATE of FLEXIBLE.
	/// If there are no residues labeled like so, CCD closure will be unable to close and
	/// will end up reporting a failure.
	bool has_movable_residues( core::pose::Pose const & pose ) const;
	/// @brief Selector to pick residues labeled TEMPLATE, FLEXIBLE or COLDSPOT.
	// This residues are allowed to move their sidechains
	core::select::residue_selector::ResidueSelectorOP packable_residues() const;
	/// @brief Selector to pick residues labeled TEMPLATE or FLEXIBLE; that are in the
	/// appropiate sequence distance of the cutpoints.
	core::select::residue_selector::ResidueSelectorOP active_movable_residues( core::pose::Pose const & pose );
	/// @brief Selector to pick residues labeled TEMPLATE, FLEXIBLE or COLDSPOT; that are part of the moved loop
	// or in an appropiate cartesian distance of them.
	core::select::residue_selector::ResidueSelectorOP active_packable_residues() const;
	/// @brief Check if there are cutpoints in the Pose
	bool has_cutpoints( core::pose::Pose const & pose ) const;
	/// @brief Checks if the residues still considered cutpoints have bee actually
	/// closed or not
	void apply_closure_trust( core::pose::Pose & pose, bool by_variant=false ) const;
	/// @brief Show process
	void show_cutpoints( core::pose::Pose const & pose ) const;
	/// @brief Re-check the residue variant according to the FoldTree
	void make_cutpoints_coherent_to_foldtree( core::pose::Pose & pose ) const;
	/// @brief Re-check FoldTree according to residue variant
	void make_foldtree_coherent_to_cutpoints( core::pose::Pose & pose ) const;

	static core::scoring::ScoreFunctionOP default_centroid_scorefxn();
	static core::Real default_centroid_chainclosure_weight();
	static core::scoring::ScoreFunctionOP set_centroid_default_chainclosure_weight(core::scoring::ScoreFunctionOP const & scorefxn );
	static core::scoring::ScoreFunctionOP default_fullatom_scorefxn();
	static core::Size default_break_side();
	static bool default_break_side_ramp();
	static core::Size default_max_trials();
	static bool default_trust();
	static bool default_label();
	static bool default_centroid();
	static bool default_design();

private:
	core::scoring::ScoreFunctionOP centroid_scorefxn_;
	core::scoring::ScoreFunctionOP fullatom_scorefxn_;
	core::Size break_side_;
	core::Size break_side_ramp_value_;
	bool break_side_ramp_;
	core::Real max_trials_;
	std::string fragments_id_;
	core::fragment::FragSetOP fragments_;
	core::select::residue_selector::ReturnResidueSubsetSelector altered_residues_;
	bool trust_;
	bool label_;
	bool centroid_;
	bool design_;

};

}
} //protocols
} //fold_from_loops

#endif //INCLUDED_fold_from_loops_NubInitioLoopClosureMover_hh
