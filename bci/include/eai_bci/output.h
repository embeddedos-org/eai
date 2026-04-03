// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_OUTPUT_H
#define EAI_BCI_OUTPUT_H

#include "eai_bci/types.h"

typedef enum {
    EAI_BCI_OUT_KEYBOARD,
    EAI_BCI_OUT_CURSOR,
    EAI_BCI_OUT_PROSTHETIC,
    EAI_BCI_OUT_GPIO,
    EAI_BCI_OUT_LOG,
    EAI_BCI_OUT_CUSTOM,
} eai_bci_output_type_t;

typedef struct eai_bci_output_s eai_bci_output_t;

typedef struct {
    const char            *name;
    eai_bci_output_type_t  type;
    eai_status_t (*init)(eai_bci_output_t *out, const eai_kv_t *params, int param_count);
    eai_status_t (*execute)(eai_bci_output_t *out, const eai_bci_intent_t *intent);
    void         (*shutdown)(eai_bci_output_t *out);
} eai_bci_output_ops_t;

struct eai_bci_output_s {
    const eai_bci_output_ops_t *ops;
    void                       *ctx;
    bool                        initialized;
};

eai_status_t eai_bci_output_init(eai_bci_output_t *out, const eai_bci_output_ops_t *ops,
                                  const eai_kv_t *params, int param_count);
eai_status_t eai_bci_output_execute(eai_bci_output_t *out, const eai_bci_intent_t *intent);
void         eai_bci_output_shutdown(eai_bci_output_t *out);

extern const eai_bci_output_ops_t eai_bci_output_log_ops;
extern const eai_bci_output_ops_t eai_bci_output_gpio_ops;

#endif /* EAI_BCI_OUTPUT_H */
