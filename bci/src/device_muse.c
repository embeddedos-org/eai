// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/device.h"
#include "eai/log.h"
#include <string.h>

#ifdef EAI_BCI_MUSE_ENABLED

typedef struct {
    char     address[64];
    bool     streaming;
    bool     ppg_enabled;
    bool     accel_enabled;
} muse_ctx_t;

static muse_ctx_t g_muse_ctx;

static eai_status_t muse_init(eai_bci_device_t *dev, const eai_kv_t *params, int param_count)
{
    memset(&g_muse_ctx, 0, sizeof(g_muse_ctx));
    g_muse_ctx.ppg_enabled   = false;
    g_muse_ctx.accel_enabled = false;

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "address") == 0)
            strncpy(g_muse_ctx.address, params[i].value, sizeof(g_muse_ctx.address) - 1);
        else if (strcmp(params[i].key, "ppg") == 0)
            g_muse_ctx.ppg_enabled = (strcmp(params[i].value, "true") == 0);
        else if (strcmp(params[i].key, "accel") == 0)
            g_muse_ctx.accel_enabled = (strcmp(params[i].value, "true") == 0);
    }

    dev->ctx            = &g_muse_ctx;
    dev->num_channels   = 4;
    dev->sample_rate_hz = 256;
    dev->state          = EAI_BCI_STATE_DISCONNECTED;

    EAI_LOG_INFO("bci", "Muse: init (4ch EEG, PPG=%s, accel=%s)",
                 g_muse_ctx.ppg_enabled ? "on" : "off",
                 g_muse_ctx.accel_enabled ? "on" : "off");
    return EAI_OK;
}

static eai_status_t muse_start(eai_bci_device_t *dev)
{
    muse_ctx_t *ctx = (muse_ctx_t *)dev->ctx;
    ctx->streaming = true;
    dev->state = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t muse_stop(eai_bci_device_t *dev)
{
    muse_ctx_t *ctx = (muse_ctx_t *)dev->ctx;
    ctx->streaming = false;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    return EAI_OK;
}

static eai_status_t muse_read(eai_bci_device_t *dev, eai_bci_sample_t *out,
                               int max_samples, int *samples_read)
{
    muse_ctx_t *ctx = (muse_ctx_t *)dev->ctx;
    if (!ctx->streaming) return EAI_ERR_BCI_SIGNAL;

    int count = max_samples > 12 ? 12 : max_samples;
    memset(out, 0, count * sizeof(eai_bci_sample_t));
    if (samples_read) *samples_read = count;
    return EAI_OK;
}

static void muse_shutdown(eai_bci_device_t *dev)
{
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    dev->ctx = NULL;
}

const eai_bci_device_ops_t eai_bci_device_muse_ops = {
    .name          = "muse",
    .type          = EAI_BCI_DEV_MUSE,
    .init          = muse_init,
    .start_stream  = muse_start,
    .stop_stream   = muse_stop,
    .read_samples  = muse_read,
    .calibrate     = NULL,
    .get_impedance = NULL,
    .shutdown      = muse_shutdown,
};

#endif /* EAI_BCI_MUSE_ENABLED */
