#include "encoding_test.h"
#include "encdet_test.h"

#include <boost/version.hpp>

#if BOOST_VERSION >= 103400
# include <boost/test/included/unit_test.hpp>
#else
# include <boost/test/included/unit_test_framework.hpp>
#endif

#include <iostream>

boost::unit_test::test_suite* init_unit_test_suite(int argc, char* argv[])
{
	boost::unit_test::test_suite* encoding_test = BOOST_TEST_SUITE("encoding_test");
	encoding_test->add(BOOST_TEST_CASE(&test_gb18030_conv));
	encoding_test->add(BOOST_TEST_CASE(&test_doublebyte_conv));
	encoding_test->add(BOOST_TEST_CASE(&test_singlebyte_conv));

	boost::unit_test::test_suite* encdet_test_with_mozcases = BOOST_TEST_SUITE("encdet_test_with_mozcases");
	encdet_test_with_mozcases->add(BOOST_TEST_CASE(&test_encdet_moz_muticases));
	encdet_test_with_mozcases->add(BOOST_TEST_CASE(&test_encdet_moz_others));

	boost::unit_test::test_suite* encdet_test_wxmedit_cases = BOOST_TEST_SUITE("encdet_test_wxmedit_cases");
	encdet_test_wxmedit_cases->add(BOOST_TEST_CASE(&test_encdet_wxmedit_iso646));
	encdet_test_wxmedit_cases->add(BOOST_TEST_CASE(&test_encdet_wxmedit_bom));
	encdet_test_wxmedit_cases->add(BOOST_TEST_CASE(&test_encdet_wxmedit_utf8));
	encdet_test_wxmedit_cases->add(BOOST_TEST_CASE(&test_encdet_wxmedit_utf16));
	encdet_test_wxmedit_cases->add(BOOST_TEST_CASE(&test_encdet_wxmedit_utf32));

	boost::unit_test::test_suite* encdet_test = BOOST_TEST_SUITE("encdet_test");
	encdet_test->add(encdet_test_wxmedit_cases);
	encdet_test->add(encdet_test_with_mozcases);
	encdet_test->add(BOOST_TEST_CASE(&test_encdet_with_icucases));

	boost::unit_test::test_suite* test = BOOST_TEST_SUITE("wxmedit_test");
	test->add(encdet_test);
	test->add(encoding_test);

	return test;
}
