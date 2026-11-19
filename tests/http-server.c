#define _POSIX_C_SOURCE 200809L
#include "BrewRuntime/http_server.h"

#include <assert.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    uint16_t port;
    const char *body;
    int expected_status;
    const char *expected_body;
} ClientArgs;

static void *run_server(void *context) {
    int status = brew_http_json_server_run(context);
    return (void *)(intptr_t)status;
}

static int send_all(int fd, const char *data, size_t length) {
    while (length) {
        ssize_t n = send(fd, data, length, 0);
        if (n < 0) return 0;
        data += n;
        length -= (size_t)n;
    }
    return 1;
}

static int perform_request(uint16_t port, const char *method,
                           const char *body, int expected_status,
                           const char *expected_body) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return 0;
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(fd);
        return 0;
    }

    size_t body_length = body ? strlen(body) : 0;
    char request[4096];
    int request_length;
    if (!strcmp(method, "GET")) {
        request_length = snprintf(request, sizeof(request),
            "GET /health HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n");
    } else {
        request_length = snprintf(request, sizeof(request),
            "%s /json HTTP/1.1\r\nHost: localhost\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
            method, body_length, body ? body : "");
    }
    if (request_length <= 0 || (size_t)request_length >= sizeof(request) ||
        !send_all(fd, request, (size_t)request_length)) {
        close(fd);
        return 0;
    }
    shutdown(fd, SHUT_WR);

    char response[8192];
    size_t used = 0;
    while (used + 1 < sizeof(response)) {
        ssize_t n = recv(fd, response + used, sizeof(response) - used - 1, 0);
        if (n < 0) { close(fd); return 0; }
        if (!n) break;
        used += (size_t)n;
    }
    response[used] = 0;
    close(fd);

    char status[32];
    snprintf(status, sizeof(status), " %d ", expected_status);
    return strstr(response, status) != NULL &&
           (!expected_body || strstr(response, expected_body) != NULL);
}

static void *run_client(void *context) {
    ClientArgs *args = (ClientArgs *)context;
    int ok = perform_request(args->port, "POST", args->body,
                             args->expected_status, args->expected_body);
    return (void *)(intptr_t)(ok ? 1 : 0);
}

int main(void) {
    void *server = brew_http_json_server_create(0, 4, 32);
    assert(server != NULL);
    int64_t port = brew_http_json_server_port(server);
    assert(port > 0 && port <= 65535);

    pthread_t server_thread;
    assert(pthread_create(&server_thread, NULL, run_server, server) == 0);

    const char *json = "{\"id\":42,\"workers\":128}";
    assert(perform_request((uint16_t)port, "POST", json, 200, json));
    assert(perform_request((uint16_t)port, "GET", NULL, 200, "{\"ok\":true}"));
    assert(perform_request((uint16_t)port, "POST", "{", 400,
                           "{\"error\":\"invalid_json\"}"));

    enum { CLIENTS = 8 };
    pthread_t clients[CLIENTS];
    ClientArgs args = {(uint16_t)port, json, 200, json};
    for (size_t i = 0; i < CLIENTS; i++)
        assert(pthread_create(&clients[i], NULL, run_client, &args) == 0);
    for (size_t i = 0; i < CLIENTS; i++) {
        void *result = NULL;
        assert(pthread_join(clients[i], &result) == 0);
        assert((intptr_t)result == 1);
    }

    assert(brew_http_json_server_completed(server) == CLIENTS + 2);
    assert(brew_http_json_server_rejected(server) == 0);
    assert(brew_http_json_server_input_bytes(server) > 0);
    assert(brew_http_json_server_output_bytes(server) > 0);

    brew_http_json_server_stop(server);
    void *run_result = NULL;
    assert(pthread_join(server_thread, &run_result) == 0);
    assert((intptr_t)run_result == 0);
    assert(brew_http_json_server_run(server) == -1);
    brew_http_json_server_destroy(server);

    puts("HTTP JSON server tests passed");
    return 0;
}
