// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file src/core/scoring/constraints/util.cc
/// @brief utility functions for defining constraints. Maybe better placed in src/numeric?
/// @author James Thompson

#include <core/types.hh>

#include <core/pose/Pose.hh>
#include <basic/options/option.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/constraints/util.hh>
#include <core/scoring/constraints/CoordinateConstraint.hh>
#include <core/scoring/constraints/Constraint.hh>
#include <core/scoring/constraints/ConstraintIO.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/AmbiguousConstraint.hh>
#include <core/scoring/constraints/AmbiguousNMRConstraint.hh>

#include <core/conformation/Residue.hh>

#include <numeric/random/random_permutation.hh>
#include <numeric/random/random.hh>
#include <basic/Tracer.hh>

#include <ObjexxFCL/FArray2D.hh>

// option key includes
#include <basic/options/keys/constraints.OptionKeys.gen.hh>

//Auto Headers
#include <cmath>

namespace core {
namespace scoring {
namespace constraints {

static numeric::random::RandomGenerator RG(42015512); // <- Magic number, do not change


static basic::Tracer tr("core.scoring.constraints");

/// @brief Returns the weighted value of a normal distribution evaluated
///  with the given mean, sd, and x values. Returns zero if the weight is
///  less than 1e-10.
Real logdgaussian( Real x, Real mean, Real sd, Real weight )
{
	using namespace std;
	Real r = x - mean;
	static Real sqrt_2pi = 2.50662721600161;

	Real answer = log(weight / (sd * sqrt_2pi)) - ( r * r / (2*sd*sd) );
	return answer;
}

/// @brief Returns the weighted value of a normal distribution evaluated
///  with the given mean, sd, and x values. Returns zero if the weight is
///  less than 1e-10.
Real logdgaussian_deriv( Real x, Real mean, Real sd, Real )
{
	using namespace std;
	Real r = x - mean;
	Real answer =  - (r / (sd*sd) );
	return answer;
}

/// @brief Returns the weighted value of a normal distribution evaluated
///  with the given mean, sd, and x values. Returns zero if the weight is
///  less than 1e-10.
Real dgaussian( Real x, Real mean, Real sd, Real weight ) {
	if ( weight < 1e-10 ) {
		return 0;
	}

	using namespace std;
	Real r = x - mean;
	static Real sqrt_2pi = 2.50662721600161;
	Real answer = weight * (1 / (sd * sqrt_2pi)) * exp( -1 * r * r / (2*sd*sd) );
	return answer;
}

/// @brief Returns the weighted derivative of a normal distribution evaluated
/// with the given mean, sd, and x values. Returns zero if the weight is less
/// than 1e-10.
Real gaussian_deriv( Real x, Real mean, Real sd, Real weight ) {
	assert( weight >= 0.0 && weight <= 1.0 );

	if ( weight < 1e-10 ) {
		return 0;
	}

	using namespace std;
	Real r = abs( x - mean );
	Real answer = dgaussian( x, mean, sd , weight ) * ( 1 / ( sd * sd ) ) * r;
	return answer;
}

/// @brief Returns the weighted value of an exponential distribution
/// evaluated with the given anchor, rate, and x values. Returns zero if the
/// weight is less than 1e-10.
Real dexponential( Real x, Real anchor, Real rate, Real weight ) {
	assert( weight >= 0.0 && weight <= 1.0 );
	if ( weight < 1e-10 ) {
		return 0;
	}

	using namespace std;
	Real r = abs( x - anchor );
	Real answer = weight * rate * exp( -1 * rate * r );
	return answer;
}

/// @brief Returns the weighted derivative of a log-exponential distribution
/// evaluated with the given anchor, rate, and x values. Returns zero if the
/// weight is less than 1e-10.
Real exponential_deriv( Real x, Real anchor, Real rate, Real weight ) {
	if ( weight < 1e-10 ) {
		return 0;
	}

	using namespace std;
	Real r = abs( x - anchor );
	return weight  * rate * rate * exp( -1 * rate * r );
}

Real linear_interpolate(
	Real const x_val,
	Real const x1,
	Real const x2,
	Real const y1,
	Real const y2
) {

	if ( x_val == x1 ) return y1;
	if ( x_val == x2 ) return y2;

	// calculate slope
	Real slope = ( y2 - y1 ) / ( x2 - x1 );
	// walk along line
	return (x_val - x1) * slope;
}

void
cull_violators(
	ConstraintCOPs const& target_list,
	ConstraintCOPs &culled_list,
	pose::Pose const& filter_pose,
	core::Real threshold
) {
	culled_list.clear();
	for ( ConstraintCOPs::const_iterator it = target_list.begin(),
					eit = target_list.end(); it != eit; ++it ) {
		if ( (*it)->show_violations( tr.Debug, filter_pose, 1, threshold ) == 0 ) {
			culled_list.push_back( *it );
		}
	}
}


////////// Centroid constraints
std::string get_cst_file_option(){
	using namespace basic::options;
	utility::vector1< std::string>  cst_files = option[ OptionKeys::constraints::cst_file ]();
	core::Size choice=1;
	if ( cst_files.size() > 1 ) choice=core::Size( RG.random_range( 1,cst_files.size() ) );
	tr.Info << "Constraint choice: " << cst_files[choice] << std::endl;
	return cst_files[choice];
}

//// @brief	add constraints if specified by user.
void add_constraints_from_cmdline_to_pose( core::pose::Pose & pose ) {
	using namespace basic::options;
	using namespace core::scoring::constraints;
	if ( option[ OptionKeys::constraints::cst_file ].user() ) {
		ConstraintSetOP cstset_ = ConstraintIO::get_instance()->read_constraints( get_cst_file_option() ,new ConstraintSet, pose	);
		pose.constraint_set( cstset_ );
	}
}

//// @brief	add constraints if specified by user.
void add_constraints_from_cmdline_to_scorefxn( core::scoring::ScoreFunction &scorefxn_  ) {
	using namespace basic::options;
	if ( option[ OptionKeys::constraints::cst_weight ].user() ) {
		scorefxn_.set_weight(	atom_pair_constraint,  option[ OptionKeys::constraints::cst_weight ]() );
		scorefxn_.set_weight(	angle_constraint,      option[ OptionKeys::constraints::cst_weight ]() );
		scorefxn_.set_weight(	dihedral_constraint,   option[ OptionKeys::constraints::cst_weight ]() );
		scorefxn_.set_weight( coordinate_constraint, option[ OptionKeys::constraints::cst_weight ]() );
	}
}

//// @brief 	add constraints if specified by user.
void add_constraints_from_cmdline( core::pose::Pose & pose, core::scoring::ScoreFunction &scorefxn_  ) {
	add_constraints_from_cmdline_to_pose( pose );
	add_constraints_from_cmdline_to_scorefxn( scorefxn_ );
}

////////// FA constraints
std::string get_cst_fa_file_option() {
	using namespace basic::options;
	utility::vector1< std::string> cst_files
		= option[ OptionKeys::constraints::cst_fa_file ]();
	core::Size choice=1;
	if ( cst_files.size() > 1 ) choice=core::Size( RG.random_range( 2,cst_files.size() ) );
	tr.Info << "Constraint choice: " << cst_files[choice] << std::endl;
	return cst_files[choice];
}

/// @brief add constraints if specified by user using the
/// -constraints::cst_fa_file flag. Setting appropriate weights for
/// ScoreFunction is done elsewhere.
void add_fa_constraints_from_cmdline_to_pose( core::pose::Pose & pose ) {
	using namespace basic::options;
	using namespace core::scoring::constraints;
	if ( option[ OptionKeys::constraints::cst_fa_file ].user() ) {
		ConstraintSetOP cstset_ = ConstraintIO::get_instance()->read_constraints(
			get_cst_fa_file_option(), new ConstraintSet, pose
		);
		pose.constraint_set( cstset_ );
	}
}

/// @brief	add constraints if specified by user.
void add_fa_constraints_from_cmdline_to_scorefxn( core::scoring::ScoreFunction & scorefxn_  ) {
	using namespace basic::options;
	if ( option[ OptionKeys::constraints::cst_fa_weight ].user() ) {
		scorefxn_.set_weight(	atom_pair_constraint,  option[ OptionKeys::constraints::cst_fa_weight ]() );
		scorefxn_.set_weight(	angle_constraint,	     option[ OptionKeys::constraints::cst_fa_weight ]()	);
		scorefxn_.set_weight(	dihedral_constraint,   option[ OptionKeys::constraints::cst_fa_weight ]()	);
		scorefxn_.set_weight( coordinate_constraint, option[ OptionKeys::constraints::cst_fa_weight ]() );
	}
}


//// @brief	add constraints if specified by user.
void add_fa_constraints_from_cmdline(
	core::pose::Pose & pose,
	core::scoring::ScoreFunction & scorefxn_
) {
	add_fa_constraints_from_cmdline_to_pose( pose );
	add_fa_constraints_from_cmdline_to_scorefxn( scorefxn_ );
}

///////////////////////////////////////////////////////////////////////////////////////////////
void
add_coordinate_constraints( pose::Pose & pose ) {

	using namespace core::id;
	using namespace core::conformation;
	using namespace core::scoring::constraints;

	Real const coord_sdev( 10.0 );
	Size const my_anchor( 1 ); //anchor atom on first residue?

	ConstraintSetOP cst_set = pose.constraint_set()->clone();

	Size const nres( pose.total_residue() );
	for ( Size i=1; i<= nres;  ++i ) {

		Residue const & i_rsd( pose.residue(i) );

		for ( Size ii = 1; ii<= i_rsd.nheavyatoms(); ++ii ) {

			cst_set->add_constraint( new CoordinateConstraint( AtomID(ii,i), AtomID(1,my_anchor), i_rsd.xyz(ii),
																												 new HarmonicFunc( 0.0, coord_sdev ) ) );
		}
	}

	pose.constraint_set( cst_set );

}

bool combinable( Constraint const& cst, utility::vector1< Size > exclude_res ) {
	if ( exclude_res.size() == 0 ) return true;
	for ( Size i=1; i<= cst.natoms(); ++i ) {
		Size const seqpos( cst.atom(i).rsd() );
		// seqpos already in list?
		runtime_assert( seqpos <= exclude_res.size() );
		if ( !exclude_res[ seqpos ] ) {
			return true;
		}
	}
	return false;
}

///@brief combine constraints randomly into AmbiguousConstraints N -> 1 this greatly decreases the odds to have a wrong constraint
void choose_effective_sequence_separation( core::kinematics::ShortestPathInFoldTree const& sp, ConstraintCOPs& in ) {
	for (	utility::vector1< ConstraintCOP >::const_iterator it = in.begin(); it != in.end(); ++it ) {
		Constraint& cst = const_cast< Constraint& >( (**it) );
		cst.choose_effective_sequence_separation( sp, RG );
	}
}


inline core::Size bin_by_seq_separation( core::Size sep ) {
	if ( sep < 5 ) return 1;
	if ( sep < 20 ) return 2;
	if ( sep < 50 ) return 3;
	return 4;
}

///@brief combine constraints randomly into AmbiguousConstraints N -> 1 this greatly decreases the odds to have a wrong constraint
void combine_constraints(
			 ConstraintCOPs& in,
			 core::Size combine_ratio,
			 utility::vector1< bool > exclude_res,
			 core::kinematics::ShortestPathInFoldTree const& sp_in
) {

	tr.Info << " combine constraints " << combine_ratio << " -> 1 " << std::endl;
	using namespace scoring::constraints;
	if ( combine_ratio <= 1 ) return;

	tr.Trace << "figure out sequence-separation bins" << std::endl;
	//first bin constraints by effective sequence separation  --- combine within bins
	typedef std::map< core::Size, ConstraintCOPs > SeqSepMap;
	SeqSepMap seq_sep_map;
	for (	utility::vector1< ConstraintCOP >::const_iterator it = in.begin(); it != in.end(); ++it ) {
		Size seq_bin = bin_by_seq_separation( (*it)->effective_sequence_separation( sp_in ) );
		seq_sep_map[ seq_bin ].push_back( *it );
	}

	tr.Trace << "combine within bins..."<< std::endl;
	// combine constraints within bins
	utility::vector1< ConstraintCOP > out;
	for ( SeqSepMap::iterator bin_it=seq_sep_map.begin(); bin_it != seq_sep_map.end(); ++bin_it ) {
		// random permutation within bin... ensures random combination
		random_permutation( bin_it->second, RG ); //I don't think a single pass of pairwise exchanges is enough to randomize the vector.
		random_permutation( bin_it->second, RG );
		random_permutation( bin_it->second, RG );

		// combine bin
		for (	utility::vector1< ConstraintCOP >::const_iterator it = bin_it->second.begin(); it != bin_it->second.end();
					//DO NOT INCREMENT, already incremented in next loop -- gives segfaults otherwise
		) {
			Size ct( combine_ratio );
			MultiConstraintOP combined_cst = new AmbiguousConstraint;
			for (	; ct > 0 && it != bin_it->second.end(); ++it ) {
				tr.Trace << " add constraint " << ct << std::endl;
				//check if constraint is combinable:
				if ( combinable( **it, exclude_res ) ) {
					combined_cst->add_individual_constraint( *it );
					--ct;
				} else {
					out.push_back( *it ); //keep uncombined constraints around
				}
			}
			//fill up with more constraints if ct is not 0 yet.
			if ( ct > 0 ) {
				tr.Trace << " fill last Ambiguous constraint " << ct << std::endl;
				for (	ConstraintCOPs::const_iterator it2 = bin_it->second.begin(); ct > 0 && it2 != bin_it->second.end(); ++it2 ) {
					if ( combinable( **it2, exclude_res ) ) {
						--ct;
						combined_cst->add_individual_constraint( *it2 );
					}
				}
			} // ct > 0
			combined_cst->choose_effective_sequence_separation( sp_in, RG );
			out.push_back( combined_cst );
		}
	}  // combination within bin
	in = out;
}

//helper function for skip_redundant_cosntraints
void count_constraint( ConstraintCOP cst, bool redundant, ObjexxFCL::FArray2D_int& count_matrix, Size influence_width, Size total_residue ){

	// figure out if it's inter-res, residue_pair, or 3+body
	utility::vector1< int > pos_list;

	// generate list of all involved residues
	for ( Size i=1; i<= cst->natoms(); ++i ) {
		int const seqpos( cst->atom(i).rsd() );
		// seqpos already in list?
		if ( std::find( pos_list.begin(), pos_list.end(), seqpos )== pos_list.end() ) {
			pos_list.push_back( seqpos );
		}
	}

	if ( pos_list.size() != 2 ) {
		tr.Error << "problems understanding constraint in skip_redundant_constraints ... ignore and keep this one" << std::endl;
		for ( utility::vector1< int >::const_iterator it = pos_list.begin(); it != pos_list.end(); ++it ) {
			tr.Debug << " resid: " << *it << std::endl;
		}
		return;
	}
// 	count_matrix( pos_list[1],pos_list[2] ) = std::max( 	count_matrix( pos_list[1],pos_list[2] ), redundant ? 1 : 2 );
// 	count_matrix( pos_list[2],pos_list[1] ) = count_matrix( pos_list[1],pos_list[2] );
	for ( int i=pos_list[1]-influence_width; i<= pos_list[1]+ (int) influence_width ; ++i ) {
		for ( int j=pos_list[2]-influence_width; j<= pos_list[2]+ (int) influence_width ; ++j ) {
			if ( i < 1 || j < 1 || i > (int) total_residue || j > (int) total_residue ) continue;
			count_matrix( i, j ) = std::max( count_matrix( i,j ), redundant ? 1 : 2 );
			count_matrix( j, i ) = count_matrix( i,j );
		}
	}
}

//helper function for skip_redundant_cosntraints
bool keep_constraint( ConstraintCOP cst, bool redundant, ObjexxFCL::FArray2D_int& count_matrix, Size influence_width, Size total_residue ) {

	// figure out if it's inter-res, residue_pair, or 3+body
	utility::vector1< int > pos_list;

	// generate list of all involved residues
	for ( Size i=1; i<= cst->natoms(); ++i ) {
		int const seqpos( cst->atom(i).rsd() );
		// seqpos already in list?
		if ( std::find( pos_list.begin(), pos_list.end(), seqpos )== pos_list.end() ) {
			pos_list.push_back( seqpos );
		}
	}

	if ( pos_list.size() != 2 ) {
		tr.Error << "problems understanding constraint in skip_redundant_constraints ... ignore and keep this one" << std::endl;
		return true;
	}
	bool keep( false );

	if ( ( !redundant && count_matrix( pos_list[1],pos_list[2] ) == 3 ) || count_matrix( pos_list[1],pos_list[2] ) == ( redundant ? 1 : 2 ) ) {
		keep = true;
		for ( int i=pos_list[1]-influence_width; i<=  pos_list[1]+ (int) influence_width ; ++i ) {
			for ( int j=pos_list[2]-influence_width; j<= (int) pos_list[2]+ (int)influence_width ; ++j ) {
				if ( i < 1 || j < 1 || i > (int) total_residue || j > (int) total_residue ) continue;
				count_matrix( i, j ) += 2;
				count_matrix( j, i ) = count_matrix( i,j );
			}
		}
	}
	// count_matrix( pos_list[1],pos_list[2] ) += 2;
	// 		count_matrix( pos_list[2],pos_list[1] ) = count_matrix( pos_list[1],pos_list[2] );
	return keep;
}

void skip_redundant_constraints( ConstraintCOPs& in, Size total_residue, Size influence_width ) {
	if ( influence_width == 0 ) return;
	tr.Info << " skip redundant constraints... starting with " << in.size() << " constraints" << std::endl;
	using namespace scoring::constraints;
	utility::vector1< ConstraintCOP > out;
	ObjexxFCL::FArray2D_int count_matrix( total_residue, total_residue, 0 );
	using namespace numeric::random;
	random_permutation( in, RG ); //I don't think a single pass of pairwise exchanges is enough to randomize the vector.
	random_permutation( in, RG );
	random_permutation( in, RG );

	for (	utility::vector1< ConstraintCOP >::const_iterator it = in.begin(); it != in.end(); ++it ) {
		AmbiguousNMRConstraint const* cst_in_casted;
		cst_in_casted = dynamic_cast< AmbiguousNMRConstraint const* >( (*it).get() );
		if ( cst_in_casted ) {
			tr.Debug << "casted to AmbiguousNMRConstraint: " << std::endl;
			for (	utility::vector1< ConstraintCOP >::const_iterator multi_it = cst_in_casted->member_constraints().begin(); multi_it != cst_in_casted->member_constraints().end(); ++multi_it ) {
				count_constraint( *multi_it, cst_in_casted->member_constraints().size() > 1 , count_matrix, influence_width, total_residue );
			}
		} else {
			count_constraint( *it, false, count_matrix, influence_width, total_residue );
		}
	}

	if ( tr.Trace.visible() ) {
		for ( Size i=1; i<=total_residue; ++i ) {
			for ( Size j=i+1; j<=total_residue; ++j ) {
				tr.Trace << i << " " << j << "   " << count_matrix( i, j ) << std::endl;
			}
		}
	}

	for (	utility::vector1< ConstraintCOP >::const_iterator it = in.begin(); it != in.end(); ++it ) {

		AmbiguousNMRConstraint const* cst_in_casted;
		cst_in_casted = dynamic_cast< AmbiguousNMRConstraint const* >( (*it).get() );
		bool keep( false );
		if ( cst_in_casted ) {
			for (	utility::vector1< ConstraintCOP >::const_iterator multi_it = cst_in_casted->member_constraints().begin(); multi_it != cst_in_casted->member_constraints().end(); ++multi_it ) {
				keep |= keep_constraint( *multi_it,  cst_in_casted->member_constraints().size() > 1, count_matrix, influence_width, total_residue );
			}
		} else {
			keep = keep_constraint( *it, false, count_matrix, influence_width, total_residue );
		}

		if ( keep ) {
			out.push_back( *it );
		}
	}

	in = out;

	if ( tr.Trace.visible() ) {
		for ( Size i=1; i<=total_residue; ++i ) {
			for ( Size j=i+1; j<=total_residue; ++j ) {
				tr.Trace << i << " " << j << "   " << count_matrix( i, j ) << std::endl;
			}
		}
	}
	tr.Info << "remaining non-redundant constraints " << in.size() << std::endl;
}

} // namespace constraints
} // namespace scoring
} // namespace core
