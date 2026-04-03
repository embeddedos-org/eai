// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_API_H
#define EAI_BCI_API_H

#include "eai_bci/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#define EAI_BCI_EXPORT __declspec(dllexport)
#else
#define EAI_BCI_EXPORT __attribute__((visibility("default")))
#endif

typedef struct eai_bci_handle_s eai_bci_handle_t;

EAI_BCI_EXPORT eai_bci_handle_t *eai_bci_create(const char *device_name,
                                                   const char *decoder_name,
                                                   const char *output_name);

EAI_BCI_EXPORT int eai_bci_start(eai_bci_handle_t *h);
EAI_BCI_EXPORT int eai_bci_stop(eai_bci_handle_t *h);
EAI_BCI_EXPORT int eai_bci_poll(eai_bci_handle_t *h);

EAI_BCI_EXPORT int eai_bci_get_intent(eai_bci_handle_t *h, char *label, int label_size,
                                        float *confidence, uint32_t *class_id);

EAI_BCI_EXPORT int eai_bci_get_signal(eai_bci_handle_t *h, float *buffer,
                                        int max_samples, int max_channels,
                                        int *samples_out);

EAI_BCI_EXPORT int eai_bci_get_channel_count(eai_bci_handle_t *h);
EAI_BCI_EXPORT int eai_bci_get_sample_rate(eai_bci_handle_t *h);
EAI_BCI_EXPORT uint64_t eai_bci_get_samples_processed(eai_bci_handle_t *h);

EAI_BCI_EXPORT void eai_bci_destroy(eai_bci_handle_t *h);

EAI_BCI_EXPORT const char *eai_bci_version(void);

#ifdef __cplusplus
}
#endif

#endif /* EAI_BCI_API_H */
