// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_CONFIG_H
#define EAI_BCI_CONFIG_H

#include "eai_bci/types.h"

typedef struct {
    char     device_name[64];
    char     decoder_name[64];
    char     output_name[64];
    uint8_t  num_channels;
    uint32_t sample_rate_hz;
    float    filter_low_hz;
    float    filter_high_hz;
    float    notch_hz;
    int      decoder_window_size;
    bool     simulator_mode;
    int      paradigm;
} eai_bci_config_t;

eai_status_t eai_bci_config_defaults(eai_bci_config_t *cfg);
eai_status_t eai_bci_config_load(eai_bci_config_t *cfg, const char *path);
eai_status_t eai_bci_config_validate(const eai_bci_config_t *cfg);
void         eai_bci_config_dump(const eai_bci_config_t *cfg);

#endif /* EAI_BCI_CONFIG_H */
