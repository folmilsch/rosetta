// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/cyclic_peptide/PeptideStubMover.hh
/// @brief Add constraints to the current pose conformation.
/// @author Yifan Song
/// @modified Vikram K. Mulligan (vmulligan@flatironinstitute.org): Added support for stripping
/// N-acetylation and C-methylamidation when appending residues, preserving phi and the previous
/// omega in the first case and psi and the following omega in the second.

#ifndef INCLUDED_protocols_cyclic_peptide_PeptideStubMover_hh
#define INCLUDED_protocols_cyclic_peptide_PeptideStubMover_hh

#include <protocols/moves/Mover.hh>
#include <protocols/cyclic_peptide/PeptideStubMover.fwd.hh>

namespace protocols {
namespace cyclic_peptide {

enum PSM_StubMode {
	PSM_append,
	PSM_prepend,
	PSM_insert
};

class PeptideStubMover : public moves::Mover {

public:
	PeptideStubMover();
	~PeptideStubMover() override;
	PeptideStubMover( PeptideStubMover const &src );

	void init();

	void apply( Pose & ) override;

	moves::MoverOP clone() const override;
	moves::MoverOP fresh_instance() const override;

	void
	parse_my_tag( utility::tag::TagCOP, basic::datacache::DataMap &, Pose const & ) override;


	/// @brief Reset mover data
	void reset_mover_data()
	{
		stub_rsd_names_.clear();
		stub_rsd_jumping_.clear();
		stub_rsd_connecting_atom_.clear();
		stub_anchor_rsd_.clear();
		stub_anchor_rsd_connecting_atom_.clear();
		return;
	}


	/// @brief Sets whether the pose gets reset (i.e. all residues deleted) or not.
	void set_reset_mode( bool reset_mode )
	{
		reset_ = reset_mode;
		return;
	}


	/// @brief Sets whether pdb numbering gets updated or not.
	void set_update_pdb_numbering_mode( bool mode )
	{
		update_pdb_numbering_ = mode;
		return;
	}


	/// @brief Adds a residue to the list of residues to be appended, prepended, or inserted.
	/// @details Calls add_residue() override that uses PSM_StubMode.
	void add_residue(
		std::string const &stubmode,
		std::string const &resname,
		core::Size const position,
		bool const jumpmode,
		std::string const &connecting_atom,
		core::Size const repeat,
		core::Size const anchor_rsd,
		std::string const &anchor_atom
	);

	/// @brief Adds a residue to the list of residues to be appended, prepended, or inserted.
	/// @details This version uses enums.
	/// @author Vikram K. Mulligan (vmullig@uw.edu).
	void add_residue(
		PSM_StubMode const stubmode,
		std::string const &resname,
		core::Size const position,
		bool const jumpmode,
		std::string const &connecting_atom,
		core::Size const repeat,
		core::Size const anchor_rsd,
		std::string const &anchor_atom
	);

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

private: //Functions

	/// @brief Remove terminal types from the upper terminus.  Store the old psi and omega values.
	/// @returns  Returns void, but if a terminal type was removed, old_psi will be set to the previous
	/// psi value and old_omega will be set to the previous omega value.  The replace_upper_terminal_type var
	/// is set to true if a terminal type was replaced and false otherwise.
	/// @author Vikram K. Mulligan (vmulligan@flatironinstitute.org).
	void
	handle_upper_terminus(
		core::pose::Pose & pose,
		core::Size const anchor_rsd,
		core::Real & old_psi,
		core::Real & old_omega,
		bool & replace_upper_terminal_type
	) const;

	/// @brief Remove terminal types from the lower terminus.  Store the old phi and omega_nminus1 values.
	/// @returns  Returns void, but if a terminal type was removed, old_phi will be set to the previous
	/// phi value and old_omega_nminus1 will be set to the previous upstream omega value.  The
	/// replace_lower_terminal_type var is set to true if a terminal type was replaced and false otherwise.
	/// @author Vikram K. Mulligan (vmulligan@flatironinstitute.org).
	void
	handle_lower_terminus(
		core::pose::Pose & pose,
		core::Size const anchor_rsd,
		core::Real & old_phi,
		core::Real & old_omega_nminus1,
		bool & replace_lower_terminal_type
	) const;

	/// @brief Update the omega-1 and phi (if we've replaced an N-acetylation) or the psi and omega
	/// (if we've replaced a C-methylamidation) to preserve these dihedral values.
	/// @details Builds a temporary foldtree rooted on the alpha carbon of the anchor residue.
	/// @author Vikram K. Mulligan (vmulligan@flatironinstitute.org).
	void
	preserve_old_mainchain_torsions(
		core::pose::Pose & pose,
		core::Size const anchor_res,
		core::Real const old_omega_minus1,
		core::Real const old_phi,
		core::Real const old_psi,
		core::Real const old_omega,
		bool const replace_upper_terminal_type,
		bool const replace_lower_terminal_type
	) const;

private:
	bool reset_;


	/// @brief As residues are added, should the PDB numbering be updated?  Default true.
	bool update_pdb_numbering_;

	utility::vector1<PSM_StubMode> stub_mode_;
	utility::vector1<std::string> stub_rsd_names_;
	utility::vector1<bool> stub_rsd_jumping_;
	utility::vector1<std::string> stub_rsd_connecting_atom_;
	utility::vector1<core::Size> stub_rsd_repeat_;
	utility::vector1<core::Size> stub_insert_pos_;
	utility::vector1<core::Size> stub_anchor_rsd_;
	utility::vector1<std::string> stub_anchor_rsd_connecting_atom_;

	//Private functions:


	/// @brief Rebuilds all atoms that are dependent on bonds between residue_index and any other residues (including atoms on the other residues).
	virtual void rebuild_atoms( core::pose::Pose &pose, core::Size const residue_index) const;


	/// @brief Updates the PDB numbering (PDB number/chain ID) as residues are added.
	virtual void update_pdb_numbering ( core::pose::Pose &pose ) const;

};

} // moves
} // protocols

#endif
