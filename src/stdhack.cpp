#include "stdhack.hpp"
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace std {
namespace chrono {

void validate(boost::any& v, const std::vector<std::string>& xs,
              std::chrono::seconds*, long) {
    po::validators::check_first_occurrence(v);

    const auto& s = po::validators::get_single_string(xs);

    v = std::chrono::seconds(std::stoull(s));
}

ostream& operator<<(ostream& os, seconds sec) {
    return os << sec.count() << "s";
}

}


}
