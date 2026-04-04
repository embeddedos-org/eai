// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_DECODER_H
#define EAI_BCI_DECODER_H

#include "eai_bci/types.h"

typedef enum {
    EAI_BCI_DEC_THRESHOLD,
    EAI_BCI_DEC_FFT_BAND,
    EAI_BCI_DEC_SNN,
    EAI_BCI_DEC_TINYML,
    EAI_BCI_DEC_CUSTOM,
} eai_bci_decoder_type_t;

typedef struct eai_bci_decoder_s eai_bci_decoder_t;

typedef struct {
    const char              *name;
    eai_bci_decoder_type_t   type;
    eai_status_t (*init)(eai_bci_decoder_t *dec, const eai_kv_t *params, int param_count);
    eai_status_t (*decode)(eai_bci_decoder_t *dec, const eai_bci_signal_t *signal,
                            eai_bci_intent_t *intent);
    eai_status_t (*reset)(eai_bci_decoder_t *dec);
    void         (*shutdown)(eai_bci_decoder_t *dec);
} eai_bci_decoder_ops_t;

struct eai_bci_decoder_s {
    const eai_bci_decoder_ops_t *ops;
    void                        *ctx;
    bool                         initialized;
};

eai_status_t eai_bci_decoder_init(eai_bci_decoder_t *dec, const eai_bci_decoder_ops_t *ops,
                                   const eai_kv_t *params, int param_count);
eai_status_t eai_bci_decoder_decode(eai_bci_decoder_t *dec, const eai_bci_signal_t *signal,
                                     eai_bci_intent_t *intent);
eai_status_t eai_bci_decoder_reset(eai_bci_decoder_t *dec);
void         eai_bci_decoder_shutdown(eai_bci_decoder_t *dec);

extern const eai_bci_decoder_ops_t eai_bci_decoder_threshold_ops;
extern const eai_bci_decoder_ops_t eai_bci_decoder_snn_ops;
extern const eai_bci_decoder_ops_t eai_bci_decoder_snn_ops;

#ifdef EAI_BCI_TINYML_ENABLED
extern const eai_bci_decoder_ops_t eai_bci_decoder_tinyml_ops;
#endif

#endif /* EAI_BCI_DECODER_H */
