#include "plugin.hpp"
#include <stdexcept>
#include <dlfcn.h>
#include <string.h> // for strsep

namespace moros {

Plugin::Plugin(std::string schema, std::string host, std::string port,
               std::string service, std::string query_string,
               std::vector<std::string> headers) noexcept
    : schema_(std::move(schema)),
      host_(std::move(host)),
      port_(std::move(port)),
      service_(std::move(service)),
      query_string_(std::move(query_string)),
      headers_(std::move(headers)),
      so_(nullptr, nullptr) {}

void Plugin::load(std::string so) {
    void* handle = ::dlopen(so.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        throw std::runtime_error(::dlerror());
    }

#define LOAD(name) \
    name##_ = reinterpret_cast<decltype(name##_)>(::dlsym(handle, #name))

    LOAD(setup);
    LOAD(init);
    LOAD(request);
    LOAD(response);
    LOAD(summary);
#undef LOAD

    if (::dlsym(handle, "want_response_headers")) {
        want_response_headers_ = true;
    }

    if (::dlsym(handle, "want_response_body")) {
        want_response_body_ = true;
    }
}

bool Plugin::wantResponseHeaders() const noexcept {
    return want_response_headers_;
}

bool Plugin::wantResponseBody() const noexcept {
    return want_response_body_;
}

void Plugin::setup() {
    if (setup_) {
        setup_();
    }
}

void Plugin::init() {
    if (init_) {
        init_();
    }
}

void Plugin::request(std::string& req) {
    if (request_) {
        thread_local static const char* headers[HEADERS_MAX_SIZE] = {nullptr};
        for (std::size_t i = 0; i < headers_.size() && i < HEADERS_MAX_SIZE - 1; ++i) {
            headers[i] = headers_[i].c_str();
        }
        headers[std::min(headers_.size(), HEADERS_MAX_SIZE - 1)] = nullptr;

        if (const char* s =
                request_(schema_.c_str(), host_.c_str(), port_.c_str(),
                         service_.c_str(), query_string_.c_str(), headers)) {
            req.assign(s);
        }
    }
}

void Plugin::response(std::uint32_t status, std::string headers, std::string body) {
    if (response_) {
        thread_local static const char* hs[HEADERS_MAX_SIZE] = {nullptr};

        char* sp = &headers[0];

        std::size_t i = 0;
        while (const char* h = strsep(&sp, "\x01")) {
            if (i >= HEADERS_MAX_SIZE - 1) {
                break;
            }

            hs[i++] = h;
        }
        hs[std::min(i, HEADERS_MAX_SIZE)] = nullptr;

        response_(status, hs, body.c_str(), body.size());
    }
}

void Plugin::summary() {
    if (summary_) {
        summary_();
    }
}

}
