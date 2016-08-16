// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/* Generated file, do not edit */

#ifndef CXXTEST_RUNNING
#define CXXTEST_RUNNING
#endif

#define _CXXTEST_HAVE_STD
#include <cxxtest/TestListener.h>
#include <cxxtest/TestTracker.h>
#include <cxxtest/TestRunner.h>
#include <cxxtest/RealDescriptions.h>

//catch unhandled exceptions in unit tests
#include <utility/excn/EXCN_Base.hh>
#include "SewHashTests.cxxtest.hh"

static SewHashTests suite_SewHashTests;

static CxxTest::List Tests_SewHashTests = { 0, 0 };
CxxTest::StaticSuiteDescription suiteDescription_SewHashTests( "SewHashTests.cxxtest.hh", 48, "SewHashTests", suite_SewHashTests, Tests_SewHashTests );

static class TestDescription_SewHashTests_test_score_one : public CxxTest::RealTestDescription {
public:
 TestDescription_SewHashTests_test_score_one() : CxxTest::RealTestDescription( Tests_SewHashTests, suiteDescription_SewHashTests, 69, "test_score_one" ) {}
 void runTest() { suite_SewHashTests.test_score_one(); }
} testDescription_SewHashTests_test_score_one;

static class TestDescription_SewHashTests_test_hashing : public CxxTest::RealTestDescription {
public:
 TestDescription_SewHashTests_test_hashing() : CxxTest::RealTestDescription( Tests_SewHashTests, suiteDescription_SewHashTests, 90, "test_hashing" ) {}
 void runTest() { suite_SewHashTests.test_hashing(); }
} testDescription_SewHashTests_test_hashing;

static class TestDescription_SewHashTests_test_serialization : public CxxTest::RealTestDescription {
public:
 TestDescription_SewHashTests_test_serialization() : CxxTest::RealTestDescription( Tests_SewHashTests, suiteDescription_SewHashTests, 122, "test_serialization" ) {}
 void runTest() { suite_SewHashTests.test_serialization(); }
} testDescription_SewHashTests_test_serialization;

#include <cxxtest/Root.cpp>
