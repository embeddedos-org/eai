// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_MIN_SECURITY_LITE_H
#define EAI_MIN_SECURITY_LITE_H

#include "eai/types.h"
#include "eai/security.h"

#define EAI_AUDIT_MAX_ENTRIES  256
#define EAI_AUDIT_MSG_MAX      256
#define EAI_INJECTION_PATTERNS 16
#define EAI_INJECTION_PAT_MAX  128

typedef enum {
    EAI_AUDIT_AGENT_START,
    EAI_AUDIT_AGENT_STOP,
    EAI_AUDIT_TOOL_CALL,
    EAI_AUDIT_TOOL_DENIED,
    EAI_AUDIT_INFERENCE,
    EAI_AUDIT_MODEL_LOAD,
    EAI_AUDIT_MODEL_UPDATE,
    EAI_AUDIT_SECURITY_VIOLATION,
    EAI_AUDIT_INPUT_SANITIZED,
    EAI_AUDIT_BOOT_CHECK,
} eai_audit_event_t;

typedef struct {
    eai_audit_event_t event;
    char              message[EAI_AUDIT_MSG_MAX];
    uint64_t          timestamp;
    const char       *identity;
    bool              flagged;
} eai_audit_entry_t;

typedef struct {
    char patterns[EAI_INJECTION_PATTERNS][EAI_INJECTION_PAT_MAX];
    int  count;
} eai_injection_rules_t;

typedef struct {
    eai_security_ctx_t   *security_ctx;
    eai_audit_entry_t     audit_log[EAI_AUDIT_MAX_ENTRIES];
    int                   audit_count;
    int                   audit_head;
    eai_injection_rules_t injection_rules;
    bool                  boot_verified;
    bool                  model_verified;
    uint32_t              max_prompt_len;
    uint32_t              violations;
} eai_min_security_lite_t;

eai_status_t eai_min_sec_init(eai_min_security_lite_t *sec, eai_security_ctx_t *ctx);
eai_status_t eai_min_sec_verify_boot(eai_min_security_lite_t *sec);
eai_status_t eai_min_sec_verify_model(eai_min_security_lite_t *sec,
                                       const char *model_path, const char *expected_hash);
eai_status_t eai_min_sec_sanitize_input(eai_min_security_lite_t *sec,
                                         const char *input, char *output, size_t output_size);
eai_status_t eai_min_sec_add_injection_pattern(eai_min_security_lite_t *sec, const char *pattern);
eai_status_t eai_min_sec_audit(eai_min_security_lite_t *sec,
                                eai_audit_event_t event, const char *message);
bool         eai_min_sec_check_tool(eai_min_security_lite_t *sec, const char *tool_name,
                                     const char **required_perms, int perm_count);
int          eai_min_sec_audit_count(const eai_min_security_lite_t *sec);
void         eai_min_sec_audit_dump(const eai_min_security_lite_t *sec);

#endif /* EAI_MIN_SECURITY_LITE_H */
