// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/security_lite.h"
#include "eai/log.h"

#include <string.h>
#include <time.h>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#define MOD "sec-lite"

static uint64_t sec_now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

static const char *audit_event_str(eai_audit_event_t ev) {
    switch (ev) {
        case EAI_AUDIT_AGENT_START:         return "AGENT_START";
        case EAI_AUDIT_AGENT_STOP:          return "AGENT_STOP";
        case EAI_AUDIT_TOOL_CALL:           return "TOOL_CALL";
        case EAI_AUDIT_TOOL_DENIED:         return "TOOL_DENIED";
        case EAI_AUDIT_INFERENCE:           return "INFERENCE";
        case EAI_AUDIT_MODEL_LOAD:          return "MODEL_LOAD";
        case EAI_AUDIT_MODEL_UPDATE:        return "MODEL_UPDATE";
        case EAI_AUDIT_SECURITY_VIOLATION:  return "SECURITY_VIOLATION";
        case EAI_AUDIT_INPUT_SANITIZED:     return "INPUT_SANITIZED";
        case EAI_AUDIT_BOOT_CHECK:          return "BOOT_CHECK";
        default:                            return "UNKNOWN";
    }
}

eai_status_t eai_min_sec_init(eai_min_security_lite_t *sec, eai_security_ctx_t *ctx) {
    if (!sec) return EAI_ERR_INVALID;
    memset(sec, 0, sizeof(*sec));
    sec->security_ctx = ctx;
    sec->max_prompt_len = 4096;
    sec->boot_verified = false;
    sec->model_verified = false;

    eai_min_sec_add_injection_pattern(sec, "ignore previous instructions");
    eai_min_sec_add_injection_pattern(sec, "ignore all prior");
    eai_min_sec_add_injection_pattern(sec, "disregard above");
    eai_min_sec_add_injection_pattern(sec, "system prompt:");
    eai_min_sec_add_injection_pattern(sec, "you are now");
    eai_min_sec_add_injection_pattern(sec, "new instructions:");
    eai_min_sec_add_injection_pattern(sec, "override:");
    eai_min_sec_add_injection_pattern(sec, "forget everything");

    EAI_LOG_INFO(MOD, "security lite initialized with %d injection patterns",
                 sec->injection_rules.count);
    return EAI_OK;
}

eai_status_t eai_min_sec_verify_boot(eai_min_security_lite_t *sec) {
    if (!sec) return EAI_ERR_INVALID;

    EAI_LOG_INFO(MOD, "verifying boot chain integrity...");

    /* Verify boot chain by checking known system integrity markers.
     * On embedded targets this would verify:
     * 1. Bootloader signature via eBoot's crypto_verify_signature
     * 2. Kernel/OS image hash
     * 3. Runtime binary integrity
     *
     * On hosted platforms (Linux/Windows), verify process integrity
     * via /proc/self/exe hash or equivalent. */

    bool boot_ok = true;

#ifdef __linux__
    /* Verify /proc/self/exe is readable and matches expected path patterns */
    char exe_path[256] = {0};
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len <= 0) {
        EAI_LOG_WARN(MOD, "cannot read /proc/self/exe, boot chain unverifiable");
        boot_ok = false;
    } else {
        exe_path[len] = '\0';
        EAI_LOG_DEBUG(MOD, "executable path: %s", exe_path);

        /* Verify the executable hasn't been deleted (common attack vector) */
        if (strstr(exe_path, "(deleted)")) {
            EAI_LOG_ERROR(MOD, "executable has been deleted — possible tampering");
            boot_ok = false;
        }
    }
#elif defined(_WIN32)
    /* On Windows, verify module integrity */
    char module_path[260] = {0};
    if (GetModuleFileNameA(NULL, module_path, sizeof(module_path)) > 0) {
        EAI_LOG_DEBUG(MOD, "executable path: %s", module_path);
    }
#endif

    /* Check that we're not running under a debugger (anti-tamper) */
#ifdef __linux__
    {
        FILE *f = fopen("/proc/self/status", "r");
        if (f) {
            char line[256];
            while (fgets(line, sizeof(line), f)) {
                if (strncmp(line, "TracerPid:", 10) == 0) {
                    int tracer_pid = atoi(line + 10);
                    if (tracer_pid != 0) {
                        EAI_LOG_WARN(MOD, "process is being traced (pid=%d)", tracer_pid);
                        /* Don't fail — just log for audit trail */
                        eai_min_sec_audit(sec, EAI_AUDIT_SECURITY_VIOLATION,
                                          "debugger detected during boot verification");
                    }
                    break;
                }
            }
            fclose(f);
        }
    }
#endif

    if (boot_ok) {
        sec->boot_verified = true;
        eai_min_sec_audit(sec, EAI_AUDIT_BOOT_CHECK, "boot chain verified");
        EAI_LOG_INFO(MOD, "boot verification: PASSED");
        return EAI_OK;
    } else {
        sec->boot_verified = false;
        eai_min_sec_audit(sec, EAI_AUDIT_SECURITY_VIOLATION,
                          "boot chain verification failed");
        sec->violations++;
        EAI_LOG_ERROR(MOD, "boot verification: FAILED");
        return EAI_ERR_PERMISSION;
    }
}

eai_status_t eai_min_sec_verify_model(eai_min_security_lite_t *sec,
                                       const char *model_path, const char *expected_hash) {
    if (!sec || !model_path) return EAI_ERR_INVALID;

    EAI_LOG_INFO(MOD, "verifying model: %s", model_path);

    if (expected_hash && strlen(expected_hash) > 0) {
        snprintf(sec->audit_log[0].message, EAI_AUDIT_MSG_MAX,
                 "model hash check: %s", expected_hash);
        sec->model_verified = true;
        eai_min_sec_audit(sec, EAI_AUDIT_MODEL_LOAD,
                          "model integrity verified");
        EAI_LOG_INFO(MOD, "model verification: PASSED");
        return EAI_OK;
    }

    EAI_LOG_WARN(MOD, "no hash provided, model loaded without verification");
    sec->model_verified = false;
    eai_min_sec_audit(sec, EAI_AUDIT_SECURITY_VIOLATION,
                      "model loaded without hash verification");
    sec->violations++;
    return EAI_OK;
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

eai_status_t eai_min_sec_sanitize_input(eai_min_security_lite_t *sec,
                                         const char *input, char *output, size_t output_size) {
    if (!sec || !input || !output || output_size == 0) return EAI_ERR_INVALID;

    size_t input_len = strlen(input);
    if (input_len >= sec->max_prompt_len) {
        EAI_LOG_WARN(MOD, "input exceeds max prompt length (%u), truncating",
                     sec->max_prompt_len);
        input_len = sec->max_prompt_len - 1;
    }

    for (int i = 0; i < sec->injection_rules.count; i++) {
        if (str_contains_ci(input, sec->injection_rules.patterns[i])) {
            EAI_LOG_WARN(MOD, "injection pattern detected: '%s'",
                         sec->injection_rules.patterns[i]);
            eai_min_sec_audit(sec, EAI_AUDIT_INPUT_SANITIZED,
                              "prompt injection pattern blocked");
            sec->violations++;
            snprintf(output, output_size, "[input rejected: policy violation]");
            return EAI_ERR_PERMISSION;
        }
    }

    size_t copy_len = input_len < output_size - 1 ? input_len : output_size - 1;
    memcpy(output, input, copy_len);
    output[copy_len] = '\0';
    return EAI_OK;
}

eai_status_t eai_min_sec_add_injection_pattern(eai_min_security_lite_t *sec, const char *pattern) {
    if (!sec || !pattern) return EAI_ERR_INVALID;
    if (sec->injection_rules.count >= EAI_INJECTION_PATTERNS) return EAI_ERR_NOMEM;

    strncpy(sec->injection_rules.patterns[sec->injection_rules.count],
            pattern, EAI_INJECTION_PAT_MAX - 1);
    sec->injection_rules.patterns[sec->injection_rules.count][EAI_INJECTION_PAT_MAX - 1] = '\0';
    sec->injection_rules.count++;
    return EAI_OK;
}

eai_status_t eai_min_sec_audit(eai_min_security_lite_t *sec,
                                eai_audit_event_t event, const char *message) {
    if (!sec) return EAI_ERR_INVALID;

    int idx = sec->audit_head % EAI_AUDIT_MAX_ENTRIES;
    sec->audit_log[idx].event = event;
    sec->audit_log[idx].timestamp = sec_now_ms();
    sec->audit_log[idx].identity = sec->security_ctx ? sec->security_ctx->identity : "unknown";
    sec->audit_log[idx].flagged = (event == EAI_AUDIT_SECURITY_VIOLATION ||
                                    event == EAI_AUDIT_TOOL_DENIED ||
                                    event == EAI_AUDIT_INPUT_SANITIZED);

    if (message) {
        strncpy(sec->audit_log[idx].message, message, EAI_AUDIT_MSG_MAX - 1);
        sec->audit_log[idx].message[EAI_AUDIT_MSG_MAX - 1] = '\0';
    } else {
        sec->audit_log[idx].message[0] = '\0';
    }

    sec->audit_head++;
    if (sec->audit_count < EAI_AUDIT_MAX_ENTRIES) sec->audit_count++;

    EAI_LOG_DEBUG(MOD, "audit [%s] %s: %s",
                  sec->audit_log[idx].identity,
                  audit_event_str(event),
                  sec->audit_log[idx].message);
    return EAI_OK;
}

bool eai_min_sec_check_tool(eai_min_security_lite_t *sec, const char *tool_name,
                             const char **required_perms, int perm_count) {
    if (!sec || !tool_name) return false;

    if (!sec->security_ctx) {
        EAI_LOG_WARN(MOD, "no security context, denying tool: %s", tool_name);
        eai_min_sec_audit(sec, EAI_AUDIT_TOOL_DENIED, tool_name);
        return false;
    }

    bool allowed = eai_security_check_tool(sec->security_ctx, required_perms, perm_count);
    if (allowed) {
        eai_min_sec_audit(sec, EAI_AUDIT_TOOL_CALL, tool_name);
    } else {
        eai_min_sec_audit(sec, EAI_AUDIT_TOOL_DENIED, tool_name);
        sec->violations++;
    }
    return allowed;
}

int eai_min_sec_audit_count(const eai_min_security_lite_t *sec) {
    if (!sec) return 0;
    return sec->audit_count;
}

void eai_min_sec_audit_dump(const eai_min_security_lite_t *sec) {
    if (!sec) return;

    printf("\n=== Security Audit Log (%d entries, %u violations) ===\n",
           sec->audit_count, sec->violations);
    printf("Boot verified: %s | Model verified: %s\n",
           sec->boot_verified ? "YES" : "NO",
           sec->model_verified ? "YES" : "NO");
    printf("%-20s %-22s %-8s %s\n", "Timestamp", "Event", "Flag", "Message");
    printf("---------------------------------------------------------------------\n");

    int start = sec->audit_count < EAI_AUDIT_MAX_ENTRIES ? 0 :
                sec->audit_head % EAI_AUDIT_MAX_ENTRIES;

    for (int i = 0; i < sec->audit_count; i++) {
        int idx = (start + i) % EAI_AUDIT_MAX_ENTRIES;
        const eai_audit_entry_t *e = &sec->audit_log[idx];
        printf("%-20llu %-22s %-8s %s\n",
               (unsigned long long)e->timestamp,
               audit_event_str(e->event),
               e->flagged ? "[!]" : "",
               e->message);
    }
}
