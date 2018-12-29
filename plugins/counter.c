#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

static __thread size_t counter = 0;

const char* request(const char* schema, const char* host, const char* port,
                    const char* service, const char* query_string,
                    const char* headers[]) {
    (void)schema;
    (void)port;
    (void)service;
    (void)query_string;

    static __thread char buf[8192];

    int n = snprintf(buf, sizeof(buf), "GET /%zu HTTP/1.1\r\n"
                                       "Host: %s\r\n", counter++, host);
    if (n < 0) {
        return NULL;
    }

    for (size_t i = 0; headers[i]; ++i) {
        int r = snprintf(buf + n, sizeof(buf) - n, "%s\r\n", headers[i]);
        if (r < 0) {
            break;
        }
        n += r;
    }

    int r = snprintf(buf + n, sizeof(buf) - n, "\r\n");
    if (r < 0) {
        return NULL;
    }

    return buf;
}

void summary() {
    printf("tid: %lu counter %zu\n", syscall(SYS_gettid), counter);
}
