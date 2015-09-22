// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/devel/denovo_design/filters/SSShapeComplementarityFilter.cc
/// @brief Tom's Denovo design protocol
/// @details
/// @author Tom Linsky (tlinsky@gmail.com)
/// @author Vikram K. Mulligan (vmullig@uw.edu -- added threshhold option)

// Unit headers
#include <devel/denovo_design/filters/SSShapeComplementarityFilter.hh>
#include <devel/denovo_design/filters/SSShapeComplementarityFilterCreator.hh>

// Protocol Headers

// Project Headers
#include <core/conformation/Residue.hh>
#include <core/pose/Pose.hh>
#include <core/scoring/dssp/Dssp.hh>
#include <core/scoring/sc/ShapeComplementarityCalculator.hh>
#include <protocols/fldsgn/topology/HelixPairing.hh>
#include <protocols/fldsgn/topology/HSSTriplet.hh>
#include <protocols/fldsgn/topology/SS_Info2.hh>
#include <protocols/jd2/parser/BluePrint.hh>
#include <protocols/toolbox/SelectResiduesByLayer.hh>

// Basic Headers
#include <basic/Tracer.hh>

// Utility Headers
#include <utility/tag/Tag.hh>

// ObjexxFCL Headers

//C++ Headers


#ifdef GL_GRAPHICS
#include <protocols/viewer/viewers.hh>
#endif

#if defined(WIN32) || defined(__CYGWIN__)
#include <ctime>
#endif

#ifdef BOINC_GRAPHICS
#include <protocols/boinc/boinc.hh>
#endif


static THREAD_LOCAL basic::Tracer TR( "devel.denovo_design.SSShapeComplementarityFilter" );

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace devel {
namespace denovo_design {
namespace filters {

std::string
SSShapeComplementarityFilterCreator::keyname() const
{
	return SSShapeComplementarityFilterCreator::filter_name();
}

protocols::filters::FilterOP
SSShapeComplementarityFilterCreator::create_filter() const {
	return protocols::filters::FilterOP( new SSShapeComplementarityFilter() );
}

std::string
SSShapeComplementarityFilterCreator::filter_name()
{
	return "SSShapeComplementarity";
}

///  ---------------------------------------------------------------------------------
///  SSShapeComplementarityFilter main code:
///  ---------------------------------------------------------------------------------
SSShapeComplementarityFilter::SSShapeComplementarityFilter() :
	Filter( "SSShapeComplementarityFilter" ),
	verbose_( false ),
	calc_loops_( true ),
	calc_helices_( true ),
	rejection_thresh_(0),
	blueprint_( /* NULL */ ),
	scc_( core::scoring::sc::ShapeComplementarityCalculatorOP( new core::scoring::sc::ShapeComplementarityCalculator() ) )
{
}

SSShapeComplementarityFilter::SSShapeComplementarityFilter( SSShapeComplementarityFilter const & rval ) :
	Filter( rval ),
	verbose_( rval.verbose_ ),
	calc_loops_( rval.calc_loops_ ),
	calc_helices_( rval.calc_helices_ ),
	rejection_thresh_(rval.rejection_thresh_),
	blueprint_( rval.blueprint_ ),
	scc_( rval.scc_ )
{
}

/// @brief destructor - this class has no dynamic allocation, so
//// nothing needs to be cleaned. C++ will take care of that for us.
SSShapeComplementarityFilter::~SSShapeComplementarityFilter()
{}


/// Return a copy of ourselves
protocols::filters::FilterOP
SSShapeComplementarityFilter::clone() const
{
	return protocols::filters::FilterOP( new SSShapeComplementarityFilter(*this) );
}

protocols::filters::FilterOP
SSShapeComplementarityFilter::fresh_instance() const
{
	return protocols::filters::FilterOP( new SSShapeComplementarityFilter() );
}

void
SSShapeComplementarityFilter::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap &,
	protocols::filters::Filters_map const &,
	protocols::moves::Movers_map const &,
	core::pose::Pose const & )
{
	std::string const bp_filename( tag->getOption< std::string >( "blueprint", "" ) );
	if ( bp_filename != "" ) {
		blueprint_ = protocols::jd2::parser::BluePrintCOP( protocols::jd2::parser::BluePrintOP( new protocols::jd2::parser::BluePrint( bp_filename ) ) );
		if ( ! blueprint_ ) {
			utility_exit_with_message( "Error reading blueprint file." );
		}
	}

	verbose_ = tag->getOption< bool >( "verbose", verbose_ );
	calc_loops_ = tag->getOption< bool >( "loops", calc_loops_ );
	calc_helices_ = tag->getOption< bool >( "helices", calc_helices_ );
	set_rejection_thresh( tag->getOption< core::Real >("min_sc", 0.0) );
}

std::string
SSShapeComplementarityFilter::get_name() const
{
	return "SSShapeComplementarity";
}

void
SSShapeComplementarityFilter::report( std::ostream & out, core::pose::Pose const & pose ) const
{
	out << "SS Shape complementarity is:" << compute( pose ) << std::endl;
}

core::Real
SSShapeComplementarityFilter::report_sm( core::pose::Pose const & pose ) const
{
	return compute( pose );
}

core::Real
SSShapeComplementarityFilter::compute( core::pose::Pose const & pose ) const
{
	// if scc_ is not set, something horrible has happened
	runtime_assert( scc_ != 0 );
	if ( !scc_->Init() ) {
		TR.Error << "Failed to initialize ShapeComplementarityCalculator!" << std::endl;
		return -1;
	}
	scc_->Reset();

	protocols::fldsgn::topology::SS_Info2_OP ss_info;
	if ( blueprint_ ) {
		ss_info = protocols::fldsgn::topology::SS_Info2_OP( new protocols::fldsgn::topology::SS_Info2( pose, blueprint_->secstruct() ) );
	} else {
		core::scoring::dssp::Dssp dssp( pose );
		std::string const & dssp_ss( dssp.get_dssp_secstruct() );

		// only count protein residues
		std::string secstruct( "" );
		for ( core::Size i=1; i<=dssp_ss.size(); ++i ) {
			if ( pose.residue( i ).is_protein() ) {
				secstruct += dssp_ss[i-1];
			}
		}

		// AMW: cppcheck noticed that secstruct wasn't being used. I think it should be used here!
		// Note that this would be compatible with the SS_Info2 constructor's interpretation of the dssp string
		// i.e. that it should skip non protein residues, and only be THAT long.
		//ss_info = protocols::fldsgn::topology::SS_Info2_OP( new protocols::fldsgn::topology::SS_Info2( pose, dssp.get_dssp_secstruct() ) );
		ss_info = protocols::fldsgn::topology::SS_Info2_OP( new protocols::fldsgn::topology::SS_Info2( pose, secstruct ) );
	}

	// we will average out the shape complementarity from HSS triplets and Helix-Helix pairings
	core::Real sum( 0.0 );
	core::Real total_area( 0.0 );
	core::Size count( 0 );
	if ( calc_helices_ ) {
		for ( core::Size i=1; i<=ss_info->helices().size(); ++i ) {
			// only do this if the helix size >= 5 residues
			if ( ( ss_info->helix( i )->end() - ss_info->helix( i )->begin() ) >= 4 ) {
				setup_sc( pose, ss_info->helix( i ) );
				core::scoring::sc::RESULTS const & r( get_sc_and_area() );
				core::Size num_res( ss_info->helix( i )->end() - ss_info->helix( i )->begin() + 1 );
				if ( r.valid == 1 ) {
					sum += (r.sc * r.area);
					total_area += r.area;
					++count;
					TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << "; num_res=" << num_res << std::endl;
				}
				// reset SCC for reuse
				scc_->Reset();
			}
		}
	}
	if ( calc_loops_ ) {
		for ( core::Size i=1; i<=ss_info->loops().size(); ++i ) {
			setup_sc( pose, ss_info->loop( i ) );
			core::scoring::sc::RESULTS const & r( get_sc_and_area() );
			core::Size num_res( ss_info->loop( i )->end() - ss_info->loop( i )->begin() + 1);
			if ( r.valid == 1 ) {
				sum += (r.sc * r.area);
				total_area += r.area;
				++count;
				TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << "; num_res=" << num_res << std::endl;
			}
			scc_->Reset();
		}
	}
	/*std::string helix_string;
	if ( blueprint_ ) {
	helix_string = blueprint_->helix_pairings();
	} else {
	helix_string = detect_helix_pairings( pose, ss_info );
	TR << "Helix_string=" << helix_string << std::endl;
	}
	protocols::fldsgn::topology::HelixPairings helix_pairs( protocols::fldsgn::topology::HelixPairingSet( helix_string ).helix_pairings() );
	for ( core::Size i=1; i<=helix_pairs.size(); ++i ) {
	setup_sc_hh( pose, *ss_info, helix_pairs[i] );

	core::scoring::sc::RESULTS const & r( get_sc_and_area() );
	if ( r.valid == 1 ) {
	sum += r.sc * r.area;
	total_area += r.area;
	++count;
	TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << std::endl;
	}
	// reset SCC for reuse
	scc_->Reset();
	}

	std::string hss_string;
	if ( blueprint_ ) {
	hss_string = blueprint_->hss_triplets();
	} else {
	hss_string = detect_hss_triplets( pose, ss_info );
	TR << "HSS string = " << hss_string << std::endl;
	}
	protocols::fldsgn::topology::HSSTriplets hss_triplets( protocols::fldsgn::topology::HSSTripletSet( hss_string ).hss_triplets() );
	for ( core::Size i=1; i<=hss_triplets.size(); ++i ) {
	setup_sc_hss( pose, *ss_info, hss_triplets[i] );

	core::scoring::sc::RESULTS const & r( get_sc_and_area() );

	// this check makes sure there wasn't an error in the filter calculation.
	// if there was an error, the result is simply ignored.
	if ( r.valid == 1 ) {
	sum += r.sc * r.area;
	total_area += r.area;
	++count;
	TR << "SUM=" << sum << "; area=" << r.area << "; sc=" << r.sc << std::endl;
	}
	// reset SCC for reuse
	scc_->Reset();
	}*/

	if ( count == 0 || total_area <= 0.0 ) {
		return 0;
	}
	//TR << "Returning " << sum / total_area << std::endl;
	//return sum / total_area;
	TR << "Returning " << sum / total_area << std::endl;
	return sum / total_area;
}

/// @brief Does the SSShapeComplementarity Filtering
bool
SSShapeComplementarityFilter::apply( core::pose::Pose const & pose ) const
{
	core::Real const returnval( report_sm( pose ) );

	if ( returnval < rejection_thresh() ) {
		if ( TR.visible() ) TR << "SSShapecomplementarity result is less than rejection threshold (" << returnval << " < " << rejection_thresh() << ").  Filter failed." << std::endl;
		return false;
	}

	if ( TR.visible() ) TR << "SSShapecomplementarityFilter passed." << std::endl;
	return true;
}


/// @brief sets up the underlying filter to work based on a helix
void
SSShapeComplementarityFilter::setup_sc( core::pose::Pose const & pose,
	protocols::fldsgn::topology::SS_BaseCOP const ss ) const
{
	utility::vector1< core::Size > set1, set2;
	runtime_assert( ss->begin() <= ss->end() );

	// set1 is the helix
	for ( core::Size i=1; i<=pose.total_residue(); ++i ) {
		// exclude ligands and other weird things
		if ( ! pose.residue( i ).is_protein() ) {
			continue;
		}

		if ( i >= ss->begin() && i <= ss->end() ) {
			set1.push_back( i );
		} else {
			if ( i < ss->begin() - 1 || i > ss->end() + 1 ) {
				set2.push_back( i );
			}
		}
	}

	TR.Debug << "Set residues1 to ";
	for ( core::Size i=1; i<=set1.size(); ++i ) {
		TR.Debug << set1[i] << " ";
		scc_->AddResidue( 0, pose.residue( set1[i] ) );
	}
	TR.Debug << std::endl << "Set residues2 to ";
	for ( core::Size i=1; i<=set2.size(); ++i ) {
		TR.Debug << set2[i] << " ";
		scc_->AddResidue( 1, pose.residue( set2[i] ) );
	}
	TR.Debug << std::endl;
}

/// @brief sets up the underlying shapecomplementarity filter to work based on secondary structure elements
void
SSShapeComplementarityFilter::setup_sc_hss( core::pose::Pose const & pose,
	protocols::fldsgn::topology::SS_Info2 const & ss_info,
	protocols::fldsgn::topology::HSSTripletCOP hss_triplet ) const
{
	utility::vector1< core::Size > set1, set2;
	// set 1 is the helix
	protocols::fldsgn::topology::HelixCOP const helix( ss_info.helix( hss_triplet->helix() ) );
	for ( core::Size i=helix->begin(); i<=helix->end(); ++i ) {
		set1.push_back( i );
	}

	protocols::fldsgn::topology::StrandCOP const strand( ss_info.strand( hss_triplet->strand1() ) );
	for ( core::Size i=strand->begin(); i<=strand->end(); ++i ) {
		set2.push_back( i );
	}
	protocols::fldsgn::topology::StrandCOP const strand2( ss_info.strand( hss_triplet->strand2() ) );
	for ( core::Size i=strand2->begin(); i<=strand2->end(); ++i ) {
		set2.push_back( i );
	}

	TR.Debug << "Set residues1 to ";
	for ( core::Size i=1; i<=set1.size(); ++i ) {
		TR.Debug << set1[i] << " ";
		scc_->AddResidue( 0, pose.residue( set1[i] ) );
	}
	TR.Debug << std::endl << "Set residues2 to ";
	for ( core::Size i=1; i<=set2.size(); ++i ) {
		TR.Debug << set2[i] << " ";
		scc_->AddResidue( 1, pose.residue( set2[i] ) );
	}
	TR.Debug << std::endl;
}

/// @brief sets up the underlying shapecomplementarity filter to work based on secondary structure elements
void
SSShapeComplementarityFilter::setup_sc_hh( core::pose::Pose const & pose,
	protocols::fldsgn::topology::SS_Info2 const & ss_info,
	protocols::fldsgn::topology::HelixPairingCOP helix_pair ) const
{
	utility::vector1< core::Size > set1, set2;
	// set 1 is helix1, set2 is helix2
	protocols::fldsgn::topology::HelixCOP const helix( ss_info.helix( helix_pair->h1() ) );
	for ( core::Size i=helix->begin(); i<=helix->end(); ++i ) {
		set1.push_back( i );
	}

	protocols::fldsgn::topology::HelixCOP const helix2( ss_info.helix( helix_pair->h2() ) );
	for ( core::Size i=helix2->begin(); i<=helix2->end(); ++i ) {
		set2.push_back( i );
	}

	TR.Debug << "Set residues1 to ";
	for ( core::Size i=1; i<=set1.size(); ++i ) {
		TR.Debug << set1[i] << " ";
		scc_->AddResidue( 0, pose.residue( set1[i] ) );
	}
	TR.Debug << std::endl << "Set residues2 to ";
	for ( core::Size i=1; i<=set2.size(); ++i ) {
		TR.Debug << set2[i] << " ";
		scc_->AddResidue( 1, pose.residue( set2[i] ) );
	}
	TR.Debug << std::endl;
}

/// @brief Runs the SC calculator to obtain an SC score and an interaction area. Returns a result in the format core::scoring::sc::RESULTS.  Assumes the SC calculator has been initialized and has the correct residues added.
core::scoring::sc::RESULTS const &
SSShapeComplementarityFilter::get_sc_and_area() const
{
	if ( ! scc_->Calc() ) {
		TR.Error << "Error in SC calculation!" << std::endl;
		return scc_->GetResults();
	}

	core::scoring::sc::RESULTS const & r = scc_->GetResults();

	if ( verbose_ ) {
		TR << "==================================================" << std::endl;
		TR << std::endl;
		for ( int i = 0; i <= 2; i++ ) {
			if ( i < 2 ) {
				TR << "Molecule " << (i+1) << ":" << std::endl;
			} else {
				TR << "Total/Average for both molecules:" << std::endl;
			}

			TR << "          Total Atoms: " << r.surface[i].nAtoms << std::endl;
			TR << "         Buried Atoms: " << r.surface[i].nBuriedAtoms << std::endl;
			TR << "        Blocked Atoms: " << r.surface[i].nBlockedAtoms << std::endl;
			TR << "           Total Dots: " << r.surface[i].nAllDots << std::endl;
			TR << " Trimmed Surface Dots: " << r.surface[i].nTrimmedDots << std::endl;
			TR << "         Trimmed Area: " << r.surface[i].trimmedArea << " (avg) " << std::endl;
			TR << std::endl;
		}
		TR << std::endl;

		for ( int i = 0; i <= 2; i++ ) {
			if ( i < 2 ) {
				TR << "Molecule " << (i+1) << "->" << ((i+1)%2+1) << ": " << std::endl;
			} else {
				TR << "Average for both molecules:" << std::endl;
			}
			TR << "      Mean Separation: " << r.surface[i].d_mean << std::endl;
			TR << "    Median Separation: " << r.surface[i].d_median << std::endl;
			TR << "    Mean Shape Compl.: " << r.surface[i].s_mean << std::endl;
			TR << "  Median Shape Compl.: " << r.surface[i].s_median << std::endl;
			TR << std::endl;
		}
	}  //verbose output
	return r;
}

} // namespace filters
} // namespace denovo_design
} // namespace devel


