#ifndef MOROS_BENCHER_HPP_
#define MOROS_BENCHER_HPP_

#include "ev.hpp"
#include "ssl.hpp"
#include "http_parser.h"
#include <chrono>
#include <string>
#include <memory>
#include <atomic>
#include <netdb.h>

namespace moros {

class Bencher {
public:
    Bencher(struct addrinfo addr, std::size_t nconn, const std::string& host,
            const std::string& req, const SslContext* ssl_ctx);

    void run() noexcept;
    void stop() noexcept;

    const struct addrinfo& addr() const noexcept;

    void countReq() noexcept;

private:
    EventLoop ev_loop_;

    struct addrinfo addr_;

    std::chrono::steady_clock::time_point start_;
    std::uint64_t requests_;
};

class Connection : public std::enable_shared_from_this<Connection> {
public:
    Connection(EventLoop& ev_loop, Bencher& b, const std::string& host,
               const std::string& req, Ssl ssl);

    void connect();
    void reconnect();

    virtual void connected();

    void request();
    void response();

private:
    virtual int read(char buf[], std::size_t len) noexcept;
    virtual int write(const char buf[], std::size_t len) noexcept;
    virtual int close() noexcept;

protected:
    EventLoop& ev_loop_;
    Bencher& bencher_;

    http_parser parser_;
    http_parser_settings parser_settings_;

    int fd_ = -1;
    Ssl ssl_;

    std::string host_;

    std::string req_;
    std::size_t written_;

    char buf_[8192];

    std::chrono::steady_clock::time_point start_;
};

class SslConnection : public Connection {
public:
    using Connection::Connection;

    void connected() override;

private:
    int read(char buf[], std::size_t len) noexcept override;
    int write(const char buf[], std::size_t len) noexcept override;
    int close() noexcept override;
};

}

#endif
