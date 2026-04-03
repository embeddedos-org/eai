// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/pipeline.h"
#include "eai/log.h"
#include <string.h>

eai_status_t eai_bci_pipeline_init(eai_bci_pipeline_t *pipe,
                                    eai_bci_device_t *device,
                                    eai_bci_decoder_t *decoder,
                                    eai_bci_output_t *output,
                                    float low_hz, float high_hz, float notch_hz)
{
    if (!pipe || !device || !decoder)
        return EAI_ERR_INVALID;

    memset(pipe, 0, sizeof(*pipe));
    pipe->device  = device;
    pipe->decoder = decoder;
    pipe->output  = output;

    eai_status_t st = eai_bci_signal_init(&pipe->signal_buf,
                                           device->num_channels,
                                           device->sample_rate_hz);
    if (st != EAI_OK) return st;

    st = eai_bci_preprocess_init(&pipe->preprocessor,
                                  device->num_channels,
                                  (float)device->sample_rate_hz,
                                  low_hz, high_hz, notch_hz);
    if (st != EAI_OK) return st;

    pipe->running = false;
    return EAI_OK;
}

eai_status_t eai_bci_pipeline_start(eai_bci_pipeline_t *pipe)
{
    if (!pipe || !pipe->device) return EAI_ERR_INVALID;

    eai_status_t st = eai_bci_device_start(pipe->device);
    if (st != EAI_OK) return st;

    pipe->running            = true;
    pipe->samples_processed  = 0;
    pipe->intents_decoded    = 0;

    EAI_LOG_INFO("BCI pipeline started: %u ch @ %u Hz",
                 pipe->device->num_channels, pipe->device->sample_rate_hz);
    return EAI_OK;
}

eai_status_t eai_bci_pipeline_step(eai_bci_pipeline_t *pipe)
{
    if (!pipe || !pipe->running) return EAI_ERR_INVALID;

    /* 1. Read samples from device */
    eai_bci_sample_t batch[EAI_BCI_PIPELINE_BATCH];
    int samples_read = 0;

    eai_status_t st = eai_bci_device_read(pipe->device, batch,
                                           EAI_BCI_PIPELINE_BATCH, &samples_read);
    if (st != EAI_OK) return st;

    /* 2. Preprocess and push into signal buffer */
    for (int i = 0; i < samples_read; i++) {
        eai_bci_preprocess_sample(&pipe->preprocessor, &batch[i]);
        eai_bci_signal_push(&pipe->signal_buf, &batch[i]);
        pipe->samples_processed++;
    }

    /* 3. Decode intent from signal buffer */
    eai_bci_intent_t intent;
    memset(&intent, 0, sizeof(intent));

    st = eai_bci_decoder_decode(pipe->decoder, &pipe->signal_buf, &intent);
    if (st != EAI_OK) return st;

    pipe->last_intent = intent;
    pipe->intents_decoded++;

    /* 4. Execute output */
    if (pipe->output) {
        eai_bci_output_execute(pipe->output, &intent);
    }

    /* 5. Notify observer */
    if (pipe->on_intent) {
        pipe->on_intent(&intent, pipe->on_intent_ctx);
    }

    return EAI_OK;
}

eai_status_t eai_bci_pipeline_stop(eai_bci_pipeline_t *pipe)
{
    if (!pipe) return EAI_ERR_INVALID;

    pipe->running = false;

    if (pipe->device) {
        eai_bci_device_stop(pipe->device);
    }

    EAI_LOG_INFO("BCI pipeline stopped: %llu samples, %llu intents",
                 (unsigned long long)pipe->samples_processed,
                 (unsigned long long)pipe->intents_decoded);
    return EAI_OK;
}

void eai_bci_pipeline_shutdown(eai_bci_pipeline_t *pipe)
{
    if (!pipe) return;

    if (pipe->running) {
        eai_bci_pipeline_stop(pipe);
    }

    if (pipe->device)  eai_bci_device_shutdown(pipe->device);
    if (pipe->decoder) eai_bci_decoder_shutdown(pipe->decoder);
    if (pipe->output)  eai_bci_output_shutdown(pipe->output);

    eai_bci_signal_clear(&pipe->signal_buf);

    memset(pipe, 0, sizeof(*pipe));
}

void eai_bci_pipeline_set_observer(eai_bci_pipeline_t *pipe,
                                    eai_bci_intent_cb callback, void *user_data)
{
    if (!pipe) return;
    pipe->on_intent     = callback;
    pipe->on_intent_ctx = user_data;
}

const eai_bci_intent_t *eai_bci_pipeline_last_intent(const eai_bci_pipeline_t *pipe)
{
    if (!pipe) return NULL;
    return &pipe->last_intent;
}
