// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/connector.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

#define LOG_MOD "conn-modbus"

/* Modbus TCP function codes */
#define MODBUS_FC_READ_HOLDING  0x03
#define MODBUS_FC_READ_INPUT    0x04
#define MODBUS_FC_WRITE_SINGLE  0x06
#define MODBUS_FC_WRITE_MULTI   0x10

/* Modbus TCP MBAP header size */
#define MBAP_HEADER_SIZE 7

typedef struct {
    char host[256];
    int  port;
    int  slave_id;
    bool connected;
    sock_t sock;
    uint16_t transaction_id;
} modbus_ctx_t;

static modbus_ctx_t g_modbus_ctx;

static int modbus_send_raw(modbus_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(ctx->sock, (const char *)(data + sent), (int)(len - sent), 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static int modbus_recv_raw(modbus_ctx_t *ctx, uint8_t *buf, size_t len)
{
    size_t recvd = 0;
    while (recvd < len) {
        int n = recv(ctx->sock, (char *)(buf + recvd), (int)(len - recvd), 0);
        if (n <= 0) return -1;
        recvd += (size_t)n;
    }
    return 0;
}

static eai_status_t modbus_connect(eai_fw_connector_t *conn,
                                    const eai_kv_t *params, int param_count)
{
    modbus_ctx_t *ctx = &g_modbus_ctx;
    memset(ctx, 0, sizeof(*ctx));
    ctx->sock = SOCK_INVALID;
    strncpy(ctx->host, "localhost", sizeof(ctx->host) - 1);
    ctx->port = 502;
    ctx->slave_id = 1;

    for (int i = 0; i < param_count; i++) {
        if (strcmp(params[i].key, "host") == 0)
            strncpy(ctx->host, params[i].value, sizeof(ctx->host) - 1);
        if (strcmp(params[i].key, "port") == 0)
            ctx->port = atoi(params[i].value);
        if (strcmp(params[i].key, "slave_id") == 0)
            ctx->slave_id = atoi(params[i].value);
    }

    conn->ctx = ctx;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", ctx->port);

    if (getaddrinfo(ctx->host, port_str, &hints, &res) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "DNS resolve failed for %s", ctx->host);
        return EAI_ERR_CONNECT;
    }

    ctx->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ctx->sock == SOCK_INVALID) {
        freeaddrinfo(res);
        return EAI_ERR_CONNECT;
    }

    if (connect(ctx->sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "TCP connect failed to %s:%d", ctx->host, ctx->port);
        sock_close(ctx->sock);
        ctx->sock = SOCK_INVALID;
        freeaddrinfo(res);
        return EAI_ERR_CONNECT;
    }
    freeaddrinfo(res);

    ctx->connected = true;
    conn->state = EAI_CONN_CONNECTED;
    EAI_LOG_INFO(LOG_MOD, "connected to %s:%d slave=%d", ctx->host, ctx->port, ctx->slave_id);
    return EAI_OK;
}

static eai_status_t modbus_disconnect(eai_fw_connector_t *conn)
{
    modbus_ctx_t *ctx = (modbus_ctx_t *)conn->ctx;
    if (ctx && ctx->connected) {
        sock_close(ctx->sock);
        ctx->sock = SOCK_INVALID;
        ctx->connected = false;
    }
    conn->state = EAI_CONN_DISCONNECTED;
    EAI_LOG_INFO(LOG_MOD, "disconnected");
    return EAI_OK;
}

static eai_status_t modbus_read(eai_fw_connector_t *conn, const char *register_addr,
                                 void *buf, size_t buf_size, size_t *bytes_read)
{
    modbus_ctx_t *ctx = (modbus_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

    uint16_t reg_addr = (uint16_t)strtoul(register_addr, NULL, 0);
    uint16_t reg_count = 1;

    ctx->transaction_id++;

    /* Build Modbus TCP request: MBAP header + PDU */
    uint8_t request[12];
    /* MBAP Header */
    request[0] = (uint8_t)(ctx->transaction_id >> 8);  /* Transaction ID */
    request[1] = (uint8_t)(ctx->transaction_id);
    request[2] = 0x00; request[3] = 0x00;               /* Protocol ID (Modbus) */
    request[4] = 0x00; request[5] = 0x06;               /* Length (6 bytes follow) */
    request[6] = (uint8_t)ctx->slave_id;                 /* Unit ID */
    /* PDU */
    request[7] = MODBUS_FC_READ_HOLDING;                 /* Function Code */
    request[8] = (uint8_t)(reg_addr >> 8);
    request[9] = (uint8_t)(reg_addr);
    request[10] = (uint8_t)(reg_count >> 8);
    request[11] = (uint8_t)(reg_count);

    if (modbus_send_raw(ctx, request, 12) != 0)
        return EAI_ERR_CONNECT;

    /* Read MBAP header of response */
    uint8_t resp_hdr[MBAP_HEADER_SIZE];
    if (modbus_recv_raw(ctx, resp_hdr, MBAP_HEADER_SIZE) != 0)
        return EAI_ERR_CONNECT;

    uint16_t resp_len = ((uint16_t)resp_hdr[4] << 8) | resp_hdr[5];
    if (resp_len > 253) return EAI_ERR_INVALID;

    /* Read PDU */
    uint8_t pdu[256];
    if (modbus_recv_raw(ctx, pdu, resp_len) != 0)
        return EAI_ERR_CONNECT;

    /* Check for error response */
    if (pdu[0] & 0x80) {
        EAI_LOG_ERROR(LOG_MOD, "Modbus error: fc=0x%02x exception=%d", pdu[0], pdu[1]);
        return EAI_ERR_INVALID;
    }

    /* Parse response: FC (1) + byte_count (1) + data */
    if (resp_len >= 4) {
        uint8_t byte_count = pdu[1];
        uint16_t reg_val = ((uint16_t)pdu[2] << 8) | pdu[3];
        int written = snprintf((char *)buf, buf_size, "%u", reg_val);
        if (bytes_read) *bytes_read = (size_t)written;
        EAI_LOG_DEBUG(LOG_MOD, "read register=%s value=%u (%d bytes)",
                      register_addr, reg_val, byte_count);
    }

    return EAI_OK;
}

static eai_status_t modbus_write(eai_fw_connector_t *conn, const char *register_addr,
                                  const void *data, size_t data_len)
{
    modbus_ctx_t *ctx = (modbus_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

    uint16_t reg_addr = (uint16_t)strtoul(register_addr, NULL, 0);
    uint16_t value = 0;

    /* Parse value from data (as string or raw uint16) */
    if (data_len == 2) {
        value = ((uint16_t)((const uint8_t *)data)[0] << 8) |
                ((const uint8_t *)data)[1];
    } else {
        value = (uint16_t)strtoul((const char *)data, NULL, 0);
    }

    ctx->transaction_id++;

    /* Build Write Single Register request */
    uint8_t request[12];
    request[0] = (uint8_t)(ctx->transaction_id >> 8);
    request[1] = (uint8_t)(ctx->transaction_id);
    request[2] = 0x00; request[3] = 0x00;
    request[4] = 0x00; request[5] = 0x06;
    request[6] = (uint8_t)ctx->slave_id;
    request[7] = MODBUS_FC_WRITE_SINGLE;
    request[8] = (uint8_t)(reg_addr >> 8);
    request[9] = (uint8_t)(reg_addr);
    request[10] = (uint8_t)(value >> 8);
    request[11] = (uint8_t)(value);

    if (modbus_send_raw(ctx, request, 12) != 0)
        return EAI_ERR_CONNECT;

    /* Read response (echo of request for write single) */
    uint8_t response[12];
    if (modbus_recv_raw(ctx, response, 12) != 0)
        return EAI_ERR_CONNECT;

    EAI_LOG_INFO(LOG_MOD, "write register=%s value=%u", register_addr, value);
    return EAI_OK;
}

const eai_connector_ops_t eai_connector_modbus_ops = {
    .name       = "modbus",
    .type       = EAI_CONN_MODBUS,
    .connect    = modbus_connect,
    .disconnect = modbus_disconnect,
    .read       = modbus_read,
    .write      = modbus_write,
    .subscribe  = NULL,
};
