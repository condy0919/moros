#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

// 通过这个符号, response 中的 body, body_len 有意义了
int want_response_body = 0;

__thread size_t total_flux_in = 0;

void response(uint32_t status, const char* headers[], const char* body,
              size_t body_len) {
    (void)status;
    (void)headers;
    (void)body;

    total_flux_in += body_len;
}

void summary() {
    printf("total flux in = %zu\n", total_flux_in);
}
