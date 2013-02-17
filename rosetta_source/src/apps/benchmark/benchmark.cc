// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
// :noTabs=false:tabSize=4:indentSize=4:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   rosetta_source/src/apps/benchmark/benchmark.cc
///
/// @brief
/// @author Sergey Lyskov

#include <core/chemical/ChemicalManager.hh>
#include <core/scoring/Energies.hh>
#include <basic/Tracer.hh>
#include <devel/init.hh>
#include <core/types.hh>

#include <basic/options/option.hh>
#include <basic/options/option_macros.hh>

#include <numeric/random/random.hh>

#include <apps/benchmark/benchmark.hh>

#include <utility/excn/Exceptions.hh>

// AUTO-REMOVED #include <time.h>
#include <fstream>

#if  !defined(WINDOWS) && !defined(WIN32)
	#include <sys/time.h>
	#include <sys/resource.h>
#endif


OPT_1GRP_KEY( Real, run, benchmark_scale )
OPT_1GRP_KEY( String, run, run_one_benchmark )

const char results_filename[] = "_performance_";

// Initialize performance benchmark tests here:
#include <apps/benchmark/score.bench.hh>
ScoreBenchmark Score_("core.scoring.Score");

#include <apps/benchmark/ScoreEach.bench.hh>
#include <apps/benchmark/ScoreAnalyticEtable.bench.hh>

#include <apps/benchmark/SmallMover.bench.hh>
SmallMoverBenchmark SmallMover_("protocols.moves.SmallMover");

#include <apps/benchmark/ShearMover.bench.hh>
ShearMoverBenchmark ShearMover_("protocols.moves.ShearMover");

#include <apps/benchmark/Minimizer.bench.hh>
//MinimizerBenchmark Minimizer_("protocols.optimization.Minimizer");
MinimizerBenchmark_dfpmin Minimizer_dfpmin_("protocols.optimization.Minimizer_dfpmin");
MinimizerBenchmark_dfpmin_armijo MinimizerBenchmark_dfpmin_armijo_("protocols.optimization.Minimizer_dfpmin_armijo");
MinimizerBenchmark_dfpmin_armijo_nonmonotone MinimizerBenchmark_dfpmin_armijo_nonmonotone_("protocols.optimization.Minimizer_dfpmin_armijo_nonmonotone");

#include <apps/benchmark/Docking.bench.hh>
DockingBenchmark_low DockingLow("protocols.docking.DockingLowRes");
DockingBenchmark_high DockingHigh("protocols.docking.DockingHighRes");

// AUTO-REMOVED #include <apps/benchmark/Design.bench.hh>
//DesignBenchmark design("protocols.moves.PackRotamersMover");

// AUTO-REMOVED #include <apps/benchmark/LigandDock.bench.hh>
//LigandDockBenchmark ligand_dock("protocols.ligand_docking.LigandDockProtocol");

// AUTO-REMOVED #include <apps/benchmark/LigandDockScript.bench.hh>
//LigandDockScriptBenchmark ligand_dock_script("protocols.ligand_docking.LigandDockScript");

#include <apps/benchmark/pdb_io.bench.hh>
PDB_IOBenchmark PDB_IO_("core.import_pose.pose_from_pdbstring");

#include <apps/benchmark/ResidueType.bench.hh>
ResidueTypeBenchmark ResidueType_("core.chemical.ResidueType");

#include <apps/benchmark/xml_parsing.bench.hh>
XMLParseBenchmark XMLParseBenchmark_("utility_tag_Tag_Create");

// option key includes

// AUTO-REMOVED #include <basic/options/keys/james.OptionKeys.gen.hh>
// AUTO-REMOVED #include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>

#include <core/optimization/MinimizerOptions.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>



using basic::T;
using basic::Error;
using basic::Warning;

using namespace core;

std::vector<Benchmark *> &Benchmark::allBenchmarks()
{
	static std::vector<Benchmark*> * allBenchmarks = new std::vector<Benchmark*>;
	return *allBenchmarks;
}

double Benchmark::execute(Real scaleFactor)
{
	/// Reseting RG system before each performance run.
	numeric::random::RandomGenerator::initializeRandomGenerators(
		 1000, numeric::random::_RND_TestRun_, "mt19937");

	TR << "Setting up "<< name() << "..." << std::endl;
	//for(int i=0; i<3; i++) {
	//	std::cout << "X i="<< i << " R=" << numeric::random::uniform() << std::endl;
	//}

	setUp();

	double t;

	#if  !defined(WINDOWS) && !defined(WIN32)
		TR << "Running(U) " << name() << "..." << std::endl;
		struct rusage R0, R1;

		getrusage(RUSAGE_SELF, &R0);
		run(scaleFactor);

		getrusage(RUSAGE_SELF, &R1);

		t = R1.ru_utime.tv_sec + R1.ru_utime.tv_usec*1e-6 - R0.ru_utime.tv_sec - R0.ru_utime.tv_usec*1e-6;
		TR << "Running(U) " << name() << "... Done. Time:" << t << std::endl;
	#else
		TR << "Running(W) " << name() << "..." << std::endl;
		t = clock();
		run(scaleFactor);
		t = clock() - t;
		t = t / CLOCKS_PER_SEC;
		TR << "Running(W) " << name() << "... Done. Time:" << t << std::endl;
	#endif


	TR << "Tear down "<< name() << "..." << std::endl;
	tearDown();
	TR << "Tear down "<< name() << "... Done." << std::endl << std::endl;

	result_ = t;
	return t;
}

void Benchmark::executeOneBenchmark(
	std::string const & name,
	Real scaleFactor
){
	TR << std::endl << "Executing benchmark '" << name << "'" << std::endl << std::endl;

	std::vector<Benchmark * > & all( allBenchmarks() );
	bool found_benchmark(false);
	for(Size i=0; i<all.size(); i++){
		Benchmark * B = all[i];
		if(B->name() == name){
			B->execute(scaleFactor);
			found_benchmark = true;
			break;
		}
	}
	if(found_benchmark){
		TR << std::endl << "Executing benchmark '" << name << "'... Done." << std::endl;
	} else {
		TR << std::endl << "Unable to locate benchmark '" << name << "'" << std::endl;
		TR << "The available benchmarks are:" << std::endl;
		for(Size i=0; i<all.size(); i++){
			Benchmark * B = all[i];
			TR << "    name: '" << B->name() << "'" << std::endl;
		}
	}
}

void Benchmark::executeAllBenchmarks(Real scaleFactor)
{
	TR << std::endl << "Executing all benchmarks..." << std::endl << std::endl;

	std::vector<Benchmark *> & all( allBenchmarks() );

	for(Size i=0; i<all.size(); i++) {
		Benchmark * B = all[i];
		B->execute(scaleFactor);
	}
	TR << std::endl << "Executing all benchmarks... Done." << std::endl;
}

///
/// Generting report file in python dict format: i.e: { 'Bench1': 1.5, 'Bench2': 1.6 }
///
std::string Benchmark::getReport()
{
	std::vector<Benchmark *> & all( allBenchmarks() );

	char buf[1024];

	std::string res = "{\n";
	for(Size i=0; i<all.size(); i++) {
		Benchmark * B = all[i];
		sprintf(buf, "%f", B->result_);
		res += "    '" + B->name_ + "':" + std::string(buf) + ",\n";
	}
	res += "}\n";
	return res;
}

///
/// Generting report file in python dict format: i.e: { 'Bench1': 1.5, 'Bench2': 1.6 }
///
std::string Benchmark::getOneReport(std::string const & name)
{
	std::vector<Benchmark *> & all( allBenchmarks() );

	char buf[1024];

	std::string res = "{\n";
	for(Size i=0; i<all.size(); i++) {
		Benchmark * B = all[i];
		if(B->name() == name){
			sprintf(buf, "%f", B->result_);
			res += "    '" + B->name_ + "':" + std::string(buf) + ",\n";
		}
	}
	res += "}\n";
	return res;
}


int real_command_line_argc; char ** real_command_line_argv;
int command_line_argc; char ** command_line_argv;


int main( int argc, char *argv[])
{
	try{
		command_line_argc=argc; command_line_argv=argv;
		real_command_line_argc=argc; real_command_line_argv=argv;

		using namespace core;
		using namespace basic::options::OptionKeys;


		NEW_OPT(run::benchmark_scale, "Amount to scale number of cycles to repeate each test", 1 );
		NEW_OPT(run::run_one_benchmark, "Run just a single performance benchmark", "" );
		basic::options::option.add_relevant(run::benchmark_scale);
		basic::options::option.add_relevant(run::run_one_benchmark);
		basic::options::option.add_relevant(in::path::database);

		devel::init(argc, argv);

		//TR << "DB:"  << basic::options::option[ in::path::database ]() << "\n";

		chemical::ResidueTypeSetCAP residue_set
			( chemical::ChemicalManager::get_instance()->residue_type_set( chemical::FA_STANDARD ) );

		//TR << "Specified()=" << basic::options::option[ run::benchmark_scale ].user() << "\n";
		//TR << "Legal" << basic::options::option[ run::benchmark_scale ].legal() << "\n";
		//TR << "Active:" << basic::options::option[ run::benchmark_scale ].user() << "\n";
		//TR << "native:"  << basic::options::option[ james::native ]() << "\n";
		//TR << "DB:"  << basic::options::option[ in::path::database ]() << "\n";
		Real scale = basic::options::option[ run::benchmark_scale ]();


		TR << "Mini Benchmark started! Scale factor: " << scale << " -------------" << std::endl;

		//TR << "CLOCKS_PER_SEC:" << CLOCKS_PER_SEC << "\n";

		std::string report;
		if(basic::options::option[ run::run_one_benchmark ].user()){
			std::string const & name(basic::options::option[ run::run_one_benchmark ]());
			Benchmark::executeOneBenchmark(name, scale);
			report = Benchmark::getOneReport(name);
		} else {
			Benchmark::executeAllBenchmarks(scale);
			report = Benchmark::getReport();
		}

		TR << "Results:" << std::endl << report;  TR.flush();

		/// Now, saving report to a file
		std::ofstream file(results_filename, std::ios::out | std::ios::binary);
		if(!file) {
			Error() << "Benchmark:: Unable to open file:" << results_filename << " for writing!!!" << std::endl;
			return 1;
		}
		file << report;

		file.close();

		TR << "Mini Benchmark ended.   --------------------------------" << std::endl;
	} catch ( utility::excn::EXCN_Msg_Exception const & e ) {
		std::cerr << "caught exception " << e.msg() << std::endl;
	}
	return 0;
}
