/********************************************************************************
 * Copyright (C) 2019-2023 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#define BOOST_TEST_MODULE(odc_misc)
#define BOOST_TEST_NO_MAIN
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>

#include <odc/MiscUtils.h>

using namespace odc::core;
using namespace boost::unit_test;

BOOST_AUTO_TEST_SUITE(utils)

BOOST_AUTO_TEST_CASE(test_seconds_parsing)
{
    BOOST_CHECK_EQUAL(parseTimeString("10s", std::chrono::seconds(60)).count(), 10);
    BOOST_CHECK_EQUAL(parseTimeString("0s", std::chrono::seconds(60)).count(), 0);
    BOOST_CHECK_EQUAL(parseTimeString("100s", std::chrono::seconds(60)).count(), 100);
    BOOST_CHECK_EQUAL(parseTimeString("1s", std::chrono::seconds(60)).count(), 1);
}

BOOST_AUTO_TEST_CASE(test_plain_number_parsing)
{
    BOOST_CHECK_EQUAL(parseTimeString("10", std::chrono::seconds(60)).count(), 10);
    BOOST_CHECK_EQUAL(parseTimeString("0", std::chrono::seconds(60)).count(), 0);
    BOOST_CHECK_EQUAL(parseTimeString("150", std::chrono::seconds(60)).count(), 150);
}

BOOST_AUTO_TEST_CASE(test_percentage_parsing)
{
    BOOST_CHECK_EQUAL(parseTimeString("50%", std::chrono::seconds(60)).count(), 30);
    BOOST_CHECK_EQUAL(parseTimeString("25%", std::chrono::seconds(60)).count(), 15);
    BOOST_CHECK_EQUAL(parseTimeString("100%", std::chrono::seconds(60)).count(), 60);
    BOOST_CHECK_EQUAL(parseTimeString("0%", std::chrono::seconds(60)).count(), 0);
    BOOST_CHECK_EQUAL(parseTimeString("200%", std::chrono::seconds(60)).count(), 120);
}

BOOST_AUTO_TEST_CASE(test_percentage_with_decimals)
{
    BOOST_CHECK_EQUAL(parseTimeString("50.5%", std::chrono::seconds(60)).count(), 30);
    BOOST_CHECK_EQUAL(parseTimeString("33.33%", std::chrono::seconds(60)).count(), 19);
}

BOOST_AUTO_TEST_CASE(test_invalid_formats)
{
    BOOST_CHECK_THROW(parseTimeString("", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("abc", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("10x", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("s10", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("%50", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("abc%", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("10.5s", std::chrono::seconds(60)), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(test_percentage_without_base_time)
{
    BOOST_CHECK_NO_THROW(parseTimeString("50%", std::chrono::seconds(60)));
    BOOST_CHECK_THROW(parseTimeString("25%", std::chrono::seconds(0)), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(test_negative_values)
{
    BOOST_CHECK_THROW(parseTimeString("-10s", std::chrono::seconds(60)), std::invalid_argument);
    BOOST_CHECK_THROW(parseTimeString("-5", std::chrono::seconds(60)), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(test_edge_cases)
{
    // Very small percentage
    BOOST_CHECK_EQUAL(parseTimeString("1%", std::chrono::seconds(3600)).count(), 36);

    // Large numbers
    BOOST_CHECK_EQUAL(parseTimeString("3600s", std::chrono::seconds(60)).count(), 3600);
    BOOST_CHECK_EQUAL(parseTimeString("3600", std::chrono::seconds(60)).count(), 3600);
}

BOOST_AUTO_TEST_SUITE_END()

int main(int argc, char* argv[])
{ return boost::unit_test::unit_test_main(init_unit_test, argc, argv); }
