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
#include <fcntl.h>
#include <errno.h>
typedef int sock_t;
#define SOCK_INVALID (-1)
#define sock_close close
#endif

#define LOG_MOD "conn-mqtt"

/* MQTT v3.1.1 packet types */
#define MQTT_CONNECT     0x10
#define MQTT_CONNACK     0x20
#define MQTT_PUBLISH     0x30
#define MQTT_SUBSCRIBE   0x82
#define MQTT_SUBACK      0x90
#define MQTT_PINGREQ     0xC0
#define MQTT_DISCONNECT  0xE0

typedef struct {
    char broker[256];
    int  port;
    char client_id[64];
    char username[64];
    char password[64];
    bool connected;
    sock_t sock;
    uint16_t packet_id;
    void (*sub_callback)(const char *, const void *, size_t);
    char sub_topic[128];
} mqtt_ctx_t;

static mqtt_ctx_t g_mqtt_ctx;

/* Encode MQTT remaining length (variable-length encoding) */
static int mqtt_encode_remaining_length(uint8_t *buf, uint32_t length)
{
    int i = 0;
    do {
        uint8_t byte = length % 128;
        length /= 128;
        if (length > 0) byte |= 0x80;
        buf[i++] = byte;
    } while (length > 0);
    return i;
}

/* Send raw bytes over socket */
static int mqtt_send_raw(mqtt_ctx_t *ctx, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(ctx->sock, (const char *)(data + sent), (int)(len - sent), 0);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

/* Receive raw bytes from socket */
static int mqtt_recv_raw(mqtt_ctx_t *ctx, uint8_t *buf, size_t len)
{
    size_t recvd = 0;
    while (recvd < len) {
        int n = recv(ctx->sock, (char *)(buf + recvd), (int)(len - recvd), 0);
        if (n <= 0) return -1;
        recvd += (size_t)n;
    }
    return 0;
}

/* Build and send MQTT CONNECT packet */
static int mqtt_send_connect(mqtt_ctx_t *ctx)
{
    uint8_t pkt[512];
    int pos = 0;

    /* Variable header: protocol name + level + flags + keepalive */
    uint8_t var_hdr[10] = {
        0x00, 0x04, 'M', 'Q', 'T', 'T',  /* Protocol Name */
        0x04,                               /* Protocol Level (3.1.1) */
        0x02,                               /* Connect Flags: Clean Session */
        0x00, 0x3C                          /* Keep Alive: 60 seconds */
    };

    /* Check if we have credentials */
    bool has_user = strlen(ctx->username) > 0;
    bool has_pass = strlen(ctx->password) > 0;
    if (has_user) var_hdr[7] |= 0x80; /* Username flag */
    if (has_pass) var_hdr[7] |= 0x40; /* Password flag */

    /* Calculate payload length */
    uint16_t cid_len = (uint16_t)strlen(ctx->client_id);
    uint32_t payload_len = 2 + cid_len;
    if (has_user) payload_len += 2 + (uint32_t)strlen(ctx->username);
    if (has_pass) payload_len += 2 + (uint32_t)strlen(ctx->password);

    uint32_t remaining = 10 + payload_len;

    /* Fixed header */
    pkt[pos++] = MQTT_CONNECT;
    pos += mqtt_encode_remaining_length(&pkt[pos], remaining);

    /* Variable header */
    memcpy(&pkt[pos], var_hdr, 10);
    pos += 10;

    /* Payload: Client ID */
    pkt[pos++] = (uint8_t)(cid_len >> 8);
    pkt[pos++] = (uint8_t)(cid_len);
    memcpy(&pkt[pos], ctx->client_id, cid_len);
    pos += cid_len;

    /* Payload: Username */
    if (has_user) {
        uint16_t ulen = (uint16_t)strlen(ctx->username);
        pkt[pos++] = (uint8_t)(ulen >> 8);
        pkt[pos++] = (uint8_t)(ulen);
        memcpy(&pkt[pos], ctx->username, ulen);
        pos += ulen;
    }

    /* Payload: Password */
    if (has_pass) {
        uint16_t plen = (uint16_t)strlen(ctx->password);
        pkt[pos++] = (uint8_t)(plen >> 8);
        pkt[pos++] = (uint8_t)(plen);
        memcpy(&pkt[pos], ctx->password, plen);
        pos += plen;
    }

    if (mqtt_send_raw(ctx, pkt, (size_t)pos) != 0)
        return -1;

    /* Read CONNACK */
    uint8_t connack[4];
    if (mqtt_recv_raw(ctx, connack, 4) != 0)
        return -1;

    if (connack[0] != MQTT_CONNACK || connack[3] != 0x00) {
        EAI_LOG_ERROR(LOG_MOD, "CONNACK failed: rc=%d", connack[3]);
        return -1;
    }

    return 0;
}

static eai_status_t mqtt_connect(eai_fw_connector_t *conn,
                                  const eai_kv_t *params, int param_count)
{
    mqtt_ctx_t *ctx = &g_mqtt_ctx;
    memset(ctx, 0, sizeof(*ctx));
    ctx->sock = SOCK_INVALID;
    strncpy(ctx->broker, "localhost", sizeof(ctx->broker) - 1);
    ctx->port = 1883;
    strncpy(ctx->client_id, "eai-client", sizeof(ctx->client_id) - 1);

    for (int i = 0; i < param_count; i++) {
        if (strcmp(params[i].key, "broker") == 0)
            strncpy(ctx->broker, params[i].value, sizeof(ctx->broker) - 1);
        if (strcmp(params[i].key, "port") == 0)
            ctx->port = atoi(params[i].value);
        if (strcmp(params[i].key, "client_id") == 0)
            strncpy(ctx->client_id, params[i].value, sizeof(ctx->client_id) - 1);
        if (strcmp(params[i].key, "username") == 0)
            strncpy(ctx->username, params[i].value, sizeof(ctx->username) - 1);
        if (strcmp(params[i].key, "password") == 0)
            strncpy(ctx->password, params[i].value, sizeof(ctx->password) - 1);
    }

    conn->ctx = ctx;

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
    snprintf(port_str, sizeof(port_str), "%d", ctx->port);

    if (getaddrinfo(ctx->broker, port_str, &hints, &res) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "DNS resolve failed for %s", ctx->broker);
        return EAI_ERR_CONNECT;
    }

    ctx->sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (ctx->sock == SOCK_INVALID) {
        freeaddrinfo(res);
        return EAI_ERR_CONNECT;
    }

    if (connect(ctx->sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "TCP connect failed to %s:%d", ctx->broker, ctx->port);
        sock_close(ctx->sock);
        ctx->sock = SOCK_INVALID;
        freeaddrinfo(res);
        return EAI_ERR_CONNECT;
    }
    freeaddrinfo(res);

    /* Send MQTT CONNECT */
    if (mqtt_send_connect(ctx) != 0) {
        EAI_LOG_ERROR(LOG_MOD, "MQTT CONNECT handshake failed");
        sock_close(ctx->sock);
        ctx->sock = SOCK_INVALID;
        return EAI_ERR_CONNECT;
    }

    ctx->connected = true;
    conn->state = EAI_CONN_CONNECTED;
    EAI_LOG_INFO(LOG_MOD, "connected to %s:%d as %s", ctx->broker, ctx->port, ctx->client_id);
    return EAI_OK;
}

static eai_status_t mqtt_disconnect(eai_fw_connector_t *conn)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)conn->ctx;
    if (ctx && ctx->connected) {
        /* Send DISCONNECT packet */
        uint8_t disc[2] = { MQTT_DISCONNECT, 0x00 };
        mqtt_send_raw(ctx, disc, 2);
        sock_close(ctx->sock);
        ctx->sock = SOCK_INVALID;
        ctx->connected = false;
    }
    conn->state = EAI_CONN_DISCONNECTED;
    EAI_LOG_INFO(LOG_MOD, "disconnected");
    return EAI_OK;
}

static eai_status_t mqtt_write(eai_fw_connector_t *conn, const char *topic,
                                const void *data, size_t data_len)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

    uint16_t topic_len = (uint16_t)strlen(topic);
    uint32_t remaining = 2 + topic_len + (uint32_t)data_len;

    uint8_t pkt[1024];
    int pos = 0;
    pkt[pos++] = MQTT_PUBLISH; /* QoS 0, no retain */
    pos += mqtt_encode_remaining_length(&pkt[pos], remaining);
    pkt[pos++] = (uint8_t)(topic_len >> 8);
    pkt[pos++] = (uint8_t)(topic_len);
    memcpy(&pkt[pos], topic, topic_len);
    pos += topic_len;

    /* Send header + payload */
    if (mqtt_send_raw(ctx, pkt, (size_t)pos) != 0)
        return EAI_ERR_CONNECT;
    if (data_len > 0 && mqtt_send_raw(ctx, (const uint8_t *)data, data_len) != 0)
        return EAI_ERR_CONNECT;

    EAI_LOG_INFO(LOG_MOD, "published topic='%s' payload_len=%zu", topic, data_len);
    return EAI_OK;
}

static eai_status_t mqtt_read(eai_fw_connector_t *conn, const char *topic,
                               void *buf, size_t buf_size, size_t *bytes_read)
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

    /* Read one MQTT packet from the socket (blocking) */
    uint8_t hdr[2];
    if (mqtt_recv_raw(ctx, hdr, 1) != 0)
        return EAI_ERR_CONNECT;

    /* Decode remaining length */
    uint32_t remaining = 0;
    uint32_t multiplier = 1;
    uint8_t byte;
    do {
        if (mqtt_recv_raw(ctx, &byte, 1) != 0)
            return EAI_ERR_CONNECT;
        remaining += (byte & 0x7F) * multiplier;
        multiplier *= 128;
    } while (byte & 0x80);

    /* Read remaining payload */
    uint8_t payload[1024];
    if (remaining > sizeof(payload)) remaining = sizeof(payload);
    if (mqtt_recv_raw(ctx, payload, remaining) != 0)
        return EAI_ERR_CONNECT;

    if ((hdr[0] & 0xF0) == MQTT_PUBLISH) {
        /* Parse PUBLISH: topic length + topic + payload */
        uint16_t tlen = ((uint16_t)payload[0] << 8) | payload[1];
        uint32_t data_start = 2 + tlen;
        uint32_t data_len = remaining - data_start;

        size_t copy = data_len < buf_size - 1 ? data_len : buf_size - 1;
        memcpy(buf, &payload[data_start], copy);
        ((char *)buf)[copy] = '\0';
        if (bytes_read) *bytes_read = copy;

        EAI_LOG_DEBUG(LOG_MOD, "received from topic bytes=%zu", copy);
    } else {
        if (bytes_read) *bytes_read = 0;
    }

    return EAI_OK;
}

static eai_status_t mqtt_subscribe(eai_fw_connector_t *conn, const char *topic,
                                    void (*callback)(const char *, const void *, size_t))
{
    mqtt_ctx_t *ctx = (mqtt_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

    ctx->sub_callback = callback;
    strncpy(ctx->sub_topic, topic, sizeof(ctx->sub_topic) - 1);

    uint16_t topic_len = (uint16_t)strlen(topic);
    ctx->packet_id++;

    /* Build SUBSCRIBE packet */
    uint32_t remaining = 2 + 2 + topic_len + 1; /* packet_id + topic_len + topic + QoS */
    uint8_t pkt[256];
    int pos = 0;
    pkt[pos++] = MQTT_SUBSCRIBE;
    pos += mqtt_encode_remaining_length(&pkt[pos], remaining);
    pkt[pos++] = (uint8_t)(ctx->packet_id >> 8);
    pkt[pos++] = (uint8_t)(ctx->packet_id);
    pkt[pos++] = (uint8_t)(topic_len >> 8);
    pkt[pos++] = (uint8_t)(topic_len);
    memcpy(&pkt[pos], topic, topic_len);
    pos += topic_len;
    pkt[pos++] = 0x00; /* QoS 0 */

    if (mqtt_send_raw(ctx, pkt, (size_t)pos) != 0)
        return EAI_ERR_CONNECT;

    /* Wait for SUBACK */
    uint8_t suback[5];
    if (mqtt_recv_raw(ctx, suback, 5) != 0)
        return EAI_ERR_CONNECT;

    EAI_LOG_INFO(LOG_MOD, "subscribed to topic='%s'", topic);
    return EAI_OK;
}

const eai_connector_ops_t eai_connector_mqtt_ops = {
    .name       = "mqtt",
    .type       = EAI_CONN_MQTT,
    .connect    = mqtt_connect,
    .disconnect = mqtt_disconnect,
    .read       = mqtt_read,
    .write      = mqtt_write,
    .subscribe  = mqtt_subscribe,
};
