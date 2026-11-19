#ifndef BREW_JSON_BENCHMARK_H
#define BREW_JSON_BENCHMARK_H
#include <stdint.h>
const char *brew_json_benchmark_payload(int64_t unused);
void *brew_json_benchmark_run(const char *payload, int64_t requests);
int64_t brew_json_benchmark_checksum(void *result);
int64_t brew_json_benchmark_bytes(void *result);
#endif
