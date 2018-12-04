#ifndef MOROS_STDHACK_HPP_
#define MOROS_STDHACK_HPP_

#include <chrono>
#include <string>
#include <vector>
#include <ostream>
#include <boost/any.hpp>

namespace std {
namespace chrono {

// let boost.program_options parse std::chrono::seconds pass
void validate(boost::any& v, const std::vector<std::string>& xs, seconds*,
              long);

ostream& operator<<(ostream& os, seconds sec);

}
}


#endif
