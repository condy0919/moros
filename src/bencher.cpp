#include "bencher.hpp"
#include "stats.hpp"
#include <boost/scope_exit.hpp>

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

extern std::unique_ptr<moros::Stats> requests;
extern std::unique_ptr<moros::Stats> latency;

namespace moros {

Bencher::Bencher(struct addrinfo addr, std::size_t nconn,
                 const std::string& host, const std::string& req,
                 const SslContext* ssl_ctx, Plugin& plugin)
    : ev_loop_(nconn), addr_(addr), plugin_(plugin) {
    for (std::size_t i = 0; i < nconn; ++i) {
        std::shared_ptr<Connection> c =
            ssl_ctx ? std::make_shared<SslConnection>(ev_loop_, *this, host, req, Ssl(*ssl_ctx), plugin)
                    : std::make_shared<Connection>(ev_loop_, *this, host, req, Ssl(), plugin);
        c->connect();
    }

    start_ = std::chrono::steady_clock::now();
    requests_ = 0;
    ev_loop_.addTimeEvent(std::chrono::milliseconds(100), [this]() {
        if (requests_ > 0) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_);
            const double qps = 1000.0 * requests_ / elapsed.count();
            requests->record(qps);

            requests_ = 0;
            start_ = std::chrono::steady_clock::now();
        }
    });

    plugin_.init();
}

void Bencher::run() noexcept {
    ev_loop_.run();
}

void Bencher::stop() noexcept {
    ev_loop_.stop();
}

const struct addrinfo& Bencher::addr() const noexcept {
    return addr_;
}

void Bencher::countReq() noexcept {
    ++requests_;
}

void Bencher::summary() {
    plugin_.summary();
}

Connection::Connection(EventLoop& ev_loop, Bencher& b, const std::string& host,
                       const std::string& req, Ssl ssl, Plugin& plugin)
    : ev_loop_(ev_loop),
      bencher_(b),
      ssl_(std::move(ssl)),
      host_(host),
      req_(req),
      plugin_(plugin) {
    http_parser_init(&parser_, HTTP_RESPONSE);
    parser_.data = this;

    http_parser_settings_init(&parser_settings_);
    parser_settings_.on_message_complete = [](http_parser* parser) {
        auto c = static_cast<Connection*>(parser->data);

        const unsigned status = parser->status_code;

        Metrics::getInstance().count(Metrics::Kind::COMPLETES);
        c->bencher_.countReq();

        if (status > 399) {
            Metrics::getInstance().count(Metrics::Kind::ESTATUS);
        }

        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - c->start_);
        if (!latency->record(elapsed.count())) {
            Metrics::getInstance().count(Metrics::Kind::ETIMEOUT);
        }

        c->plugin_.response(status, std::move(c->headers_), std::move(c->body_));

        if (!http_should_keep_alive(parser)) {
            c->reconnect();
        } else {
            c->written_ = 0;
            c->body_.clear();
            c->headers_.clear();
            c->header_state_ = HeaderState::FIELD;
            http_parser_init(parser, HTTP_RESPONSE);
        }

        return 0;
    };

    if (plugin.wantResponseHeaders()) {
        parser_settings_.on_header_field = [](http_parser* parser,
                                              const char* s, std::size_t len) {
            auto c = static_cast<Connection*>(parser->data);
            if (c->header_state_ == HeaderState::VALUE) {
                c->headers_.push_back('\x01');
                c->header_state_ = HeaderState::FIELD;
            }
            c->headers_.append(s, len);

            return 0;
        };

        parser_settings_.on_header_value = [](http_parser* parser,
                                              const char* s, std::size_t len) {
            auto c = static_cast<Connection*>(parser->data);
            if (c->header_state_ == HeaderState::FIELD) {
                c->headers_.push_back(':');
                c->header_state_ = HeaderState::VALUE;
            }
            c->headers_.append(s, len);

            return 0;
        };
    }

    if (plugin.wantResponseBody()) {
        parser_settings_.on_body = [](http_parser* parser, const char* s,
                                      std::size_t len) {
            auto c = static_cast<Connection*>(parser->data);
            c->body_.append(s, len);

            return 0;
        };
    }
}

void Connection::reconnect() {
    // 延长生命周期，delEvent 会删除 Connection 的拷贝
    auto self = shared_from_this();

    ev_loop_.delEvent(fd_, Mask::READABLE | Mask::WRITABLE);

    close();
    ::close(fd_);
    connect();
}

void Connection::connect() {
    bool dismiss = false;
    BOOST_SCOPE_EXIT_ALL(&) {
        if (!dismiss) {
            Metrics::getInstance().count(Metrics::Kind::ECONNECT);
        }
    };

    int fd = ::socket(bencher_.addr().ai_family,
                      bencher_.addr().ai_socktype | O_NONBLOCK,
                      bencher_.addr().ai_protocol);
    if (fd == -1) {
        return;
    }

    if (::connect(fd, bencher_.addr().ai_addr, bencher_.addr().ai_addrlen) == -1) {
        if (errno != EINPROGRESS) {
            ::close(fd);
            return;
        }
    }

    const int flags = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));

    auto self = shared_from_this();
    if (!ev_loop_.addEvent(fd, Mask::WRITABLE, [self] { self->connected(); }) ||
        !ev_loop_.addEvent(fd, Mask::READABLE, [self] { self->response(); })) {
        return;
    }

    dismiss = true;

    fd_ = fd;
    written_ = 0;
    body_.clear();
    headers_.clear();
    header_state_ = HeaderState::FIELD;
    http_parser_init(&parser_, HTTP_RESPONSE);
}

void Connection::connected() {
    auto self = shared_from_this();
    if (!ev_loop_.addEvent(fd_, Mask::WRITABLE, [self] { self->request(); })) {
        return;
    }

    request();
}

void Connection::request() {
    if (written_ == 0) {
        plugin_.request(req_);

        start_ = std::chrono::steady_clock::now();
    }

    while (written_ < req_.size()) {
        const char* buf = req_.data() + written_;
        const std::size_t len = req_.size() - written_;

        const ssize_t n = write(buf, len);
        if (n >= 0) {
            written_ += static_cast<std::size_t>(n);
        } else if (errno == EAGAIN) {
            break;
        } else {
            Metrics::getInstance().count(Metrics::Kind::EWRITE);
            reconnect();
        }
    }
}

void Connection::response() {
    ssize_t n = 0;
    while ((n = read(buf_, sizeof(buf_))) > 0) {
        Metrics::getInstance().count(Metrics::Kind::BYTES, n);

        if (http_parser_execute(&parser_, &parser_settings_, buf_, n) != static_cast<std::size_t>(n)) {
            Metrics::getInstance().count(Metrics::Kind::EREAD);
            reconnect();
            return;
        }
    }

    if (n == 0) {
        if (!http_body_is_final(&parser_)) {
            Metrics::getInstance().count(Metrics::Kind::EREAD);
        }
        reconnect();
    } else if (errno != EAGAIN) {
        Metrics::getInstance().count(Metrics::Kind::EREAD);
        reconnect();
    }
}

int Connection::read(char buf[], std::size_t len) noexcept {
    return ::read(fd_, buf, len);
}

int Connection::write(const char buf[], std::size_t len) noexcept {
    return ::write(fd_, buf, len);
}

int Connection::close() noexcept {
    return 0;
}

void SslConnection::connected() {
    auto self = shared_from_this();
    if (!ev_loop_.addEvent(fd_, Mask::WRITABLE, [self]() { self->request(); })) {
        return;
    }

    ssl_.fd(fd_);
    ssl_.sni(host_.c_str());
    ssl_.connect();
}

int SslConnection::read(char buf[], std::size_t len) noexcept {
    return ssl_.read(buf, len);
}

int SslConnection::write(const char buf[], std::size_t len) noexcept {
    return ssl_.write(buf, len);
}

int SslConnection::close() noexcept {
    return ssl_.close();
}

}
