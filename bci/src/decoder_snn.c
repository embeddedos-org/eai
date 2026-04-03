// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
// Spiking Neural Network (SNN) decoder — Leaky Integrate-and-Fire model

#include "eai_bci/decoder.h"
#include "eai_bci/signal.h"
#include <string.h>
#include <math.h>

#define SNN_MAX_HIDDEN  32
#define SNN_MAX_OUTPUT  8

typedef struct {
    /* LIF neuron parameters */
    float tau_mem;
    float v_threshold;
    float v_reset;
    float dt;

    /* Network state */
    float hidden_v[SNN_MAX_HIDDEN];
    float output_v[SNN_MAX_OUTPUT];
    int   hidden_spikes[SNN_MAX_HIDDEN];
    int   output_spikes[SNN_MAX_OUTPUT];

    /* Weights (static, pre-initialized) */
    float w_in[EAI_BCI_MAX_CHANNELS][SNN_MAX_HIDDEN];
    float w_out[SNN_MAX_HIDDEN][SNN_MAX_OUTPUT];

    int num_hidden;
    int num_output;
    int num_inputs;
    int window_size;
    int spike_window;
} snn_ctx_t;

static snn_ctx_t g_snn_ctx;

static void snn_init_weights(snn_ctx_t *ctx)
{
    /* Simple deterministic weight initialization */
    for (int i = 0; i < ctx->num_inputs; i++) {
        for (int h = 0; h < ctx->num_hidden; h++) {
            ctx->w_in[i][h] = 0.5f * ((float)((i * 7 + h * 13) % 17) / 17.0f - 0.5f);
        }
    }
    for (int h = 0; h < ctx->num_hidden; h++) {
        for (int o = 0; o < ctx->num_output; o++) {
            ctx->w_out[h][o] = 0.3f * ((float)((h * 11 + o * 5) % 13) / 13.0f - 0.5f);
        }
    }
}

static float lif_step(float v, float input, float tau, float threshold,
                       float reset, float dt, int *spiked)
{
    float dv = (-v + input) / tau * dt;
    v += dv;
    if (v >= threshold) {
        *spiked = 1;
        return reset;
    }
    *spiked = 0;
    return v;
}

static eai_status_t snn_init(eai_bci_decoder_t *dec, const eai_kv_t *params, int param_count)
{
    memset(&g_snn_ctx, 0, sizeof(g_snn_ctx));

    g_snn_ctx.tau_mem     = 20.0f;
    g_snn_ctx.v_threshold = 1.0f;
    g_snn_ctx.v_reset     = 0.0f;
    g_snn_ctx.dt          = 1.0f;
    g_snn_ctx.num_hidden  = 16;
    g_snn_ctx.num_output  = 4;
    g_snn_ctx.num_inputs  = 4;
    g_snn_ctx.window_size = 32;
    g_snn_ctx.spike_window = 10;

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "hidden") == 0)
            g_snn_ctx.num_hidden = atoi(params[i].value);
        else if (strcmp(params[i].key, "classes") == 0)
            g_snn_ctx.num_output = atoi(params[i].value);
    }

    if (g_snn_ctx.num_hidden > SNN_MAX_HIDDEN) g_snn_ctx.num_hidden = SNN_MAX_HIDDEN;
    if (g_snn_ctx.num_output > SNN_MAX_OUTPUT) g_snn_ctx.num_output = SNN_MAX_OUTPUT;

    snn_init_weights(&g_snn_ctx);

    dec->ctx = &g_snn_ctx;
    dec->initialized = true;
    return EAI_OK;
}

static eai_status_t snn_decode(eai_bci_decoder_t *dec, const eai_bci_signal_t *signal,
                                eai_bci_intent_t *intent)
{
    snn_ctx_t *ctx = (snn_ctx_t *)dec->ctx;
    if (!ctx || !signal || !intent) return EAI_ERR_INVALID;
    if (signal->count < 4) return EAI_ERR_BCI_SIGNAL;

    /* Reset spike counters */
    memset(ctx->output_spikes, 0, sizeof(ctx->output_spikes));

    /* Rate-encode: compute band power per channel as input current */
    float inputs[EAI_BCI_MAX_CHANNELS];
    int nch = signal->num_channels < ctx->num_inputs ? signal->num_channels : ctx->num_inputs;
    for (int ch = 0; ch < nch; ch++) {
        inputs[ch] = eai_bci_band_power(signal, ch, ctx->window_size);
        inputs[ch] = sqrtf(inputs[ch]) * 0.1f;
    }

    /* Run SNN for spike_window timesteps */
    for (int t = 0; t < ctx->spike_window; t++) {
        /* Hidden layer */
        for (int h = 0; h < ctx->num_hidden; h++) {
            float I = 0.0f;
            for (int i = 0; i < nch; i++)
                I += inputs[i] * ctx->w_in[i][h];

            int spiked;
            ctx->hidden_v[h] = lif_step(ctx->hidden_v[h], I, ctx->tau_mem,
                                         ctx->v_threshold, ctx->v_reset,
                                         ctx->dt, &spiked);
            ctx->hidden_spikes[h] = spiked;
        }

        /* Output layer */
        for (int o = 0; o < ctx->num_output; o++) {
            float I = 0.0f;
            for (int h = 0; h < ctx->num_hidden; h++) {
                if (ctx->hidden_spikes[h])
                    I += ctx->w_out[h][o];
            }

            int spiked;
            ctx->output_v[o] = lif_step(ctx->output_v[o], I, ctx->tau_mem,
                                         ctx->v_threshold, ctx->v_reset,
                                         ctx->dt, &spiked);
            if (spiked) ctx->output_spikes[o]++;
        }
    }

    /* Winner-take-all: class with most spikes */
    int best_class = 0;
    int max_spikes = 0;
    int total_spikes = 0;
    for (int o = 0; o < ctx->num_output; o++) {
        total_spikes += ctx->output_spikes[o];
        if (ctx->output_spikes[o] > max_spikes) {
            max_spikes = ctx->output_spikes[o];
            best_class = o;
        }
    }

    static const char *snn_labels[] = {
        "rest", "left_hand", "right_hand", "feet",
        "tongue", "mental_math", "relax", "focus"
    };

    memset(intent, 0, sizeof(*intent));
    strncpy(intent->label, snn_labels[best_class], EAI_BCI_LABEL_MAX - 1);
    intent->class_id = (uint32_t)best_class;
    intent->confidence = total_spikes > 0 ?
        (float)max_spikes / (float)total_spikes : 0.0f;

    int latest_idx = (signal->head - 1 + EAI_BCI_RING_SIZE) % EAI_BCI_RING_SIZE;
    intent->timestamp_us = signal->samples[latest_idx].timestamp_us;

    return EAI_OK;
}

static eai_status_t snn_reset(eai_bci_decoder_t *dec)
{
    snn_ctx_t *ctx = (snn_ctx_t *)dec->ctx;
    if (!ctx) return EAI_ERR_INVALID;
    memset(ctx->hidden_v, 0, sizeof(ctx->hidden_v));
    memset(ctx->output_v, 0, sizeof(ctx->output_v));
    return EAI_OK;
}

static void snn_shutdown(eai_bci_decoder_t *dec)
{
    dec->initialized = false;
    dec->ctx = NULL;
}

const eai_bci_decoder_ops_t eai_bci_decoder_snn_ops = {
    .name     = "snn",
    .type     = EAI_BCI_DEC_SNN,
    .init     = snn_init,
    .decode   = snn_decode,
    .reset    = snn_reset,
    .shutdown = snn_shutdown,
};
