// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/device.h"
#include "eai/log.h"
#include <string.h>

#ifdef EAI_BCI_EMOTIV_ENABLED

typedef struct {
    char     serial[64];
    bool     streaming;
    uint8_t  headset_type;
} emotiv_ctx_t;

static emotiv_ctx_t g_emotiv_ctx;

static eai_status_t emotiv_init(eai_bci_device_t *dev, const eai_kv_t *params, int param_count)
{
    memset(&g_emotiv_ctx, 0, sizeof(g_emotiv_ctx));
    g_emotiv_ctx.headset_type = 0;

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "serial") == 0)
            strncpy(g_emotiv_ctx.serial, params[i].value, sizeof(g_emotiv_ctx.serial) - 1);
        else if (strcmp(params[i].key, "headset") == 0)
            g_emotiv_ctx.headset_type = (uint8_t)atoi(params[i].value);
    }

    dev->ctx            = &g_emotiv_ctx;
    dev->num_channels   = g_emotiv_ctx.headset_type == 1 ? 32 : 14;
    dev->sample_rate_hz = 256;
    dev->state          = EAI_BCI_STATE_DISCONNECTED;

    EAI_LOG_INFO("bci", "Emotiv: init headset type %d (%d ch)", g_emotiv_ctx.headset_type, dev->num_channels);
    return EAI_OK;
}

static eai_status_t emotiv_start(eai_bci_device_t *dev)
{
    emotiv_ctx_t *ctx = (emotiv_ctx_t *)dev->ctx;
    ctx->streaming = true;
    dev->state = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t emotiv_stop(eai_bci_device_t *dev)
{
    emotiv_ctx_t *ctx = (emotiv_ctx_t *)dev->ctx;
    ctx->streaming = false;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    return EAI_OK;
}

static eai_status_t emotiv_read(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                 int max_samples, int *samples_read)
{
    emotiv_ctx_t *ctx = (emotiv_ctx_t *)dev->ctx;
    if (!ctx->streaming) return EAI_ERR_BCI_SIGNAL;

    int count = max_samples > 8 ? 8 : max_samples;
    memset(out, 0, count * sizeof(eai_bci_sample_t));
    if (samples_read) *samples_read = count;
    return EAI_OK;
}

static void emotiv_shutdown(eai_bci_device_t *dev)
{
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    dev->ctx = NULL;
}

const eai_bci_device_ops_t eai_bci_device_emotiv_ops = {
    .name          = "emotiv",
    .type          = EAI_BCI_DEV_EMOTIV,
    .init          = emotiv_init,
    .start_stream  = emotiv_start,
    .stop_stream   = emotiv_stop,
    .read_samples  = emotiv_read,
    .calibrate     = NULL,
    .get_impedance = NULL,
    .shutdown      = emotiv_shutdown,
};

#endif /* EAI_BCI_EMOTIV_ENABLED */
