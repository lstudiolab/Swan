#include "../../runtime/json_abi.c"

const char *swan_json_benchmark_payload(int64_t unused) {
    (void)unused;
    return "{\"id\":42,\"workers\":128,\"enabled\":true,\"service\":\"swan\",\"items\":[1,2,3,4,5],\"meta\":{\"region\":\"ap-southeast-1\",\"version\":1}}";
}
