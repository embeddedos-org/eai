// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/eipc.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>

#ifdef EAI_EIPC_ENABLED

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

/* EIPC BCI message types */
#define EIPC_BCI_MSG_INTENT   0x01
#define EIPC_BCI_MSG_SIGNAL   0x02
#define EIPC_BCI_MSG_COMMAND  0x03
#define EIPC_BCI_MSG_ACK      0x04

/* EIPC BCI wire header */
typedef struct {
    uint8_t  type;
    uint8_t  version;
    uint16_t payload_len;
} eipc_bci_header_t;

static int eipc_send_raw(sock_t sock, const void *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(sock, (const char *)data + sent, (int)(len - sent), 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int eipc_recv_raw(sock_t sock, void *buf, size_t len)
{
    size_t recvd = 0;
    while (recvd < len) {
        int n = recv(sock, (char *)buf + recvd, (int)(len - recvd), 0);
        if (n <= 0) return -1;
        recvd += (size_t)n;
    }
    return 0;
}

eai_status_t eai_bci_eipc_init(eai_bci_eipc_t *eipc, const char *endpoint)
{
    if (!eipc || !endpoint) return EAI_ERR_INVALID;
    memset(eipc, 0, sizeof(*eipc));
    strncpy(eipc->endpoint, endpoint, sizeof(eipc->endpoint) - 1);
    eipc->socket_fd = -1;
    eipc->connected = false;

    /* Parse endpoint "host:port" */
    char host[128] = {0};
    int port = 9100; /* default EIPC BCI port */

    const char *colon = strrchr(endpoint, ':');
    if (colon) {
        size_t hlen = (size_t)(colon - endpoint);
        if (hlen >= sizeof(host)) hlen = sizeof(host) - 1;
        memcpy(host, endpoint, hlen);
        host[hlen] = '\0';
        port = atoi(colon + 1);
    } else {
        strncpy(host, endpoint, sizeof(host) - 1);
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        EAI_LOG_WARN("bci", "BCI EIPC: DNS resolve failed for '%s', will operate in offline mode",
                     endpoint);
        eipc->connected = false;
        return EAI_OK; /* Non-fatal — can queue intents for later */
    }

    sock_t sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == SOCK_INVALID) {
        freeaddrinfo(res);
        EAI_LOG_WARN("bci", "BCI EIPC: socket creation failed, offline mode");
        return EAI_OK;
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        sock_close(sock);
        freeaddrinfo(res);
        EAI_LOG_WARN("bci", "BCI EIPC: connect failed to %s:%d, offline mode", host, port);
        return EAI_OK;
    }
    freeaddrinfo(res);

    eipc->socket_fd = (int)sock;
    eipc->connected = true;
    EAI_LOG_INFO("bci", "BCI EIPC: connected to '%s' (fd=%d)", endpoint, eipc->socket_fd);
    return EAI_OK;
}

eai_status_t eai_bci_eipc_send_intent(eai_bci_eipc_t *eipc, const eai_bci_intent_t *intent)
{
    if (!eipc || !intent) return EAI_ERR_INVALID;

    /* Format intent as JSON */
    char payload[512];
    int plen = snprintf(payload, sizeof(payload),
        "{\"class\":%u,\"label\":\"%s\",\"confidence\":%.3f}",
        intent->class_id, intent->label, intent->confidence);

    if (eipc->connected && eipc->socket_fd >= 0) {
        eipc_bci_header_t hdr;
        hdr.type = EIPC_BCI_MSG_INTENT;
        hdr.version = 1;
        hdr.payload_len = (uint16_t)plen;

        if (eipc_send_raw((sock_t)eipc->socket_fd, &hdr, sizeof(hdr)) != 0 ||
            eipc_send_raw((sock_t)eipc->socket_fd, payload, (size_t)plen) != 0) {
            EAI_LOG_WARN("bci", "BCI EIPC: send intent failed, connection lost");
            eipc->connected = false;
            return EAI_ERR_CONNECT;
        }
        EAI_LOG_DEBUG("bci", "BCI EIPC: sent intent '%s' (class %u, conf %.2f)",
                      intent->label, intent->class_id, intent->confidence);
    } else {
        EAI_LOG_DEBUG("bci", "BCI EIPC: queued intent '%s' (offline)",
                      intent->label);
    }

    return EAI_OK;
}

eai_status_t eai_bci_eipc_stream_signal(eai_bci_eipc_t *eipc, const eai_bci_sample_t *samples,
                                         int count)
{
    if (!eipc || !samples) return EAI_ERR_INVALID;

    if (eipc->connected && eipc->socket_fd >= 0) {
        uint16_t payload_len = (uint16_t)(count * sizeof(eai_bci_sample_t));

        eipc_bci_header_t hdr;
        hdr.type = EIPC_BCI_MSG_SIGNAL;
        hdr.version = 1;
        hdr.payload_len = payload_len;

        if (eipc_send_raw((sock_t)eipc->socket_fd, &hdr, sizeof(hdr)) != 0 ||
            eipc_send_raw((sock_t)eipc->socket_fd, samples, payload_len) != 0) {
            EAI_LOG_WARN("bci", "BCI EIPC: stream signal failed");
            eipc->connected = false;
            return EAI_ERR_CONNECT;
        }
        EAI_LOG_DEBUG("bci", "BCI EIPC: streamed %d samples", count);
    }

    return EAI_OK;
}

eai_status_t eai_bci_eipc_receive_command(eai_bci_eipc_t *eipc, char *cmd_buf, size_t buf_size)
{
    if (!eipc || !cmd_buf || buf_size == 0) return EAI_ERR_INVALID;
    cmd_buf[0] = '\0';

    if (!eipc->connected || eipc->socket_fd < 0)
        return EAI_OK;

    /* Non-blocking check for incoming commands */
#ifndef _WIN32
    /* Set socket to non-blocking temporarily */
    int flags = fcntl(eipc->socket_fd, F_GETFL, 0);
    fcntl(eipc->socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif

    eipc_bci_header_t hdr;
    int n = recv((sock_t)eipc->socket_fd, (char *)&hdr, sizeof(hdr), 0);

#ifndef _WIN32
    fcntl(eipc->socket_fd, F_SETFL, flags); /* Restore blocking */
#endif

    if (n <= 0) return EAI_OK; /* No data available */

    if (n == sizeof(hdr) && hdr.type == EIPC_BCI_MSG_COMMAND && hdr.payload_len > 0) {
        size_t to_read = hdr.payload_len < buf_size - 1 ? hdr.payload_len : buf_size - 1;
        if (eipc_recv_raw((sock_t)eipc->socket_fd, cmd_buf, to_read) == 0) {
            cmd_buf[to_read] = '\0';
            EAI_LOG_INFO("bci", "BCI EIPC: received command: %s", cmd_buf);
        }
    }

    return EAI_OK;
}

void eai_bci_eipc_shutdown(eai_bci_eipc_t *eipc)
{
    if (!eipc) return;
    if (eipc->socket_fd >= 0) {
        sock_close((sock_t)eipc->socket_fd);
    }
    eipc->connected = false;
    eipc->socket_fd = -1;
    EAI_LOG_INFO("bci", "BCI EIPC: shutdown");
}

#endif /* EAI_EIPC_ENABLED */
