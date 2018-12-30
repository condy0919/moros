#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

__thread size_t counter = 0;

void response(uint32_t status, const char* headers[], const char* body,
              size_t body_len) {
    (void)headers;
    (void)body;
    (void)body_len;

    if (status == 200) {
        ++counter;
    }

    if (counter == 1000) {
        // XXX fvck
        exit(0);
    }
}
