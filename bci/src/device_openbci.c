// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/device.h"
#include "eai/log.h"
#include <string.h>

#ifdef EAI_BCI_OPENBCI_ENABLED

typedef struct {
    char     port[64];
    uint32_t baud_rate;
    uint8_t  board_type;
    bool     streaming;
} openbci_ctx_t;

static openbci_ctx_t g_openbci_ctx;

static eai_status_t openbci_init(eai_bci_device_t *dev, const eai_kv_t *params, int param_count)
{
    memset(&g_openbci_ctx, 0, sizeof(g_openbci_ctx));
    strncpy(g_openbci_ctx.port, "/dev/ttyUSB0", sizeof(g_openbci_ctx.port) - 1);
    g_openbci_ctx.baud_rate  = 115200;
    g_openbci_ctx.board_type = 0;

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "port") == 0)
            strncpy(g_openbci_ctx.port, params[i].value, sizeof(g_openbci_ctx.port) - 1);
        else if (strcmp(params[i].key, "baud_rate") == 0)
            g_openbci_ctx.baud_rate = (uint32_t)atoi(params[i].value);
    }

    dev->ctx            = &g_openbci_ctx;
    dev->num_channels   = 8;
    dev->sample_rate_hz = 250;
    dev->state          = EAI_BCI_STATE_DISCONNECTED;

    EAI_LOG_INFO("OpenBCI: init on %s @ %u baud", g_openbci_ctx.port, g_openbci_ctx.baud_rate);
    return EAI_OK;
}

static eai_status_t openbci_start(eai_bci_device_t *dev)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;
    EAI_LOG_INFO("OpenBCI: start stream (stub) on %s", ctx->port);
    ctx->streaming = true;
    dev->state = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t openbci_stop(eai_bci_device_t *dev)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;
    ctx->streaming = false;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    return EAI_OK;
}

static eai_status_t openbci_read(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                  int max_samples, int *samples_read)
{
    openbci_ctx_t *ctx = (openbci_ctx_t *)dev->ctx;
    if (!ctx->streaming) return EAI_ERR_BCI_SIGNAL;

    int count = max_samples > 8 ? 8 : max_samples;
    memset(out, 0, count * sizeof(eai_bci_sample_t));
    for (int i = 0; i < count; i++) {
        out[i].quality = 80;
    }
    if (samples_read) *samples_read = count;
    return EAI_OK;
}

static eai_status_t openbci_calibrate(eai_bci_device_t *dev)
{
    dev->state = EAI_BCI_STATE_CALIBRATING;
    dev->state = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t openbci_impedance(eai_bci_device_t *dev, float *ohms, int max_ch)
{
    (void)dev;
    int n = max_ch > 8 ? 8 : max_ch;
    for (int i = 0; i < n; i++) ohms[i] = 10000.0f;
    return EAI_OK;
}

static void openbci_shutdown(eai_bci_device_t *dev)
{
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    dev->ctx = NULL;
}

const eai_bci_device_ops_t eai_bci_device_openbci_ops = {
    .name          = "openbci",
    .type          = EAI_BCI_DEV_OPENBCI,
    .init          = openbci_init,
    .start_stream  = openbci_start,
    .stop_stream   = openbci_stop,
    .read_samples  = openbci_read,
    .calibrate     = openbci_calibrate,
    .get_impedance = openbci_impedance,
    .shutdown      = openbci_shutdown,
};

#endif /* EAI_BCI_OPENBCI_ENABLED */
