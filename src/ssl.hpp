#ifndef MOROS_SSL_HPP_
#define MOROS_SSL_HPP_

#include <memory>
#include <openssl/ossl_typ.h>

namespace moros {

class SslContext {
public:
    SslContext();

    SSL_CTX* data() const noexcept;

private:
    std::unique_ptr<SSL_CTX, void (*)(SSL_CTX*)> ssl_ctx_;
};

class Ssl {
public:
    Ssl() noexcept : ssl_(nullptr, nullptr) {}

    Ssl(const SslContext& ssl_ctx);

    Ssl(Ssl&& ssl) noexcept
        : ssl_(std::move(ssl.ssl_)), enabled_(ssl.enabled_) {
        ssl.ssl_.reset();
    }

    bool enabled() const noexcept {
        return enabled_;
    }

    void fd(int f) noexcept;

    void sni(const char* host) noexcept;

    int connect() noexcept;

    int close() noexcept;

    int read(char buf[], std::size_t len) noexcept;

    int write(const char buf[], std::size_t len) noexcept;

private:
    std::unique_ptr<SSL, void (*)(SSL*)> ssl_;
    bool enabled_ = false;
};

}

#endif
