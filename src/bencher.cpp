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
                 const std::string& req)
    : ev_loop_(nconn), addr_(addr) {
    for (std::size_t i = 0; i < nconn; ++i) {
        auto c = std::make_shared<Connection>(ev_loop_, *this, req);
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

Connection::Connection(EventLoop& ev_loop, Bencher& b, const std::string& req)
    : ev_loop_(ev_loop), bencher_(b), req_(req) {
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

        if (!http_should_keep_alive(parser)) {
            c->reconnect();
        } else {
            c->written_ = 0;
            http_parser_init(parser, HTTP_RESPONSE);
        }

        return 0;
    };
}

void Connection::reconnect() {
    // 延长生命周期，delEvent 会删除 Connection 的拷贝
    auto self = shared_from_this();

    ev_loop_.delEvent(fd_, Mask::READABLE | Mask::WRITABLE);

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
    if (!ev_loop_.addEvent(fd, Mask::WRITABLE, [self] { self->request(); }) ||
        !ev_loop_.addEvent(fd, Mask::READABLE, [self] { self->response(); })) {
        return;
    }

    dismiss = true;

    fd_ = fd;
    written_ = 0;
    http_parser_init(&parser_, HTTP_RESPONSE);
}

void Connection::request() {
    if (written_ == 0) {
        // TODO dynamic request

        start_ = std::chrono::steady_clock::now();
    }

    while (written_ < req_.size()) {
        const char* buf = req_.data() + written_;
        const std::size_t len = req_.size() - written_;

        const ssize_t n = ::write(fd_, buf, len);
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
    while ((n = ::read(fd_, buf_, sizeof(buf_))) > 0) {
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

}
