#include "../../runtime/json_abi.c"

typedef struct {
    int64_t checksum;
    int64_t bytes;
} SwanJsonBenchmarkResult;

static SwanJsonBenchmarkResult result;

const char *swan_json_benchmark_payload(int64_t unused) {
    (void)unused;
    return "{\"id\":42,\"workers\":128,\"enabled\":true,\"service\":\"swan\",\"items\":[1,2,3,4,5],\"meta\":{\"region\":\"ap-southeast-1\",\"version\":1}}";
}

void *swan_json_benchmark_run(const char *payload, int64_t requests) {
    if (!payload || requests <= 0) return NULL;
    void *document = swan_json_create(64);
    if (!document) return NULL;
    int64_t checksum = 0;
    int64_t bytes = 0;
    for (int64_t i = 0; i < requests; i++) {
        if (!swan_json_parse_text(document, payload)) {
            swan_json_close(document);
            return NULL;
        }
        int64_t id = swan_json_get_i64(document, "id", -1);
        int64_t workers = swan_json_get_i64(document, "workers", -1);
        int64_t input_length = swan_json_input_length(document);
        int64_t output_length = swan_json_compact_length(document);
        if (id != 42 || workers != 128 || input_length <= 0 ||
            output_length != input_length) {
            swan_json_close(document);
            return NULL;
        }
        checksum += id + workers;
        bytes += input_length + output_length;
    }
    swan_json_close(document);
    result.checksum = checksum;
    result.bytes = bytes;
    return &result;
}

int64_t swan_json_benchmark_checksum(void *value) {
    SwanJsonBenchmarkResult *r = (SwanJsonBenchmarkResult *)value;
    return r ? r->checksum : -1;
}

int64_t swan_json_benchmark_bytes(void *value) {
    SwanJsonBenchmarkResult *r = (SwanJsonBenchmarkResult *)value;
    return r ? r->bytes : -1;
}
