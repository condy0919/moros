#include "ssl.hpp"
#include <stdexcept>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/err.h>

namespace moros {

SslContext::SslContext() : ssl_ctx_(nullptr, nullptr) {
    SSL_CTX* ctx = nullptr;

    SSL_load_error_strings();
    SSL_library_init();
    OpenSSL_add_all_algorithms();

    if (!(ctx = SSL_CTX_new(SSLv23_client_method()))) {
        throw std::runtime_error("ssl init failed");
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, nullptr);
    SSL_CTX_set_verify_depth(ctx, 0);
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_CLIENT);

    ssl_ctx_ = std::unique_ptr<SSL_CTX, void (*)(SSL_CTX*)>(ctx, SSL_CTX_free);
}

SSL_CTX* SslContext::data() const noexcept {
    return ssl_ctx_.get();
}

Ssl::Ssl(const SslContext& ssl_ctx)
    : ssl_(SSL_new(ssl_ctx.data()), SSL_free), enabled_(true) {}

void Ssl::fd(int f) noexcept {
    SSL_set_fd(ssl_.get(), f);
}

void Ssl::sni(const char* host) noexcept {
    SSL_set_tlsext_host_name(ssl_.get(), host);
}

int Ssl::connect() noexcept {
    int r = SSL_connect(ssl_.get());
    if (r != 1) {
        switch (SSL_get_error(ssl_.get(), r)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            errno = EAGAIN;
            break;
        default:
            errno = 0;
            break;
        }
        return -1;
    }
    return r;
}

int Ssl::close() noexcept {
    SSL_clear(ssl_.get());
    return 0;
}

int Ssl::read(char buf[], std::size_t len) noexcept {
    int r = SSL_read(ssl_.get(), buf, len);
    if (r < 0) {
        switch (SSL_get_error(ssl_.get(), r)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            errno = EAGAIN;
            break;
        default:
            errno = 0;
            break;
        }
        return -1;
    }
    return r;
}

int Ssl::write(const char buf[], std::size_t len) noexcept {
    int r = SSL_write(ssl_.get(), buf, len);
    if (r < 0) {
        switch (SSL_get_error(ssl_.get(), r)) {
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            errno = EAGAIN;
            break;
        default:
            errno = 0;
            break;
        }
        return -1;
    }
    return r;
}

}
