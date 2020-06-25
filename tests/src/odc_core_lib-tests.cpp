// Copyright 2020 GSI, Inc. All rights reserved.
//
//
//
// Unit tests
//
// BOOST: tests
// Defines test_main function to link with actual unit test code.
#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MAIN

// BOOST
#include <boost/test/unit_test.hpp>

using boost::unit_test::test_suite;

BOOST_AUTO_TEST_SUITE(test_odc_core_lib);

BOOST_AUTO_TEST_CASE(test_odc_core_lib_1)
{
    BOOST_CHECK(true);
}

BOOST_AUTO_TEST_SUITE_END();
