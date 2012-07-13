// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// This file is part of the Rosetta software suite and is made available under license.
// The Rosetta software is developed by the contributing members of the Rosetta Commons consortium.
// Copyright in the Rosetta software belongs to the developers and their institutions.
// For more information, see www.rosettacommons.org.

/// @file ./src/protocols/fldsgn/SheetFoldTypeManager.hh
/// @brief
/// @author Nobuyasu Koga ( nobuyasu@u.washington.edu )

// unit headers

#ifndef INCLUDED_protocols_fldsgn_topology_SheetFoldTypeManager_hh
#define INCLUDED_protocols_fldsgn_topology_SheetFoldTypeManager_hh

// project headers
// AUTO-REMOVED #include <core/types.hh>

// utility headers
// AUTO-REMOVED #include <utility/vector1.hh>

// C++ headers
#include <iostream>
#include <map>
// AUTO-REMOVED #include <string>

#include <utility/vector1_bool.hh>


namespace protocols {
namespace fldsgn {
namespace topology {

/// @brief List of topologies determined by strand pairings
enum SheetFoldType {

	// 2strands
	BABx1 = 1,  // beta-aplpha-beta motif

	/// 3strands
	//  parallel
	RosI,
	RosO,
	BABx2,  // repeat beta-aplpha-beta motif twice

	// mixture
	Thio,   // part of structure of thioredoxin-fold
	BFr,
	EFr,

	// anti-parallel
	CFr,
	DFr,

	/// 4strands
	// parallel
	Rsmn2x2, /// rossmann2x2
	Ploop2x2, /// ploop2x2
	Rsmn3x3_Half, /// Half structure of Rossmann3x3
	BABx3,  // repeat beta-aplpha-beta motif 3 times

	// mixture
  BAB_HPN_BAB,
	PG_like,
	Thioredoxin,
	//L30E_like,
	BAB_CFr,
	DFr_BAB,
	BEFr,

	// anti-parallel
	Fd_like, /// ferredoxin-like
	RFd_like, /// reverse ferredoxin-like
	CDFr,
	HPN_CFr,
	DFr_HPN,

	/// 5strands
	// parallel
	Flavodoxin,
	Ploop2x3,

	/// mixture
	RNAseH,

	// anti-parallel
	Top7,

	/// 6 strands
	Rsmn3x3,
	Ploop3x3,

	///
	UNFOLD,

	///
	NO_STRANDS,

	///
	UNKNOWN,

	n_fold_types = UNKNOWN

};

////////////////////////////////////////////////////////////////////////////////////////////////////////////
class SheetFoldTypeManager {
public:


  typedef std::string String;


public:

	SheetFoldTypeManager();


public:

	SheetFoldType
	foldtype_from_name( std::string const & name );


	std::string
	name_from_foldtype( SheetFoldType score_type );


	bool
	is_foldtype( std::string const & name );


	SheetFoldType
	foldtype_from_spairs( std::string const & spairs );


	std::string
	spairs_from_foldtype( SheetFoldType foldtype );


	bool
	is_sparis_foldtype( std::string const & spairs );

private:

	/// @brief
	void initialize();


	/// @brief initialize the SheetFoldType name vector and map
	void setup_foldtype_names();


/// @brief initialize the map of strand pairings and SheetFoldType
	void setup_foldtype_strand_pairings();


private:

	bool initialized_;

	std::map< String, SheetFoldType > name2foldtype_;
	utility::vector1< String > foldtype2name_;

	std::map< String, SheetFoldType > spairs2foldtype_;
	utility::vector1< String > foldtype2spairs_;

};


} // namespace topology
} // namespace fldsgn
} // namespace protocols


#endif
