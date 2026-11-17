#define _POSIX_C_SOURCE 200809L
#include "http_server.h"
#include "json_abi.h"
#include "thread_pool.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define SWAN_HTTP_HEADER_LIMIT 16384u
#define SWAN_HTTP_BODY_LIMIT (8u * 1024u * 1024u)

typedef struct SwanHttpServer SwanHttpServer;
typedef struct {
    SwanHttpServer *server;
    int socket_fd;
} SwanHttpClient;

struct SwanHttpServer {
    int listen_fd;
    uint16_t port;
    SwanThreadPool pool;
    atomic_bool stopping;
    atomic_bool running;
    atomic_int_fast64_t completed;
    atomic_int_fast64_t rejected;
    atomic_int_fast64_t input_bytes;
    atomic_int_fast64_t output_bytes;
};

static bool swan_http_send_all(int fd, const void *data, size_t length) {
    const char *p = (const char *)data;
    while (length) {
        ssize_t sent = send(fd, p, length, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (!sent) return false;
        p += sent;
        length -= (size_t)sent;
    }
    return true;
}

static const char *swan_http_reason(int status) {
    switch (status) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 413: return "Payload Too Large";
        case 431: return "Request Header Fields Too Large";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 503: return "Service Unavailable";
        default: return "Error";
    }
}

static void swan_http_reply(int fd, int status, const char *body, size_t length) {
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Cache-Control: no-store\r\n\r\n",
        status, swan_http_reason(status), length);
    if (n > 0 && (size_t)n < sizeof(header)) {
        (void)swan_http_send_all(fd, header, (size_t)n);
        if (length) (void)swan_http_send_all(fd, body, length);
    }
}

static char *swan_http_header_end(char *data, size_t length) {
    if (length < 4) return NULL;
    for (size_t i = 0; i + 3 < length; i++)
        if (data[i] == '\r' && data[i + 1] == '\n' &&
            data[i + 2] == '\r' && data[i + 3] == '\n')
            return data + i;
    return NULL;
}

static int swan_http_parse_content_length(char *headers, size_t *content_length) {
    bool found = false;
    char *line = strstr(headers, "\r\n");
    if (!line) return -1;
    line += 2;
    while (*line) {
        char *end = strstr(line, "\r\n");
        if (!end) return -1;
        if (end == line) break;
        *end = 0;
        char *colon = strchr(line, ':');
        if (!colon) return -1;
        *colon++ = 0;
        while (*colon == ' ' || *colon == '\t') colon++;
        if (!strcasecmp(line, "Content-Length")) {
            if (found || !*colon) return -1;
            found = true;
            size_t value = 0;
            for (char *p = colon; *p; p++) {
                if (*p < '0' || *p > '9') return -1;
                unsigned digit = (unsigned)(*p - '0');
                if (value > (SIZE_MAX - digit) / 10) return -1;
                value = value * 10 + digit;
            }
            *content_length = value;
        } else if (!strcasecmp(line, "Transfer-Encoding")) {
            return -2;
        }
        line = end + 2;
    }
    return found ? 1 : 0;
}

static void swan_http_process_client(void *context) {
    SwanHttpClient *client = (SwanHttpClient *)context;
    SwanHttpServer *server = client->server;
    int fd = client->socket_fd;
    free(client);

    struct timeval timeout = {5, 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    char headers[SWAN_HTTP_HEADER_LIMIT + 1];
    size_t used = 0;
    char *separator = NULL;
    while (used < SWAN_HTTP_HEADER_LIMIT) {
        ssize_t n = recv(fd, headers + used, SWAN_HTTP_HEADER_LIMIT - used, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { close(fd); return; }
        used += (size_t)n;
        separator = swan_http_header_end(headers, used);
        if (separator) break;
    }
    if (!separator) {
        swan_http_reply(fd, 431, "{\"error\":\"headers_too_large\"}", 29);
        close(fd);
        return;
    }

    size_t header_length = (size_t)(separator - headers);
    size_t body_offset = header_length + 4;
    size_t already_read = used - body_offset;
    *separator = 0;

    char method[16], target[2048], version[16];
    if (sscanf(headers, "%15s %2047s %15s", method, target, version) != 3 ||
        strncmp(version, "HTTP/1.", 7) != 0) {
        swan_http_reply(fd, 400, "{\"error\":\"bad_request_line\"}", 28);
        close(fd);
        return;
    }

    size_t body_length = 0;
    int length_status = swan_http_parse_content_length(headers, &body_length);
    if (length_status == -2) {
        swan_http_reply(fd, 501, "{\"error\":\"chunked_not_supported\"}", 33);
        close(fd);
        return;
    }
    if (length_status < 0 || (length_status == 0 && already_read != 0)) {
        swan_http_reply(fd, 400, "{\"error\":\"invalid_content_length\"}", 34);
        close(fd);
        return;
    }
    if (body_length > SWAN_HTTP_BODY_LIMIT) {
        swan_http_reply(fd, 413, "{\"error\":\"body_too_large\"}", 26);
        close(fd);
        return;
    }

    char *body = (char *)malloc(body_length + 1);
    if (!body) {
        swan_http_reply(fd, 500, "{\"error\":\"out_of_memory\"}", 25);
        close(fd);
        return;
    }
    size_t copied = already_read < body_length ? already_read : body_length;
    if (copied) memcpy(body, headers + body_offset, copied);
    size_t received = copied;
    while (received < body_length) {
        ssize_t n = recv(fd, body + received, body_length - received, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            free(body);
            swan_http_reply(fd, 400, "{\"error\":\"incomplete_body\"}", 27);
            close(fd);
            return;
        }
        received += (size_t)n;
    }
    body[body_length] = 0;
    atomic_fetch_add_explicit(&server->input_bytes, (int64_t)body_length,
                              memory_order_relaxed);

    if (!strcmp(method, "GET") && body_length == 0) {
        static const char ok[] = "{\"ok\":true}";
        swan_http_reply(fd, 200, ok, sizeof(ok) - 1);
        atomic_fetch_add_explicit(&server->output_bytes, sizeof(ok) - 1,
                                  memory_order_relaxed);
        atomic_fetch_add_explicit(&server->completed, 1, memory_order_relaxed);
        free(body);
        close(fd);
        return;
    }

    void *document = swan_json_create(64);
    if (!document) {
        free(body);
        swan_http_reply(fd, 500, "{\"error\":\"out_of_memory\"}", 25);
        close(fd);
        return;
    }
    if (!swan_json_parse_text(document, body)) {
        swan_json_close(document);
        free(body);
        static const char invalid[] = "{\"error\":\"invalid_json\"}";
        swan_http_reply(fd, 400, invalid, sizeof(invalid) - 1);
        close(fd);
        return;
    }
    const char *output = swan_json_compact(document);
    int64_t output_length = swan_json_compact_length(document);
    if (!output || output_length < 0) {
        swan_json_close(document);
        free(body);
        swan_http_reply(fd, 500, "{\"error\":\"encode_failed\"}", 26);
        close(fd);
        return;
    }
    swan_http_reply(fd, 200, output, (size_t)output_length);
    atomic_fetch_add_explicit(&server->output_bytes, output_length,
                              memory_order_relaxed);
    atomic_fetch_add_explicit(&server->completed, 1, memory_order_relaxed);
    swan_json_close(document);
    free(body);
    close(fd);
}

void *swan_http_json_server_create(int64_t port, int64_t workers,
                                   int64_t queue_capacity) {
    if (port < 0 || port > 65535 || workers < 1 || workers > 256 ||
        queue_capacity < 1 || queue_capacity > 65536) return NULL;
    SwanHttpServer *server = (SwanHttpServer *)calloc(1, sizeof(*server));
    if (!server) return NULL;
    server->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server->listen_fd < 0) { free(server); return NULL; }
    int reuse = 1;
    (void)setsockopt(server->listen_fd, SOL_SOCKET, SO_REUSEADDR,
                     &reuse, sizeof(reuse));
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((uint16_t)port);
    if (bind(server->listen_fd, (struct sockaddr *)&address,
             sizeof(address)) != 0 ||
        listen(server->listen_fd, 256) != 0) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    socklen_t address_length = sizeof(address);
    if (getsockname(server->listen_fd, (struct sockaddr *)&address,
                    &address_length) != 0) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    server->port = ntohs(address.sin_port);
    atomic_init(&server->stopping, false);
    atomic_init(&server->running, false);
    atomic_init(&server->completed, 0);
    atomic_init(&server->rejected, 0);
    atomic_init(&server->input_bytes, 0);
    atomic_init(&server->output_bytes, 0);
    if (!swan_thread_pool_init(&server->pool, (size_t)workers,
                               (size_t)queue_capacity)) {
        close(server->listen_fd);
        free(server);
        return NULL;
    }
    return server;
}

int32_t swan_http_json_server_run(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    if (!server) return -1;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&server->running, &expected, true))
        return -1;
    while (!atomic_load_explicit(&server->stopping, memory_order_relaxed)) {
        int fd = accept(server->listen_fd, NULL, NULL);
        if (fd < 0) {
            if (errno == EINTR) continue;
            if (atomic_load_explicit(&server->stopping, memory_order_relaxed))
                break;
            continue;
        }
        SwanHttpClient *client = (SwanHttpClient *)malloc(sizeof(*client));
        if (!client) {
            static const char busy[] = "{\"error\":\"busy\"}";
            swan_http_reply(fd, 503, busy, sizeof(busy) - 1);
            close(fd);
            atomic_fetch_add_explicit(&server->rejected, 1, memory_order_relaxed);
            continue;
        }
        client->server = server;
        client->socket_fd = fd;
        if (!swan_thread_pool_submit(&server->pool, swan_http_process_client,
                                     client)) {
            static const char busy[] = "{\"error\":\"busy\"}";
            swan_http_reply(fd, 503, busy, sizeof(busy) - 1);
            close(fd);
            free(client);
            atomic_fetch_add_explicit(&server->rejected, 1, memory_order_relaxed);
        }
    }
    if (server->listen_fd >= 0) {
        close(server->listen_fd);
        server->listen_fd = -1;
    }
    swan_thread_pool_shutdown(&server->pool, true);
    atomic_store_explicit(&server->running, false, memory_order_relaxed);
    return 0;
}

void swan_http_json_server_stop(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    if (!server) return;
    atomic_store_explicit(&server->stopping, true, memory_order_relaxed);
    if (server->listen_fd >= 0)
        (void)shutdown(server->listen_fd, SHUT_RDWR);
}

void swan_http_json_server_destroy(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    if (!server) return;
    swan_http_json_server_stop(server);
    if (atomic_load_explicit(&server->running, memory_order_relaxed)) return;
    if (server->pool.initialized) swan_thread_pool_destroy(&server->pool);
    if (server->listen_fd >= 0) close(server->listen_fd);
    free(server);
}

int64_t swan_http_json_server_port(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    return server ? server->port : -1;
}
int64_t swan_http_json_server_completed(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    return server ? atomic_load_explicit(&server->completed, memory_order_relaxed) : -1;
}
int64_t swan_http_json_server_rejected(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    return server ? atomic_load_explicit(&server->rejected, memory_order_relaxed) : -1;
}
int64_t swan_http_json_server_input_bytes(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    return server ? atomic_load_explicit(&server->input_bytes, memory_order_relaxed) : -1;
}
int64_t swan_http_json_server_output_bytes(void *value) {
    SwanHttpServer *server = (SwanHttpServer *)value;
    return server ? atomic_load_explicit(&server->output_bytes, memory_order_relaxed) : -1;
}
