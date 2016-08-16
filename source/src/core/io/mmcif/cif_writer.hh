// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/io/mmcif/util.hh
/// @brief Functions for MMCIF writing.
/// @author Andy Watkins (andy.watkins2@gmail.com)
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)


#ifndef INCLUDED_core_io_mmcif_cif_writer_HH
#define INCLUDED_core_io_mmcif_cif_writer_HH

// Package headers
#include <core/io/StructFileRep.hh>
#include <core/io/StructFileReaderOptions.fwd.hh>

// Project header
#include <core/pose/Pose.hh>


namespace core {
namespace io {
namespace mmcif {

void dump_cif( core::pose::Pose const & pose, std::string const & cif_file );

void dump_cif( std::string const & cif_file, StructFileRepOP sfr, StructFileReaderOptions const & options );

}  // namespace mmcif
}  // namespace io
}  // namespace core

#endif  // INCLUDED_core_io_mmcif_cif_writer_HH
