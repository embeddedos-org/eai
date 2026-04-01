// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/policy.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>

#define LOG_MOD "fw-policy"

eai_status_t eai_fw_policy_init(eai_fw_policy_t *policy)
{
    if (!policy) return EAI_ERR_INVALID;
    memset(policy, 0, sizeof(*policy));
    policy->cloud_fallback = false;
    policy->allow_external_tools = false;
    policy->max_inference_ms = 30000;
    policy->max_memory_mb = 512;

    /* Adaptive defaults */
    policy->allow_on_device_learning = true;
    policy->max_training_steps_per_cycle = 100;
    policy->user_data_retention_days = 30;
    policy->allow_adapter_updates = true;
    return EAI_OK;
}

eai_status_t eai_fw_policy_add_rule(eai_fw_policy_t *policy, const eai_policy_rule_t *rule)
{
    if (!policy || !rule) return EAI_ERR_INVALID;
    if (policy->rule_count >= EAI_POLICY_MAX_RULES) return EAI_ERR_NOMEM;

    memcpy(&policy->rules[policy->rule_count], rule, sizeof(eai_policy_rule_t));
    policy->rule_count++;
    return EAI_OK;
}

static bool matches(const char *pattern, const char *value)
{
    if (!pattern || !value) return false;
    if (strcmp(pattern, "*") == 0) return true;
    return strcmp(pattern, value) == 0;
}

eai_policy_action_t eai_fw_policy_check(const eai_fw_policy_t *policy,
                                         const char *subject,
                                         const char *resource,
                                         const char *operation)
{
    if (!policy) return EAI_POLICY_ALLOW;

    for (int i = 0; i < policy->rule_count; i++) {
        const eai_policy_rule_t *r = &policy->rules[i];
        if (matches(r->subject, subject) &&
            matches(r->resource, resource) &&
            matches(r->operation, operation)) {
            if (r->action == EAI_POLICY_AUDIT) {
                EAI_LOG_INFO(LOG_MOD, "AUDIT: %s -> %s [%s]", subject, resource, operation);
            }
            return r->action;
        }
    }

    return EAI_POLICY_ALLOW;
}

void eai_fw_policy_dump(const eai_fw_policy_t *policy)
{
    if (!policy) return;
    static const char *action_names[] = {"ALLOW", "DENY", "AUDIT"};

    printf("Policy (%d rules):\n", policy->rule_count);
    printf("  cloud_fallback:     %s\n", policy->cloud_fallback ? "true" : "false");
    printf("  external_tools:     %s\n", policy->allow_external_tools ? "true" : "false");
    printf("  max_inference_ms:   %u\n", policy->max_inference_ms);
    printf("  max_memory_mb:      %u\n", policy->max_memory_mb);
    printf("  on_device_learning: %s\n", policy->allow_on_device_learning ? "true" : "false");
    printf("  max_train_steps:    %u\n", policy->max_training_steps_per_cycle);
    printf("  data_retention:     %u days\n", policy->user_data_retention_days);
    printf("  adapter_updates:    %s\n", policy->allow_adapter_updates ? "true" : "false");
    for (int i = 0; i < policy->rule_count; i++) {
        const eai_policy_rule_t *r = &policy->rules[i];
        printf("  [%d] %s | %s | %s -> %s\n", i,
               r->subject, r->resource, r->operation, action_names[r->action]);
    }
}
