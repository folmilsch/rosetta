// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available
// (c) under license. The Rosetta software is developed by the contributing
// (c) members of the Rosetta Commons. For more information, see
// (c) http://www.rosettacommons.org. Questions about this can be addressed to
// (c) University of Washington UW TechTransfer,email:license@u.washington.edu.

/// @file protocols/antibody2/AntibodyInfo.cc
/// @brief
/// @author Jianqing Xu (xubest@gmail.com)

#include <protocols/antibody2/AntibodyInfo.hh>
#include <protocols/antibody2/AntibodyUtil.hh>
#include <protocols/rigid/RB_geometry.hh>
#include <protocols/loops/loops_main.hh>
#include <protocols/toolbox/task_operations/RestrictToInterface.hh>
#include <core/scoring/ScoreType.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/rms_util.hh>
#include <core/pose/PDBInfo.hh>
#include <core/pose/util.hh>
#include <core/pose/util.tmpl.hh>
#include <basic/Tracer.hh>

// ObjexxFCL Headers
#include <ObjexxFCL/FArray1D.hh>
#include <ObjexxFCL/format.hh>

// Utility headers
#include <utility/excn/Exceptions.hh>



///////////////////////////////////////////////////////////////////////////////

static basic::Tracer TR("antibody2.AntibodyInfo");

namespace protocols{
namespace antibody2{
	
	

	
    
AntibodyInfo::AntibodyInfo( pose::Pose const & pose,
						   AntibodyNumberingEnum const & numbering_scheme,
						   bool const & cdr_pdb_numbered) {
    set_default();
	
    numbering_scheme_ = numbering_scheme;
	cdr_pdb_numbered_ = cdr_pdb_numbered;
	
    identify_antibody(pose);
    
	init(pose);
}


    
void AntibodyInfo::set_default()
{
    is_camelid_ = false;
    InputPose_has_antigen_ = false;
    predicted_H3_base_type_ = Kinked;
    loopsop_having_allcdrs_=NULL;	
    numbering_scheme_ = Aroop;
}


void AntibodyInfo::identify_antibody(pose::Pose const & pose){
    
    switch (pose.conformation().num_chains() ) {
        case 0:
            throw excn::EXCN_Msg_Exception("the number of chains in the input pose is '0' !!");
            break;
        case 1:     //if pose has only "1" chain, it is a nanobody
            if (pose.pdb_info()->chain(pose.conformation().chain_end(1)) == 'H'){
                is_camelid_ = true;
                InputPose_has_antigen_=false;
            }
            else{
                throw excn::EXCN_Msg_Exception("  A): the input pose has only 1 chain, if it is a nanobody, the chain ID is supposed to be 'H' !!");
            }
            break;
        case 2:       // if pose has "2" chains, it can be 2 possibilities
            // possiblity 1): L and H, regular antibody
            if (  (pose.pdb_info()->chain(pose.conformation().chain_end(1)) == 'L')  &&  (pose.pdb_info()->chain(pose.conformation().chain_end(2)) == 'H')  ) {
                is_camelid_ = false;
                InputPose_has_antigen_ = false;
            }
            // possiblity 2): H nanobody and antigen
            else if ( pose.pdb_info()->chain(pose.conformation().chain_end(1)) == 'H' ) {
                is_camelid_ = true;
                InputPose_has_antigen_ = true;
            }
            else{
                throw excn::EXCN_Msg_Exception("  B): the input pose has two chains, 1). if it is nanobody, the 1st chain should be 'H'. 2). If it is a regular antibody, the 1st and 2nd chains should be 'L' and 'H' !!");
            }
            break;
        default:      // if pose has >=3 chains, it can be 2 possibilities
            // possiblity 1): L and H, and antigen
            if(   pose.pdb_info()->chain(pose.conformation().chain_end(1)) == 'L'  &&  pose.pdb_info()->chain(pose.conformation().chain_end(2)) == 'H'    ){
                is_camelid_ = false;
                InputPose_has_antigen_ = true;
            }
            // possiblity 2):  H annobody and antigen
            else if (pose.pdb_info()->chain(pose.conformation().chain_end(1)) == 'H'){
                is_camelid_ = true;
                InputPose_has_antigen_ = true;
            }
            else{
                throw excn::EXCN_Msg_Exception("  C). the input pose has more than two chains, 1). if it is nanobody, the 1st chain should be 'H'. 2). If it is a regular antibody, the 1st and 2nd chains should be 'L' and 'H' !!");
            }
            break;
    }
    
	/// record the antibody sequence
	Size chain_count = (is_camelid_)? 1:2 ;
	for (Size i=1; i<=pose.conformation().chain_end(chain_count) ; ++i){
		ab_sequence_.push_back(pose.residue(i).name1());
	}
	
}


void AntibodyInfo::init(pose::Pose const & pose){
    
    if(is_camelid_) total_cdr_loops_ = camelid_last_loop;
    else            total_cdr_loops_ = num_cdr_loops;
    
    setup_CDRsInfo(pose) ;
    
    setup_FrameWorkInfo(pose) ;
    
    predict_H3_base_type( pose );
}


    
/// TODO: 
// JQX:
// The code assumed that the input PDB has been been renumbered using the Aroop
// numbering scheme [see the "get_AntibodyNumberingScheme()" function below]: as a matter
// of fact, since this code is desigend for the Rosetta Antibody Homology Modeling
// the input is always the structure made from different templates, and they are
// always renumbered by the perl script from the Rosetta Antibody Server
// A smart way would be to use the "identify_CDR_from_a_sequence()" to automatically
// check this out. On my list!

void AntibodyInfo::setup_CDRsInfo( pose::Pose const & pose ) {
	
	vector1<char> Chain_IDs_for_CDRs;
	for (Size i=1;i<=3;++i) { Chain_IDs_for_CDRs.push_back('H'); } // HEAVY chain first
    for (Size i=1;i<=3;++i) { Chain_IDs_for_CDRs.push_back('L'); } // light
	
	vector1< vector1<int> > cdr_numbering_info = get_CDR_NumberingInfo(numbering_scheme_);

    int loop_start_in_pose, loop_stop_in_pose, cut_position ;
    loopsop_having_allcdrs_ = new loops::Loops();
    
    for (AntibodyCDRNameEnum i=start_cdr_loop; i<=total_cdr_loops_; i=AntibodyCDRNameEnum(i+1) ){
        loop_start_in_pose = pose.pdb_info()->pdb2pose( Chain_IDs_for_CDRs[i], cdr_numbering_info[Begin][i]);
        if(i != h3 ){
            loop_stop_in_pose= pose.pdb_info()->pdb2pose( Chain_IDs_for_CDRs[i], cdr_numbering_info[End][i]);
            cut_position = (loop_stop_in_pose - loop_start_in_pose +1) /2 + loop_start_in_pose;
        }
        else{
            loop_stop_in_pose  = pose.pdb_info()->pdb2pose( Chain_IDs_for_CDRs[i], cdr_numbering_info[End][i]+1 );
            loop_stop_in_pose -=1;
            // JQX:
            // One should always see 95-102 as the positions for your H3 in your FR02.pdb, but as a matter of fact,
            // the antibody script just copied h3.pdb (heavy atoms) into the FR02.pdb, sometimes one sees the stop
            // postition pdb number 98, not 102, if the h3.pdb is short. Therefore, one useing the pdb number 102 to
            // define h3 fails!
            // But in FR02.pdb, you always see 103, because 103 is on the framework. The idea is to find the pose number
            // of PDB number 103, then minus 1 will give you the last residue of h3.
            cut_position = (loop_start_in_pose +1 ) ;
            // JQX:
            // why this is different compared to other cuts of other loops?
            // Aroop seems did this in his old R3 code, CHECK LATER !!!
        }
            
        loops::Loop  one_loop(loop_start_in_pose, loop_stop_in_pose, cut_position);
        loops::LoopsOP one_loops = new loops::Loops(); one_loops->add_loop(one_loop);
    
        // make a "LoopsOP" object, in which each "Loop" was saved
        loopsop_having_allcdrs_->add_loop(one_loop);
        
        // make a "vector1" of "LoopsOP" object, each "LoopsOP" has one "Loop" object
        vector1_loopsop_having_cdr_.push_back(one_loops);
    }
    
                                            /// FIXME:  ***********************
	loopsop_having_allcdrs_->sequential_order(); /// TODO: kind of dangerous here
    
    TR<<"Successfully finished the CDR defintion"<<std::endl;
            
}
    



void AntibodyInfo::setup_FrameWorkInfo( pose::Pose const & pose ) {

    FrameWork frmwk;
	vector1<FrameWork> Lfr, Hfr;

	if(is_Camelid() == false ){
		if (! pose.pdb_info()->pdb2pose('L', 5)) {
			throw excn::EXCN_Msg_Exception( "L chain 5th residues missing, framework definition failed!!! " );
		}
		if (! pose.pdb_info()->pdb2pose('L', 105)) {
			throw excn::EXCN_Msg_Exception( "L chain 105th residues missing, framework definition failed!!! " );
		}
	}
	if (! pose.pdb_info()->pdb2pose('H', 5)) {
		throw excn::EXCN_Msg_Exception( "H chain 5th residues missing, framework definition failed!!! " );
	}
	if (! pose.pdb_info()->pdb2pose('H', 110)) {
		throw excn::EXCN_Msg_Exception( "H chain 110th residues missing, framework definition failed!!! " );
	}
	
	
	switch (numbering_scheme_) {
	case Aroop:
		if(! is_camelid_){
			frmwk.chain_name='L';
			frmwk.start=pose.pdb_info()->pdb2pose('L',5); frmwk.stop=pose.pdb_info()->pdb2pose('L',6);  Lfr.push_back(frmwk);
			frmwk.start=pose.pdb_info()->pdb2pose('L',10);frmwk.stop=pose.pdb_info()->pdb2pose('L',23); Lfr.push_back(frmwk);
			frmwk.start=pose.pdb_info()->pdb2pose('L',35);frmwk.stop=pose.pdb_info()->pdb2pose('L',38); Lfr.push_back(frmwk);
			frmwk.start=pose.pdb_info()->pdb2pose('L',45);frmwk.stop=pose.pdb_info()->pdb2pose('L',49); Lfr.push_back(frmwk);
			frmwk.start=pose.pdb_info()->pdb2pose('L',57);frmwk.stop=pose.pdb_info()->pdb2pose('L',66); Lfr.push_back(frmwk);
			frmwk.start=pose.pdb_info()->pdb2pose('L',71);frmwk.stop=pose.pdb_info()->pdb2pose('L',88); Lfr.push_back(frmwk);
			frmwk.start=pose.pdb_info()->pdb2pose('L',98);frmwk.stop=pose.pdb_info()->pdb2pose('L',105);Lfr.push_back(frmwk);
		}

		frmwk.chain_name='H';
		frmwk.start=pose.pdb_info()->pdb2pose('H',5);  frmwk.stop=pose.pdb_info()->pdb2pose('H',6);  Hfr.push_back(frmwk);
		frmwk.start=pose.pdb_info()->pdb2pose('H',10); frmwk.stop=pose.pdb_info()->pdb2pose('H',25); Hfr.push_back(frmwk);
		frmwk.start=pose.pdb_info()->pdb2pose('H',36); frmwk.stop=pose.pdb_info()->pdb2pose('H',39); Hfr.push_back(frmwk);
		frmwk.start=pose.pdb_info()->pdb2pose('H',46); frmwk.stop=pose.pdb_info()->pdb2pose('H',49); Hfr.push_back(frmwk);
		frmwk.start=pose.pdb_info()->pdb2pose('H',66); frmwk.stop=pose.pdb_info()->pdb2pose('H',94); Hfr.push_back(frmwk);
		frmwk.start=pose.pdb_info()->pdb2pose('H',103);frmwk.stop=pose.pdb_info()->pdb2pose('H',110);Hfr.push_back(frmwk);
		break;
	case Chothia:
		break;
	case Kabat:
		break;
	case Enhanced_Chothia:
		break;
	case AHO:
		break;
	case IMGT:
		break;
	default:
		throw excn::EXCN_Msg_Exception("the numbering schemes can only be 'Aroop','Chothia','Kabat', 'Enhanced_Chothia', 'AHO', 'IMGT' !!!!!! ");
		break;
	}
	
	if (Lfr.size()>0)  { framework_info_.push_back(Lfr);}
	if (Hfr.size()>0)  { framework_info_.push_back(Hfr);}
	else { throw excn::EXCN_Msg_Exception("The heavy chain has no framework? This cannot be correct");}
	

}
	
	
    

	
////////////////////////////////////////////////////////////////////////////////
///                                                                          ///
///	predicit H3 cterminus base type (Kinked or Extended) based on sequence   ///
///                                                                          ///
////////////////////////////////////////////////////////////////////////////////
void AntibodyInfo::predict_H3_base_type( pose::Pose const & pose ) {
	if( is_camelid_ ){
		detect_and_set_camelid_CDR_H3_stem_type( pose );
    }
	else{
		//detect_and_set_regular_CDR_H3_stem_type( pose );
        detect_and_set_regular_CDR_H3_stem_type_new_rule( pose );
    }
} // detect_CDR_H3_stem_type


void AntibodyInfo::detect_and_set_camelid_CDR_H3_stem_type(pose::Pose const & pose ) {
	TR << "AC Detecting Camelid CDR H3 Stem Type" << std::endl;

    bool kinked_H3 (false);
    bool extended_H3 (false);
    
	// extract single letter aa codes for the chopped loop residues
	vector1< char > cdr_h3_sequence;
	for( Size ii = get_CDR_loop(h3).start() - 2; ii <= get_CDR_loop(h3).stop(); ++ii )
		cdr_h3_sequence.push_back( pose.sequence()[ii-1] );

	// Rule for extended
	if( ( ( get_CDR_loop(h3).stop() - get_CDR_loop(h3).start() ) ) >= 12 ) {
		if( ( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'Y' ) ||
					( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'W' ) ||
					( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'F' ) ) &&
				( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] != 'H' ) &&
				( cdr_h3_sequence[ cdr_h3_sequence.size() - 1 ] != 'G' ) )
			extended_H3 = true;
	}

	if( !extended_H3 ) {
		kinked_H3 = true;
		if(           ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'R' ) ||
				      ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'Y' ) ||
				((     ( cdr_h3_sequence[ cdr_h3_sequence.size() - 1 ] != 'Y' ) || ( cdr_h3_sequence[ cdr_h3_sequence.size() - 1 ] != 'W' )    ) &&
				(     ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] != 'Y' ) || ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] != 'W' )    ) &&
				( 	  ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] != 'Y' ) || ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] != 'W' )    ))
		)
			kinked_H3 = false;
	}

    if (kinked_H3) predicted_H3_base_type_ = Kinked;
    if (extended_H3) predicted_H3_base_type_ = Extended;
    if (!kinked_H3 && !extended_H3) predicted_H3_base_type_ = Neutral;
	TR << "AC Finished Detecting Camelid CDR H3 Stem Type: " << get_string_h3_base_type()[predicted_H3_base_type_] << std::endl;
} 


void AntibodyInfo::detect_and_set_regular_CDR_H3_stem_type( pose::Pose const & pose ) {
	TR << "AC Detecting Regular CDR H3 Stem Type" << std::endl;
    bool extended_H3 (false) ;
    bool kinked_H3 (false);
	bool is_H3( false );

	// extract single letter aa codes for the chopped loop residues
	vector1< char > cdr_h3_sequence;
	for( Size ii = get_CDR_loop(h3).start() - 2; ii <= get_CDR_loop(h3).stop()+1 ; ++ii )
		cdr_h3_sequence.push_back( pose.sequence()[ii-1] );

	// Rule 1a for standard kink
	if( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] != 'D') {
		kinked_H3 = true;
		is_H3 = true;
	}

	// Rule 1b for standard extended form
	if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D')
			&& ( (cdr_h3_sequence[2] != 'K') &&
					 (cdr_h3_sequence[2] != 'R') ) && (is_H3 != true)) {
		extended_H3 = true;
		is_H3 = true;
	}

	if( !is_H3 ) {
		// Rule 1b extension for special kinked form
		bool is_basic( false ); // Special basic residue exception flag
		for(Size ii = 3; ii <= Size(cdr_h3_sequence.size() - 4); ++ii) {
			if( cdr_h3_sequence[ii] == 'R' || cdr_h3_sequence[ii] == 'K') {
				is_basic = true;
				break;
			}
		}

		if( !is_basic ) {
			Size L49_pose_number = pose.pdb_info()->pdb2pose( 'L', 49 );
			char aa_code_L49 = pose.residue( L49_pose_number ).name1();
			if( aa_code_L49 == 'R' || aa_code_L49 == 'K')
				is_basic = true;
		}
		if( is_basic ) {
			kinked_H3 = true;
			is_H3 = true;
		}
	}

	// Rule 1c for kinked form with salt bridge
	if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D') &&
			( (cdr_h3_sequence[2] == 'K') ||
				(cdr_h3_sequence[2] == 'R') ) &&
			( (cdr_h3_sequence[1] != 'K') &&
				(cdr_h3_sequence[1] != 'R') ) && (is_H3 != true) ) {
		kinked_H3 = true;
		is_H3 = true;
		if( !is_H3 ) {
			bool is_basic( false ); // Special basic residue exception flag
			Size L46_pose_number = pose.pdb_info()->pdb2pose( 'L', 46 );
			char aa_code_L46 = pose.residue( L46_pose_number ).name1();
			if( aa_code_L46 == 'R' || aa_code_L46 == 'K')
				is_basic = true;
			if( is_basic ) {
				extended_H3 = true;
				is_H3 = true;
			}
		}
	}

	// Rule 1d for extened form with salt bridge
	if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D') &&
			( ( cdr_h3_sequence[ 2 ] == 'K') ||
				(cdr_h3_sequence[2] == 'R')) &&
			( (cdr_h3_sequence[1] == 'K') ||
				(cdr_h3_sequence[1] == 'R') ) && (is_H3 != true) ) {
		extended_H3 = true;
		is_H3 = true;
	}

    if (kinked_H3) predicted_H3_base_type_ = Kinked;
    if (extended_H3) predicted_H3_base_type_ = Extended;
    if (!kinked_H3 && !extended_H3) predicted_H3_base_type_ = Neutral;
	TR << "AC Finished Detecting Regular CDR H3 Stem Type: " << get_string_h3_base_type()[predicted_H3_base_type_] << std::endl;
} // detect_regular_CDR_H3_stem_type()
    
    
void AntibodyInfo::detect_and_set_regular_CDR_H3_stem_type_new_rule( pose::Pose const & pose ) {
    TR << "AC Detecting Regular CDR H3 Stem Type" << std::endl;
        
    bool extended_H3 (false) ;
    bool kinked_H3 (false);
    bool is_H3( false );	// "is_H3" is no longer used. 06/18/12
        
    // extract single letter aa codes for the chopped loop residues
    vector1< char > cdr_h3_sequence;
    for( Size ii = get_CDR_loop(h3).start() - 2; ii <= get_CDR_loop(h3).stop() + 1; ++ii )
        cdr_h3_sequence.push_back( pose.sequence()[ii-1] );
    //for (Size i=1; i<=cdr_h3_sequence.size();++i){    TR<<cdr_h3_sequence[i];} TR<<std::endl;
    
    /// @author: Daisuke Kuroda (dkuroda1981@gmail.com) 06/18/2012
    ///
    /// @last_modified 06/18/2012
    ///
    /// @reference Kuroda et al. Proteins. 2008 Nov 15;73(3):608-20.
    ///			   Koliansnikov et al. J Bioinform Comput Biol. 2006 Apr;4(2):415-24.
        
    // This is only for rule 1b
    bool is_basic( false ); // Special basic residue exception flag
    if( !is_basic ) {
            Size L49_pose_number = pose.pdb_info()->pdb2pose( 'L', 49 );
            char aa_code_L49 = pose.residue( L49_pose_number ).name1();
            if( aa_code_L49 == 'R' || aa_code_L49 == 'K')
                is_basic = true;
        }
        
        /// START H3-RULE 2007
        if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D') &&
           ( ( cdr_h3_sequence[ 2 ] == 'K') || (cdr_h3_sequence[2] == 'R') ) &&
           ( ( cdr_h3_sequence[ 1 ] == 'K') || (cdr_h3_sequence[1] == 'R') ) ) {
            // Rule 1d for extened form with salt bridge
            extended_H3 = true;
        }else if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D') &&
                 ( ( cdr_h3_sequence[ 2 ] == 'K') || ( cdr_h3_sequence[ 2 ] == 'R') ) &&
                 ( ( cdr_h3_sequence[ 1 ] != 'K') && ( cdr_h3_sequence[ 1 ] != 'R') ) ) {
            // Rule 1c for kinked form with salt bridge with/without Notable signal (L46)
            // Special basic residue exception flag
            Size L46_pose_number = pose.pdb_info()->pdb2pose( 'L', 46 );
            char aa_code_L46 = pose.residue( L46_pose_number ).name1();
            
            // Special Tyr residue exception flag
            Size L36_pose_number = pose.pdb_info()->pdb2pose( 'L', 36 );
            char aa_code_L36 = pose.residue( L36_pose_number ).name1();
            
            if( ( aa_code_L46 == 'R' || aa_code_L46 == 'K') && aa_code_L36 != 'Y' ){
                extended_H3 = true;
            }else{
                kinked_H3   = true;
            }
        }else if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D' ) &&
                 ( cdr_h3_sequence[ 2 ] != 'K' ) && ( cdr_h3_sequence[ 2 ] != 'R' ) &&
                 ( is_basic == true ) ) {
            // Rule 1b for standard extended form with Notable signal (L49)
            kinked_H3   = true;
        }else if( ( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'F' ) &&
                   ( cdr_h3_sequence[ cdr_h3_sequence.size() - 4 ] == 'A' ) ) || 
                 ( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'F' ) &&
                  ( cdr_h3_sequence[ cdr_h3_sequence.size() - 4 ] == 'G' ) ) ||
                 ( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'M' ) &&
                  ( cdr_h3_sequence[ cdr_h3_sequence.size() - 4 ] == 'A' ) ) ||
                 ( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'M' ) &&
                  ( cdr_h3_sequence[ cdr_h3_sequence.size() - 4 ] == 'G' ) ) ) {
                     // This is new feature
                     kinked_H3   = true;
                 }else if( ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'R' ) ||
                          ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'K' ) ||
                          ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'D' ) ||
                          ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'N' ) ){
                     // This is new feature
                     extended_H3 = true;
                 }else if( ( ( cdr_h3_sequence[ 3 ] == 'Y' ) &&
                            ( cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'F' ) ) ||
                          ( (cdr_h3_sequence[ 3 ] == 'Y' ) &&
                           (cdr_h3_sequence[ cdr_h3_sequence.size() - 3 ] == 'M') ) ){ 
                              // This is new feature
                              extended_H3 = true;
                          }else if( cdr_h3_sequence.size() - 3  == 7 ) {
                              // This is new feature
                              extended_H3 = true;
                          }else if( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] == 'D' ) {
                              // Rule 1b for standard extended form without Notable signal (L49)
                              extended_H3 = true;
                          }else if( cdr_h3_sequence[ cdr_h3_sequence.size() - 2 ] != 'D' ) {
                              // Rule 1a for standard kink. i.e. No sequence feature...
                              kinked_H3 = true;
                          }
        // END H3-RULE 2007
        
    if (kinked_H3) predicted_H3_base_type_ = Kinked;
    if (extended_H3) predicted_H3_base_type_ = Extended;
    if (!kinked_H3 && !extended_H3) predicted_H3_base_type_ = Neutral;
	TR << "AC Finished Detecting Regular CDR H3 Stem Type: " << get_string_h3_base_type()[predicted_H3_base_type_] << std::endl;
        
        TR << "AC Finished Detecting Regular CDR H3 Stem Type: "
    << "Kink: " << kinked_H3 << " Extended: " << extended_H3 << std::endl;
} // detect_regular_CDR_H3_stem_type()
    
    
    
////////////////////////////////////////////////////////////////////////////////
///                                                                          ///
///				provide fold tree utilities for various purpose              ///
///                                                                          ///
////////////////////////////////////////////////////////////////////////////////
	
kinematics::FoldTreeCOP AntibodyInfo::setup_simple_fold_tree(
								Size const & jumppoint1,
								Size const & cutpoint,
								Size const & jumppoint2,
								pose::Pose const & pose ) const {
        
	using namespace kinematics;
	
	
	FoldTreeOP f = new FoldTree();
	f->clear();
	
	f->add_edge( 1, jumppoint1, Edge::PEPTIDE );
	f->add_edge( jumppoint1, cutpoint, Edge::PEPTIDE );
	f->add_edge( cutpoint + 1, jumppoint2, Edge::PEPTIDE );
	f->add_edge( jumppoint2, pose.total_residue(), Edge::PEPTIDE );
	f->add_edge( jumppoint1, jumppoint2, 1 );
	f->reorder( 1 );
		
	return f;
        
}
    

kinematics::FoldTreeCOP AntibodyInfo::get_FoldTree_AllCDRs (pose::Pose const & pose ) const {
	using namespace kinematics;

	FoldTreeOP f = new FoldTree();
	f->clear();

	Size jump_num = 0;
	for( loops::Loops::const_iterator it=loopsop_having_allcdrs_->begin(), it_end=loopsop_having_allcdrs_->end(), it_next; it < it_end; ++it ) {

		it_next = it;
		it_next++;

		if( it == loopsop_having_allcdrs_->begin() ) f->add_edge( 1, it->start()-1, Edge::PEPTIDE );

		jump_num++;
		f->add_edge( it->start()-1, it->stop()+1, jump_num );
		f->add_edge( it->start()-1, it->cut(),  Edge::PEPTIDE );
		f->add_edge( it->cut()+1, it->stop()+1, Edge::PEPTIDE );
		if( it == (it_end-1) )
			f->add_edge( it->stop()+1, pose.total_residue(), Edge::PEPTIDE);
		else
			f->add_edge( it->stop()+1, it_next->start()-1, Edge::PEPTIDE );
	}

	f->reorder(1);
	
	return f;

} // all_cdr_fold_tree()

///////////////////////////////////////////////////////////////////////////
/// @begin all_cdr_VL_VH_fold_tree
///
/// @brief change to all CDR and VL-VH dock fold tree
///
/// @authors Aroop 07/13/2010
///
/// @last_modified 07/13/2010
///////////////////////////////////////////////////////////////////////////
kinematics::FoldTreeCOP AntibodyInfo::get_FoldTree_AllCDRs_LHDock( pose::Pose & pose ) const {
       
	using namespace kinematics;
        
	Size nres = pose.total_residue();
	pose::PDBInfoCOP pdb_info = pose.pdb_info();
	char second_chain = 'H';
	Size rb_cutpoint(0);
        
	for ( Size i = 1; i <= nres; ++i ) {
		if( pdb_info->chain( i ) == second_chain) {
			rb_cutpoint = i-1;
			break;
		}
	}
        
    Size jump_pos1 ( geometry::residue_center_of_mass( pose, 1, rb_cutpoint ) );
    Size jump_pos2 ( geometry::residue_center_of_mass( pose,rb_cutpoint+1, nres ) );
    //TR<<rb_cutpoint<<std::endl;
    //TR<<jump_pos1<<std::endl;
    //TR<<jump_pos2<<std::endl;
        
    // make sure rb jumps do not reside in the loop region
    for( loops::Loops::const_iterator it= loopsop_having_allcdrs_->begin(), it_end = loopsop_having_allcdrs_->end(); it != it_end; ++it ) {
        if (   jump_pos1 >= ( it->start() - 1 ) && jump_pos1 <= ( it->stop() + 1)   )
            jump_pos1 = it->stop() + 2;
        if (   jump_pos2 >= ( it->start() - 1 ) && jump_pos2 <= ( it->stop() + 1)   )
            jump_pos2 = it->start() - 2;
    }
        
    // make a simple rigid-body jump first
    FoldTreeOP f = new FoldTree(* setup_simple_fold_tree(jump_pos1,rb_cutpoint,jump_pos2, pose ));
        
    for( loops::Loops::const_iterator it=loopsop_having_allcdrs_->begin(), it_end=loopsop_having_allcdrs_->end(); it != it_end; ++it ) {
        Size const loop_start ( it->start() );
        Size const loop_stop ( it->stop() );
        Size const loop_cutpoint ( it->cut() );
        Size edge_start(0), edge_stop(0);
        bool edge_found = false;
        const FoldTree & f_const = *f;
        Size const num_jump = f_const.num_jump();
        for( FoldTree::const_iterator it2=f_const.begin(), it2_end=f_const.end(); it2 !=it2_end; ++it2 ) {
			//TR<<it2->start()<<std::endl;
			//TR<<it2->stop()<<std::endl;
            edge_start = std::min( it2->start(), it2->stop() );
            edge_stop  = std::max( it2->start(), it2->stop() );
            if ( ! it2->is_jump() && loop_start > edge_start && loop_stop < edge_stop ) {
                edge_found = true;
                break;
            }
        }
            
        f->delete_unordered_edge( edge_start, edge_stop, Edge::PEPTIDE);
        f->add_edge( loop_start-1, loop_stop+1, num_jump+1 );
        f->add_edge( edge_start, loop_start-1, Edge::PEPTIDE );
        f->add_edge( loop_start-1, loop_cutpoint, Edge::PEPTIDE );
        f->add_edge( loop_cutpoint+1, loop_stop+1, Edge::PEPTIDE );
        f->add_edge( loop_stop+1, edge_stop, Edge::PEPTIDE );
    }
        
    f->reorder(1);
    return f;
}
    
    
///////////////////////////////////////////////////////////////////////////
/// @begin LH_A_foldtree
///
/// @brief	Fold tree for snugdock, docks LH with the antigen chains. The function
///			assumes that the coordinates for antigen chains in the input PDB file
///			are right after the antibody heavy chain (which must be named H).The
///			expected order of chains is thus L, H followed by the antigen chains.
///
/// @authors Krishna Praneeth Kilambi 08/14/2012
///
/// @last_modified 08/14/2012
///////////////////////////////////////////////////////////////////////////
kinematics::FoldTree AntibodyInfo::get_FoldTree_LH_A( pose::Pose const & pose ) const {
    
    using namespace core;
    using namespace kinematics;
    
    Size nres = pose.total_residue();
    pose::PDBInfoCOP pdb_info = pose.pdb_info();
    char second_chain = 'H';
    Size cutpoint = 0;
    
    kinematics::FoldTree LH_A_foldtree;
    
    for ( Size i = 1; i <= nres; ++i ) {
        if(pdb_info->chain(1) != 'L'){
            throw excn::EXCN_Msg_Exception("Chains are not named correctly or are not in the expected order");
            break;
        }
        if( (pdb_info->chain(i) == 'L') && (pdb_info->chain(i) != pdb_info->chain(i+1))) {
            if(pdb_info->chain(i+1) != second_chain){
                throw excn::EXCN_Msg_Exception("Chains are not named correctly or are not in the expected order");
                break;
            }
        }
        if( (pdb_info->chain(i) == second_chain) && (pdb_info->chain(i) != pdb_info->chain(i+1))) {
            cutpoint = i;
            break;
        }
    }
    
    Size jump_pos1 ( geometry::residue_center_of_mass( pose, 1, cutpoint ) );
    Size jump_pos2 ( geometry::residue_center_of_mass( pose, cutpoint+1, pose.total_residue() ) );
    
    //setup fold tree based on cutpoints and jump points
    LH_A_foldtree.clear();
    LH_A_foldtree.simple_tree( pose.total_residue() );
    LH_A_foldtree.new_jump( jump_pos1, jump_pos2, cutpoint);
    
    Size chain_begin(0), chain_end(0);
    
    //rebuild jumps between antibody light and heavy chains
    chain_end = cutpoint;
    chain_begin = pose.conformation().chain_begin( pose.chain(chain_end) );
    while (chain_begin != 1){
        chain_end = chain_begin-1;
        LH_A_foldtree.new_jump( chain_end, chain_begin, chain_end);
        chain_begin = pose.conformation().chain_begin( pose.chain(chain_end) );
    }
    
    //rebuild jumps between all the antigen chains
    chain_begin = cutpoint+1;
    chain_end = pose.conformation().chain_end( pose.chain(chain_begin) );
    while (chain_end != pose.total_residue()){
        chain_begin = chain_end+1;
        LH_A_foldtree.new_jump( chain_end, chain_begin, chain_end);
        chain_end = pose.conformation().chain_end( pose.chain(chain_begin) );
    }
    
    LH_A_foldtree.reorder( 1 );
    LH_A_foldtree.check_fold_tree();
    
    return LH_A_foldtree;
}
    
    
///////////////////////////////////////////////////////////////////////////
/// @begin L_HA_foldtree
///
/// @brief	Fold tree for LH refinement in snugdock, docks L with H + antigen
///			chains. The function assumes that the coordinates for antigen chains
///			in the input PDB file are right after the antibody heavy chain
///			(which must be named H).The expected order of chains is thus
///			L, H followed by the antigen chains.
///
/// @authors Krishna Praneeth Kilambi 08/14/2012
///
/// @last_modified 08/14/2012
///////////////////////////////////////////////////////////////////////////
kinematics::FoldTree AntibodyInfo::get_FoldTree_L_HA( pose::Pose const & pose ) const {
	
	using namespace core;
	using namespace kinematics;
	
	Size nres = pose.total_residue();
	pose::PDBInfoCOP pdb_info = pose.pdb_info();
	char second_chain = 'H';
	Size cutpoint = 0;
	
	kinematics::FoldTree L_HA_foldtree;
	
	for ( Size i = 1; i <= nres; ++i ) {
		if(pdb_info->chain(1) != 'L'){
			throw excn::EXCN_Msg_Exception("Chains are not named correctly or are not in the expected order");
			break;
		}
		if( (pdb_info->chain(i) == 'L') && (pdb_info->chain(i) != pdb_info->chain(i+1))) {
			if(pdb_info->chain(i+1) != second_chain){
				throw excn::EXCN_Msg_Exception("Chains are not named correctly or are not in the expected order");
				break;
			}
		}
		if( (pdb_info->chain(i) == 'L') && (pdb_info->chain(i+1) == second_chain)) {
			cutpoint = i;
			break;
		}
	}
	
	Size jump_pos1 ( geometry::residue_center_of_mass( pose, 1, cutpoint ) );
	Size jump_pos2 ( geometry::residue_center_of_mass( pose, cutpoint+1, pose.conformation().chain_end( pose.chain(cutpoint+1) ) ) );
	
	//setup fold tree based on cutpoints and jump points
	L_HA_foldtree.clear();
	L_HA_foldtree.simple_tree( pose.total_residue() );
	L_HA_foldtree.new_jump( jump_pos1, jump_pos2, cutpoint);
	
	Size chain_begin(0), chain_end(0);
	
	//rebuild jumps between heavy chain and antigen chains
	chain_begin = cutpoint+1;
	chain_end = pose.conformation().chain_end( pose.chain(chain_begin) );
	while (chain_end != pose.total_residue()){
		chain_begin = chain_end+1;
		L_HA_foldtree.new_jump( chain_end, chain_begin, chain_end);
		chain_end = pose.conformation().chain_end( pose.chain(chain_begin) );
	}
	
	L_HA_foldtree.reorder( 1 );
	L_HA_foldtree.check_fold_tree();
	
	return L_HA_foldtree;
	
}
    
///////////////////////////////////////////////////////////////////////////
/// @begin LA_H_foldtree
///
/// @brief	Fold tree for LH refinement in snugdock, docks L + antigen chains
///			with H. The function assumes that the coordinates for antigen chains
///			in the input PDB file are right after the antibody heavy chain
///			(which must be named H).The expected order of chains is thus
///			L, H followed by the antigen chains.
///
/// @authors Krishna Praneeth Kilambi 08/14/2012
///
/// @last_modified 08/14/2012
///////////////////////////////////////////////////////////////////////////
kinematics::FoldTree AntibodyInfo::get_FoldTree_LA_H( pose::Pose const & pose ) const {
    
	using namespace core;
	using namespace kinematics;

	Size nres = pose.total_residue();
	pose::PDBInfoCOP pdb_info = pose.pdb_info();
	char second_chain = 'H';
	Size cutpoint = 0;
	bool lchain_jump = false;

	kinematics::FoldTree LA_H_foldtree ;
	
	for ( Size i = 1; i <= nres; ++i ) {
		if(pdb_info->chain(1) != 'L'){
			throw excn::EXCN_Msg_Exception("Chains are not named correctly or are not in the expected order");
			break;
		}
		if( (pdb_info->chain(i) == 'L') && (pdb_info->chain(i) != pdb_info->chain(i+1))) {
			if(pdb_info->chain(i+1) != second_chain){
				throw excn::EXCN_Msg_Exception("Chains are not named correctly or are not in the expected order");
				break;
			}
		}
		if( (pdb_info->chain(i) == 'L') && (pdb_info->chain(i+1) == second_chain)) {
			cutpoint = i;
			break;
		}
	}
	
	Size jump_pos1 ( geometry::residue_center_of_mass( pose, 1, cutpoint ) );
	Size jump_pos2 ( geometry::residue_center_of_mass( pose, cutpoint+1, pose.conformation().chain_end( pose.chain(cutpoint+1) ) ) );
	
	//setup fold tree based on cutpoints and jump points
	LA_H_foldtree.clear();
	LA_H_foldtree.simple_tree( pose.total_residue() );
	LA_H_foldtree.new_jump( jump_pos1, jump_pos2, cutpoint);
	
	Size chain_begin(0), chain_end(0);
	
	//rebuild jumps between the light chain and antigen chains
	chain_begin = cutpoint+1;
	chain_end = pose.conformation().chain_end( pose.chain(chain_begin) );
	while (chain_end != pose.total_residue()){
		chain_begin = chain_end+1;
		if (!lchain_jump){
			LA_H_foldtree.new_jump( pose.conformation().chain_end( pose.chain(1) ), chain_begin, chain_end);
			lchain_jump = true;
		}
		else{
			LA_H_foldtree.new_jump( chain_end, chain_begin, chain_end);
		}
		chain_end = pose.conformation().chain_end( pose.chain(chain_begin) );
	}
	
	LA_H_foldtree.reorder( 1 );
	LA_H_foldtree.check_fold_tree();
	
	return LA_H_foldtree;

	
}

kinematics::MoveMap AntibodyInfo::get_MoveMap_for_Loops(pose::Pose const & pose,
														loops::Loops const & the_loops,
														bool const & bb_only,
														bool const & include_nb_sc,
														Real const & nb_dist) const {
	kinematics::MoveMap move_map ;
	
    move_map.clear();
    move_map.set_chi( false );
    move_map.set_bb( false );
    utility::vector1< bool> bb_is_flexible( pose.total_residue(), false );
    utility::vector1< bool> sc_is_flexible( pose.total_residue(), false );
	
    select_loop_residues( pose, the_loops, false/*include_neighbors*/, bb_is_flexible, nb_dist);
    move_map.set_bb( bb_is_flexible );
	if (bb_only==false) {
		select_loop_residues( pose, the_loops, include_nb_sc/*include_neighbors*/, sc_is_flexible, nb_dist);
		move_map.set_chi( sc_is_flexible );
	}
	for( Size ii = 1; ii <= the_loops.num_loop(); ++ii ){
		move_map.set_jump( ii, false );
	}

	return move_map;
}
	
	
kinematics::MoveMap AntibodyInfo::get_MoveMap_for_LoopsandDock(pose::Pose const & pose,
														loops::Loops const & the_loops,
														bool const & bb_only,
														bool const & include_nb_sc,
														Real const & nb_dist) const {
	
	kinematics::MoveMap move_map = get_MoveMap_for_Loops(pose, the_loops, bb_only, include_nb_sc, nb_dist);
	
	move_map.set_jump( 1, true );
	for( Size ii = 2; ii <= the_loops.num_loop() +1 ; ++ii ){
		move_map.set_jump( ii, false );
	}
	
	return move_map;
}
	
    
    

//JQX: doesn't matter only antibody or antibody-antigen complex, just include CDRs and their neighbors
pack::task::TaskFactory AntibodyInfo::get_TaskFactory_AllCDRs(pose::Pose  & pose)  {
	
	vector1< bool> sc_is_packable( pose.total_residue(), false );
	select_loop_residues( pose, *loopsop_having_allcdrs_, true/*include_neighbors*/, sc_is_packable);
	
	using namespace pack::task;
	using namespace pack::task::operation;
	// selecting movable c-terminal residues
	ObjexxFCL::FArray1D_bool loop_residues( pose.total_residue(), false );
	for( Size i = 1; i <= pose.total_residue(); ++i ) {
		loop_residues(i) = sc_is_packable[i];
	} // check mapping
	
	using namespace protocols::toolbox::task_operations;
	pack::task::TaskFactoryOP tf ;
	tf= setup_packer_task(pose);
	tf->push_back( new RestrictToInterface(loop_residues) );
	
	
	//pack::task::PackerTaskOP my_task2(tf->create_task_and_apply_taskoperations(pose));
	//TR<<*my_task2<<std::endl; //exit(-1);
	
	return *tf;
}
    
    
    
    
// JQX:: assuming Aroop numbering for now
vector1< vector1<Size> > AntibodyInfo::get_CDR_NumberingInfo(AntibodyNumberingEnum const & numbering_scheme) const {
    

    // definte local variables
    vector1<int> start, stop;
    vector1< vector1<int> > local_numbering_info;

    // doesn't hurt to clear all the contents, no matter they are empty or not
    start.clear(); stop.clear();
    for (Size i=1;i<=local_numbering_info.size(); ++i){
        local_numbering_info[i].clear();
    }
    local_numbering_info.clear();
        
    
    // JQX: always make the heavy chain first, so that one can always use enum names
    
    //**********************************************************************************
    //  Aroop Numbering                                                                *
    //    citation:
    //**********************************************************************************
    if(numbering_scheme == Aroop ){
        // Heavy Chain
        start.push_back(26); stop.push_back(35);  //h1
        start.push_back(50); stop.push_back(65);  //h2
        start.push_back(95); stop.push_back(102); //h3
        // Light Chain
        start.push_back(24); stop.push_back(34);  //l1
        start.push_back(50); stop.push_back(56);  //l2
        start.push_back(89); stop.push_back(97);  //l3
    }
    //**********************************************************************************
    //  Chothia Numbering                                                              *
    //    citation: 
    //**********************************************************************************
    else if(numbering_scheme == Chothia ){
        // Heavy Chain
        start.push_back(26); stop.push_back(32);  //h1
        start.push_back(52); stop.push_back(56);  //h2
        start.push_back(95); stop.push_back(102); //h3
        // Light Chain
        start.push_back(24); stop.push_back(34);  //l1
        start.push_back(50); stop.push_back(56);  //l2
        start.push_back(89); stop.push_back(97);  //l3
    }

    //**********************************************************************************
    //  Kabat Numbering                                                                *
    //    citation:
    //**********************************************************************************
    else if(numbering_scheme == Kabat ){
    }
    //**********************************************************************************
    //  Enhanced_Chothia Numbering                                                     *
    //    Abhinandan, K.R. and Martin, A.C.R. (2008) Immunology, 45, 3832-3839.        *
    //**********************************************************************************
    else if(numbering_scheme == Enhanced_Chothia){
    }
    //**********************************************************************************
    //  AHO Numbering                                                                  *
    //    citation:
    //**********************************************************************************
    else if(numbering_scheme == AHO){
    }
    //**********************************************************************************
    //  IMGT Numbering                                                                 *
    //    citation:
    //**********************************************************************************
    else if(numbering_scheme == IMGT){
    }
    else{
        throw excn::EXCN_Msg_Exception("the numbering schemes can only be 'Aroop','Chothia','Kabat', 'Enhanced_Chothia', 'AHO', 'IMGT' !!!!!! ");
    }
	
	local_numbering_info.push_back(start);
	local_numbering_info.push_back(stop);
	return local_numbering_info;
}

    





    
    
/// TODO:
//JQX: make Daisuke's code compatible with my code
//
	
///////////////////////////////////////////////////////////////////////////
/// @author: Daisuke Kuroda (dkuroda1981@gmail.com) 06/18/2012
///
/// @brief: Identify 3 CDRs from a sequence. Automatically judge heavy or light chains (I hope!).
///         The input can be either a light chain, a heavy chain or another sequence.
///
/// @last_modified 08/28/2012 by DK
///////////////////////////////////////////////////////////////////////////
void AntibodyInfo::identify_CDR_from_a_sequence(std::string const & querychain){
	
	int l1found = 0, l2found = 0, l3found = 1, h1found = 1, h2found = 0, h3found = 1; // 0 if exist; otherwise 1.
	int lenl1 = 0, lenl2 = 0, lenl3 = 0, lenh1 = 0, lenh2 = 0, lenh3 = 0;
	int posl1_s = 0, posl1_e = 0, posl2_s = 0, posl2_e = 0, posl3_s = 0, posl3_e = 0;
	int posh1_s = 0, posh1_e = 0, posh2_s = 0, posh2_e = 0, posh3_s = 0, posh3_e = 0;
	int i = 0, k = 0, l = 0, m = 0, n = 0;
	
	int pos_fr1_s = 0, pos_fr1_e = 0, pos_fr2_s = 0, pos_fr2_e = 0;
	int pos_fr3_s = 0, pos_fr3_e = 0, pos_fr4_s = 0, pos_fr4_e = 0;
	int len_fr1 = 0, len_fr2 = 0, len_fr3 = 0, len_fr4 = 0;
	
	int len;
	std::string check;
	
	std::string seql1, seql2, seql3, seqh1, seqh2, seqh3;
	std::string frl3, frh1, frh3;
	
	std::string seq_fr1, seq_fr2, seq_fr3, seq_fr4;
	
	// For L3:	[FVI]-[GAEV]-X-[GY]
	std::string p1_l3[] = {"F","V","I"};
	std::string p2_l3[] = {"G","A","E","V"};
	std::string p3_l3[] = {"G","A","P","C","D","E","Q","N","R","K","H","W","Y","F","M","T","V","I","S","L"};
	std::string p4_l3[] = {"G","Y"};
	
	// For H1
	std::string p1_h1[] = {"W","L"};
	std::string p2_h1[] = {"I","V","F","Y","A","M","L","N","G","E","W"};
	std::string p3_h1[] = {"R","K","Q","V","N","C"};
	std::string p4_h1[] = {"Q","K","H","E","L","R"};
	
	// For H3	W-[GA]-X-[DRG]
	std::string p1_h3[] = {"W","V"};
	std::string p2_h3[] = {"G","A","C"};
	std::string p3_h3[] = {"A","P","C","D","E","Q","N","R","K","H","W","Y","F","M","T","V","I","S","L","G"};
	std::string p4_h3[] = {"G","R","D","Q","V"};
	
	// Input sequence is here
	std::string querychain2(querychain, 0,140);
	std::string querychain3(querychain, 0,110);
	
	std::string querychain_first(querychain, 0, 50);
	std::string querychain_last(querychain, 70);
	
	//TR << querychain2 << endl;
	//TR << querychain_last << endl;
	
	len = querychain.length();
	if(len < 130){
		check = "Fv";
	}else if(len < 250){
		check = "Fab";
	}else{
		check = "Weird";
	}
	
	//TR << "*** Query sequence ***" << endl;
	//TR << querychain << endl;
	//TR << endl;
	
	/*****************************************************/
	/***************** Is it light chain? ****************/
	/*****************************************************/
	/* L1 search Start */
	if(querychain_first.find("WYL") != std::string::npos){
		posl1_e = querychain_first.find("WYL") - 1;
	}else if(querychain_first.find("WLQ") != std::string::npos){
		posl1_e = querychain_first.find("WLQ") - 1;
	}else if(querychain_first.find("WFQ") != std::string::npos){
		posl1_e = querychain_first.find("WFQ") - 1;
	}else if(querychain_first.find("WYQ") != std::string::npos){
		posl1_e = querychain_first.find("WYQ") - 1;
	}else if(querychain_first.find("WYH") != std::string::npos){
		posl1_e = querychain_first.find("WYH") - 1;
	}else if(querychain_first.find("WVQ") != std::string::npos){
		posl1_e = querychain_first.find("WVQ") - 1;
	}else if(querychain_first.find("WVR") != std::string::npos){
		posl1_e = querychain_first.find("WVR") - 1;
	}else if(querychain_first.find("WWQ") != std::string::npos){
		posl1_e = querychain_first.find("WWQ") - 1;
	}else if(querychain_first.find("WVK") != std::string::npos){
		posl1_e = querychain_first.find("WVK") - 1;
	}else if(querychain_first.find("WLL") != std::string::npos){
		posl1_e = querychain_first.find("WLL") - 1;
	}else if(querychain_first.find("WFL") != std::string::npos){
		posl1_e = querychain_first.find("WFL") - 1;
	}else if(querychain_first.find("WVF") != std::string::npos){
		posl1_e = querychain_first.find("WVF") - 1;
	}else if(querychain_first.find("WIQ") != std::string::npos){
		posl1_e = querychain_first.find("WIQ") - 1;
	}else if(querychain_first.find("WYR") != std::string::npos){
		posl1_e = querychain_first.find("WYR") - 1;
	}else if(querychain_first.find("WNQ") != std::string::npos){
		posl1_e = querychain_first.find("WNQ") - 1;
	}else if(querychain_first.find("WHL") != std::string::npos){
		posl1_e = querychain_first.find("WHL") - 1;
	}else if(querychain_first.find("WYM") != std::string::npos){	// Add 06/10/2012
		posl1_e = querychain_first.find("WYM") - 1;
	}else{
		l1found = 1;
	}
	
	if(l1found != 1){
		posl1_s   = querychain_first.find("C") + 1;
		lenl1     = posl1_e - posl1_s + 1;
		seql1     = querychain_first.substr(posl1_s,lenl1);
		
		pos_fr1_s = 0;
		pos_fr1_e = posl1_s - 1;
		len_fr1   = pos_fr1_e - pos_fr1_s + 1;
		seq_fr1   = querychain_first.substr(pos_fr1_s,len_fr1);
	}
	/* L1 search Finish */
	
	/* L2 search start */
	if(l1found != 1){
		posl2_s   = posl1_e + 16;
		posl2_e   = posl2_s + 6;
		lenl2     = posl2_e - posl2_s + 1;
		seql2     = querychain.substr(posl2_s,lenl2);
		
		pos_fr2_s = posl1_e + 1;
		pos_fr2_e = posl2_s - 1;
		len_fr2   = pos_fr2_e - pos_fr2_s + 1;
		seq_fr2   = querychain.substr(pos_fr2_s,len_fr2);
	}else{
		l2found = 1;
	}
	/* L2 search end */
	
	/* L3 search Start */
	//string p1_l3[] = {"F","V","I"};
	//string p2_l3[] = {"G","A","E","V"};
	//string p3_l3[] = {"G","A","P","C","D","E","Q","N","R","K","H","W","Y","F","M","T","V","I","S","L"};
	//string p4_l3[] = {"G","Y"};
	for(l = 0;l < 3; ++l){
		for(m = 0;m < 4; ++m){
			for(n = 0;n < 2; ++n){
				for(k = 0;k < 20; ++k){
					//frl3 = "FG" + p3_l3[k] + "G";
					//frl3 = p1_l3[l] + "G" + p3_l3[k] + "G";			//[VF]GXG
					frl3 = p1_l3[l] + p2_l3[m] + p3_l3[k] + p4_l3[n];	//[VF][AG]XG
					
					if(querychain3.find(frl3, 80) != std::string::npos){
						posl3_e = querychain3.find(frl3,80) - 1;
						posl3_s = querychain3.find("C",80) + 1;
						lenl3   = posl3_e - posl3_s + 1;
						
						//TR << frl3 << "\t" << posl3_s << "\t" << lenl3 << endl;
						
						seql3   = querychain3.substr(posl3_s,lenl3);
						
						if(seql3.length() > 4){
							l3found = 0;
							break;
						}else{
							l3found = 1;
						}
						
						pos_fr3_s = posl2_e + 1;
						pos_fr3_e = posl3_s - 1;
						pos_fr4_s = posl3_e + 1;
						pos_fr4_e = pos_fr4_s + 5;
						len_fr3   = pos_fr3_e - pos_fr3_s + 1;
						len_fr4   = pos_fr4_e - pos_fr4_s + 1;
						seq_fr3   = querychain3.substr(pos_fr3_s,len_fr3);
						seq_fr4   = querychain3.substr(pos_fr4_s,len_fr4);
					}
				}
				
				l = 3;
				m = 4;
				n = 2;
				k = 20;
			}
		}
	}
	/* L3 search Finish */
	
	/*****************************************************/
	/***************** Is it heavy chain? ****************/
	/*****************************************************/
	if(l1found == 1 || l2found == 1 || l3found == 1){
		/* H1 search Start */
		//string p1_h1[] = {"W","L"};
		//string p2_h1[] = {"I","V","F","Y","A","M","L","N","G","E"};
		//string p3_h1[] = {"R","K","Q","V","N","C"};
		//string p4_h1[] = {"Q","K","H","E","L","R"};
		for(n = 0; n < 2; ++n){
			for(l = 0; l < 6; ++l){
				for(m = 0; m < 6; ++m){
					for(k = 0;k < 10; ++k){
						//frh1 = "W" + p2_h1[k] + p3_h1[l] + p4_h1[m];
						frh1 = p1_h1[n] + p2_h1[k] + p3_h1[l] + p4_h1[m];
						
						if(querychain_first.find(frh1, 0) != std::string::npos){
							posh1_e = querychain_first.find(frh1, 0) - 1;
							h1found = 0;
							n = 2;
							l = 6;
							m = 6;
							k = 10;
						}
					}
				}
			}
		}
		
		if(h1found != 1){
			posh1_s  = querychain_first.find("C") + 4;
			lenh1    = posh1_e - posh1_s + 1;
			seqh1    = querychain_first.substr(posh1_s, lenh1);
			
			pos_fr1_s = 0;
			pos_fr1_e = posh1_s - 1;
			len_fr1   = pos_fr1_e - pos_fr1_s + 1;
			seq_fr1   = querychain_first.substr(pos_fr1_s,len_fr1);
		}
		/* H1 search Finish */
		
		/* H3 search Start */
		//string p1_h3[] = {"W","V"};
		//string p2_h3[] = {"G","A","C"};
		//string p3_h3[] = {"A","P","C","D","E","Q","N","R","K","H","W","Y","F","M","T","V","I","S","L","G"};
		//string p4_h3[] = {"G","R","D","Q","V"};
		for(m = 0;m < 3; ++m){
			for(l = 0;l < 5; ++l){
				for(k = 0;k < 20; ++k){
					//frh3 = "WG" + p3_h3[k] + p4_h3[l];
					frh3 = "W" + p2_h3[m] + p3_h3[k] + p4_h3[l];
					
					if(querychain2.find(frh3, 80) != std::string::npos){
						posh3_e = querychain2.find(frh3,80) - 1;
						h3found = 0;
						m = 3;
						l = 5;
						k = 20;
					}
				}
			}
		}
		
		if(querychain2.find("C", 80) != std::string::npos){
			posh3_s = querychain2.find("C", 80) + 3;
		}else{
			h3found = 1;
		}
		
		if(h3found != 1){
			lenh3 = posh3_e - posh3_s + 1;
			seqh3 = querychain2.substr(posh3_s,lenh3);
		}
		/* H3 search Finish */
		
		/* H2 search start */
		if(h1found != 1 && h3found != 1){
			posh2_s = posh1_e + 15;
			posh2_e = posh3_s - 33;
			lenh2   = posh2_e - posh2_s + 1;
			seqh2   = querychain.substr(posh2_s,lenh2);
			
			pos_fr2_s = posh1_e + 1;
			pos_fr2_e = posh2_s - 1;
			pos_fr3_s = posh2_e + 1;
			pos_fr3_e = posh3_s - 1;
			pos_fr4_s = posh3_e + 1;
			pos_fr4_e = pos_fr4_s + 5;
			len_fr2   = pos_fr2_e - pos_fr2_s + 1;
			len_fr3   = pos_fr3_e - pos_fr3_s + 1;
			len_fr4   = pos_fr4_e - pos_fr4_s + 1;
			seq_fr2   = querychain.substr(pos_fr2_s,len_fr2);
			seq_fr3   = querychain.substr(pos_fr3_s,len_fr3);
			seq_fr4   = querychain.substr(pos_fr4_s,len_fr4);
		}
		/* H2 search end */
	}
	
	if(l1found == 0 && l2found == 0 && l3found == 0){
		TR << lenl1 << "\t" << lenl2 << "\t" << lenl3 << "\t";
		TR << seql1 << "\t" << seql2 << "\t" << seql3 << "\t" << check << "\tLIGHT" << std::endl;
	}else if(h1found == 0 && h2found == 0 && h3found == 0){
		TR << lenh1 << "\t" << lenh2 << "\t" << lenh3 << "\t";
		TR << seqh1 << "\t" << seqh2 << "\t" << seqh3 << "\t" << check << "\tHEAVY" << std::endl;
	}else if(l1found == 0 && l2found == 0 && l3found == 0 && h1found == 0 && h2found == 0 && h3found == 0){
		TR << lenl1 << "\t" << lenl2 << "\t" << lenl3 << "\t";
		TR << lenh1 << "\t" << lenh2 << "\t" << lenh3 << "\t";
		TR << seql1 << "\t" << seql2 << "\t" << seql3 << "\t" << check << "\tLIGHT" << "\t";
		TR << seqh1 << "\t" << seqh2 << "\t" << seqh3 << "\t" << check << "\tHEAVY" << std::endl;
	}else{
		TR << "Some CDRs seem to be missing!\t" << querychain << "\t";
		TR << lenh1 << "\t" << lenh2 << "\t" << lenh3 << "\t";
		TR << "H1:" << seqh1 << "\tH2: " << seqh2 << "\tH3 " << seqh3 << "\t" << posh3_s << std::endl;
	}
	
	// return 0;
}

    


vector1<char> AntibodyInfo::get_CDR_Sequence_with_Stem(AntibodyCDRNameEnum const & cdr_name,
													   Size left_stem ,
													   Size right_stem ) const {
	vector1<char> sequence;
	loops::Loop the_loop = get_CDR_loop(cdr_name);
	/// JQX: the pose number in the loop should be consistent with the number in the ab_sequence_
	for (Size i=the_loop.start()-left_stem; i<=the_loop.stop()+right_stem; ++i){
		sequence.push_back( ab_sequence_[i]  );
	}
	return sequence;
}
    
    
vector1<std::string> const & AntibodyInfo::get_string_cdr_name(void)  {
	static vector1<std::string> *string_cdr_name = 0; /// JQX: this will only be executed once
	if(string_cdr_name==0){
		/// JQX: only the first time you can come here
		string_cdr_name=new vector1<std::string>;
		string_cdr_name->push_back("H1"); string_cdr_name->push_back("H2");string_cdr_name->push_back("H3");
		string_cdr_name->push_back("L1"); string_cdr_name->push_back("L2");string_cdr_name->push_back("L3");
	}
	return *string_cdr_name;
}

vector1<std::string> const & AntibodyInfo::get_string_h3_base_type(void)  {
	static vector1<std::string> *string_h3_base_type = 0; /// JQX: this will only be executed once
	if(string_h3_base_type==0){
		/// JQX: only the first time you can come here
		string_h3_base_type=new vector1<std::string>;
		string_h3_base_type->push_back("KINKED");
		string_h3_base_type->push_back("EXTENDED");
		string_h3_base_type->push_back("NEUTRAL");
	}
	return *string_h3_base_type;
}

vector1<std::string> const & AntibodyInfo::get_string_numbering_scheme(void)  {
	static vector1<std::string> *string_numbering_scheme = 0; /// JQX: this will only be executed once
	if(string_numbering_scheme==0){
		/// JQX: only the first time you can come here
		string_numbering_scheme=new vector1<std::string>;
		string_numbering_scheme->push_back("Aroop");
		string_numbering_scheme->push_back("Chothia");
		string_numbering_scheme->push_back("Kabat");
		string_numbering_scheme->push_back("Enhanced_Chothia");
		string_numbering_scheme->push_back("AHO");
		string_numbering_scheme->push_back("IMGT");
	}
	return *string_numbering_scheme;
}
	
	
scoring::ScoreFunctionCOP get_Pack_ScoreFxn(void){
	static scoring::ScoreFunctionOP pack_scorefxn = 0;
	if(pack_scorefxn == 0){
		pack_scorefxn = core::scoring::ScoreFunctionFactory::create_score_function("standard" );
	}
	return pack_scorefxn;
}
scoring::ScoreFunctionCOP get_Dock_ScoreFxn(void){
	static scoring::ScoreFunctionOP dock_scorefxn = 0;
	if(dock_scorefxn == 0){
		dock_scorefxn = core::scoring::ScoreFunctionFactory::create_score_function( "docking", "docking_min" );
        dock_scorefxn->set_weight( core::scoring::chainbreak, 1.0 );
        dock_scorefxn->set_weight( core::scoring::overlap_chainbreak, 10./3. );
	}
	return dock_scorefxn;
}
scoring::ScoreFunctionCOP get_LoopCentral_ScoreFxn(void){
	static scoring::ScoreFunctionOP loopcentral_scorefxn = 0;
	if(loopcentral_scorefxn == 0){
		loopcentral_scorefxn = core::scoring::ScoreFunctionFactory::create_score_function( "cen_std", "score4L" );
        loopcentral_scorefxn->set_weight( scoring::chainbreak, 10./3. );
	}
	return loopcentral_scorefxn;
}
scoring::ScoreFunctionCOP get_LoopHighRes_ScoreFxn(void){
	static scoring::ScoreFunctionOP loophighres_scorefxn = 0;
	if(loophighres_scorefxn == 0){
		loophighres_scorefxn = scoring::ScoreFunctionFactory::create_score_function("standard", "score12" );
        loophighres_scorefxn->set_weight( scoring::chainbreak, 1.0 );
        loophighres_scorefxn->set_weight( scoring::overlap_chainbreak, 10./3. );
        loophighres_scorefxn->set_weight(scoring::dihedral_constraint, 1.0);
	}
	return loophighres_scorefxn;
}
	
	
	
	
	
	
	
	
/// @details  Show the complete setup of the docking protocol
void AntibodyInfo::show( std::ostream & out ) {
	out << *this;
}

std::ostream & operator<<(std::ostream& out, const AntibodyInfo & ab_info )  {
	
	using namespace ObjexxFCL::fmt;
	std::string line_marker = "///";
	out << "////////////////////////////////////////////////////////////////////////////////" << std::endl;
	out << line_marker << A( 47, "Rosetta Antibody Info" ) << space( 27 ) << line_marker << std::endl;
	out << line_marker << space( 74 ) << line_marker << std::endl;
	
	out << line_marker << "             Antibody Type:";
	if(ab_info.is_Camelid()){ out << "  Camelid Antibody"<< std::endl;}
	else                    { out << "  Regular Antibody"<< std::endl;}
	
	out << line_marker << " Predict H3 Cterminus Base:";
	out <<"  "<<ab_info.get_string_h3_base_type()[ab_info.get_Predicted_H3BaseType()]<<std::endl;
	
	out << line_marker << space( 74 ) << std::endl;
	for (AntibodyCDRNameEnum i=start_cdr_loop; i<=ab_info.total_cdr_loops_; i=AntibodyCDRNameEnum(i+1) ){
		out << line_marker << " "+ab_info.get_CDR_Name(i)+" info: "<<std::endl;
		out << line_marker << "           length:  "<< ab_info.get_CDR_loop(i).length() <<std::endl;
		out << line_marker << "         sequence:  ";
		for (Size j=1;j<=ab_info.get_CDR_Sequence_with_Stem(i,0,0).size();++j) {
			out << ab_info.get_CDR_Sequence_with_Stem(i,0,0)[j] ;
		}
		out <<std::endl;
		out << line_marker << "        loop_info:  "<< ab_info.get_CDR_loop(i)<<std::endl;
	}
	out << "////////////////////////////////////////////////////////////////////////////////" << std::endl;
	return out;
}



} // namespace antibody2
} // namespace protocols
