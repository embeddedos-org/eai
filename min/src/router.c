// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/router.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET sock_t;
#define SOCK_INVALID INVALID_SOCKET
#define sock_close closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

#define LOG_MOD "min-router"

eai_status_t eai_min_router_init(eai_min_router_t *router, eai_route_target_t default_target)
{
    if (!router) return EAI_ERR_INVALID;
    memset(router, 0, sizeof(*router));
    router->default_target  = default_target;
    router->timeout_ms      = 5000;
    router->cloud_available = false;
    return EAI_OK;
}

eai_status_t eai_min_router_set_cloud(eai_min_router_t *router,
                                       const char *endpoint, const char *api_key)
{
    if (!router || !endpoint) return EAI_ERR_INVALID;
    router->cloud_endpoint  = endpoint;
    router->api_key         = api_key;
    router->cloud_available = true;
    EAI_LOG_INFO(LOG_MOD, "cloud endpoint set: %s", endpoint);
    return EAI_OK;
}

eai_route_target_t eai_min_router_decide(const eai_min_router_t *router,
                                          const eai_inference_input_t *input)
{
    if (!router) return EAI_ROUTE_LOCAL;

    if (router->default_target == EAI_ROUTE_LOCAL)
        return EAI_ROUTE_LOCAL;

    if (router->default_target == EAI_ROUTE_CLOUD) {
        if (router->cloud_available) return EAI_ROUTE_CLOUD;
        EAI_LOG_WARN(LOG_MOD, "cloud requested but unavailable, falling back to local");
        return EAI_ROUTE_LOCAL;
    }

    /* AUTO mode: decide based on input complexity */
    if (input && input->text_len > 2048 && router->cloud_available) {
        EAI_LOG_DEBUG(LOG_MOD, "auto-routing to cloud (large input: %zu bytes)", input->text_len);
        return EAI_ROUTE_CLOUD;
    }

    return EAI_ROUTE_LOCAL;
}

/* Parse host and port from endpoint URL */
static void parse_endpoint(const char *endpoint, char *host, size_t host_size,
                           int *port, char *path, size_t path_size, int *use_https)
{
    *port = 80;
    *use_https = 0;
    const char *p = endpoint;

    if (strncmp(p, "https://", 8) == 0) {
        *use_https = 1;
        *port = 443;
        p += 8;
    } else if (strncmp(p, "http://", 7) == 0) {
        p += 7;
    }

    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        size_t hlen = (size_t)(colon - p);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
        *port = atoi(colon + 1);
    } else if (slash) {
        size_t hlen = (size_t)(slash - p);
        if (hlen >= host_size) hlen = host_size - 1;
        memcpy(host, p, hlen);
        host[hlen] = '\0';
    } else {
        strncpy(host, p, host_size - 1);
        host[host_size - 1] = '\0';
    }

    if (slash) {
        strncpy(path, slash, path_size - 1);
        path[path_size - 1] = '\0';
    } else {
        strncpy(path, "/v1/chat/completions", path_size - 1);
    }
}

eai_status_t eai_min_router_infer_cloud(eai_min_router_t *router,
                                         const eai_inference_input_t *in,
                                         eai_inference_output_t *out)
{
    if (!router || !router->cloud_available) return EAI_ERR_CONNECT;

    EAI_LOG_INFO(LOG_MOD, "cloud inference: endpoint=%s, input_len=%zu",
                 router->cloud_endpoint, in->text_len);

    char host[256] = {0};
    char path[256] = {0};
    int port = 80;
    int use_https = 0;
    parse_endpoint(router->cloud_endpoint, host, sizeof(host),
                   &port, path, sizeof(path), &use_https);

    if (use_https) {
        EAI_LOG_WARN(LOG_MOD, "HTTPS not supported in minimal build, use HTTP endpoint");
        strncpy(out->text, "[cloud] HTTPS requires TLS support", sizeof(out->text) - 1);
        out->text_len = strlen(out->text);
        return EAI_ERR_UNSUPPORTED;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    /* Resolve and connect */
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "DNS resolve failed for %s", host);
        return EAI_ERR_CONNECT;
    }

    sock_t sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == SOCK_INVALID) {
        freeaddrinfo(res);
        return EAI_ERR_CONNECT;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "TCP connect failed to %s:%d", host, port);
        sock_close(sock);
        freeaddrinfo(res);
        return EAI_ERR_CONNECT;
    }
    freeaddrinfo(res);

    /* Build JSON request body */
    char body[4096];
    int body_len = snprintf(body, sizeof(body),
        "{\"model\":\"default\",\"messages\":[{\"role\":\"user\",\"content\":\"%.*s\"}],"
        "\"max_tokens\":256,\"temperature\":0.7}",
        (int)(in->text_len < 3000 ? in->text_len : 3000), in->text);

    /* Build HTTP POST request */
    char request[8192];
    int req_len = snprintf(request, sizeof(request),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "%s%s%s"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        path, host, body_len,
        router->api_key ? "Authorization: Bearer " : "",
        router->api_key ? router->api_key : "",
        router->api_key ? "\r\n" : "",
        body);

    /* Send request */
    size_t sent = 0;
    while (sent < (size_t)req_len) {
        int n = send(sock, request + sent, req_len - (int)sent, 0);
        if (n <= 0) {
            sock_close(sock);
            return EAI_ERR_CONNECT;
        }
        sent += (size_t)n;
    }

    /* Read response */
    char response[8192];
    int total = 0;
    while (total < (int)sizeof(response) - 1) {
        int n = recv(sock, response + total, (int)(sizeof(response) - 1 - total), 0);
        if (n <= 0) break;
        total += n;
    }
    response[total] = '\0';
    sock_close(sock);

    /* Parse HTTP response: find body after \r\n\r\n */
    const char *body_start = strstr(response, "\r\n\r\n");
    if (!body_start) {
        strncpy(out->text, "[cloud] malformed HTTP response", sizeof(out->text) - 1);
        out->text_len = strlen(out->text);
        return EAI_ERR_RUNTIME;
    }
    body_start += 4;

    /* Extract "content" from JSON response (minimal parser) */
    const char *content = strstr(body_start, "\"content\":");
    if (content) {
        content = strchr(content + 10, '"');
        if (content) {
            content++;
            const char *end = strchr(content, '"');
            if (end) {
                size_t clen = (size_t)(end - content);
                if (clen >= sizeof(out->text)) clen = sizeof(out->text) - 1;
                memcpy(out->text, content, clen);
                out->text[clen] = '\0';
                out->text_len = clen;
                out->confidence = 0.9f;
                out->tokens_used = (uint32_t)(clen / 4);
                EAI_LOG_INFO(LOG_MOD, "cloud inference completed: %zu bytes", clen);
                return EAI_OK;
            }
        }
    }

    /* Fallback: return raw body */
    size_t raw_len = strlen(body_start);
    if (raw_len >= sizeof(out->text)) raw_len = sizeof(out->text) - 1;
    memcpy(out->text, body_start, raw_len);
    out->text[raw_len] = '\0';
    out->text_len = raw_len;
    out->confidence = 0.5f;

    return EAI_OK;
}
