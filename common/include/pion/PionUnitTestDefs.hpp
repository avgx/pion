// -----------------------------------------------------------------------
// pion-common: a collection of common libraries used by the Pion Platform
// -----------------------------------------------------------------------
// Copyright (C) 2007 Atomic Labs, Inc.  (http://www.atomiclabs.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file COPYING or copy at http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_PIONUNITTESTDEFS_HEADER__
#define __PION_PIONUNITTESTDEFS_HEADER__

#include <boost/test/unit_test.hpp>
#include <boost/test/test_case_template.hpp>

/*
Using BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE and
BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE has two additional benefits relative to 
using BOOST_FIXTURE_TEST_SUITE and BOOST_AUTO_TEST_CASE:
1) it allows a test to be run with more than one fixture, and
2) it makes the current fixture part of the test name, e.g. 
   checkPropertyX<myFixture_F>

For an example of 1), see HTTPMessageTests.cpp.

There are probably simpler ways to achieve 2), but since it comes for free,
it makes sense to use it.  The benefit of this is that the test names don't
have to include redundant information about the fixture, e.g. 
checkMyFixtureHasPropertyX.  (In this example, checkPropertyX<myFixture_F> is 
not obviously better than checkMyFixtureHasPropertyX, but in many cases the 
test names become too long and/or hard to parse, or the fixture just isn't
part of the name, making some error reports ambiguous.)

(BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE is based on BOOST_AUTO_TEST_CASE_TEMPLATE,
in unit_test_suite.hpp.)

Minimal example demonstrating usage of BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE:

class ObjectToTest_F { // suffix _F is used for fixtures
public:
	ObjectToTest_F() {
		m_value = 2;
	}
	int m_value;
};

// This illustrates the most common case, where just one fixture will be used,
// so the list only has one fixture in it.
// ObjectToTest_S is the name of the test suite.
BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(ObjectToTest_S,
									   boost::mpl::list<ObjectToTest_F>)

BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(checkValueEqualsTwo) {
	BOOST_CHECK_EQUAL(m_value, 2);
}

BOOST_AUTO_TEST_SUITE_END()
*/

#define BOOST_AUTO_TEST_SUITE_FIXTURE_TEMPLATE(suite_name, fixture_types) \
BOOST_AUTO_TEST_SUITE(suite_name)                                         \
typedef fixture_types BOOST_AUTO_TEST_CASE_FIXTURE_TYPES;                 \
/**/

#define BOOST_AUTO_TEST_CASE_FIXTURE_TEMPLATE(test_name)		\
template<typename type_name>									\
struct test_name : public type_name								\
{ void test_method(); };										\
																\
struct BOOST_AUTO_TC_INVOKER( test_name ) {						\
	template<typename TestType>									\
	static void run( boost::type<TestType>* = 0 )				\
	{															\
		test_name<TestType> t;									\
		t.test_method();										\
	}															\
};																\
																\
BOOST_AUTO_TC_REGISTRAR( test_name )(							\
	boost::unit_test::ut_detail::template_test_case_gen<		\
		BOOST_AUTO_TC_INVOKER( test_name ),						\
		BOOST_AUTO_TEST_CASE_FIXTURE_TYPES >(					\
			BOOST_STRINGIZE( test_name ) ) );					\
																\
template<typename type_name>									\
void test_name<type_name>::test_method()						\
/**/

#endif
