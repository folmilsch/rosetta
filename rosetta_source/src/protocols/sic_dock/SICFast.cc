// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:

#include <protocols/sic_dock/SICFast.hh>

#include <protocols/sic_dock/xyzStripeHashPose.hh>
#include <protocols/sic_dock/util.hh>

#include <basic/options/keys/sicdock.OptionKeys.gen.hh>
#include <basic/options/option.hh>
#include <basic/options/option_macros.hh>
#include <numeric/constants.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyz.io.hh>
#include <ObjexxFCL/format.hh>
#include <ObjexxFCL/string.functions.hh>
#include <utility/io/ozstream.hh>
#include <utility/string_util.hh>
#include <core/id/AtomID_Map.hh>
#include <core/scoring/sasa.hh>
#include <core/scoring/constraints/Func.hh>
#include <core/pose/util.hh>

namespace protocols {
namespace sic_dock {

using platform::Size;
using platform::Real;
using std::string;
using utility::vector1;
using ObjexxFCL::fmt::I;
using ObjexxFCL::fmt::F;
using ObjexxFCL::fmt::RJ;
using numeric::min;
using numeric::max;
using std::cout;
using std::cerr;
using std::endl;
typedef numeric::xyzVector<platform::Real> Vec;
typedef numeric::xyzMatrix<platform::Real> Mat;

template<typename T> inline T sqr(T x) { return x*x; }


void dump_points_pdb(utility::vector1<Vec> const & p, std::string fn) {
        using namespace ObjexxFCL::fmt;
        std::ofstream o(fn.c_str());
        for(Size i = 1; i <= p.size(); ++i) {
                std::string rn = "VIZ"; o<<"HETATM"<<I(5,i)<<' '<<" CA "<<' '<<rn<<' '<<"A"<<I(4,i)<<"    "<<F(8,3,p[i].x())<<F(8,3,p[i].y())<<F(8,3,p[i].z())<<F(6,2,1.0)<<F(6,2,1.0)<<'\n';
        }        o.close();
}


inline double dist_score( double const & sqdist, double const & start, double const & stop ) {
	if( sqdist > stop*stop ) {
		return 0.0;
	} else if( sqdist < start*start ) {
		return 1.0;
	} else {
		double dist = sqrt( sqdist );
		return (stop-dist)/(stop-start);
		//return sqr(1.0	- sqr( (dist - start) / (stop - start) ) );
	}
}

SICFast::SICFast(core::Real clash_dis) :
	CLD(clash_dis),
	CLD2(sqr(CLD)),
	BIN(CLD*basic::options::option[basic::options::OptionKeys::sicdock::hash_2D_vs_3D]()),
	h1_(new xyzStripeHashPose(CLD+0.01)),
	h2_(new xyzStripeHashPose(CLD+0.01))
{}

SICFast::SICFast() :
	CLD(basic::options::option[basic::options::OptionKeys::sicdock::clash_dis]()),
	CLD2(sqr(CLD)),
	BIN(CLD*basic::options::option[basic::options::OptionKeys::sicdock::hash_2D_vs_3D]()),
	h1_(new xyzStripeHashPose(CLD+0.01)),
	h2_(new xyzStripeHashPose(CLD+0.01))
{}

SICFast::~SICFast(){
	if(h1_) delete h1_;
	if(h2_) delete h2_;
}


void
SICFast::init(
	core::pose::Pose const & pose
) {
	init(pose,pose);
}

void
SICFast::init(
	core::pose::Pose const & pose,
	core::id::AtomID_Map<platform::Real> const & clash_atoms
){
	init(pose,pose,clash_atoms,clash_atoms);
}

void
SICFast::init(
	core::pose::Pose const & pose1,
	core::pose::Pose const & pose2
){
	using core::id::AtomID;
	core::id::AtomID_Map<platform::Real> clashmap1,clashmap2;
	core::pose::initialize_atomid_map(  clashmap1,pose1,-1.0);
	core::pose::initialize_atomid_map(  clashmap2,pose2,-1.0);
	for(Size i = 1; i <= pose1.n_residue(); ++i) {
		for(int j = 1; j <= ((pose1.residue(i).name3()=="GLY")?4:5); ++j)
			clashmap1[AtomID(j,i)] = pose1.residue(i).atom_type(j).lj_radius();
	}
	for(Size i = 1; i <= pose2.n_residue(); ++i) {
		for(int j = 1; j <= ((pose2.residue(i).name3()=="GLY")?4:5); ++j)
			clashmap2[AtomID(j,i)] = pose2.residue(i).atom_type(j).lj_radius();
	}
	init(pose1,pose2,clashmap1,clashmap2);
}

void
SICFast::init(
	core::pose::Pose const & pose1,
	core::pose::Pose const & pose2,
	core::id::AtomID_Map<platform::Real> const & clash_atoms1,
	core::id::AtomID_Map<platform::Real> const & clash_atoms2
){
	using core::id::AtomID;
	h1_->init_with_pose(pose1,clash_atoms1);
	h2_->init_with_pose(pose2,clash_atoms2);
	w1_ = cb_weights_from_pose(pose1);
	w2_ = cb_weights_from_pose(pose2);
}


inline
bool
get_bounds_intersection(
	vector1<Vec> const & pb,
	vector1<Vec> const & pa,
	double & xmx, double & xmn,
	double & ymx, double & ymn
){
	// get bounds for plane hashes
	double xmx1=-9e9,xmn1=9e9,ymx1=-9e9,ymn1=9e9;
	xmx=-9e9,xmn=9e9,ymx=-9e9,ymn=9e9;
	for(vector1<Vec>::const_iterator ia = pb.begin(); ia != pb.end(); ++ia) {
		xmx1 = max(xmx1,ia->x()); xmn1 = min(xmn1,ia->x());
		ymx1 = max(ymx1,ia->y()); ymn1 = min(ymn1,ia->y());
	}
	for(vector1<Vec>::const_iterator ib = pa.begin(); ib != pa.end(); ++ib) {
		xmx = max(xmx,ib->x()); xmn = min(xmn,ib->x());
		ymx = max(ymx,ib->y()); ymn = min(ymn,ib->y());
	}
	xmx = min(xmx,xmx1); xmn = max(xmn,xmn1);
	ymx = min(ymx,ymx1); ymn = max(ymn,ymn1);
	if(ymn > ymx || xmn > xmx) return false; //utility_exit_with_message("slide disjoint, maybe something wrong.");
	return true;
}


inline
void
fill_plane_hash(
	vector1<Vec> const & pb,
	vector1<Vec> const & pa,
	double const & xmx,
	double const & xmn,
	double const & ymx,
	double const & ymn,
	double const & BIN,
	ObjexxFCL::FArray2D<Vec> & ha,
	ObjexxFCL::FArray2D<Vec> & hb,
	int & xlb,
	int & ylb,
	int & xub,
	int & yub
){
	xlb = (int)(xmn/BIN)-2; xub = (int)(xmx/BIN+0.999999999)+2; // one extra on each side for correctness,
	ylb = (int)(ymn/BIN)-2; yub = (int)(ymx/BIN+0.999999999)+2; // and one extra for outside atoms
	ha.dimension(xub-xlb+1,yub-ylb+1,Vec(0,0,-9e9));
	hb.dimension(xub-xlb+1,yub-ylb+1,Vec(0,0, 9e9));
	int const xsize = xub-xlb+1;
	int const ysize = yub-ylb+1;
	for(vector1<Vec>::const_iterator ia = pb.begin(); ia != pb.end(); ++ia) {
		int const ix = (int)((ia->x()/BIN)-xlb+0.999999999);
		int const iy = (int)((ia->y()/BIN)-ylb+0.999999999);
		if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
		if( ha(ix,iy).z() < ia->z() ) ha(ix,iy) = *ia;
		// bool const test = !( ix < 1 || ix > xsize || iy < 1 || iy > ysize) && ha(ix,iy).z() < ia->z();
		// ha(ix,iy) = test ? *ia : ha(ix,iy);
	}
	for(vector1<Vec>::const_iterator ib = pa.begin(); ib != pa.end(); ++ib) {
		int const ix = (int)((ib->x()/BIN)-xlb+0.999999999);
		int const iy = (int)((ib->y()/BIN)-ylb+0.999999999);
		if( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) continue;
		if( hb(ix,iy).z() > ib->z() ) hb(ix,iy) = *ib;
		// bool const test = !( ix < 1 || ix > xsize || iy < 1 || iy > ysize ) && hb(ix,iy).z() > ib->z();
		// hb(ix,iy) = test ? *ib : hb(ix,iy);
	}

}

inline
double
get_mindis_with_plane_hashes(
	int const & xlb,
	int const & ylb,
	int const & xub,
	int const & yub,
	ObjexxFCL::FArray2D<Vec> const & ha,
	ObjexxFCL::FArray2D<Vec> const & hb,
	double const & clashdis2
){
	int const xsize=xub-xlb+1, ysize=yub-ylb+1;
	double m = 9e9;
	for(int i = 1; i <= xsize; ++i) { // skip 1 and N because they contain outside atoms (faster than clashcheck?)
		for(int j = 1; j <= ysize; ++j) {
			for(int k = -1; k <= 1; ++k) {
				if(i+k < 1 || i+k > xsize) continue;
				for(int l = -1; l <= 1; ++l) {
					if(j+l < 1 || j+l > ysize) continue;
					double const xa1=ha(i,j).x(),ya1=ha(i,j).y(),xb1=hb(i+k,j+l).x(),yb1=hb(i+k,j+l).y(),d21=(xa1-xb1)*(xa1-xb1)+(ya1-yb1)*(ya1-yb1);
					if(d21<clashdis2){ double const dz=hb(i+k,j+l).z()-ha(i,j).z()-sqrt(clashdis2-d21); if(dz<m) m=dz; }
				}
			}
		}
	}
	return m;
}

struct CorrectionVisitor {
	Vec const dof;
	Real const clash_dis_sq;
	Real correction;
	int contacts;
	CorrectionVisitor(Vec const & dof_in, Real const & clash_dis_sq_in) : dof(dof_in),clash_dis_sq(clash_dis_sq_in),correction(9e9),contacts(0) {}
	void visit( numeric::xyzVector<double> const & v, numeric::xyzVector<double> const & c){
		double const dxy2 = dof.cross(v-c).length_squared();
		if( dxy2 < clash_dis_sq ){
			double const dz = dof.dot(v) - dof.dot(c) - sqrt(clash_dis_sq-dxy2);
			correction = min(dz,correction);
			contacts++;
		}
	}
};

inline
double
refine_mindis_with_xyzHash(
	xyzStripeHashPose *xh,
	core::kinematics::Stub const & xform_to_struct2_start,
	vector1<Vec> const & pa,
	Vec const & ori,
	double const & clash_dis_sq,
	double const & mindis_approx
){
	double mindis = mindis_approx;
	Mat Rori = rotation_matrix_degrees( (ori.z() < -0.99999) ? Vec(1,0,0) : (Vec(0,0,1)+ori.normalized())/2.0 , 180.0 );
	Vec hash_ori = xform_to_struct2_start.M.transposed() * ori;
	while(true){
		CorrectionVisitor visitor(hash_ori,clash_dis_sq);
		for(vector1<Vec>::const_iterator ipa = pa.begin(); ipa != pa.end(); ++ipa) {
			Vec const v = xform_to_struct2_start.global2local(Rori*((*ipa)-Vec(0,0,mindis)));
			xh->visit_lax(v,visitor);
		}
		if(visitor.contacts == 0){
			mindis = 9e9;
			break;
		}
		mindis += visitor.correction;
		if( fabs(visitor.correction) < 0.001 ) break;
	}
	return mindis; // now fixed
}

double
SICFast::slide_into_contact(
	core::kinematics::Stub const & xa,
	core::kinematics::Stub const & xb,
	Vec                            ori
) const {
	ori.normalize();

	// get rotated points
	utility::vector1<Vec> pa(h1_->natom()), pb(h2_->natom());
	utility::vector1<Vec>::iterator ipa(pa.begin()),ipb(pb.begin());
	for(xyzStripeHashPose::const_iterator i = h1_->begin(); i != h1_->end(); ++i,++ipa) *ipa = xa.local2global(*i-h1_->translation());
	for(xyzStripeHashPose::const_iterator i = h2_->begin(); i != h2_->end(); ++i,++ipb) *ipb = xb.local2global(*i-h2_->translation());

	double xmx,xmn,ymx,ymn;
	int xlb,ylb,xub,yub;
	ObjexxFCL::FArray2D<Vec> ha,hb; // 2D hashes

	// rotate points, should merge with above
	Mat rot = rotation_matrix_degrees( (ori.z() < -0.99999) ? Vec(1,0,0) : (Vec(0,0,1)+ori)/2.0 , 180.0 );
	for(vector1<Vec>::iterator ia = pb.begin(); ia != pb.end(); ++ia) *ia = rot*(*ia);
	for(vector1<Vec>::iterator ib = pa.begin(); ib != pa.end(); ++ib) *ib = rot*(*ib);

	if( ! get_bounds_intersection(pb,pa,xmx,xmn,ymx,ymn) ) return 9e9;

	fill_plane_hash(pb,pa,xmx,xmn,ymx,ymn,BIN,ha,hb,xlb,ylb,xub,yub);

	double const mindis_approx = get_mindis_with_plane_hashes(xlb,ylb,xub,yub,ha,hb,CLD2);
	if( fabs(mindis_approx) > 9e8 ) return 9e9;

	double const mindis = refine_mindis_with_xyzHash(h2_,xb,pa,ori,CLD2,mindis_approx);
	if( fabs(mindis) > 9e8 ) return 9e9;

	return -mindis;
}




} // namespace sic_dock
} // namespace protocols
