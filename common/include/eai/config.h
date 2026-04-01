// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_CONFIG_H
#define EAI_CONFIG_H

#include "eai/types.h"
#include "eai/adaptive.h"

#define EAI_CONFIG_MAX_TOOLS      32
#define EAI_CONFIG_MAX_CONNECTORS 16
#define EAI_CONFIG_MAX_PROVIDERS  8

typedef struct {
    const char *provider;
} eai_runtime_config_single_t;

typedef struct {
    const char *providers[EAI_CONFIG_MAX_PROVIDERS];
    int         provider_count;
} eai_runtime_config_multi_t;

typedef struct {
    bool cloud_fallback;
} eai_policy_config_t;

typedef struct {
    eai_variant_t variant;
    eai_mode_t    mode;

    eai_runtime_config_single_t runtime_single;
    eai_runtime_config_multi_t runtime_multi;

    const char *tools[EAI_CONFIG_MAX_TOOLS];
    int         tool_count;

    const char *connectors[EAI_CONFIG_MAX_CONNECTORS];
    int         connector_count;

    eai_policy_config_t policy;
    bool observability;

    /* Adaptive / Personalized AI */
    eai_adaptive_config_t adaptive;
} eai_config_t;

eai_status_t eai_config_init(eai_config_t *cfg);
eai_status_t eai_config_load_file(eai_config_t *cfg, const char *path);
eai_status_t eai_config_load_profile(eai_config_t *cfg, const char *profile_name);
void         eai_config_dump(const eai_config_t *cfg);
void         eai_config_free(eai_config_t *cfg);

#endif /* EAI_CONFIG_H */
