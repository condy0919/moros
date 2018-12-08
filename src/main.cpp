#include "config.hpp"
#include "bencher.hpp"
#include "version.hpp"
#include "stdhack.hpp"
#include "http_parser.h"
#include "stats.hpp"
#include "numfmt.hpp"
#include <csignal>
#include <memory>
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


namespace po = boost::program_options;

std::unique_ptr<moros::Stats> requests, latency;

static moros::Config cfg;

static std::vector<moros::Bencher> benchers;

int main(int argc, char* argv[]) {
    std::signal(SIGINT, [](int sig) {
        (void)sig;

        for (auto& b : benchers) {
            b.stop();
        }
    });
    std::signal(SIGPIPE, SIG_IGN);

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help,h", "Produce help message")
        ("version,v", "Print version details")
        ("url,u", po::value<std::string>(&cfg.url), "HTTP url")
        ("header,H", po::value<std::vector<std::string>>(&cfg.headers), "HTTP header")
        ("threads,t", po::value<std::size_t>(&cfg.threads)->default_value(1), "The number of HTTP benchers")
        ("connections,c", po::value<std::size_t>(&cfg.connections)->default_value(10), "The number of HTTP connections per bencher")
        ("duration,d", po::value<std::chrono::seconds>(&cfg.duration)->default_value(std::chrono::seconds(10)), "Duration of bench")
        ("timeout,o", po::value<std::chrono::seconds>(&cfg.timeout)->default_value(std::chrono::seconds(2)), "Mark HTTP Request timeouted if HTTP Response is not received within this amount of time")
        ("latency,l", "Print latency distribution")
        ;

    po::positional_options_description pd;
    pd.add("url", -1);

    po::variables_map vm;
    po::store(
        po::command_line_parser(argc, argv).options(desc).positional(pd).run(),
        vm);
    po::notify(vm);
    if (argc == 1 || vm.count("help")) {
        std::cerr << "Usage: " << argv[0] << " <url> [options]" << '\n'
                  << desc << '\n';
        return 0;
    } else if (vm.count("version")) {
        std::cerr << "version: " << version << '\n';
        return 0;
    }

    cfg.display_latency = vm.count("latency");


    // Max QPS = 1M
    requests = std::make_unique<moros::Stats>(1000000);
    // Max Latency = t * 1000 ms, t is specified by --timeout option
    latency = std::make_unique<moros::Stats>(cfg.timeout.count() * 1000);


    struct http_parser_url parts = {};
    if (http_parser_parse_url(cfg.url.c_str(), cfg.url.size(), false, &parts) == 0) {
        if (!(parts.field_set & ((1 << UF_SCHEMA) | (1 << UF_HOST)))) {
            std::cerr << "Invalid url: " << cfg.url << '\n';
            return -1;
        }
    }

    const std::string schema = cfg.url.substr(parts.field_data[UF_SCHEMA].off,
                                              parts.field_data[UF_SCHEMA].len),
                      host = cfg.url.substr(parts.field_data[UF_HOST].off,
                                            parts.field_data[UF_HOST].len),
                      port = (parts.field_set & (1 << UF_PORT))
                                 ? cfg.url.substr(parts.field_data[UF_PORT].off,
                                                  parts.field_data[UF_PORT].len)
                                 : "",
                      service = !port.empty() ? port : schema,
                      path = (parts.field_set & (1 << UF_PATH))
                                 ? cfg.url.substr(parts.field_data[UF_PATH].off,
                                                  parts.field_data[UF_PATH].len)
                                 : "/",
                      query_string =
                          (parts.field_set & (1 << UF_QUERY))
                              ? cfg.url.substr(parts.field_data[UF_QUERY].off,
                                               parts.field_data[UF_QUERY].len)
                              : "";

    const std::string http_req = ({
        const auto iter = std::find_if(
            cfg.headers.begin(), cfg.headers.end(), [](const std::string& s) {
                return (s.size() <= 5)
                           ? false
                           : ::strncasecmp(s.c_str(), "Host:", 5) == 0;
            });

        const std::string uri =
            path + (query_string.empty() ? "" : "?") + query_string;

        std::string req;
        if (iter == cfg.headers.end()) {
            req = str(boost::format("GET %1% HTTP/1.1\r\n"
                                    "Host: %2%\r\n") %
                      uri % host);
        } else {
            req = str(boost::format("GET %1% HTTP/1.1\r\n") % uri);
        }

        for (const auto& header : cfg.headers) {
            req.append(header);
            req.append("\r\n");
        }
        req.append("\r\n");

        req;
    });

    const struct addrinfo hints = {
        .ai_flags = 0,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
        .ai_addrlen = 0,
        .ai_addr = nullptr,
        .ai_canonname = nullptr,
        .ai_next = nullptr,
    };

    struct addrinfo* result = nullptr;
    int ret = ::getaddrinfo(host.c_str(), service.c_str(), &hints, &result);
    if (ret != 0) {
        std::cerr << "resolve " << host << " failed: " << ::gai_strerror(ret) << '\n';
        return -2;
    }

    std::unique_ptr<struct addrinfo, void (*)(struct addrinfo*)> rptr(
        result, ::freeaddrinfo);


    for (std::size_t i = 0; i < cfg.threads; ++i) {
        benchers.emplace_back(*rptr, cfg.connections, http_req);
    }

    std::vector<std::thread> thread_group;
    for (std::size_t i = 0; i < cfg.threads; ++i) {
        auto& bencher = benchers[i];

        thread_group.emplace_back([&]() {
            bencher.run();
        });
    }

    const auto bench_start = std::chrono::steady_clock::now();

    // benchmark result title
    std::cerr << "Running " << moros::numfmt(cfg.duration) << " test @ "
              << cfg.url << '\n'
              << "  " << cfg.threads << " thread(s) and " << cfg.connections
              << " connection(s) each" << std::endl;

    std::this_thread::sleep_for(cfg.duration);
    for (auto& b : benchers) {
        b.stop();
    }

    for (auto& t : thread_group) {
        if (t.joinable()) {
            t.join();
        }
    }

    const auto runtime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - bench_start);

    // benchmark result
    std::cerr << "  Thread Stats   Avg      Stdev     Max   +/- Stdev\n";
    const auto print_stats = [](std::string name, const moros::Stats& st, auto fn) {
        const double mean = st.mean();
        const double stdev = st.stdev(mean);
        const double max = st.max();
        const double p = st.inStdevPercent(mean, stdev, 1);

        std::cerr << "    " << name
                  << std::setw(9) << moros::numfmt(fn(mean))
                  << std::setw(11) << moros::numfmt(fn(stdev))
                  << std::setw(8) << moros::numfmt(fn(max))
                  << std::setw(12) << p << "%\n";
    };
    print_stats("Latency", *latency, [](auto x) {
        return std::chrono::milliseconds(static_cast<std::uint64_t>(x));
    });
    print_stats("Req/Sec", *requests, [](auto x) { return x; });

    if (cfg.display_latency) {
        std::cerr << "  Latency Distribution\n";
        for (double p : {0.55, 0.75, 0.95, 0.99}) {
            const std::chrono::milliseconds ms(latency->derank(p));
            std::cerr << std::setw(7) << 100 * p << "%\t" << moros::numfmt(ms) << '\n';
        }
    }

    // total requests and bytes
    std::cerr << "  "
              << moros::Metrics::getInstance()[moros::Metrics::Kind::COMPLETES]
              << " requests in " << moros::numfmt(runtime) << ", "
              << moros::numfmt(
                     moros::Metrics::getInstance()[moros::Metrics::Kind::BYTES])
              << "B read" << std::endl;

    // socket errors
    if (moros::Metrics::getInstance()[moros::Metrics::Kind::ECONNECT] ||
        moros::Metrics::getInstance()[moros::Metrics::Kind::EREAD] ||
        moros::Metrics::getInstance()[moros::Metrics::Kind::EWRITE] ||
        moros::Metrics::getInstance()[moros::Metrics::Kind::ETIMEOUT]) {
        std::cerr << "  Socket errors:"
                  << "connect " << moros::Metrics::getInstance()[moros::Metrics::Kind::ECONNECT]
                  << ", read " << moros::Metrics::getInstance()[moros::Metrics::Kind::EREAD]
                  << ", write " << moros::Metrics::getInstance()[moros::Metrics::Kind::EWRITE]
                  << ", timeout " << moros::Metrics::getInstance()[moros::Metrics::Kind::ETIMEOUT]
                  << std::endl;
    }

    // http status code errors
    if (moros::Metrics::getInstance()[moros::Metrics::Kind::ESTATUS]) {
        std::cerr << "  Non-2xx or 3xx responses: "
                  << moros::Metrics::getInstance()[moros::Metrics::Kind::ESTATUS]
                  << std::endl;
    }

    // request per sec
    std::cerr << "Requests/sec: "
              << moros::Metrics::getInstance()[moros::Metrics::Kind::COMPLETES] *
                     1000.0 / runtime.count()
              << '\n'
              << "Transfer/sec: "
              << moros::numfmt(
                     moros::Metrics::getInstance()[moros::Metrics::Kind::BYTES] *
                     1000.0 / runtime.count())
              << "B" << std::endl;

    return 0;
}
