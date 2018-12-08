#ifndef MOROS_CONFIG_HPP_
#define MOROS_CONFIG_HPP_

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace moros {

struct Config {
    std::size_t threads;
    std::size_t connections;
    std::chrono::seconds duration;
    std::chrono::seconds timeout;
    std::string url;
    std::vector<std::string> headers;
    bool display_latency;
};

}

#endif
