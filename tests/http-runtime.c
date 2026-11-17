#include "runtime/http.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void parse_ok(void) {
    const char request[] =
        "POST /v1/jobs?limit=4 HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 7\r\n\r\n"
        "{\"x\":1}";
    SwanHttpRequest parsed;
    assert(swan_http_parse_request(request, sizeof(request) - 1, 0, 0, &parsed) == SWAN_HTTP_OK);
    assert(parsed.method_length == 4 && !memcmp(parsed.method, "POST", 4));
    assert(parsed.path_length == 8 && !memcmp(parsed.path, "/v1/jobs", 8));
    assert(parsed.query_length == 7 && !memcmp(parsed.query, "limit=4", 7));
    assert(parsed.body_length == 7 && !memcmp(parsed.body, "{\"x\":1}", 7));
    const SwanHttpHeader *host = swan_http_header(&parsed, "HOST");
    assert(host && host->value_length == 9 && !memcmp(host->value, "localhost", 9));
}

static void parse_errors(void) {
    SwanHttpRequest parsed;
    const char *incomplete = "GET / HTTP/1.1\r\nHost: localhost\r\n";
    assert(swan_http_parse_request(incomplete, strlen(incomplete), 0, 0, &parsed) == SWAN_HTTP_INCOMPLETE);
    const char *missing_host = "GET / HTTP/1.1\r\n\r\n";
    assert(swan_http_parse_request(missing_host, strlen(missing_host), 0, 0, &parsed) == SWAN_HTTP_BAD_REQUEST);
    const char *conflicting =
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 1\r\n"
        "Content-Length: 2\r\n\r\nx";
    assert(swan_http_parse_request(conflicting, strlen(conflicting), 0, 0, &parsed) == SWAN_HTTP_BAD_REQUEST);
    const char *transfer =
        "POST / HTTP/1.1\r\nHost: x\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
    assert(swan_http_parse_request(transfer, strlen(transfer), 0, 0, &parsed) == SWAN_HTTP_UNSUPPORTED);
    const char *bad_name = "GET / HTTP/1.1\r\nHost : x\r\n\r\n";
    assert(swan_http_parse_request(bad_name, strlen(bad_name), 0, 0, &parsed) == SWAN_HTTP_BAD_REQUEST);
    const char *too_large =
        "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 999\r\n\r\n";
    assert(swan_http_parse_request(too_large, strlen(too_large), 0, 8, &parsed) == SWAN_HTTP_TOO_LARGE);
}

static void response_ok(void) {
    SwanByteBuffer response;
    swan_bytes_init(&response);
    const char body[] = "{\"ok\":true}";
    assert(swan_http_response(&response, 200, "OK", "application/json",
                              body, sizeof(body) - 1));
    assert(strstr((const char *)response.data, "Content-Length: 11\r\n"));
    assert(response.length > sizeof(body));
    assert(!swan_http_response(&response, 200, "OK\r\nInjected: yes",
                               "application/json", body, sizeof(body) - 1));
    assert(!swan_http_response(&response, 200, "OK", "application/json\r\nX: y",
                               body, sizeof(body) - 1));
    swan_bytes_destroy(&response);
}

int main(void) {
    parse_ok();
    parse_errors();
    response_ok();
    puts("HTTP runtime tests passed");
    return 0;
}
