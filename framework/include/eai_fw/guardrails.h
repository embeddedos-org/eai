// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_GUARDRAILS_H
#define EAI_FW_GUARDRAILS_H

#include "eai/types.h"

#define EAI_GUARD_MAX_RULES    32
#define EAI_GUARD_PATTERN_MAX  256
#define EAI_GUARD_REASON_MAX   256

typedef enum {
    EAI_GUARD_BLOCK,
    EAI_GUARD_WARN,
    EAI_GUARD_LOG,
    EAI_GUARD_ALLOW,
} eai_guard_action_t;

typedef enum {
    EAI_GUARD_CAT_HARMFUL,
    EAI_GUARD_CAT_UNSAFE_ACTION,
    EAI_GUARD_CAT_OUT_OF_SCOPE,
    EAI_GUARD_CAT_PII_LEAK,
    EAI_GUARD_CAT_INJECTION,
    EAI_GUARD_CAT_EXCESSIVE_RESOURCE,
    EAI_GUARD_CAT_CUSTOM,
} eai_guard_category_t;

typedef enum {
    EAI_AUTONOMY_FULL_AUTO,
    EAI_AUTONOMY_SUPERVISED,
    EAI_AUTONOMY_MANUAL_ONLY,
} eai_autonomy_level_t;

typedef struct {
    char                pattern[EAI_GUARD_PATTERN_MAX];
    eai_guard_category_t category;
    eai_guard_action_t   action;
    bool                 active;
    bool                 applies_to_input;
    bool                 applies_to_output;
} eai_guard_rule_t;

typedef struct {
    uint32_t max_per_window;
    uint32_t window_ms;
    uint32_t current_count;
    uint64_t window_start_ts;
} eai_rate_limiter_t;

typedef struct {
    eai_guard_rule_t    rules[EAI_GUARD_MAX_RULES];
    int                 rule_count;
    eai_autonomy_level_t autonomy;
    eai_rate_limiter_t  rate_limiter;
    uint32_t            max_input_len;
    uint32_t            max_output_len;
    bool                kill_switch;
    uint32_t            blocked_count;
    uint32_t            warned_count;
    uint32_t            total_checked;
    bool                initialized;
} eai_fw_guardrails_t;

eai_status_t eai_fw_guard_init(eai_fw_guardrails_t *gr);
eai_status_t eai_fw_guard_add_rule(eai_fw_guardrails_t *gr, const char *pattern,
                                    eai_guard_category_t cat, eai_guard_action_t action,
                                    bool input, bool output);
eai_status_t eai_fw_guard_check_input(eai_fw_guardrails_t *gr, const char *input,
                                       char *reason, size_t reason_size);
eai_status_t eai_fw_guard_check_output(eai_fw_guardrails_t *gr, const char *output,
                                        char *reason, size_t reason_size);
eai_status_t eai_fw_guard_set_autonomy(eai_fw_guardrails_t *gr, eai_autonomy_level_t level);
eai_status_t eai_fw_guard_set_rate_limit(eai_fw_guardrails_t *gr,
                                          uint32_t max_per_window, uint32_t window_ms);
bool         eai_fw_guard_check_rate(eai_fw_guardrails_t *gr);
eai_status_t eai_fw_guard_kill_switch(eai_fw_guardrails_t *gr, bool activate);
bool         eai_fw_guard_is_operational(const eai_fw_guardrails_t *gr);
void         eai_fw_guard_report(const eai_fw_guardrails_t *gr);

#endif /* EAI_FW_GUARDRAILS_H */
