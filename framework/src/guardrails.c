// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/guardrails.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "guardrails"

static uint64_t guard_now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

static bool str_contains_ci(const char *haystack, const char *needle) {
    if (!haystack || !needle) return false;
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return false;

    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            char h = haystack[i + j];
            char n = needle[j];
            if (h >= 'A' && h <= 'Z') h += 32;
            if (n >= 'A' && n <= 'Z') n += 32;
            if (h != n) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

static const char *cat_str(eai_guard_category_t c) {
    switch (c) {
        case EAI_GUARD_CAT_HARMFUL:           return "HARMFUL";
        case EAI_GUARD_CAT_UNSAFE_ACTION:     return "UNSAFE_ACTION";
        case EAI_GUARD_CAT_OUT_OF_SCOPE:      return "OUT_OF_SCOPE";
        case EAI_GUARD_CAT_PII_LEAK:          return "PII_LEAK";
        case EAI_GUARD_CAT_INJECTION:         return "INJECTION";
        case EAI_GUARD_CAT_EXCESSIVE_RESOURCE: return "EXCESSIVE_RESOURCE";
        case EAI_GUARD_CAT_CUSTOM:            return "CUSTOM";
        default:                              return "UNKNOWN";
    }
}

static const char *action_str(eai_guard_action_t a) {
    switch (a) {
        case EAI_GUARD_BLOCK: return "BLOCK";
        case EAI_GUARD_WARN:  return "WARN";
        case EAI_GUARD_LOG:   return "LOG";
        case EAI_GUARD_ALLOW: return "ALLOW";
        default:              return "?";
    }
}

eai_status_t eai_fw_guard_init(eai_fw_guardrails_t *gr) {
    if (!gr) return EAI_ERR_INVALID;
    memset(gr, 0, sizeof(*gr));

    gr->autonomy = EAI_AUTONOMY_SUPERVISED;
    gr->max_input_len = 8192;
    gr->max_output_len = 16384;
    gr->kill_switch = false;
    gr->initialized = true;

    gr->rate_limiter.max_per_window = 100;
    gr->rate_limiter.window_ms = 60000;
    gr->rate_limiter.window_start_ts = guard_now_ms();

    /* Default safety rules */
    eai_fw_guard_add_rule(gr, "ignore previous instructions", EAI_GUARD_CAT_INJECTION,
                          EAI_GUARD_BLOCK, true, false);
    eai_fw_guard_add_rule(gr, "system prompt", EAI_GUARD_CAT_INJECTION,
                          EAI_GUARD_BLOCK, true, false);
    eai_fw_guard_add_rule(gr, "forget everything", EAI_GUARD_CAT_INJECTION,
                          EAI_GUARD_BLOCK, true, false);
    eai_fw_guard_add_rule(gr, "override safety", EAI_GUARD_CAT_UNSAFE_ACTION,
                          EAI_GUARD_BLOCK, true, true);
    eai_fw_guard_add_rule(gr, "personal information", EAI_GUARD_CAT_PII_LEAK,
                          EAI_GUARD_WARN, false, true);

    EAI_LOG_INFO(MOD, "guardrails initialized (%d default rules, autonomy=SUPERVISED)",
                 gr->rule_count);
    return EAI_OK;
}

eai_status_t eai_fw_guard_add_rule(eai_fw_guardrails_t *gr, const char *pattern,
                                    eai_guard_category_t cat, eai_guard_action_t action,
                                    bool input, bool output) {
    if (!gr || !pattern) return EAI_ERR_INVALID;
    if (gr->rule_count >= EAI_GUARD_MAX_RULES) return EAI_ERR_NOMEM;

    eai_guard_rule_t *r = &gr->rules[gr->rule_count];
    memset(r, 0, sizeof(*r));
    strncpy(r->pattern, pattern, EAI_GUARD_PATTERN_MAX - 1);
    r->category = cat;
    r->action = action;
    r->applies_to_input = input;
    r->applies_to_output = output;
    r->active = true;
    gr->rule_count++;

    return EAI_OK;
}

static eai_status_t check_text(eai_fw_guardrails_t *gr, const char *text,
                                bool is_input, char *reason, size_t reason_size) {
    if (!gr || !text) return EAI_ERR_INVALID;

    if (gr->kill_switch) {
        if (reason) snprintf(reason, reason_size, "kill switch active");
        return EAI_ERR_PERMISSION;
    }

    size_t len = strlen(text);
    uint32_t max_len = is_input ? gr->max_input_len : gr->max_output_len;
    if (len > max_len) {
        if (reason) snprintf(reason, reason_size, "text exceeds max length (%u)", max_len);
        gr->blocked_count++;
        return EAI_ERR_RESOURCE_BUDGET;
    }

    gr->total_checked++;

    for (int i = 0; i < gr->rule_count; i++) {
        eai_guard_rule_t *r = &gr->rules[i];
        if (!r->active) continue;
        if (is_input && !r->applies_to_input) continue;
        if (!is_input && !r->applies_to_output) continue;

        if (str_contains_ci(text, r->pattern)) {
            switch (r->action) {
                case EAI_GUARD_BLOCK:
                    gr->blocked_count++;
                    if (reason) snprintf(reason, reason_size, "blocked: %s (%s)",
                                        r->pattern, cat_str(r->category));
                    EAI_LOG_WARN(MOD, "BLOCKED [%s]: pattern '%s' matched",
                                 cat_str(r->category), r->pattern);
                    return EAI_ERR_PERMISSION;

                case EAI_GUARD_WARN:
                    gr->warned_count++;
                    if (reason) snprintf(reason, reason_size, "warning: %s (%s)",
                                        r->pattern, cat_str(r->category));
                    EAI_LOG_WARN(MOD, "WARNING [%s]: pattern '%s' matched",
                                 cat_str(r->category), r->pattern);
                    break;

                case EAI_GUARD_LOG:
                    EAI_LOG_INFO(MOD, "LOG [%s]: pattern '%s' matched",
                                 cat_str(r->category), r->pattern);
                    break;

                case EAI_GUARD_ALLOW:
                    break;
            }
        }
    }

    return EAI_OK;
}

eai_status_t eai_fw_guard_check_input(eai_fw_guardrails_t *gr, const char *input,
                                       char *reason, size_t reason_size) {
    return check_text(gr, input, true, reason, reason_size);
}

eai_status_t eai_fw_guard_check_output(eai_fw_guardrails_t *gr, const char *output,
                                        char *reason, size_t reason_size) {
    return check_text(gr, output, false, reason, reason_size);
}

eai_status_t eai_fw_guard_set_autonomy(eai_fw_guardrails_t *gr, eai_autonomy_level_t level) {
    if (!gr) return EAI_ERR_INVALID;
    gr->autonomy = level;

    const char *level_str;
    switch (level) {
        case EAI_AUTONOMY_FULL_AUTO:   level_str = "FULL_AUTO";   break;
        case EAI_AUTONOMY_SUPERVISED:  level_str = "SUPERVISED";  break;
        case EAI_AUTONOMY_MANUAL_ONLY: level_str = "MANUAL_ONLY"; break;
        default:                       level_str = "?";            break;
    }
    EAI_LOG_INFO(MOD, "autonomy level set to %s", level_str);
    return EAI_OK;
}

eai_status_t eai_fw_guard_set_rate_limit(eai_fw_guardrails_t *gr,
                                          uint32_t max_per_window, uint32_t window_ms) {
    if (!gr) return EAI_ERR_INVALID;
    gr->rate_limiter.max_per_window = max_per_window;
    gr->rate_limiter.window_ms = window_ms;
    gr->rate_limiter.current_count = 0;
    gr->rate_limiter.window_start_ts = guard_now_ms();
    return EAI_OK;
}

bool eai_fw_guard_check_rate(eai_fw_guardrails_t *gr) {
    if (!gr) return false;

    uint64_t now = guard_now_ms();
    eai_rate_limiter_t *rl = &gr->rate_limiter;

    if ((now - rl->window_start_ts) > rl->window_ms) {
        rl->window_start_ts = now;
        rl->current_count = 0;
    }

    if (rl->current_count >= rl->max_per_window) {
        EAI_LOG_WARN(MOD, "rate limit exceeded: %u / %u in %u ms",
                     rl->current_count, rl->max_per_window, rl->window_ms);
        return false;
    }

    rl->current_count++;
    return true;
}

eai_status_t eai_fw_guard_kill_switch(eai_fw_guardrails_t *gr, bool activate) {
    if (!gr) return EAI_ERR_INVALID;
    gr->kill_switch = activate;

    if (activate) {
        EAI_LOG_FATAL(MOD, "KILL SWITCH ACTIVATED — all autonomous operations halted");
    } else {
        EAI_LOG_INFO(MOD, "kill switch deactivated — operations resumed");
    }
    return EAI_OK;
}

bool eai_fw_guard_is_operational(const eai_fw_guardrails_t *gr) {
    if (!gr || !gr->initialized) return false;
    return !gr->kill_switch;
}

void eai_fw_guard_report(const eai_fw_guardrails_t *gr) {
    if (!gr) return;

    const char *autonomy_str;
    switch (gr->autonomy) {
        case EAI_AUTONOMY_FULL_AUTO:   autonomy_str = "FULL_AUTO";   break;
        case EAI_AUTONOMY_SUPERVISED:  autonomy_str = "SUPERVISED";  break;
        case EAI_AUTONOMY_MANUAL_ONLY: autonomy_str = "MANUAL_ONLY"; break;
        default:                       autonomy_str = "?";            break;
    }

    printf("\n=== Guardrails Report ===\n");
    printf("Operational: %s | Kill switch: %s\n",
           eai_fw_guard_is_operational(gr) ? "YES" : "NO",
           gr->kill_switch ? "ACTIVE" : "off");
    printf("Autonomy:    %s\n", autonomy_str);
    printf("Checked:     %u | Blocked: %u | Warned: %u\n",
           gr->total_checked, gr->blocked_count, gr->warned_count);
    printf("Rate limit:  %u / %u per %u ms\n",
           gr->rate_limiter.current_count,
           gr->rate_limiter.max_per_window,
           gr->rate_limiter.window_ms);

    printf("\nRules (%d):\n", gr->rule_count);
    for (int i = 0; i < gr->rule_count; i++) {
        const eai_guard_rule_t *r = &gr->rules[i];
        printf("  [%d] %-8s %-18s %-20s in=%s out=%s\n",
               i, action_str(r->action), cat_str(r->category),
               r->pattern,
               r->applies_to_input ? "yes" : "no",
               r->applies_to_output ? "yes" : "no");
    }
}
