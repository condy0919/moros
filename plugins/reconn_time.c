#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

int want_response_headers = 0;

size_t reconnect_times = 0;
int summary_displayed = 0;

void setup() {
    printf("setup before benchers initialized\n");
}

void init() {
    printf("bencher starts\n");
}

void response(uint32_t status, const char* headers[], const char* body,
              size_t body_len) {
    (void)status;
    (void)body;
    (void)body_len;

    for (size_t i = 0; headers[i]; ++i) {
        if (strncasecmp(headers[i], "connection:close", 16) == 0) {
            __atomic_add_fetch(&reconnect_times, 1, __ATOMIC_RELAXED);
            return;
        }
    }
}

void summary() {
    int ov = 0, nv = 0;

    ov = __atomic_load_n(&summary_displayed, __ATOMIC_ACQUIRE);
    do {
        if (ov > 0) {
            return;
        }
        nv = ov + 1;
    } while (!__atomic_compare_exchange_n(&summary_displayed, &ov, nv, 1,
                                          __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE));

    printf("reconnect times = %zu\n", reconnect_times);
}
