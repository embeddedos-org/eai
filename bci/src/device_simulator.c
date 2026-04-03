// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/device.h"
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ========== Simulator Context ========== */

typedef struct {
    uint32_t sample_rate_hz;
    uint8_t  num_channels;
    uint64_t sample_counter;
    uint32_t seed;
    bool     streaming;
    /* Paradigm: 0=alpha, 1=motor_imagery, 2=p300, 3=ssvep */
    int      paradigm;
    float    snr_db;
} sim_ctx_t;

static sim_ctx_t g_sim_ctx;

/* Deterministic PRNG (xorshift32) */
static float sim_noise(sim_ctx_t *ctx)
{
    ctx->seed ^= ctx->seed << 13;
    ctx->seed ^= ctx->seed >> 17;
    ctx->seed ^= ctx->seed << 5;
    return ((float)(ctx->seed & 0xFFFF) / 32768.0f) - 1.0f;
}

/* ========== Simulator Ops ========== */

static eai_status_t sim_init(eai_bci_device_t *dev, const eai_kv_t *params, int param_count)
{
    memset(&g_sim_ctx, 0, sizeof(g_sim_ctx));
    g_sim_ctx.sample_rate_hz = 250;
    g_sim_ctx.num_channels   = 4;
    g_sim_ctx.seed           = 42;
    g_sim_ctx.paradigm       = 0;
    g_sim_ctx.snr_db         = 10.0f;

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "sample_rate") == 0)
            g_sim_ctx.sample_rate_hz = (uint32_t)atoi(params[i].value);
        else if (strcmp(params[i].key, "channels") == 0)
            g_sim_ctx.num_channels = (uint8_t)atoi(params[i].value);
        else if (strcmp(params[i].key, "paradigm") == 0)
            g_sim_ctx.paradigm = atoi(params[i].value);
        else if (strcmp(params[i].key, "seed") == 0)
            g_sim_ctx.seed = (uint32_t)atoi(params[i].value);
    }

    if (g_sim_ctx.num_channels > EAI_BCI_MAX_CHANNELS)
        g_sim_ctx.num_channels = EAI_BCI_MAX_CHANNELS;

    dev->ctx            = &g_sim_ctx;
    dev->num_channels   = g_sim_ctx.num_channels;
    dev->sample_rate_hz = g_sim_ctx.sample_rate_hz;
    dev->state          = EAI_BCI_STATE_DISCONNECTED;

    return EAI_OK;
}

static eai_status_t sim_start_stream(eai_bci_device_t *dev)
{
    sim_ctx_t *ctx = (sim_ctx_t *)dev->ctx;
    ctx->streaming      = true;
    ctx->sample_counter = 0;
    dev->state          = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t sim_stop_stream(eai_bci_device_t *dev)
{
    sim_ctx_t *ctx = (sim_ctx_t *)dev->ctx;
    ctx->streaming = false;
    dev->state     = EAI_BCI_STATE_DISCONNECTED;
    return EAI_OK;
}

static float generate_eeg_sample(sim_ctx_t *ctx, int channel, uint64_t sample_idx)
{
    float t = (float)sample_idx / (float)ctx->sample_rate_hz;
    float signal = 0.0f;
    float noise_amp = powf(10.0f, -ctx->snr_db / 20.0f);

    switch (ctx->paradigm) {
    case 0: /* Alpha rhythm (8-13 Hz) */
        signal = 10.0f * sinf(2.0f * (float)M_PI * 10.0f * t);
        if (channel > 0)
            signal *= (1.0f - 0.1f * channel);
        break;

    case 1: /* Motor imagery - mu rhythm (8-12 Hz) with lateralization */
        if (channel < ctx->num_channels / 2) {
            /* Left hemisphere: mu suppression */
            signal = 3.0f * sinf(2.0f * (float)M_PI * 10.0f * t);
        } else {
            /* Right hemisphere: mu enhancement */
            signal = 12.0f * sinf(2.0f * (float)M_PI * 10.0f * t);
        }
        /* Add beta rebound */
        signal += 5.0f * sinf(2.0f * (float)M_PI * 20.0f * t);
        break;

    case 2: /* P300 - stimulus-locked positive deflection */
        {
            float epoch_t = fmodf(t, 1.0f);
            if (epoch_t > 0.3f && epoch_t < 0.5f) {
                signal = 8.0f * sinf((float)M_PI * (epoch_t - 0.3f) / 0.2f);
            }
        }
        break;

    case 3: /* SSVEP - steady-state at stimulus frequency */
        signal = 8.0f * sinf(2.0f * (float)M_PI * 15.0f * t);
        signal += 4.0f * sinf(2.0f * (float)M_PI * 30.0f * t);
        break;

    default:
        signal = 5.0f * sinf(2.0f * (float)M_PI * 10.0f * t);
        break;
    }

    /* Add background EEG + noise */
    signal += 2.0f * sinf(2.0f * (float)M_PI * 7.0f * t);   /* theta */
    signal += 1.0f * sinf(2.0f * (float)M_PI * 40.0f * t);  /* gamma */
    signal += noise_amp * sim_noise(ctx) * 5.0f;

    return signal;
}

static eai_status_t sim_read_samples(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                      int max_samples, int *samples_read)
{
    sim_ctx_t *ctx = (sim_ctx_t *)dev->ctx;
    if (!ctx->streaming) return EAI_ERR_BCI_SIGNAL;

    int count = max_samples > 16 ? 16 : max_samples;

    for (int s = 0; s < count; s++) {
        memset(&out[s], 0, sizeof(eai_bci_sample_t));
        out[s].timestamp_us = ctx->sample_counter * 1000000ULL / ctx->sample_rate_hz;
        out[s].quality      = 100;

        for (int ch = 0; ch < ctx->num_channels; ch++) {
            out[s].channels[ch] = generate_eeg_sample(ctx, ch, ctx->sample_counter);
        }
        ctx->sample_counter++;
    }

    if (samples_read) *samples_read = count;
    return EAI_OK;
}

static eai_status_t sim_calibrate(eai_bci_device_t *dev)
{
    dev->state = EAI_BCI_STATE_CALIBRATING;
    dev->state = EAI_BCI_STATE_STREAMING;
    return EAI_OK;
}

static eai_status_t sim_get_impedance(eai_bci_device_t *dev, float *impedance_ohms,
                                       int max_channels)
{
    sim_ctx_t *ctx = (sim_ctx_t *)dev->ctx;
    int n = max_channels < ctx->num_channels ? max_channels : ctx->num_channels;
    for (int i = 0; i < n; i++) {
        impedance_ohms[i] = 5000.0f + (float)(i * 500);
    }
    return EAI_OK;
}

static void sim_shutdown(eai_bci_device_t *dev)
{
    sim_ctx_t *ctx = (sim_ctx_t *)dev->ctx;
    if (ctx) ctx->streaming = false;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    dev->ctx   = NULL;
}

/* ========== Device Convenience Functions ========== */

eai_status_t eai_bci_device_init(eai_bci_device_t *dev, const eai_bci_device_ops_t *ops,
                                  const eai_kv_t *params, int param_count)
{
    if (!dev || !ops || !ops->init) return EAI_ERR_INVALID;
    memset(dev, 0, sizeof(*dev));
    dev->ops   = ops;
    dev->state = EAI_BCI_STATE_DISCONNECTED;
    return ops->init(dev, params, param_count);
}

eai_status_t eai_bci_device_start(eai_bci_device_t *dev)
{
    if (!dev || !dev->ops || !dev->ops->start_stream) return EAI_ERR_INVALID;
    return dev->ops->start_stream(dev);
}

eai_status_t eai_bci_device_stop(eai_bci_device_t *dev)
{
    if (!dev || !dev->ops || !dev->ops->stop_stream) return EAI_ERR_INVALID;
    return dev->ops->stop_stream(dev);
}

eai_status_t eai_bci_device_read(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                  int max_samples, int *samples_read)
{
    if (!dev || !dev->ops || !dev->ops->read_samples) return EAI_ERR_INVALID;
    return dev->ops->read_samples(dev, out, max_samples, samples_read);
}

void eai_bci_device_shutdown(eai_bci_device_t *dev)
{
    if (!dev || !dev->ops || !dev->ops->shutdown) return;
    dev->ops->shutdown(dev);
}

/* ========== Exported Simulator Ops ========== */

const eai_bci_device_ops_t eai_bci_device_simulator_ops = {
    .name          = "simulator",
    .type          = EAI_BCI_DEV_SIMULATOR,
    .init          = sim_init,
    .start_stream  = sim_start_stream,
    .stop_stream   = sim_stop_stream,
    .read_samples  = sim_read_samples,
    .calibrate     = sim_calibrate,
    .get_impedance = sim_get_impedance,
    .shutdown      = sim_shutdown,
};
