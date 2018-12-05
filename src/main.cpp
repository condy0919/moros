#include "config.hpp"
#include "bencher.hpp"
#include "version.hpp"
#include "stdhack.hpp"
#include "http_parser.h"
#include <csignal>
#include <memory>
#include <iostream>
#include <thread>
#include <boost/format.hpp>
#include <boost/program_options.hpp>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>


namespace po = boost::program_options;

static moros::Config cfg;

std::vector<moros::Bencher> benchers;

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


    std::this_thread::sleep_for(cfg.duration);
    for (auto& b : benchers) {
        b.stop();
    }

    for (auto& t : thread_group) {
        if (t.joinable()) {
            t.join();
        }
    }

    return 0;
}
