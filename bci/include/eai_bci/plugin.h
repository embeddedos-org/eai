// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_PLUGIN_H
#define EAI_BCI_PLUGIN_H

#include "eai_bci/device.h"
#include "eai_bci/decoder.h"
#include "eai_bci/output.h"

#define EAI_BCI_PLUGIN_MAX       16
#define EAI_BCI_PLUGIN_NAME_MAX  64

typedef struct {
    char                          name[EAI_BCI_PLUGIN_NAME_MAX];
    const char                   *version;
    const char                   *description;
    const eai_bci_device_ops_t   *device_ops;
    const eai_bci_decoder_ops_t  *decoder_ops;
    const eai_bci_output_ops_t   *output_ops;
} eai_bci_plugin_t;

typedef struct {
    eai_bci_plugin_t plugins[EAI_BCI_PLUGIN_MAX];
    int              count;
} eai_bci_plugin_registry_t;

eai_status_t eai_bci_plugin_registry_init(eai_bci_plugin_registry_t *reg);
eai_status_t eai_bci_plugin_register(eai_bci_plugin_registry_t *reg,
                                      const eai_bci_plugin_t *plugin);
eai_bci_plugin_t *eai_bci_plugin_find(eai_bci_plugin_registry_t *reg, const char *name);
void eai_bci_plugin_list(const eai_bci_plugin_registry_t *reg);
int  eai_bci_plugin_count(const eai_bci_plugin_registry_t *reg);

#endif /* EAI_BCI_PLUGIN_H */
