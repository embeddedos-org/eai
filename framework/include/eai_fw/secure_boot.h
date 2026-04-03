// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_SECURE_BOOT_H
#define EAI_FW_SECURE_BOOT_H

#include "eai/types.h"

#define EAI_BOOT_CHAIN_MAX    4
#define EAI_BOOT_HASH_MAX     72
#define EAI_BOOT_KEY_MAX      8
#define EAI_BOOT_KEY_ID_MAX   64

typedef enum {
    EAI_BOOT_STAGE_BOOTLOADER,
    EAI_BOOT_STAGE_KERNEL,
    EAI_BOOT_STAGE_RUNTIME,
    EAI_BOOT_STAGE_MODEL,
} eai_boot_stage_t;

typedef enum {
    EAI_BOOT_UNVERIFIED,
    EAI_BOOT_VERIFIED,
    EAI_BOOT_FAILED,
    EAI_BOOT_SKIPPED,
} eai_boot_status_t;

typedef struct {
    eai_boot_stage_t  stage;
    eai_boot_status_t status;
    char              component_name[64];
    char              hash[EAI_BOOT_HASH_MAX];
    char              expected_hash[EAI_BOOT_HASH_MAX];
    uint64_t          verified_ts;
} eai_boot_chain_entry_t;

typedef struct {
    char     key_id[EAI_BOOT_KEY_ID_MAX];
    char     fingerprint[EAI_BOOT_HASH_MAX];
    uint64_t expiry_ts;
    bool     revoked;
    bool     active;
} eai_trusted_key_t;

typedef struct {
    char     device_id[64];
    char     firmware_version[32];
    char     measurements[EAI_BOOT_CHAIN_MAX][EAI_BOOT_HASH_MAX];
    int      measurement_count;
    uint64_t generated_ts;
    bool     valid;
} eai_attestation_report_t;

typedef struct {
    eai_boot_chain_entry_t chain[EAI_BOOT_CHAIN_MAX];
    int                    chain_length;
    eai_trusted_key_t      trusted_keys[EAI_BOOT_KEY_MAX];
    int                    key_count;
    eai_attestation_report_t attestation;
    bool                   chain_verified;
    bool                   runtime_integrity;
    uint64_t               last_integrity_check;
    uint32_t               integrity_check_interval_ms;
} eai_fw_secure_boot_t;

eai_status_t eai_fw_secboot_init(eai_fw_secure_boot_t *sb);
eai_status_t eai_fw_secboot_add_chain_entry(eai_fw_secure_boot_t *sb,
                                             eai_boot_stage_t stage,
                                             const char *component,
                                             const char *expected_hash);
eai_status_t eai_fw_secboot_verify_chain(eai_fw_secure_boot_t *sb);
eai_status_t eai_fw_secboot_add_key(eai_fw_secure_boot_t *sb,
                                     const char *key_id, const char *fingerprint,
                                     uint64_t expiry_ts);
eai_status_t eai_fw_secboot_revoke_key(eai_fw_secure_boot_t *sb, const char *key_id);
eai_status_t eai_fw_secboot_generate_attestation(eai_fw_secure_boot_t *sb,
                                                  const char *device_id,
                                                  const char *firmware_version);
eai_status_t eai_fw_secboot_check_integrity(eai_fw_secure_boot_t *sb);
bool         eai_fw_secboot_is_verified(const eai_fw_secure_boot_t *sb);
void         eai_fw_secboot_report(const eai_fw_secure_boot_t *sb);

#endif /* EAI_FW_SECURE_BOOT_H */
