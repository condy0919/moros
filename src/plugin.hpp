#ifndef MOROS_PLUGIN_HPP_
#define MOROS_PLUGIN_HPP_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace moros {

class Plugin {
public:
    Plugin(std::string schema, std::string host, std::string port,
           std::string service, std::string query_string,
           std::vector<std::string> headers) noexcept;

    void load(std::string so);

    bool wantResponseHeaders() const noexcept;
    bool wantResponseBody() const noexcept;

    // hooks
    void setup();
    void init();
    void request(std::string& req);
    void response(std::uint32_t status, std::string headers, std::string body);
    void summary();

private:
    std::string schema_;
    std::string host_;
    std::string port_;
    std::string service_;
    std::string query_string_;
    std::vector<std::string> headers_;

    std::unique_ptr<void, int (*)(void*)> so_;

    void (*setup_)() = nullptr;
    void (*init_)() = nullptr;
    const char* (*request_)(const char* schema, const char* host,
                            const char* port, const char* service,
                            const char* query_string,
                            const char* headers[]) = nullptr;
    void (*response_)(std::uint32_t status, const char* headers[],
                      const char* body, std::size_t body_len) = nullptr;
    void (*summary_)() = nullptr;

    bool want_response_headers_ = false;
    bool want_response_body_ = false;

    constexpr static std::size_t HEADERS_MAX_SIZE = 32;
};

}

#endif
