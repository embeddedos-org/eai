// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/plugin.h"
#include "eai/log.h"
#include <string.h>

eai_status_t eai_bci_plugin_registry_init(eai_bci_plugin_registry_t *reg)
{
    if (!reg) return EAI_ERR_INVALID;
    memset(reg, 0, sizeof(*reg));
    return EAI_OK;
}

eai_status_t eai_bci_plugin_register(eai_bci_plugin_registry_t *reg,
                                      const eai_bci_plugin_t *plugin)
{
    if (!reg || !plugin || plugin->name[0] == '\0') return EAI_ERR_INVALID;
    if (reg->count >= EAI_BCI_PLUGIN_MAX) return EAI_ERR_NOMEM;

    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->plugins[i].name, plugin->name) == 0)
            return EAI_ERR_INVALID;
    }

    reg->plugins[reg->count] = *plugin;
    reg->count++;

    EAI_LOG_INFO("BCI plugin registered: '%s' v%s",
                 plugin->name, plugin->version ? plugin->version : "?");
    return EAI_OK;
}

eai_bci_plugin_t *eai_bci_plugin_find(eai_bci_plugin_registry_t *reg, const char *name)
{
    if (!reg || !name) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->plugins[i].name, name) == 0)
            return &reg->plugins[i];
    }
    return NULL;
}

void eai_bci_plugin_list(const eai_bci_plugin_registry_t *reg)
{
    if (!reg) return;
    EAI_LOG_INFO("BCI plugins (%d registered):", reg->count);
    for (int i = 0; i < reg->count; i++) {
        const eai_bci_plugin_t *p = &reg->plugins[i];
        EAI_LOG_INFO("  [%d] %s v%s - device:%s decoder:%s output:%s",
                     i, p->name,
                     p->version ? p->version : "?",
                     p->device_ops ? p->device_ops->name : "none",
                     p->decoder_ops ? p->decoder_ops->name : "none",
                     p->output_ops ? p->output_ops->name : "none");
    }
}

int eai_bci_plugin_count(const eai_bci_plugin_registry_t *reg)
{
    return reg ? reg->count : 0;
}
