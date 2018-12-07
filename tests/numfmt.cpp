#define BOOST_TEST_MODULE NUMFMT
#include "numfmt.hpp"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(numfmt) {
    BOOST_CHECK_EQUAL(moros::numfmt(1998), "1.95K");
    BOOST_CHECK_EQUAL(moros::numfmt(std::chrono::seconds(3661)), "1h 1min 1s");
}
