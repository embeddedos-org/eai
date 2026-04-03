// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_SIGNAL_H
#define EAI_BCI_SIGNAL_H

#include "eai_bci/types.h"

typedef struct {
    float a[EAI_BCI_FILTER_ORDER + 1];
    float b[EAI_BCI_FILTER_ORDER + 1];
    float x_hist[EAI_BCI_MAX_CHANNELS][EAI_BCI_FILTER_ORDER];
    float y_hist[EAI_BCI_MAX_CHANNELS][EAI_BCI_FILTER_ORDER];
    int   num_channels;
} eai_bci_iir_filter_t;

typedef struct {
    eai_bci_iir_filter_t bandpass;
    eai_bci_iir_filter_t notch;
    float                dc_offset[EAI_BCI_MAX_CHANNELS];
    float                dc_alpha;
    bool                 initialized;
    int                  num_channels;
} eai_bci_preprocessor_t;

/* Ring buffer operations */
eai_status_t eai_bci_signal_init(eai_bci_signal_t *sig, uint8_t num_channels,
                                  uint32_t sample_rate_hz);
eai_status_t eai_bci_signal_push(eai_bci_signal_t *sig, const eai_bci_sample_t *sample);
int          eai_bci_signal_get_window(const eai_bci_signal_t *sig, eai_bci_sample_t *out,
                                        int window_size);
void         eai_bci_signal_clear(eai_bci_signal_t *sig);
int          eai_bci_signal_count(const eai_bci_signal_t *sig);

/* Preprocessing */
eai_status_t eai_bci_preprocess_init(eai_bci_preprocessor_t *pp, int num_channels,
                                      float sample_rate_hz, float low_hz, float high_hz,
                                      float notch_hz);
eai_status_t eai_bci_preprocess_sample(eai_bci_preprocessor_t *pp, eai_bci_sample_t *sample);

/* Band power computation */
float eai_bci_band_power(const eai_bci_signal_t *sig, int channel, int window_size);

#endif /* EAI_BCI_SIGNAL_H */
