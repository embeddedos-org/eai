// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_TYPES_H
#define EAI_BCI_TYPES_H

#include "eai/types.h"

#ifndef EAI_BCI_MAX_CHANNELS
#define EAI_BCI_MAX_CHANNELS    8
#endif

#ifndef EAI_BCI_RING_SIZE
#define EAI_BCI_RING_SIZE       256
#endif

#define EAI_BCI_LABEL_MAX       64
#define EAI_BCI_MAX_CLASSES     8
#define EAI_BCI_FILTER_ORDER    2

typedef enum {
    EAI_BCI_STATE_DISCONNECTED,
    EAI_BCI_STATE_CONNECTING,
    EAI_BCI_STATE_STREAMING,
    EAI_BCI_STATE_ERROR,
    EAI_BCI_STATE_CALIBRATING,
} eai_bci_device_state_t;

typedef struct {
    float    channels[EAI_BCI_MAX_CHANNELS];
    uint64_t timestamp_us;
    uint8_t  quality;
} eai_bci_sample_t;

typedef struct {
    eai_bci_sample_t samples[EAI_BCI_RING_SIZE];
    int              head;
    int              count;
    uint8_t          num_channels;
    uint32_t         sample_rate_hz;
} eai_bci_signal_t;

typedef struct {
    char     label[EAI_BCI_LABEL_MAX];
    float    confidence;
    uint32_t class_id;
    uint64_t timestamp_us;
} eai_bci_intent_t;

#endif /* EAI_BCI_TYPES_H */
