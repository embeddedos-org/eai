// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_DEVICE_H
#define EAI_BCI_DEVICE_H

#include "eai_bci/types.h"

typedef enum {
    EAI_BCI_DEV_SIMULATOR,
    EAI_BCI_DEV_OPENBCI,
    EAI_BCI_DEV_EMOTIV,
    EAI_BCI_DEV_MUSE,
    EAI_BCI_DEV_GENERIC_EEG,
    EAI_BCI_DEV_NEURALINK,
    EAI_BCI_DEV_CUSTOM,
} eai_bci_device_type_t;

typedef struct eai_bci_device_s eai_bci_device_t;

typedef struct {
    const char             *name;
    eai_bci_device_type_t   type;
    eai_status_t (*init)(eai_bci_device_t *dev, const eai_kv_t *params, int param_count);
    eai_status_t (*start_stream)(eai_bci_device_t *dev);
    eai_status_t (*stop_stream)(eai_bci_device_t *dev);
    eai_status_t (*read_samples)(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                  int max_samples, int *samples_read);
    eai_status_t (*calibrate)(eai_bci_device_t *dev);
    eai_status_t (*get_impedance)(eai_bci_device_t *dev, float *impedance_ohms,
                                   int max_channels);
    void         (*shutdown)(eai_bci_device_t *dev);
} eai_bci_device_ops_t;

struct eai_bci_device_s {
    const eai_bci_device_ops_t *ops;
    void                       *ctx;
    eai_bci_device_state_t      state;
    uint8_t                     num_channels;
    uint32_t                    sample_rate_hz;
};

eai_status_t eai_bci_device_init(eai_bci_device_t *dev, const eai_bci_device_ops_t *ops,
                                  const eai_kv_t *params, int param_count);
eai_status_t eai_bci_device_start(eai_bci_device_t *dev);
eai_status_t eai_bci_device_stop(eai_bci_device_t *dev);
eai_status_t eai_bci_device_read(eai_bci_device_t *dev, eai_bci_sample_t *out,
                                  int max_samples, int *samples_read);
void         eai_bci_device_shutdown(eai_bci_device_t *dev);

extern const eai_bci_device_ops_t eai_bci_device_simulator_ops;

#ifdef EAI_BCI_OPENBCI_ENABLED
extern const eai_bci_device_ops_t eai_bci_device_openbci_ops;
#endif

#ifdef EAI_BCI_EMOTIV_ENABLED
extern const eai_bci_device_ops_t eai_bci_device_emotiv_ops;
#endif

#ifdef EAI_BCI_MUSE_ENABLED
extern const eai_bci_device_ops_t eai_bci_device_muse_ops;
#endif

#endif /* EAI_BCI_DEVICE_H */
