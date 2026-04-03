// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/signal.h"
#include <string.h>
#include <math.h>

/* ========== Ring Buffer Operations ========== */

eai_status_t eai_bci_signal_init(eai_bci_signal_t *sig, uint8_t num_channels,
                                  uint32_t sample_rate_hz)
{
    if (!sig || num_channels == 0 || num_channels > EAI_BCI_MAX_CHANNELS)
        return EAI_ERR_INVALID;

    memset(sig, 0, sizeof(*sig));
    sig->num_channels  = num_channels;
    sig->sample_rate_hz = sample_rate_hz;
    sig->head  = 0;
    sig->count = 0;
    return EAI_OK;
}

eai_status_t eai_bci_signal_push(eai_bci_signal_t *sig, const eai_bci_sample_t *sample)
{
    if (!sig || !sample)
        return EAI_ERR_INVALID;

    sig->samples[sig->head] = *sample;
    sig->head = (sig->head + 1) % EAI_BCI_RING_SIZE;
    if (sig->count < EAI_BCI_RING_SIZE)
        sig->count++;
    return EAI_OK;
}

int eai_bci_signal_get_window(const eai_bci_signal_t *sig, eai_bci_sample_t *out,
                               int window_size)
{
    if (!sig || !out || window_size <= 0)
        return 0;

    int available = (window_size < sig->count) ? window_size : sig->count;
    int start = (sig->head - available + EAI_BCI_RING_SIZE) % EAI_BCI_RING_SIZE;

    for (int i = 0; i < available; i++) {
        int idx = (start + i) % EAI_BCI_RING_SIZE;
        out[i] = sig->samples[idx];
    }
    return available;
}

void eai_bci_signal_clear(eai_bci_signal_t *sig)
{
    if (!sig) return;
    sig->head  = 0;
    sig->count = 0;
}

int eai_bci_signal_count(const eai_bci_signal_t *sig)
{
    return sig ? sig->count : 0;
}

/* ========== IIR Filter Design ========== */

static void design_bandpass_iir(eai_bci_iir_filter_t *f, float sample_rate,
                                 float low_hz, float high_hz)
{
    float w_low  = 2.0f * (float)M_PI * low_hz  / sample_rate;
    float w_high = 2.0f * (float)M_PI * high_hz / sample_rate;
    float bw = w_high - w_low;
    float w0 = (w_low + w_high) / 2.0f;

    float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bw * w0 / sinf(w0));

    float b0 =  alpha;
    float b1 =  0.0f;
    float b2 = -alpha;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * cosf(w0);
    float a2 =  1.0f - alpha;

    f->b[0] = b0 / a0;
    f->b[1] = b1 / a0;
    f->b[2] = b2 / a0;
    f->a[0] = 1.0f;
    f->a[1] = a1 / a0;
    f->a[2] = a2 / a0;
}

static void design_notch_iir(eai_bci_iir_filter_t *f, float sample_rate, float notch_hz)
{
    float w0 = 2.0f * (float)M_PI * notch_hz / sample_rate;
    float bw = w0 * 0.05f;
    float alpha = sinf(w0) * sinhf(logf(2.0f) / 2.0f * bw * w0 / sinf(w0));

    float b0 =  1.0f;
    float b1 = -2.0f * cosf(w0);
    float b2 =  1.0f;
    float a0 =  1.0f + alpha;
    float a1 = -2.0f * cosf(w0);
    float a2 =  1.0f - alpha;

    f->b[0] = b0 / a0;
    f->b[1] = b1 / a0;
    f->b[2] = b2 / a0;
    f->a[0] = 1.0f;
    f->a[1] = a1 / a0;
    f->a[2] = a2 / a0;
}

static float apply_iir(eai_bci_iir_filter_t *f, int channel, float x)
{
    float y = f->b[0] * x
            + f->b[1] * f->x_hist[channel][0]
            + f->b[2] * f->x_hist[channel][1]
            - f->a[1] * f->y_hist[channel][0]
            - f->a[2] * f->y_hist[channel][1];

    f->x_hist[channel][1] = f->x_hist[channel][0];
    f->x_hist[channel][0] = x;
    f->y_hist[channel][1] = f->y_hist[channel][0];
    f->y_hist[channel][0] = y;

    return y;
}

/* ========== Preprocessor ========== */

eai_status_t eai_bci_preprocess_init(eai_bci_preprocessor_t *pp, int num_channels,
                                      float sample_rate_hz, float low_hz, float high_hz,
                                      float notch_hz)
{
    if (!pp || num_channels <= 0 || num_channels > EAI_BCI_MAX_CHANNELS)
        return EAI_ERR_INVALID;

    memset(pp, 0, sizeof(*pp));
    pp->num_channels = num_channels;
    pp->dc_alpha = 0.995f;

    pp->bandpass.num_channels = num_channels;
    design_bandpass_iir(&pp->bandpass, sample_rate_hz, low_hz, high_hz);

    pp->notch.num_channels = num_channels;
    design_notch_iir(&pp->notch, sample_rate_hz, notch_hz);

    pp->initialized = true;
    return EAI_OK;
}

eai_status_t eai_bci_preprocess_sample(eai_bci_preprocessor_t *pp, eai_bci_sample_t *sample)
{
    if (!pp || !sample || !pp->initialized)
        return EAI_ERR_INVALID;

    for (int ch = 0; ch < pp->num_channels; ch++) {
        float x = sample->channels[ch];

        /* DC offset removal (exponential moving average) */
        pp->dc_offset[ch] = pp->dc_alpha * pp->dc_offset[ch] + (1.0f - pp->dc_alpha) * x;
        x -= pp->dc_offset[ch];

        /* Bandpass filter */
        x = apply_iir(&pp->bandpass, ch, x);

        /* Notch filter */
        x = apply_iir(&pp->notch, ch, x);

        sample->channels[ch] = x;
    }
    return EAI_OK;
}

/* ========== Band Power ========== */

float eai_bci_band_power(const eai_bci_signal_t *sig, int channel, int window_size)
{
    if (!sig || channel < 0 || channel >= sig->num_channels || sig->count == 0)
        return 0.0f;

    int n = (window_size < sig->count) ? window_size : sig->count;
    int start = (sig->head - n + EAI_BCI_RING_SIZE) % EAI_BCI_RING_SIZE;

    float sum_sq = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = (start + i) % EAI_BCI_RING_SIZE;
        float v = sig->samples[idx].channels[channel];
        sum_sq += v * v;
    }
    return sum_sq / (float)n;
}
