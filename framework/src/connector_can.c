// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/connector.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#define CAN_SOCKET_SUPPORT 1
#else
#define CAN_SOCKET_SUPPORT 0
#endif

#define LOG_MOD "conn-can"

typedef struct {
    char interface_name[32];  /* e.g., "can0", "vcan0" */
    int  bitrate;
    bool connected;
#if CAN_SOCKET_SUPPORT
    int  sock;
#endif
} can_ctx_t;

static can_ctx_t g_can_ctx;

static eai_status_t can_connect(eai_fw_connector_t *conn,
                                 const eai_kv_t *params, int param_count)
{
    can_ctx_t *ctx = &g_can_ctx;
    memset(ctx, 0, sizeof(*ctx));
    strncpy(ctx->interface_name, "can0", sizeof(ctx->interface_name) - 1);
    ctx->bitrate = 500000;
#if CAN_SOCKET_SUPPORT
    ctx->sock = -1;
#endif

    for (int i = 0; i < param_count; i++) {
        if (strcmp(params[i].key, "interface") == 0)
            strncpy(ctx->interface_name, params[i].value, sizeof(ctx->interface_name) - 1);
        if (strcmp(params[i].key, "bitrate") == 0)
            ctx->bitrate = atoi(params[i].value);
    }

    conn->ctx = ctx;

#if CAN_SOCKET_SUPPORT
    /* Create SocketCAN raw socket */
    ctx->sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (ctx->sock < 0) {
        EAI_LOG_ERROR(LOG_MOD, "failed to create CAN socket");
        return EAI_ERR_CONNECT;
    }

    /* Bind to CAN interface */
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ctx->interface_name, IFNAMSIZ - 1);
    if (ioctl(ctx->sock, SIOCGIFINDEX, &ifr) < 0) {
        EAI_LOG_ERROR(LOG_MOD, "CAN interface '%s' not found", ctx->interface_name);
        close(ctx->sock);
        ctx->sock = -1;
        return EAI_ERR_CONNECT;
    }

    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(ctx->sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        EAI_LOG_ERROR(LOG_MOD, "CAN bind failed on %s", ctx->interface_name);
        close(ctx->sock);
        ctx->sock = -1;
        return EAI_ERR_CONNECT;
    }
#else
    EAI_LOG_WARN(LOG_MOD, "SocketCAN not available on this platform, using loopback mode");
#endif

    ctx->connected = true;
    conn->state = EAI_CONN_CONNECTED;
    EAI_LOG_INFO(LOG_MOD, "connected to %s at %d bps", ctx->interface_name, ctx->bitrate);
    return EAI_OK;
}

static eai_status_t can_disconnect(eai_fw_connector_t *conn)
{
    can_ctx_t *ctx = (can_ctx_t *)conn->ctx;
    if (ctx && ctx->connected) {
#if CAN_SOCKET_SUPPORT
        if (ctx->sock >= 0) {
            close(ctx->sock);
            ctx->sock = -1;
        }
#endif
        ctx->connected = false;
    }
    conn->state = EAI_CONN_DISCONNECTED;
    EAI_LOG_INFO(LOG_MOD, "disconnected");
    return EAI_OK;
}

static eai_status_t can_read(eai_fw_connector_t *conn, const char *can_id,
                              void *buf, size_t buf_size, size_t *bytes_read)
{
    can_ctx_t *ctx = (can_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

#if CAN_SOCKET_SUPPORT
    struct can_frame frame;
    int nbytes = read(ctx->sock, &frame, sizeof(frame));
    if (nbytes < 0) {
        EAI_LOG_ERROR(LOG_MOD, "CAN read error");
        return EAI_ERR_CONNECT;
    }
    if (nbytes < (int)sizeof(frame)) {
        return EAI_ERR_INVALID;
    }

    /* Optionally filter by CAN ID */
    if (can_id) {
        uint32_t filter_id = (uint32_t)strtoul(can_id, NULL, 0);
        if ((frame.can_id & CAN_SFF_MASK) != filter_id) {
            if (bytes_read) *bytes_read = 0;
            return EAI_OK; /* Not matching ID, return empty */
        }
    }

    size_t copy = frame.can_dlc < buf_size ? frame.can_dlc : buf_size;
    memcpy(buf, frame.data, copy);
    if (bytes_read) *bytes_read = copy;

    EAI_LOG_DEBUG(LOG_MOD, "read CAN id=0x%03X dlc=%d", frame.can_id & CAN_SFF_MASK, frame.can_dlc);
#else
    /* Non-Linux fallback: return empty frame */
    uint8_t frame[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    size_t copy = sizeof(frame) < buf_size ? sizeof(frame) : buf_size;
    memcpy(buf, frame, copy);
    if (bytes_read) *bytes_read = copy;
    EAI_LOG_DEBUG(LOG_MOD, "read CAN id=%s bytes=%zu (loopback)", can_id, copy);
#endif

    return EAI_OK;
}

static eai_status_t can_write(eai_fw_connector_t *conn, const char *can_id,
                               const void *data, size_t data_len)
{
    can_ctx_t *ctx = (can_ctx_t *)conn->ctx;
    if (!ctx || !ctx->connected) return EAI_ERR_CONNECT;

    if (data_len > 8) {
        EAI_LOG_ERROR(LOG_MOD, "CAN frame max 8 bytes, got %zu", data_len);
        return EAI_ERR_INVALID;
    }

#if CAN_SOCKET_SUPPORT
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = (uint32_t)strtoul(can_id, NULL, 0);
    frame.can_dlc = (uint8_t)data_len;
    memcpy(frame.data, data, data_len);

    int nbytes = write(ctx->sock, &frame, sizeof(frame));
    if (nbytes != sizeof(frame)) {
        EAI_LOG_ERROR(LOG_MOD, "CAN write error");
        return EAI_ERR_CONNECT;
    }

    EAI_LOG_INFO(LOG_MOD, "write CAN id=0x%03X dlc=%d", frame.can_id, frame.can_dlc);
#else
    EAI_LOG_INFO(LOG_MOD, "write CAN id=%s data_len=%zu (loopback)", can_id, data_len);
#endif

    return EAI_OK;
}

const eai_connector_ops_t eai_connector_can_ops = {
    .name       = "can",
    .type       = EAI_CONN_CAN,
    .connect    = can_connect,
    .disconnect = can_disconnect,
    .read       = can_read,
    .write      = can_write,
    .subscribe  = NULL,
};

const eai_connector_ops_t eai_connector_can_ops = {
    .name       = "can",
    .type       = EAI_CONN_CAN,
    .connect    = can_connect,
    .disconnect = can_disconnect,
    .read       = can_read,
    .write      = can_write,
    .subscribe  = NULL,
};

/* ---- Connector manager implementation ---- */

eai_status_t eai_fw_conn_mgr_init(eai_fw_connector_mgr_t *mgr)
{
    if (!mgr) return EAI_ERR_INVALID;
    memset(mgr, 0, sizeof(*mgr));
    return EAI_OK;
}

eai_status_t eai_fw_conn_add(eai_fw_connector_mgr_t *mgr, const char *name,
                              const eai_connector_ops_t *ops)
{
    if (!mgr || !name || !ops) return EAI_ERR_INVALID;
    if (mgr->count >= EAI_CONNECTOR_MAX) return EAI_ERR_NOMEM;

    eai_fw_connector_t *conn = &mgr->connectors[mgr->count];
    strncpy(conn->name, name, EAI_CONNECTOR_NAME_MAX - 1);
    conn->ops = ops;
    conn->state = EAI_CONN_DISCONNECTED;
    conn->ctx = NULL;
    mgr->count++;

    EAI_LOG_INFO("conn-mgr", "added connector: %s (%s)", name, ops->name);
    return EAI_OK;
}

eai_fw_connector_t *eai_fw_conn_find(eai_fw_connector_mgr_t *mgr, const char *name)
{
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->connectors[i].name, name) == 0)
            return &mgr->connectors[i];
    }
    return NULL;
}

eai_status_t eai_fw_conn_connect_all(eai_fw_connector_mgr_t *mgr,
                                      const eai_kv_t *params, int param_count)
{
    if (!mgr) return EAI_ERR_INVALID;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->connectors[i].ops->connect) {
            eai_status_t s = mgr->connectors[i].ops->connect(
                &mgr->connectors[i], params, param_count);
            if (s != EAI_OK) {
                EAI_LOG_ERROR("conn-mgr", "failed to connect: %s", mgr->connectors[i].name);
            }
        }
    }
    return EAI_OK;
}

void eai_fw_conn_disconnect_all(eai_fw_connector_mgr_t *mgr)
{
    if (!mgr) return;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->connectors[i].ops->disconnect) {
            mgr->connectors[i].ops->disconnect(&mgr->connectors[i]);
        }
    }
}
