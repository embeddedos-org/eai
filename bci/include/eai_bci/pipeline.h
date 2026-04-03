// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_PIPELINE_H
#define EAI_BCI_PIPELINE_H

#include "eai_bci/device.h"
#include "eai_bci/signal.h"
#include "eai_bci/decoder.h"
#include "eai_bci/output.h"

#define EAI_BCI_PIPELINE_BATCH  16

typedef void (*eai_bci_intent_cb)(const eai_bci_intent_t *intent, void *user_data);

typedef struct {
    eai_bci_device_t        *device;
    eai_bci_preprocessor_t   preprocessor;
    eai_bci_signal_t         signal_buf;
    eai_bci_decoder_t       *decoder;
    eai_bci_output_t        *output;
    eai_bci_intent_cb        on_intent;
    void                    *on_intent_ctx;
    eai_bci_intent_t         last_intent;
    uint64_t                 samples_processed;
    uint64_t                 intents_decoded;
    bool                     running;
} eai_bci_pipeline_t;

eai_status_t eai_bci_pipeline_init(eai_bci_pipeline_t *pipe,
                                    eai_bci_device_t *device,
                                    eai_bci_decoder_t *decoder,
                                    eai_bci_output_t *output,
                                    float low_hz, float high_hz, float notch_hz);

eai_status_t eai_bci_pipeline_start(eai_bci_pipeline_t *pipe);
eai_status_t eai_bci_pipeline_step(eai_bci_pipeline_t *pipe);
eai_status_t eai_bci_pipeline_stop(eai_bci_pipeline_t *pipe);
void         eai_bci_pipeline_shutdown(eai_bci_pipeline_t *pipe);

void eai_bci_pipeline_set_observer(eai_bci_pipeline_t *pipe,
                                    eai_bci_intent_cb callback, void *user_data);

const eai_bci_intent_t *eai_bci_pipeline_last_intent(const eai_bci_pipeline_t *pipe);

#endif /* EAI_BCI_PIPELINE_H */
