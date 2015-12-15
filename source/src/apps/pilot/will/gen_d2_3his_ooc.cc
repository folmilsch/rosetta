// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file /src/apps/pilat/will/genmatch.cc
/// @brief ???

#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/smhybrid.OptionKeys.gen.hh>
#include <basic/options/keys/willmatch.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/options/util.hh>
#include <basic/Tracer.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/util.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/conformation/symmetry/SymDof.hh>
#include <core/conformation/symmetry/SymmData.hh>
#include <core/conformation/symmetry/SymmetricConformation.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/conformation/symmetry/util.hh>
#include <core/import_pose/import_pose.hh>
#include <devel/init.hh>
#include <core/io/pdb/pose_io.hh>
#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/kinematics/Stub.hh>
#include <core/graph/Graph.hh>
#include <core/pack/packer_neighbors.hh>
#include <core/pack/rotamer_set/RotamerSetFactory.hh>
#include <core/pack/rotamer_set/RotamerSet.hh>
#include <core/pack/optimizeH.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/Pose.hh>
#include <core/pose/symmetry/util.hh>
#include <core/pose/util.hh>
#include <core/scoring/constraints/AngleConstraint.hh>
#include <core/scoring/constraints/AtomPairConstraint.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/DihedralConstraint.hh>
#include <core/scoring/func/HarmonicFunc.hh>
#include <core/scoring/constraints/MultiConstraint.hh>
#include <core/scoring/constraints/util.hh>
#include <core/scoring/func/XYZ_Func.hh>
#include <core/scoring/dssp/Dssp.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/sasa.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/symmetry/SymmetricScoreFunction.hh>
#include <numeric/model_quality/rms.hh>
#include <numeric/random/random.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
#include <protocols/simple_moves/FragmentMover.hh>
#include <numeric/kinematic_closure/bridgeObjects.hh>
#include <numeric/kinematic_closure/kinematic_closure_helpers.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/simple_moves/symmetry/SymMinMover.hh>
#include <protocols/simple_moves/symmetry/SymPackRotamersMover.hh>
#include <protocols/moves/TrialMover.hh>
#include <protocols/scoring/ImplicitFastClashCheck.hh>
#include <sstream>
#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>

#include <apps/pilot/will/will_util.ihh>
#include <apps/pilot/will/mynamespaces.ihh>

using core::kinematics::Stub;
using protocols::scoring::ImplicitFastClashCheck;
using core::Real;
using core::Size;
using core::pose::Pose;
using core::kinematics::Stub;
using core::conformation::Residue;
using protocols::scoring::ImplicitFastClashCheck;
using std::string;
using utility::vector1;
using numeric::min;
using core::import_pose::pose_from_pdb;
using basic::options::option;
using numeric::min;
using numeric::max;
using utility::io::ozstream;
using ObjexxFCL::format::I;
using ObjexxFCL::format::F;

typedef utility::vector1<core::Real> Reals;
typedef utility::vector1<core::Size> Sizes;
typedef numeric::xyzVector<Real> Vec;
typedef numeric::xyzMatrix<Real> Mat;
typedef utility::vector1<Vec> Vecs;

static THREAD_LOCAL basic::Tracer TR( "gen_d2" );


inline Real const sqr(Real const r) { return r*r; }
inline Real sigmoidish_neighbor( Real const & sqdist ) {
  if( sqdist > 9.*9. ) {
    return 0.0;
  } else if( sqdist<6.*6. ) {
    return 1.0;
  } else {
    Real dist=sqrt( sqdist );
    return sqr(1.0  - sqr( (dist - 6.) / (9. - 6.) ) );
  }
}


vector1<Size> read_res_list(string fn) {
  vector1<Size> l;
  if(fn=="") return l;
  if(fn=="_") return l;
  if(fn.size()==1 && fn[0]==(char)0) return l;
  izstream in(fn);
  if(!in.good()) {
    utility_exit_with_message("can't open res list file '"+fn+"'");
  }
  Size r;
  while( in >> r ) l.push_back(r);
  return l;
}

void repack(Pose & pose, Size nres, ScoreFunctionOP sf) {
  using namespace core::pack::task;
  PackerTaskOP task=TaskFactory::create_packer_task(pose);
  task->initialize_extra_rotamer_flags_from_command_line();
  for(Size i=1; i<=nres; ++i) {
    if(pose.residue(i).name3()=="BPY") {
      task->nonconst_residue_task(i).prevent_repacking();
    } else {
      task->nonconst_residue_task(i).restrict_to_repacking();
    }
  }
  // TR << *task << std::endl;
  protocols::simple_moves::symmetry::SymPackRotamersMover repack( sf, task );
  repack.apply(pose);
}

void design(Pose & pose, Size nres, ScoreFunctionOP sf) {
  core::id::AtomID_Map< bool > atom_map;
  core::pose::initialize_atomid_map( atom_map, pose, false );
  for ( Size ir=1; ir<=pose.total_residue(); ++ir ) {
    atom_map.set(AtomID(2,ir) , true );
    atom_map.set(AtomID(3,ir) , true );
    atom_map.set(AtomID(5,ir) , true );
  }
  core::id::AtomID_Map<Real> atom_sasa; utility::vector1<Real> sasa;
  core::scoring::calc_per_atom_sasa( pose, atom_sasa, sasa, 2.3, false, atom_map );
  for(Size i=1; i<=sasa.size(); ++i) if( atom_sasa.n_atom(i) > 4 ) sasa[i]=atom_sasa[AtomID(5,i)];

  using namespace core::pack::task;
  PackerTaskOP task=TaskFactory::create_packer_task(pose);
  vector1< bool > aas(20,true);
  aas[core::chemical::aa_cys]=false;
  aas[core::chemical::aa_his]=false;
  // aas[core::chemical::aa_met]=false;
  aas[core::chemical::aa_pro]=false;
  aas[core::chemical::aa_gly]=false;
  aas[core::chemical::aa_gly]=false;
  if(option[basic::options::OptionKeys::willmatch::exclude_ala]()) aas[core::chemical::aa_ala]=false;
  if(option[basic::options::OptionKeys::smhybrid::design_hydrophobic]()) {
    aas[core::chemical::aa_ser]=false;
    aas[core::chemical::aa_thr]=false;
    aas[core::chemical::aa_asp]=false;
    aas[core::chemical::aa_glu]=false;
    aas[core::chemical::aa_lys]=false;
    aas[core::chemical::aa_arg]=false;
    aas[core::chemical::aa_asn]=false;
    aas[core::chemical::aa_gln]=false;
  }

  vector1<Size> fixed;
  if(option[basic::options::OptionKeys::willmatch::fixed_res].user()) {
    utility::io::izstream in(option[basic::options::OptionKeys::willmatch::fixed_res]());
    Size tmp;
    while(in>>tmp) fixed.push_back(tmp);
    in.close();
  }

  vector1<Size> interface;
  if(option[basic::options::OptionKeys::willmatch::design_interface]()) {
    for(Size i=1; i<=nres; ++i) {
      if( sasa.size() >= i && sasa[i] > 15.0 ) continue;
      AtomID aid(5,i);
      if(pose.residue(i).nheavyatoms()<5) aid.atomno()=2;
      for(Size j=nres+1; j<=3*nres; ++j) {
        AtomID aid2(5,j);
        if(pose.residue(j).nheavyatoms()<5) aid.atomno()=2;
        if(pose.xyz(aid).distance_squared(pose.xyz(aid2))<49) {
          interface.push_back(i);
        }
      }
    }
    for(Size i=1; i<=nres; ++i) {
      Vec xyz=pose.xyz(AtomID(5,i));
      xyz.z()=0;
      if( xyz.length()<8.0 ) interface.push_back(i);
    }
  }
  for(Size i=1; i<=nres; ++i) {
    if(pose.residue(i).name3()=="BPY") {
      task->nonconst_residue_task(i).prevent_repacking();
      // std::exit(-1);
    } else if(std::find(fixed.begin(),fixed.end(),i)!=fixed.end()){
      task->nonconst_residue_task(i).restrict_to_repacking();
      task->nonconst_residue_task(i).or_ex1_sample_level( core::pack::task::EX_ONE_STDDEV );
      task->nonconst_residue_task(i).or_ex2_sample_level( core::pack::task::EX_ONE_STDDEV );
    } else if(std::find(interface.begin(),interface.end(),i)!=interface.end()){
      task->nonconst_residue_task(i).restrict_absent_canonical_aas(aas);
      task->nonconst_residue_task(i).initialize_extra_rotamer_flags_from_command_line();
    } else {
      task->nonconst_residue_task(i).restrict_to_repacking();
      task->nonconst_residue_task(i).or_ex1_sample_level( core::pack::task::EX_ONE_STDDEV );
      task->nonconst_residue_task(i).or_ex2_sample_level( core::pack::task::EX_ONE_STDDEV );
    }
  }
  for(Size i=nres+1; i<=pose.n_residue(); ++i) {
    task->nonconst_residue_task(i).prevent_repacking();
  }
  TR << *task << std::endl;

  protocols::simple_moves::symmetry::SymPackRotamersMover repack( sf, task );
  repack.apply(pose);
}


void minimize(Pose & pose, Size nres, Size , ScoreFunctionOP sf, int bb=0) {
  core::kinematics::MoveMapOP movemap=new core::kinematics::MoveMap;
  // core::pose::symmetry::make_symmetric_movemap(pose,*movemap);
  movemap->set_chi(true);
  movemap->set_bb(false);
  movemap->set_jump(false);
  if(bb==1) for(Size i=1; i<=nres; ++i) if(pose.secstruct(i)=='L') movemap->set_bb(i,true);
  if(bb>=2) for(Size i=1; i<=nres; ++i) movemap->set_bb(i,true);
  // movemap->set_chi(bpyres,false);

  core::pose::symmetry::make_symmetric_movemap( pose, *movemap );

  protocols::simple_moves::symmetry::SymMinMover m( movemap, sf, "dfpmin_armijo_nonmonotone", 1e-5, true, false, false );

  m.apply(pose);

}

struct Hit : public utility::pointer::ReferenceCount {
  Size ihis,jhis,khis,lhis;
};
typedef utility::pointer::owning_ptr<Hit> HitOP;


vector1<Reals> vecs2vv(Vecs const & v) {
  vector1<Reals> vv;
  for(Vecs::const_iterator i = v.begin(); i != v.end(); ++i) {
    Reals r(3);
    r[1] = i->x();
    r[2] = i->y();
    r[3] = i->z();
    vv.push_back(r);
  }
  return vv;
}
vector1<Vec> vv2vecs(vector1<Reals> const & vv) {
  vector1<Vec> r;
  for(vector1<Reals>::const_iterator i = vv.begin(); i != vv.end(); ++i) {
    r.push_back(Vec( (*i)[1], (*i)[2], (*i)[3] ));
  }
  return r;
}

core::pack::rotamer_set::RotamerSetOP get_rotset(Pose & pose, Size ir, ScoreFunctionOP sf, ImplicitFastClashCheck const & ifc,  vector1<bool> & clash ) {
  core::pack::rotamer_set::RotamerSetOP rotset;
		core::scoring::ScoreFunction & sfxn(*sf);
  sfxn( pose );
  core::pack::task::PackerTaskOP task = core::pack::task::TaskFactory::create_packer_task( pose );
  task->initialize_from_command_line();
  task->nonconst_residue_task( ir ).and_extrachi_cutoff(0);
  task->nonconst_residue_task( ir ).restrict_to_repacking();
  task->nonconst_residue_task( ir ).or_include_current( false ); //need to do this because the residue was built from internal coords and is probably crumpled up
  task->nonconst_residue_task( ir ).or_fix_his_tautomer( true ); //since we only want rotamers for the specified restype
  core::graph::GraphOP png = core::pack::create_packer_graph( pose, sfxn, task );
  core::pack::rotamer_set::RotamerSetFactory rsf;
  rotset = rsf.create_rotamer_set( pose.residue( ir ) );
  rotset->set_resid( ir );
  rotset->build_rotamers( pose, sfxn, *task, png );

		clash.resize( rotset->num_rotamers(), false );
		for(Size irt = 1; irt <= rotset->num_rotamers(); ++irt) {
				pose.set_chi(1,ir,rotset->rotamer(irt)->chi(1));
				pose.set_chi(2,ir,rotset->rotamer(irt)->chi(2));
				for(Size i = 6; i <= pose.residue(ir).nheavyatoms(); ++i) if(! ifc.clash_check( pose.xyz(AtomID(i,ir)), ir )) clash[irt] = true;
				// if(ir==24&&irt==14) {
				// 		pose.dump_pdb("test.pdb");
				// 		utility_exit_with_message("get_rotset " + str( clash[irt] ));
				// }
		}

  return rotset;
}


double
isctfast(
         vector1<Vec>  pa,
         vector1<Vec>  pb,
         Vec ori,
         bool debug = false
         ) {
  double BIN = 2.0;

  // get points, rotated ro ori is 0,0,1, might already be done
  Mat rot = Mat::identity();
  if     ( ori.dot(Vec(0,0,1)) < -0.99999 ) rot = rotation_matrix( Vec(1,0,0).cross(ori), -acos(Vec(0,0,1).dot(ori)) );
  else if( ori.dot(Vec(0,0,1)) <  0.99999 ) rot = rotation_matrix( Vec(0,0,1).cross(ori), -acos(Vec(0,0,1).dot(ori)) );
  if( rot != Mat::identity() ) {
    for(vector1<Vec>::iterator ia = pa.begin(); ia != pa.end(); ++ia) *ia = rot*(*ia);
    for(vector1<Vec>::iterator ib = pb.begin(); ib != pb.end(); ++ib) *ib = rot*(*ib);
  }

  // get bounds for plane hashes
  double xmx1=-9e9,xmn1=9e9,ymx1=-9e9,ymn1=9e9,xmx=-9e9,xmn=9e9,ymx=-9e9,ymn=9e9;
  for(vector1<Vec>::const_iterator ia = pa.begin(); ia != pa.end(); ++ia) {
    xmx1 = max(xmx1,ia->x()); xmn1 = min(xmn1,ia->x());
    ymx1 = max(ymx1,ia->y()); ymn1 = min(ymn1,ia->y());
  }
  for(vector1<Vec>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
    xmx = max(xmx,ib->x()); xmn = min(xmn,ib->x());
    ymx = max(ymx,ib->y()); ymn = min(ymn,ib->y());
  }
  xmx = min(xmx,xmx1); xmn = max(xmn,xmn1);
  ymx = min(ymx,ymx1); ymn = max(ymn,ymn1);


  int xlb = floor(xmn/BIN)-2; int xub = ceil(xmx/BIN)+2; // one extra on each side for correctness,
  int ylb = floor(ymn/BIN)-2; int yub = ceil(ymx/BIN)+2; // and one extra for outside atoms

  // TR << "BOUNDS " << xmn << " " << xmx << " " << ymn << " " << ymx << std::endl;
  // TR << "BOUNDS " << xlb << " " << xub << " " << ylb << " " << yub << std::endl;

  // insert points into hashes
  int const xsize = xub-xlb+1;
  int const ysize = yub-ylb+1;
  ObjexxFCL::FArray2D<Vec> ha(xsize,ysize,Vec(0,0,-9e9)),hb(xsize,ysize,Vec(0,0,9e9));
  for(vector1<Vec>::const_iterator ia = pa.begin(); ia != pa.end(); ++ia) {
    // int const ix = min(xsize,max(1,(int)ceil(ia->x()/BIN)-xlb));
    // int const iy = min(ysize,max(1,(int)ceil(ia->y()/BIN)-ylb));
    int const ix = (int)ceil(ia->x()/BIN)-xlb;
    int const iy = (int)ceil(ia->y()/BIN)-ylb;
    if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
    if( ha(ix,iy).z() < ia->z() ) ha(ix,iy) = *ia;
  }
  for(vector1<Vec>::const_iterator ib = pb.begin(); ib != pb.end(); ++ib) {
    // int const ix = min(xsize,max(1,(int)ceil(ib->x()/BIN)-xlb));
    // int const iy = min(ysize,max(1,(int)ceil(ib->y()/BIN)-ylb));
    int const ix = (int)ceil(ib->x()/BIN)-xlb;
    int const iy = (int)ceil(ib->y()/BIN)-ylb;
    if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
    if( hb(ix,iy).z() > ib->z() ) hb(ix,iy) = *ib;
  }

  // check hashes for min dis
  int imna=0,jmna=0,imnb=0,jmnb=0;
  double mindis = 9e9;
  for(int i = 1; i <= xsize; ++i) { // skip 1 and N because they contain outside atoms (faster than clashcheck?)
    for(int j = 1; j <= ysize; ++j) {
      for(int k = -2; k <= 2; ++k) {
        if(i+k < 1 || i+k > xsize) continue;
        for(int l = -2; l <= 2; ++l) {
          if(j+l < 1 || j+l > ysize) continue;
          double const xa = ha(i  ,j  ).x();
          double const ya = ha(i  ,j  ).y();
          double const xb = hb(i+k,j+l).x();
          double const yb = hb(i+k,j+l).y();
          double const d2 = (xa-xb)*(xa-xb) + (ya-yb)*(ya-yb);

          if( d2 < 16.0 ) {
            double dz = hb(i+k,j+l).z() - ha(i,j).z() - sqrt(16.0-d2);
            if( dz < mindis ) {
              mindis = dz;
              imna = i;
              jmna = j;
              imnb = i+k;
              jmnb = j+l;
            }
          }
        }
      }
    }
  }

  rot = rot.transposed();
  if(debug){
    {
      utility::io::ozstream out("hasha.pdb");
      for(int i = 2; i <= xsize-1; ++i) { // skip 1 and N because they contain outside atoms (faster than clashcheck?)
        for(int j = 2; j <= ysize-1; ++j) {
          Vec viz = rot*ha(i,j) + mindis*ori;
          if(viz.z() < -9e8 || 9e8 < viz.z()) continue;
          out<<"HETATM"<<I(5,1000+i)<<' '<<"VIZ "<<' ' << "VIZ"<<' '<<"B"<<I(4,100+j)<<"    "<<F(8,3,viz.x())<<F(8,3,viz.y())<<F(8,3,viz.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
        }
      }
      Vec viz = rot*ha(imna,jmna) + mindis*ori;
      out<<"HETATM"<<I(5,1000+imna)<<' '<<"MIN "<<' ' <<  "MIN"<<' '<<"B"<<I(4,100+jmna)<<"    "<<F(8,3,viz.x())<<F(8,3,viz.y())<<F(8,3,viz.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
      out.close();
    }
    {
      utility::io::ozstream out("hashb.pdb");
      for(int i = 2; i <= xsize-1; ++i) { // skip 1 and N because they contain outside atoms (faster than clashcheck?)
        for(int j = 2; j <= ysize-1; ++j) {
          Vec viz = rot*hb(i,j);
          if(viz.z() < -9e8 || 9e8 < viz.z()) continue;
          out<<"HETATM"<<I(5,1000+i)<<' '<<"VIZ "<<' ' << "VIZ"<<' '<<"C"<<I(4,100+j)<<"    "<<F(8,3,viz.x())<<F(8,3,viz.y())<<F(8,3,viz.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
        }
      }
      Vec viz = rot*hb(imnb,jmnb);
      out<<"HETATM"<<I(5,1000+imnb)<<' '<<"MIN "<<' ' <<  "MIN"<<' '<<"C"<<I(4,100+jmnb)<<"    "<<F(8,3,viz.x())<<F(8,3,viz.y())<<F(8,3,viz.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
      out.close();
    }
  }

  return mindis;
}

double isctfast(
                Pose const & a,
                Pose const & b,
                Vec ori_in
                ) {
  // get points, rotated ro ori is 0,0,1
  vector1<Vec> pa,pb;
  Vec ori = ori_in.normalized();
  Mat rot = Mat::identity();
  if     ( ori.dot(Vec(0,0,1)) < -0.999 ) rot = rotation_matrix( Vec(1,0,0).cross(ori), -acos(Vec(0,0,1).dot(ori)) );
  else if( ori.dot(Vec(0,0,1)) <  0.999 ) rot = rotation_matrix( Vec(0,0,1).cross(ori), -acos(Vec(0,0,1).dot(ori)) );
  for(int i = 1; i <= (int)a.n_residue(); ++i) {
    int const natom = (a.residue(i).name3()=="GLY") ? 4 : 5;
    for(int j = 1; j <= natom; ++j) pa.push_back(rot*Vec(a.residue(i).xyz(j)));
  }
  for(int i = 1; i <= (int)b.n_residue(); ++i) {
    int const natom = (b.residue(i).name3()=="GLY") ? 4 : 5;
    for(int j = 1; j <= natom; ++j) pb.push_back(rot*Vec(b.residue(i).xyz(j)));
  }
  return isctfast( pa, pb, Vec(0,0,1) );
}


core::kinematics::Stub res2stub(core::pose::Pose const & pose, Size rsd, core::kinematics::Stub ref = core::kinematics::Stub()) {
  Vec const p1 = ref.local2global( pose.xyz(AtomID(1,rsd)) );
  Vec const p2 = ref.local2global( pose.xyz(AtomID(2,rsd)) );
  Vec const p3 = ref.local2global( pose.xyz(AtomID(3,rsd)) );
  return core::kinematics::Stub( p1, p2, p3 );
}


Real ik_his_clamp(Pose & pose,
																		Size rsd1, Size rsd2,
																		ImplicitFastClashCheck & ifc,
																		vector1<HitOP> & hits,
																		vector1<core::pack::rotamer_set::RotamerSetOP> const & rots1,
																		vector1<core::pack::rotamer_set::RotamerSetOP> const & rots2,
																		vector1<vector1<bool> > const & rotc1,	vector1<vector1<bool> > const & rotc2,
																		std::string const & tag
																		) {

		Real MINMDIS = 9.0;

  using namespace basic::options::OptionKeys;
  using namespace core::id;
  if( pose.residue(rsd1).xyz("CB").distance_squared(pose.residue(rsd2).xyz("CB")) > 144.0 ) return 0;
  Real mxcb = 0.0;
  Real his_rep_th = 5.0;

  Pose his,asp;
  make_pose_from_sequence(his,"H",core::chemical::FA_STANDARD,false);
  make_pose_from_sequence(asp,"D",core::chemical::FA_STANDARD,false);
  if(asp.residue(1).is_lower_terminus()) remove_lower_terminus_type_from_pose_residue(asp,1);
  if(asp.residue(1).is_upper_terminus()) remove_upper_terminus_type_from_pose_residue(asp,1);
  if(his.residue(1).is_lower_terminus()) remove_lower_terminus_type_from_pose_residue(his,1);
  if(his.residue(1).is_upper_terminus()) remove_upper_terminus_type_from_pose_residue(his,1);
		his.set_dof(core::id::DOF_ID(AtomID(his.residue(1).atom_index("HE2"),1),D ),2.1);

		Residue rtmp1(pose.residue(rsd1));	pose.replace_residue(rsd1,his.residue(1),true);
		Residue rtmp2(pose.residue(rsd2));	pose.replace_residue(rsd2,his.residue(1),true);
		Pose tmp1(pose),tmp2(pose),tmp3(pose);

  utility::vector1<Size> pivots (3), order (3);
  utility::vector1<utility::vector1<Real> > t_ang, b_ang, b_len;
  int nsol=0;
  utility::vector1<utility::vector1<Real> > Q0 (3);
  utility::vector1<Real> dt_ang, db_len, db_ang, R0 (3);

  TR << rsd1 << " " << rsd2 << std::endl;

  // N CA C N CA CB CG CD NE
  Vecs atoms(15);
  atoms[ 1] = tmp1.residue(rsd1-1).xyz( "CA" );
  atoms[ 2] = tmp1.residue(rsd1-1).xyz( "C"  );
  atoms[ 3] = tmp1.residue(rsd1  ).xyz( "N"  );
  atoms[ 4] = tmp1.residue(rsd1  ).xyz( "CA" );
  atoms[ 5] = tmp1.residue(rsd1  ).xyz( "CB" );
  atoms[ 6] = tmp1.residue(rsd1  ).xyz( "CG" );
  atoms[ 7] = tmp1.residue(rsd1  ).xyz( "NE2");
  atoms[ 8] = Vec(990,990,990);
  atoms[ 9] = tmp1.residue(rsd2  ).xyz( "NE2");
  atoms[10] = tmp1.residue(rsd2  ).xyz( "CG" );
  atoms[11] = tmp1.residue(rsd2  ).xyz( "CB" );
  atoms[12] = tmp1.residue(rsd2  ).xyz( "CA" );
  atoms[13] = tmp1.residue(rsd2  ).xyz( "N"  );
  atoms[14] = tmp1.residue(rsd2-1).xyz( "C"  );
  atoms[15] = tmp1.residue(rsd2-1).xyz( "CA" );

  order [1]=1; order [2]=2; order [3]=3;
  pivots[1]=5, pivots[2]=8, pivots[3]=11;

  using namespace numeric::kinematic_closure;
  chainTORS(atoms.size(), vecs2vv(atoms), dt_ang, db_ang, db_len, R0, Q0);

  db_ang[ 7] = 161.2;
  db_ang[ 9] = 161.2;
  dt_ang[ 6] = 0.0;
  dt_ang[ 9] = 0.0;

  Vec com(0.0,0.0,0.0); for(Size i = 1; i <= tmp1.n_residue(); ++i) com += tmp1.xyz(AtomID(2,i)); com /= Real(tmp1.n_residue());

  for(Size blen1 = 20; blen1 < 24; ++blen1) {
    db_len[ 7] = Real(blen1)/10.0;
    tmp1.set_dof(core::id::DOF_ID(AtomID(tmp1.residue(rsd1).atom_index("HE2"),rsd1),D ),Real(blen1)/10.0);
    for(Size blen2 = 20; blen2 < 24; ++blen2) {
      db_len[ 8] = Real(blen2)/10.0;
      tmp1.set_dof(core::id::DOF_ID(AtomID(tmp1.residue(rsd2).atom_index("HE2"),rsd2),D ),Real(blen1)/10.0);
      for(Size bang = 97; bang < 122; bang+=3) {
        db_ang[ 8] = (Real)bang;
        bridgeObjects(vecs2vv(atoms), dt_ang, db_ang, db_len, pivots, order, t_ang, b_ang, b_len, nsol);
        for(int isol = 1; isol <= nsol; isol++) {
          utility::vector1<utility::vector1<core::Real> > vv_atm_out;
          numeric::kinematic_closure::chainXYZ(atoms.size(),b_len[isol],b_ang[isol],t_ang[isol],false,R0,Q0,vv_atm_out);
          Vecs apos = vv2vecs(vv_atm_out);
          bool clash = false;
          for( Size i = 1; i <= atoms.size(); ++i ) {
            for( Size j = i+3; j <= atoms.size(); ++j ) {
              if( apos[i].distance_squared(apos[j]) < 6.0 ) { clash=true; break; }
            } if(clash) break;
          } if(clash) continue;

          tmp1.set_chi(1,rsd1, t_ang[isol][ 4]);
          tmp1.set_chi(1,rsd2, t_ang[isol][11]);
          Real dlta1 = t_ang[isol][ 5] - dihedral_degrees(tmp1.residue(rsd1).xyz(2),tmp1.residue(rsd1).xyz(5),tmp1.residue(rsd1).xyz(6),tmp1.residue(rsd1).xyz(10));
          Real dlta2 = t_ang[isol][10] - dihedral_degrees(tmp1.residue(rsd2).xyz(2),tmp1.residue(rsd2).xyz(5),tmp1.residue(rsd2).xyz(6),tmp1.residue(rsd2).xyz(10));
          tmp1.set_chi(2,rsd1, tmp1.chi(2,rsd1)+dlta1 );
          tmp1.set_chi(2,rsd2, tmp1.chi(2,rsd2)+dlta2 );

          for(Size i = 6; i <= tmp1.residue(rsd1).nheavyatoms(); ++i) if(! ifc.clash_check( tmp1.xyz(AtomID(i,rsd1)), rsd1 ) ) { clash=true; break; }
          for(Size i = 6; i <= tmp1.residue(rsd2).nheavyatoms(); ++i) if(! ifc.clash_check( tmp1.xyz(AtomID(i,rsd2)), rsd2 ) ) { clash=true; break; }
          if(clash){ continue; }

          Vec ZN = (tmp1.residue(rsd1).xyz("HE2")+tmp1.residue(rsd1).xyz("HE2"))/2.0;
          Pose his3,his4;
          his3.append_residue_by_jump(tmp1.residue(rsd1),1);
          his4.append_residue_by_jump(tmp1.residue(rsd1),1);
          Vec dir = (2*ZN - tmp1.residue(rsd1).xyz("NE2") - tmp1.residue(rsd2).xyz("NE2")).normalized();
          Vec ori = (ZN - tmp1.residue(rsd1).xyz("NE2")).cross(ZN-tmp1.residue(rsd2).xyz("NE2")).normalized();
          Vec his3axs0 = rotation_matrix_degrees(ori,-109.5/2.0)*dir;
										//#ifdef USE_OPENMP
										//#pragma omp parallel for schedule(dynamic,1)
										//#endif
          for(Size rsd3 = 1; rsd3 <= tmp1.n_residue(); ++rsd3) {
												if( std::abs((int)rsd1-(int)rsd3) < 1 ) continue;
												if( std::abs((int)rsd2-(int)rsd3) < 1 ) continue;

												Residue rtmp3(tmp1.residue(rsd3));
												tmp1.replace_residue(rsd3,his.residue(1),true);
            tmp1.set_dof(core::id::DOF_ID(AtomID(tmp1.residue(rsd3).atom_index("HE2"),rsd3),D ),2.1);
            //core::pack::rotamer_set::RotamerSetOP rotset = get_rotset(tmp1,rsd3);
												for(Size imer = 1; imer < 2; ++imer) {
              for(Size irt3 = 1; irt3 <= rots1[rsd3]->num_rotamers(); ++irt3) {
																if( rotc1[rsd3][irt3] ) continue;
                tmp1.set_chi(1,rsd3,rots1[rsd3]->rotamer(irt3)->chi(1));
                tmp1.set_chi(2,rsd3,rots1[rsd3]->rotamer(irt3)->chi(2));

                Vec his3axs = rotation_matrix_degrees(dir,imer?-90.0:90.0)*his3axs0;
                his3.replace_residue(1,tmp1.residue(rsd3),false);
																{
                  Vec his3axs2 = (his3.xyz(AtomID(10,1))-his3.residue(1).xyz("HE2")).normalized();
                  Vec irt3 = his3axs.cross(his3axs2);
                  rot_pose( his3, irt3, -dihedral_degrees( his3axs, Vec(0,0,0), irt3, his3axs2 ) );
                  trans_pose(his3, ZN - his3.residue(1).xyz("HE2"));
                }

																clash = false;
																for(Size ia = 1; ia <= his3.residue(1).nheavyatoms(); ++ia) {
																		if(!ifc.clash_check(his3.residue(1).xyz(ia))) { clash=true; break; }
																} if(clash) continue;

                Vec symcen1 = (tmp1.residue(rsd1).xyz("HE2")+tmp1.residue(rsd3).xyz("HE2"))/2.0;
                Vec symaxs1 = (tmp1.residue(rsd3).xyz("NE2")+his3.residue(1).xyz("NE2"))/2.0 - symcen1;
                symaxs1 = projperp(tmp1.residue(rsd1).xyz("HE2")-tmp1.residue(rsd3).xyz("HE2"),symaxs1).normalized();
                if( symaxs1.dot( projection_matrix(symaxs1)*(symcen1-com) ) < 0 ) symaxs1 *= -1.0;
                Mat Rsym1 = rotation_matrix_degrees(symaxs1,180.0);

																if( projperp(symaxs1,ZN-symcen1).length() < MINMDIS/2.0 ) continue;

                if( fabs(angle_degrees(his3axs,Vec(0,0,0), Rsym1*(tmp1.residue(rsd3).xyz("NE2")-symcen1)+symcen1-ZN)) > 25.0 ) continue;
																// other checks?

																// // clash p1r3
																// clash=false;
																// tmp1.replace_residue(rsd3,his.residue(1),true);
																// tmp1.set_chi(1,rsd3,rots1[rsd3]->rotamer(irt3)->chi(1));
																// tmp1.set_chi(2,rsd3,rots1[rsd3]->rotamer(irt3)->chi(2));
																// for(Size i = 6; i <= tmp1.residue(rsd3).nheavyatoms(); ++i) if(! ifc.clash_check( tmp1.xyz(AtomID(i,rsd3)), rsd3 ) ) { clash=true; break; }
																// if(clash){
																// 		tmp1.dump_pdb("test.pdb");
																// 		utility_exit_with_message( str(rsd3) +" "+ str(irt3) );
																// 		continue;
																// }

                tmp2 = tmp1;
                rot_pose(tmp2,symaxs1,180.0,symcen1);

                clash=false;
                for(Size i = 1; i <= tmp2.n_residue(); ++i) {
                  for(Size j = 1; j <= 5; ++j) {
                    if(!ifc.clash_check(tmp2.xyz(AtomID(j,i)))) clash=true;
                  } if(clash) break;
                } if(clash) continue;
                //if( isctfast(tmp1,tmp2,Vec(1,0,0)) < -0.5 && isctfast(tmp1,tmp2,Vec(-1,0,0)) < -0.5 ) continue;

               Vec his4axs = rotation_matrix_degrees(dir,imer?90.0:-90.0)*his3axs0;
                {
                  Vec his4axs2 = (his4.xyz(AtomID(10,1))-his4.residue(1).xyz("HE2")).normalized();
                  Vec irt3 = his4axs.cross(his4axs2);
                  rot_pose( his4, irt3, -dihedral_degrees( his4axs, Vec(0,0,0), irt3, his4axs2 ) );
                  trans_pose(his4, ZN - his4.residue(1).xyz("HE2"));
                }
                Real Dzn = projperp(symaxs1,ZN-symcen1).length();
																Vec symaxs2_0 = symaxs1.cross(ZN-symcen1);
                Mat Rsym2 = rotation_matrix_degrees( symaxs2_0,180.0);// cross could be anything
                Mat Psym1 = projection_matrix(symaxs1);

																tmp3 = tmp1;
                rot_pose(tmp3,Rsym2,symcen1+3.0*symaxs1);
                //tmp3.dump_pdb("test3.pdb");

                for(Size rsd4 = 1; rsd4 <= tmp1.n_residue(); ++rsd4) {
																		if( std::abs((int)rsd1-(int)rsd4) < 1 ) continue;
																		if( std::abs((int)rsd2-(int)rsd4) < 1 ) continue;
																		if( std::abs((int)rsd3-(int)rsd4) < 1 ) continue;
                  //ik_his4(tmp1,his4,rsd1,rsd2,rsd3,rsd4,symcen1,symaxs1,ZN);

																		Residue rtmp4(tmp3.residue(rsd4));
																		tmp3.replace_residue(rsd4,asp.residue(1),true);
																		tmp2.replace_residue(rsd4,asp.residue(1),true);
																		//for(Size i = 1; i <= tmp3.residue(rsd4).natoms(); ++i) TR << i << " " << tmp3.residue(rsd4).atom_name(i) << std::endl;
																		//tmp3.set_dof(core::id::DOF_ID(AtomID(tmp3.residue(rsd4).atom_index("HG"),rsd4),D ),2.4);

                  Vec const CB = tmp1.xyz(AtomID(5,rsd4));
                  //if( fabs(projperp(symaxs1,CB-symcen1).length() - Dzn) > 5.0 ) continue;

                  for(Size irt4 = 1; irt4 <= rots2[rsd4]->num_rotamers(); ++irt4) {
																				if( rotc2[rsd4][irt4] ) continue;
																				for(Size od1 = 0; od1 < 2; od1++) {
																						for(Size hbo = 0; hbo < 2; hbo++) {

																						tmp3.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						tmp3.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));
																						//his4.replace_residue(1,tmp1.residue(rsd4),false);
																						Vec const CB = tmp3.residue(rsd4).xyz("CB");
																						Vec const CG = tmp3.residue(rsd4).xyz("CG");
																						Vec const O1 = tmp3.residue(rsd4).xyz(od1?"OD1":"OD2");
																						Vec const O2 = tmp3.residue(rsd4).xyz(od1?"OD2":"OD1");
																						Vec h4m = O1 + 2.4 * ( hbo ?  (CG-CB).normalized() : (O1-O2).normalized() );
																						if( fabs(((h4m-symcen1) - Psym1*(h4m-symcen1)).length() - Dzn) > 0.8 ) { /*TR << "adis fail" << std::endl;*/ continue; }
																						Real h4sa1 = dihedral_degrees(ZN-symcen1,symaxs1,Vec(0,0,0),h4m-symcen1);
																						Vec h4n = rotation_matrix_degrees(symaxs1,h4sa1)*(O1-symcen1)+symcen1;
																						Vec h4axs = (h4n-ZN).normalized();
																						if( his4axs.dot(h4axs) < 0.90 ) { /*TR << "aang fail" << std::endl;*/ continue; }

																						clash = false;
																						for(Size ia = 1; ia <= tmp3.residue(rsd3).nheavyatoms(); ++ia) {
																								if(!ifc.clash_check(tmp3.residue(rsd3).xyz(ia))) { clash=true; break; }
																						} if(clash) continue;
																						clash = false;
																						for(Size ia = 1; ia <= tmp3.residue(rsd4).nheavyatoms(); ++ia) {
																								if(!ifc.clash_check(tmp3.residue(rsd4).xyz(ia))) { clash=true; break; }
																						} if(clash) continue;

																						tmp2.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						tmp2.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));
																						for(Size ia = 1; ia <= tmp2.residue(rsd4).nheavyatoms(); ++ia) {
																								if(!ifc.clash_check(tmp2.residue(rsd4).xyz(ia))) { clash=true; break; }
																						} if(clash) continue;

																						//Vec symaxs2 = rotation_matrix_degrees(symaxs1,h4sa1/2.0)*symaxs2_0;
																						Mat Rsym2_0 = rotation_matrix_degrees(symaxs1,h4sa1);
																						h4m = Rsym2_0*(h4m-symcen1)+symcen1;
																						Vec delta = Psym1*(ZN-h4m);

																						// // clash p1r4
																						// clash=false;
																						// tmp1.replace_residue(rsd4,asp.residue(1),true);
																						// tmp1.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						// tmp1.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));
																						// for(Size i = 6; i <= tmp1.residue(rsd4).nheavyatoms(); ++i) if(! ifc.clash_check( tmp1.xyz(AtomID(i,rsd4)), rsd4 ) ) { clash=true; break; }
																						// if(clash){ continue; }

																						clash=false;
																						for(Size i = 1; i <= tmp3.n_residue(); ++i) {
																								for(Size j = 1; j <= 5; ++j) {
																										Vec const X = Rsym2_0*(tmp3.xyz(AtomID(j,i))-symcen1)+symcen1+delta;
																										if(!ifc.clash_check(       X                 )) clash=true;
																										if(!ifc.clash_check(Rsym1*(X-symcen1)+symcen1)) clash=true;
																										//                        if(!ifc2.clash_check(tmp3.xyz(AtomID(j,i)))) clash=true;
																								} if(clash) break;
																						} if(clash) { /*TR << "clash fail" << std::endl;*/ continue; }
																						//                    TR << isctfast(tmp1,tmp3,Vec(1,0,0)) << " " << isctfast(tmp1,tmp3,Vec(-1,0,0)) << " " << std::endl;
																						//  if( isctfast(tmp1,tmp3,Vec(1,0,0)) < -0.5 && isctfast(tmp1,tmp3,Vec(-1,0,0)) < -0.5 ) continue;

																						rot_pose(tmp3,Rsym2_0,symcen1);
																						trans_pose(tmp3,delta);

																						tmp1.replace_residue(rsd3,his.residue(1),true);
																						tmp1.replace_residue(rsd4,asp.residue(1),true);
																						tmp2.replace_residue(rsd3,his.residue(1),true);
																						tmp2.replace_residue(rsd4,asp.residue(1),true);
																						tmp3.replace_residue(rsd3,his.residue(1),true);
																						tmp3.replace_residue(rsd4,asp.residue(1),true);
																						tmp1.set_chi(1,rsd3,rots1[rsd3]->rotamer(irt3)->chi(1));
																						tmp1.set_chi(2,rsd3,rots1[rsd3]->rotamer(irt3)->chi(2));
																						tmp2.set_chi(1,rsd3,rots1[rsd3]->rotamer(irt3)->chi(1));
																						tmp2.set_chi(2,rsd3,rots1[rsd3]->rotamer(irt3)->chi(2));
																						tmp3.set_chi(1,rsd3,rots1[rsd3]->rotamer(irt3)->chi(1));
																						tmp3.set_chi(2,rsd3,rots1[rsd3]->rotamer(irt3)->chi(2));
																						tmp1.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						tmp1.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));
																						tmp2.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						tmp2.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));
																						tmp3.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						tmp3.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));

																						vector1<Size> tres(4); tres[1]=rsd1; tres[2]=rsd2; tres[3]=rsd3; tres[4]=rsd4;
																						for(Size i = 6; i <= tmp1.residue(rsd1).nheavyatoms(); ++i) {
																								for(Size j = 6; j <= tmp1.residue(rsd1).nheavyatoms(); ++j) {
																										for(Size k = 1; k <= 4; ++k) {
																												for(Size l = 1; l <= 4; ++l) {
																														if( tmp1.residue(tres[k]).xyz(i).distance_squared(tmp2.residue(tres[l]).xyz(j)) < 6.0 ) clash=true;
																														if( tmp1.residue(tres[k]).xyz(i).distance_squared(tmp3.residue(tres[l]).xyz(j)) < 6.0 ) clash=true;
																														if( tmp2.residue(tres[k]).xyz(i).distance_squared(tmp3.residue(tres[l]).xyz(j)) < 6.0 ) clash=true;
																												}
																										}
																								}
																						}
																						if(clash) continue;

																						// for(Size i = 1; i <= tmp1.n_residue(); ++i) {
																						// 		if(i==rsd1||i==rsd2||i==rsd3||i==rsd4) continue;
																						// 		replace_pose_residue_copying_existing_coordinates( tmp1, i, tmp1.residue(i).residue_type_set().name_map("ALA"));
																						// 		replace_pose_residue_copying_existing_coordinates( tmp2, i, tmp1.residue(i).residue_type_set().name_map("ALA"));
																						// 		replace_pose_residue_copying_existing_coordinates( tmp3, i, tmp1.residue(i).residue_type_set().name_map("ALA"));
																						// }
																						//tmp1.dump_pdb("test1.pdb");
																						//tmp2.dump_pdb("test2.pdb");
																						//tmp3.dump_pdb("test3.pdb");

																						//utility_exit_with_message("aoristn");

																						Vec symcen2 = (tmp1.residue(rsd1).xyz("HE2")+tmp3.residue(rsd1).xyz("HE2"))/2.0;
																						Vec symaxs2 = (tmp1.residue(rsd1).xyz("NE2")+tmp3.residue(rsd1).xyz("NE2"))/2.0 - symcen2;
																						symaxs2 = projperp(tmp1.residue(rsd1).xyz("HE2")-tmp3.residue(rsd1).xyz("HE2"),symaxs2).normalized();
																						Vec symaxs3 = symaxs1.cross(symaxs2);
																						Vec symcen = symcen1-Psym1*(symcen1-symcen2);

																						if( projperp(symaxs2,ZN-symcen).length() < MINMDIS/2.0 ) continue;
																						if( projperp(symaxs3,ZN-symcen).length() < MINMDIS/2.0 ) continue;

																						Pose tmp4(tmp1);
																						rot_pose(tmp4,symaxs3,180.0,symcen);

																						string fn = tag+"_"+lzs(rsd1,3)+"-"+lzs(rsd2,3)+"-"+lzs(blen1,2)+"-"+lzs(blen2,2)+"-"+lzs(bang,3)+"-"+lzs(isol,2)+"_"+lzs(rsd3,3)+"-"+lzs(imer,1)+"-"+lzs(irt3,3)+"_"+lzs(rsd4,3)+"-"+lzs(irt4,3)+"-"+lzs(od1,1)+"-"+lzs(hbo,1);
																						TR << "HIT " << fn << std::endl;

																						Pose out1(tmp1);
																						out1.replace_residue(rsd4,asp.residue(1),true);
																						out1.set_chi(1,rsd4,rots2[rsd4]->rotamer(irt4)->chi(1));
																						out1.set_chi(2,rsd4,rots2[rsd4]->rotamer(irt4)->chi(2));
																						for(Size i = 1; i <= tmp1.n_residue(); ++i) {
																								if(i==rsd1||i==rsd2||i==rsd3||i==rsd4) continue;
																								replace_pose_residue_copying_existing_coordinates( out1, i, out1.residue(i).residue_type_set().name_map("ALA"));
																						}
																						trans_pose(out1,-symcen);
																						Mat m = alignVectorSets( symaxs1, symaxs2, Vec(1,0,0),Vec(0,1,0));
																						rot_pose(out1,m);
																						core::pose::symmetry::make_symmetric_pose(out1);
																						//Pose out2(out1),out3(out1),out4(out1);
																						//rot_pose(out2,symaxs1,180.0,symcen);
																						//rot_pose(out3,symaxs2,180.0,symcen);
																						//rot_pose(out4,symaxs3,180.0,symcen);

																						out1.dump_pdb(fn+".pdb");
																						//out2.dump_pdb(fn+"_2.pdb");
																						//out3.dump_pdb(fn+"_3.pdb");
																						//out4.dump_pdb(fn+"_4.pdb");
																						//utility_exit_with_message("test4");

																						} // hbo
																				} // od1
																		} //
																}


                // {
                //  TR << "output " << rsd1 << " " << rsd2 << " " << rsd3 << " " << imer << " " << irt3 << std::endl;
                //   ozstream out("ikrs_his_clamp_"+lzs(rsd1,3)+"_"+lzs(rsd2,3)+"_"+lzs(blen1,2)+"_"+lzs(blen2,2)+"_"+lzs(bang,3)+"_"+str(isol)+".pdb");
                //   Size ano = 0;
                //   core::io::pdb::dump_pdb_residue(tmp1.residue(rsd1  ),ano,out);
                //   core::io::pdb::dump_pdb_residue(tmp1.residue(rsd2  ),ano,out);
                //   core::io::pdb::dump_pdb_residue(his3.residue(1     ),ano,out);
                //   Vec ccom = ZN;
                //   Vec cen  = ZN2a;
                //   Vec dir2 = ZN2b;
                //   //Vec axs3 = ZN;
                //   out<<"ATOM  "<<I(5,11)<<' '<<" COM"<<' '<<"COM"<<' '<<"A"<<I(4,2)<<"    "<<F(8,3,ccom.x())<<F(8,3,ccom.y())<<F(8,3,ccom.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
                //   out<<"ATOM  "<<I(5,11)<<' '<<" CEN"<<' '<<"CEN"<<' '<<"A"<<I(4,2)<<"    "<<F(8,3, cen.x())<<F(8,3, cen.y())<<F(8,3, cen.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
                //   out<<"ATOM  "<<I(5,11)<<' '<<" DIR"<<' '<<"DIR"<<' '<<"A"<<I(4,2)<<"    "<<F(8,3,dir2.x())<<F(8,3,dir2.y())<<F(8,3,dir2.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
                //   //out<<"ATOM  "<<I(5,11)<<' '<<" AXS"<<' '<<"AXS"<<' '<<"A"<<I(4,2)<<"    "<<F(8,3,axs3.x())<<F(8,3,axs3.y())<<F(8,3,axs3.z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
                //   out.close();
                //   utility_exit_with_message("asfdl;s");
                // }
              }
														tmp1.replace_residue(rsd3,rtmp3,false); //
												}
          } // rsd3
        }
      }
    }
  }
		tmp1.replace_residue(rsd1,rtmp1,true);
		tmp1.replace_residue(rsd2,rtmp2,true);
  return mxcb;
}
// }}}


void run(std::string fname) {
  using namespace std;
  using basic::options::option;
  using namespace basic::options::OptionKeys;
  using namespace core;
  using namespace chemical;
  using namespace id;
  using namespace pose;
  using namespace scoring;

  // setup stuff
  ResidueTypeSetCAP frs=ChemicalManager::get_instance()->residue_type_set( FA_STANDARD );
  ScoreFunctionOP sfstd=get_score_function();

  // read pose info
  Pose natp,his,asp,ala;
  make_pose_from_sequence(his,"H",core::chemical::FA_STANDARD,false);
  make_pose_from_sequence(asp,"D",core::chemical::FA_STANDARD,false);
  make_pose_from_sequence(ala,"A",core::chemical::FA_STANDARD,false);
  if(asp.residue(1).is_lower_terminus()) remove_lower_terminus_type_from_pose_residue(asp,1);
  if(asp.residue(1).is_upper_terminus()) remove_upper_terminus_type_from_pose_residue(asp,1);
  if(his.residue(1).is_lower_terminus()) remove_lower_terminus_type_from_pose_residue(his,1);
  if(his.residue(1).is_upper_terminus()) remove_upper_terminus_type_from_pose_residue(his,1);
  if(ala.residue(1).is_lower_terminus()) remove_lower_terminus_type_from_pose_residue(ala,1);
  if(ala.residue(1).is_upper_terminus()) remove_upper_terminus_type_from_pose_residue(ala,1);
  pose_from_pdb(natp,*frs,fname);
  Size nres=natp.n_residue();
  core::scoring::dssp::Dssp dssp(natp);
  dssp.insert_ss_into_pose(natp);
  core::pack::optimizeH(natp,*sfstd);
  //core::pack::optimizeH(his,*sfstd); // fails

  for(Size ir=1; ir<=nres; ++ir) natp.replace_residue(ir,ala.residue(1),true);

  ImplicitFastClashCheck ifc(natp,2.4);

  // get optional res list
  vector1<Size> ifres; {
    vector1<Size> tmp=read_res_list(option[willmatch::exclude_res1]());
    for(Size i=1; i<=nres; ++i) if(find(tmp.begin(),tmp.end(),i)==tmp.end()) ifres.push_back(i);
  }

		TR << "making rotamers" << std::endl;
  core::scoring::ScoreFunctionOP sf = core::scoring::get_score_function_legacy( core::scoring::PRE_TALARIS_2013_STANDARD_WTS );
		vector1<core::pack::rotamer_set::RotamerSetOP> rots1(natp.n_residue(),NULL);
		vector1<core::pack::rotamer_set::RotamerSetOP> rots2(natp.n_residue(),NULL);
		vector1<vector1<bool> > rotc1(natp.n_residue()),rotc2(natp.n_residue());
		for(Size ir = 1; ir <= natp.n_residue(); ++ir) {
				natp.replace_residue(ir,his.residue(1),true);
				rots1[ir] = get_rotset(natp,ir,sf,ifc,rotc1[ir]);
				natp.replace_residue(ir,asp.residue(1),true);
				rots2[ir] = get_rotset(natp,ir,sf,ifc,rotc2[ir]);
				natp.replace_residue(ir,ala.residue(1),true);
		}

  vector1<HitOP> hits;

		TR << "searching" << std::endl;
		//		ik_his_clamp(hisp,73,77,ifc,hits,rotsets);

#ifdef USE_OPENMP
#pragma omp parallel for schedule(dynamic,1)
#endif
   for(int ir=2; ir<=nres-1; ++ir) {
					Pose tmp;
#ifdef USE_OPENMP
#pragma omp critical
#endif
					tmp = natp;
					if(std::find(ifres.begin(),ifres.end(),ir)==ifres.end()) continue;
					//     if(tmp.secstruct(ir)!='H') continue;
     TR << "ir: " << ir << " " << hits.size() << std::endl;
     for(Size jr=2; jr<=nres-1; ++jr) {
       if(std::find(ifres.begin(),ifres.end(),jr)==ifres.end()) continue;
							//							if( !(ir==12&&jr==16 || ir==51&&jr==55) ) continue;
							//       if(tmp.secstruct(jr)!='H') continue;
       ik_his_clamp(tmp,ir,jr,ifc,hits,rots1,rots2,rotc1,rotc2,utility::file_basename(fname));
     } // crot
   } // ir

}


int main (int argc, char *argv[]) {

	try {


  devel::init(argc,argv);

  using basic::options::option;
  using namespace basic::options::OptionKeys;

  run(option[in::file::s]()[1]);

	} catch ( utility::excn::EXCN_Base const & e ) {
		std::cout << "caught exception " << e.msg() << std::endl;
		return -1;
	}

}


