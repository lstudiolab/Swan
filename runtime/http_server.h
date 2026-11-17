#ifndef SWAN_RUNTIME_HTTP_SERVER_H
#define SWAN_RUNTIME_HTTP_SERVER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Bounded HTTP/1.1 JSON echo server.
 * One request per connection; request bodies must be JSON.
 * Chunked transfer encoding and keep-alive are intentionally unsupported.
 * run() blocks; call stop() from another thread, wait for run() to return,
 * then call destroy().
 */
void *swan_http_json_server_create(int64_t port, int64_t workers,
                                   int64_t queue_capacity);
int32_t swan_http_json_server_run(void *server);
void swan_http_json_server_stop(void *server);
void swan_http_json_server_destroy(void *server);
int64_t swan_http_json_server_port(void *server);
int64_t swan_http_json_server_completed(void *server);
int64_t swan_http_json_server_rejected(void *server);
int64_t swan_http_json_server_input_bytes(void *server);
int64_t swan_http_json_server_output_bytes(void *server);

#ifdef __cplusplus
}
#endif
#endif
