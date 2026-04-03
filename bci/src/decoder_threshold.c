// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/decoder.h"
#include "eai_bci/signal.h"
#include <string.h>
#include <stdio.h>

/* ========== Threshold Decoder Context ========== */

typedef struct {
    float    thresholds[EAI_BCI_MAX_CLASSES];
    char     labels[EAI_BCI_MAX_CLASSES][EAI_BCI_LABEL_MAX];
    int      num_classes;
    int      window_size;
    int      primary_channel;
} threshold_ctx_t;

static threshold_ctx_t g_threshold_ctx;

static const char *default_labels[] = {
    "rest", "left_hand", "right_hand", "feet",
    "tongue", "mental_math", "relax", "focus"
};

/* ========== Threshold Decoder Ops ========== */

static eai_status_t threshold_init(eai_bci_decoder_t *dec, const eai_kv_t *params,
                                    int param_count)
{
    memset(&g_threshold_ctx, 0, sizeof(g_threshold_ctx));

    g_threshold_ctx.num_classes     = 4;
    g_threshold_ctx.window_size     = 64;
    g_threshold_ctx.primary_channel = 0;

    /* Default thresholds for band-power classification */
    g_threshold_ctx.thresholds[0] = 5.0f;    /* rest: low power */
    g_threshold_ctx.thresholds[1] = 15.0f;   /* left_hand: medium */
    g_threshold_ctx.thresholds[2] = 30.0f;   /* right_hand: high */
    g_threshold_ctx.thresholds[3] = 50.0f;   /* feet: very high */

    for (int i = 0; i < EAI_BCI_MAX_CLASSES && i < 8; i++) {
        strncpy(g_threshold_ctx.labels[i], default_labels[i], EAI_BCI_LABEL_MAX - 1);
    }

    for (int i = 0; i < param_count && params; i++) {
        if (strcmp(params[i].key, "num_classes") == 0)
            g_threshold_ctx.num_classes = atoi(params[i].value);
        else if (strcmp(params[i].key, "window_size") == 0)
            g_threshold_ctx.window_size = atoi(params[i].value);
        else if (strcmp(params[i].key, "channel") == 0)
            g_threshold_ctx.primary_channel = atoi(params[i].value);
    }

    if (g_threshold_ctx.num_classes > EAI_BCI_MAX_CLASSES)
        g_threshold_ctx.num_classes = EAI_BCI_MAX_CLASSES;

    dec->ctx         = &g_threshold_ctx;
    dec->initialized = true;
    return EAI_OK;
}

static eai_status_t threshold_decode(eai_bci_decoder_t *dec, const eai_bci_signal_t *signal,
                                      eai_bci_intent_t *intent)
{
    threshold_ctx_t *ctx = (threshold_ctx_t *)dec->ctx;

    if (!ctx || !signal || !intent)
        return EAI_ERR_INVALID;

    if (signal->count < 4)
        return EAI_ERR_BCI_SIGNAL;

    /* Compute band power on primary channel */
    float power = eai_bci_band_power(signal, ctx->primary_channel, ctx->window_size);

    /* Multi-channel: also check adjacent channels for lateralization */
    float power_sum = power;
    int ch_count = 1;
    for (int ch = 0; ch < signal->num_channels && ch < EAI_BCI_MAX_CHANNELS; ch++) {
        if (ch != ctx->primary_channel) {
            power_sum += eai_bci_band_power(signal, ch, ctx->window_size);
            ch_count++;
        }
    }
    float avg_power = power_sum / (float)ch_count;

    /* Classify by threshold comparison */
    int best_class = 0;
    float best_diff = 1e9f;

    for (int c = 0; c < ctx->num_classes; c++) {
        float diff = fabsf(avg_power - ctx->thresholds[c]);
        if (diff < best_diff) {
            best_diff  = diff;
            best_class = c;
        }
    }

    /* Compute confidence (inverse distance, normalized) */
    float max_threshold = ctx->thresholds[ctx->num_classes - 1];
    float confidence = 1.0f - (best_diff / (max_threshold + 1.0f));
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    memset(intent, 0, sizeof(*intent));
    strncpy(intent->label, ctx->labels[best_class], EAI_BCI_LABEL_MAX - 1);
    intent->class_id   = (uint32_t)best_class;
    intent->confidence = confidence;

    /* Use latest sample timestamp */
    int latest_idx = (signal->head - 1 + EAI_BCI_RING_SIZE) % EAI_BCI_RING_SIZE;
    intent->timestamp_us = signal->samples[latest_idx].timestamp_us;

    return EAI_OK;
}

static eai_status_t threshold_reset(eai_bci_decoder_t *dec)
{
    (void)dec;
    return EAI_OK;
}

static void threshold_shutdown(eai_bci_decoder_t *dec)
{
    dec->initialized = false;
    dec->ctx = NULL;
}

/* ========== Decoder Convenience Functions ========== */

eai_status_t eai_bci_decoder_init(eai_bci_decoder_t *dec, const eai_bci_decoder_ops_t *ops,
                                   const eai_kv_t *params, int param_count)
{
    if (!dec || !ops || !ops->init) return EAI_ERR_INVALID;
    memset(dec, 0, sizeof(*dec));
    dec->ops = ops;
    return ops->init(dec, params, param_count);
}

eai_status_t eai_bci_decoder_decode(eai_bci_decoder_t *dec, const eai_bci_signal_t *signal,
                                     eai_bci_intent_t *intent)
{
    if (!dec || !dec->ops || !dec->ops->decode || !dec->initialized)
        return EAI_ERR_INVALID;
    return dec->ops->decode(dec, signal, intent);
}

eai_status_t eai_bci_decoder_reset(eai_bci_decoder_t *dec)
{
    if (!dec || !dec->ops || !dec->ops->reset) return EAI_ERR_INVALID;
    return dec->ops->reset(dec);
}

void eai_bci_decoder_shutdown(eai_bci_decoder_t *dec)
{
    if (!dec || !dec->ops || !dec->ops->shutdown) return;
    dec->ops->shutdown(dec);
}

/* ========== Exported Threshold Ops ========== */

const eai_bci_decoder_ops_t eai_bci_decoder_threshold_ops = {
    .name     = "threshold",
    .type     = EAI_BCI_DEC_THRESHOLD,
    .init     = threshold_init,
    .decode   = threshold_decode,
    .reset    = threshold_reset,
    .shutdown = threshold_shutdown,
};
