#ifndef MOROS_BENCHER_HPP_
#define MOROS_BENCHER_HPP_

#include "ev.hpp"
#include "http_parser.h"
#include <chrono>
#include <string>
#include <memory>
#include <atomic>

#include <netdb.h>

namespace moros {

class Bencher {
public:
    Bencher(struct addrinfo addr, std::size_t nconn, const std::string& req);

    void run() noexcept;
    void stop() noexcept;

    const struct addrinfo& addr() const noexcept;

private:
    EventLoop ev_loop_;

    struct addrinfo addr_;
};

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(EventLoop& ev_loop, Bencher& b, const std::string& req);

    void reconnect();
    void connect();

    void request();
    void response();

private:
    EventLoop& ev_loop_;
    Bencher& bencher_;

    http_parser parser_;
    http_parser_settings parser_settings_;

    int fd_ = -1;

    std::chrono::steady_clock::time_point start_;
    
    std::string req_;
    std::size_t written_;

    char buf_[8192];
};

}

#endif
