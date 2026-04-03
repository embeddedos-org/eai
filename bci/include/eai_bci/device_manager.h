// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_DEVICE_MANAGER_H
#define EAI_BCI_DEVICE_MANAGER_H

#include "eai_bci/device.h"

#define EAI_BCI_DEVICE_MAX      8
#define EAI_BCI_DEV_NAME_MAX    32

typedef struct {
    char                name[EAI_BCI_DEV_NAME_MAX];
    eai_bci_device_t    device;
    bool                active;
} eai_bci_device_entry_t;

typedef struct {
    eai_bci_device_entry_t devices[EAI_BCI_DEVICE_MAX];
    int                    count;
} eai_bci_device_mgr_t;

eai_status_t eai_bci_dev_mgr_init(eai_bci_device_mgr_t *mgr);
eai_status_t eai_bci_dev_mgr_add(eai_bci_device_mgr_t *mgr, const char *name,
                                  const eai_bci_device_ops_t *ops,
                                  const eai_kv_t *params, int param_count);
eai_status_t eai_bci_dev_mgr_remove(eai_bci_device_mgr_t *mgr, const char *name);
eai_bci_device_t *eai_bci_dev_mgr_find(eai_bci_device_mgr_t *mgr, const char *name);
eai_status_t eai_bci_dev_mgr_start_all(eai_bci_device_mgr_t *mgr);
eai_status_t eai_bci_dev_mgr_stop_all(eai_bci_device_mgr_t *mgr);
void         eai_bci_dev_mgr_shutdown(eai_bci_device_mgr_t *mgr);
int          eai_bci_dev_mgr_count(const eai_bci_device_mgr_t *mgr);

#endif /* EAI_BCI_DEVICE_MANAGER_H */
