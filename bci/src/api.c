// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/api.h"
#include "eai_bci/eai_bci.h"
#include <stdlib.h>
#include <string.h>

struct eai_bci_handle_s {
    eai_bci_device_t   device;
    eai_bci_decoder_t  decoder;
    eai_bci_output_t   output;
    eai_bci_pipeline_t pipeline;
    bool               has_output;
};

EAI_BCI_EXPORT eai_bci_handle_t *eai_bci_create(const char *device_name,
                                                   const char *decoder_name,
                                                   const char *output_name)
{
    eai_bci_handle_t *h = (eai_bci_handle_t *)calloc(1, sizeof(eai_bci_handle_t));
    if (!h) return NULL;

    const eai_bci_device_ops_t *dev_ops = &eai_bci_device_simulator_ops;
    const eai_bci_decoder_ops_t *dec_ops = &eai_bci_decoder_threshold_ops;
    const eai_bci_output_ops_t *out_ops = &eai_bci_output_log_ops;

    (void)device_name;
    (void)decoder_name;

    if (eai_bci_device_init(&h->device, dev_ops, NULL, 0) != EAI_OK) goto fail;
    if (eai_bci_decoder_init(&h->decoder, dec_ops, NULL, 0) != EAI_OK) goto fail;

    h->has_output = (output_name != NULL);
    if (h->has_output) {
        if (eai_bci_output_init(&h->output, out_ops, NULL, 0) != EAI_OK) goto fail;
    }

    if (eai_bci_pipeline_init(&h->pipeline, &h->device, &h->decoder,
                               h->has_output ? &h->output : NULL,
                               1.0f, 50.0f, 50.0f) != EAI_OK) goto fail;
    return h;

fail:
    free(h);
    return NULL;
}

EAI_BCI_EXPORT int eai_bci_start(eai_bci_handle_t *h)
{
    if (!h) return -1;
    return eai_bci_pipeline_start(&h->pipeline) == EAI_OK ? 0 : -1;
}

EAI_BCI_EXPORT int eai_bci_stop(eai_bci_handle_t *h)
{
    if (!h) return -1;
    return eai_bci_pipeline_stop(&h->pipeline) == EAI_OK ? 0 : -1;
}

EAI_BCI_EXPORT int eai_bci_poll(eai_bci_handle_t *h)
{
    if (!h) return -1;
    return eai_bci_pipeline_step(&h->pipeline) == EAI_OK ? 0 : -1;
}

EAI_BCI_EXPORT int eai_bci_get_intent(eai_bci_handle_t *h, char *label, int label_size,
                                        float *confidence, uint32_t *class_id)
{
    if (!h) return -1;
    const eai_bci_intent_t *intent = eai_bci_pipeline_last_intent(&h->pipeline);
    if (!intent) return -1;

    if (label && label_size > 0)
        strncpy(label, intent->label, label_size - 1);
    if (confidence) *confidence = intent->confidence;
    if (class_id) *class_id = intent->class_id;
    return 0;
}

EAI_BCI_EXPORT int eai_bci_get_signal(eai_bci_handle_t *h, float *buffer,
                                        int max_samples, int max_channels,
                                        int *samples_out)
{
    if (!h || !buffer) return -1;

    eai_bci_sample_t window[256];
    int got = eai_bci_signal_get_window(&h->pipeline.signal_buf, window, max_samples);

    int ch = max_channels < h->device.num_channels ? max_channels : h->device.num_channels;
    for (int s = 0; s < got; s++) {
        for (int c = 0; c < ch; c++) {
            buffer[s * ch + c] = window[s].channels[c];
        }
    }
    if (samples_out) *samples_out = got;
    return 0;
}

EAI_BCI_EXPORT int eai_bci_get_channel_count(eai_bci_handle_t *h)
{
    return h ? h->device.num_channels : 0;
}

EAI_BCI_EXPORT int eai_bci_get_sample_rate(eai_bci_handle_t *h)
{
    return h ? (int)h->device.sample_rate_hz : 0;
}

EAI_BCI_EXPORT uint64_t eai_bci_get_samples_processed(eai_bci_handle_t *h)
{
    return h ? h->pipeline.samples_processed : 0;
}

EAI_BCI_EXPORT void eai_bci_destroy(eai_bci_handle_t *h)
{
    if (!h) return;
    eai_bci_pipeline_shutdown(&h->pipeline);
    free(h);
}

EAI_BCI_EXPORT const char *eai_bci_version(void)
{
    return "0.2.0-bci";
}
