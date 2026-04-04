// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/device_manager.h"
#include "eai/log.h"
#include <string.h>

eai_status_t eai_bci_dev_mgr_init(eai_bci_device_mgr_t *mgr)
{
    if (!mgr) return EAI_ERR_INVALID;
    memset(mgr, 0, sizeof(*mgr));
    return EAI_OK;
}

eai_status_t eai_bci_dev_mgr_add(eai_bci_device_mgr_t *mgr, const char *name,
                                  const eai_bci_device_ops_t *ops,
                                  const eai_kv_t *params, int param_count)
{
    if (!mgr || !name || !ops) return EAI_ERR_INVALID;
    if (mgr->count >= EAI_BCI_DEVICE_MAX) return EAI_ERR_NOMEM;

    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->devices[i].name, name) == 0)
            return EAI_ERR_INVALID;
    }

    eai_bci_device_entry_t *entry = &mgr->devices[mgr->count];
    strncpy(entry->name, name, EAI_BCI_DEV_NAME_MAX - 1);
    entry->active = false;

    eai_status_t st = eai_bci_device_init(&entry->device, ops, params, param_count);
    if (st != EAI_OK) return st;

    mgr->count++;
    EAI_LOG_INFO("bci", "BCI device manager: added '%s' (%s)", name, ops->name);
    return EAI_OK;
}

eai_status_t eai_bci_dev_mgr_remove(eai_bci_device_mgr_t *mgr, const char *name)
{
    if (!mgr || !name) return EAI_ERR_INVALID;

    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->devices[i].name, name) == 0) {
            if (mgr->devices[i].active)
                eai_bci_device_stop(&mgr->devices[i].device);
            eai_bci_device_shutdown(&mgr->devices[i].device);

            if (i < mgr->count - 1) {
                memmove(&mgr->devices[i], &mgr->devices[i + 1],
                        (size_t)(mgr->count - i - 1) * sizeof(eai_bci_device_entry_t));
            }
            mgr->count--;
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_bci_device_t *eai_bci_dev_mgr_find(eai_bci_device_mgr_t *mgr, const char *name)
{
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->count; i++) {
        if (strcmp(mgr->devices[i].name, name) == 0)
            return &mgr->devices[i].device;
    }
    return NULL;
}

eai_status_t eai_bci_dev_mgr_start_all(eai_bci_device_mgr_t *mgr)
{
    if (!mgr) return EAI_ERR_INVALID;
    for (int i = 0; i < mgr->count; i++) {
        eai_status_t st = eai_bci_device_start(&mgr->devices[i].device);
        if (st == EAI_OK)
            mgr->devices[i].active = true;
        else
            EAI_LOG_WARN("bci", "BCI device '%s' failed to start: %s",
                         mgr->devices[i].name, eai_status_str(st));
    }
    return EAI_OK;
}

eai_status_t eai_bci_dev_mgr_stop_all(eai_bci_device_mgr_t *mgr)
{
    if (!mgr) return EAI_ERR_INVALID;
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->devices[i].active) {
            eai_bci_device_stop(&mgr->devices[i].device);
            mgr->devices[i].active = false;
        }
    }
    return EAI_OK;
}

void eai_bci_dev_mgr_shutdown(eai_bci_device_mgr_t *mgr)
{
    if (!mgr) return;
    eai_bci_dev_mgr_stop_all(mgr);
    for (int i = 0; i < mgr->count; i++) {
        eai_bci_device_shutdown(&mgr->devices[i].device);
    }
    mgr->count = 0;
}

int eai_bci_dev_mgr_count(const eai_bci_device_mgr_t *mgr)
{
    return mgr ? mgr->count : 0;
}
