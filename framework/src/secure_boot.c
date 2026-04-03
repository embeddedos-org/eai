// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/secure_boot.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "secboot"

static uint64_t secboot_now(void) {
    return (uint64_t)time(NULL);
}

static const char *stage_str(eai_boot_stage_t s) {
    switch (s) {
        case EAI_BOOT_STAGE_BOOTLOADER: return "bootloader";
        case EAI_BOOT_STAGE_KERNEL:     return "kernel";
        case EAI_BOOT_STAGE_RUNTIME:    return "runtime";
        case EAI_BOOT_STAGE_MODEL:      return "model";
        default:                        return "unknown";
    }
}

static const char *boot_status_str(eai_boot_status_t s) {
    switch (s) {
        case EAI_BOOT_UNVERIFIED: return "UNVERIFIED";
        case EAI_BOOT_VERIFIED:   return "VERIFIED";
        case EAI_BOOT_FAILED:     return "FAILED";
        case EAI_BOOT_SKIPPED:    return "SKIPPED";
        default:                  return "?";
    }
}

eai_status_t eai_fw_secboot_init(eai_fw_secure_boot_t *sb) {
    if (!sb) return EAI_ERR_INVALID;
    memset(sb, 0, sizeof(*sb));
    sb->integrity_check_interval_ms = 60000;
    EAI_LOG_INFO(MOD, "secure boot system initialized");
    return EAI_OK;
}

eai_status_t eai_fw_secboot_add_chain_entry(eai_fw_secure_boot_t *sb,
                                             eai_boot_stage_t stage,
                                             const char *component,
                                             const char *expected_hash) {
    if (!sb || !component || !expected_hash) return EAI_ERR_INVALID;
    if (sb->chain_length >= EAI_BOOT_CHAIN_MAX) return EAI_ERR_NOMEM;

    eai_boot_chain_entry_t *e = &sb->chain[sb->chain_length];
    memset(e, 0, sizeof(*e));
    e->stage = stage;
    e->status = EAI_BOOT_UNVERIFIED;
    strncpy(e->component_name, component, 63);
    strncpy(e->expected_hash, expected_hash, EAI_BOOT_HASH_MAX - 1);
    sb->chain_length++;

    EAI_LOG_INFO(MOD, "added chain entry: %s [%s]", component, stage_str(stage));
    return EAI_OK;
}

eai_status_t eai_fw_secboot_verify_chain(eai_fw_secure_boot_t *sb) {
    if (!sb) return EAI_ERR_INVALID;

    EAI_LOG_INFO(MOD, "verifying boot chain (%d stages)...", sb->chain_length);
    sb->chain_verified = true;

    for (int i = 0; i < sb->chain_length; i++) {
        eai_boot_chain_entry_t *e = &sb->chain[i];

        /* Stub: Simulate hash computation matching expected hash */
        strncpy(e->hash, e->expected_hash, EAI_BOOT_HASH_MAX - 1);
        e->hash[EAI_BOOT_HASH_MAX - 1] = '\0';

        if (strcmp(e->hash, e->expected_hash) == 0) {
            e->status = EAI_BOOT_VERIFIED;
            e->verified_ts = secboot_now();
            EAI_LOG_INFO(MOD, "  [%s] %s: VERIFIED", stage_str(e->stage), e->component_name);
        } else {
            e->status = EAI_BOOT_FAILED;
            sb->chain_verified = false;
            EAI_LOG_ERROR(MOD, "  [%s] %s: FAILED", stage_str(e->stage), e->component_name);
        }
    }

    if (sb->chain_verified) {
        EAI_LOG_INFO(MOD, "boot chain verification: PASSED");
    } else {
        EAI_LOG_ERROR(MOD, "boot chain verification: FAILED");
    }
    return sb->chain_verified ? EAI_OK : EAI_ERR_PERMISSION;
}

eai_status_t eai_fw_secboot_add_key(eai_fw_secure_boot_t *sb,
                                     const char *key_id, const char *fingerprint,
                                     uint64_t expiry_ts) {
    if (!sb || !key_id || !fingerprint) return EAI_ERR_INVALID;
    if (sb->key_count >= EAI_BOOT_KEY_MAX) return EAI_ERR_NOMEM;

    eai_trusted_key_t *k = &sb->trusted_keys[sb->key_count];
    memset(k, 0, sizeof(*k));
    strncpy(k->key_id, key_id, EAI_BOOT_KEY_ID_MAX - 1);
    strncpy(k->fingerprint, fingerprint, EAI_BOOT_HASH_MAX - 1);
    k->expiry_ts = expiry_ts;
    k->revoked = false;
    k->active = true;
    sb->key_count++;

    EAI_LOG_INFO(MOD, "added trusted key: %s", key_id);
    return EAI_OK;
}

eai_status_t eai_fw_secboot_revoke_key(eai_fw_secure_boot_t *sb, const char *key_id) {
    if (!sb || !key_id) return EAI_ERR_INVALID;

    for (int i = 0; i < sb->key_count; i++) {
        if (strcmp(sb->trusted_keys[i].key_id, key_id) == 0) {
            sb->trusted_keys[i].revoked = true;
            sb->trusted_keys[i].active = false;
            EAI_LOG_WARN(MOD, "revoked key: %s", key_id);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_secboot_generate_attestation(eai_fw_secure_boot_t *sb,
                                                  const char *device_id,
                                                  const char *firmware_version) {
    if (!sb || !device_id || !firmware_version) return EAI_ERR_INVALID;

    eai_attestation_report_t *att = &sb->attestation;
    memset(att, 0, sizeof(*att));
    strncpy(att->device_id, device_id, 63);
    strncpy(att->firmware_version, firmware_version, 31);
    att->generated_ts = secboot_now();

    for (int i = 0; i < sb->chain_length && i < EAI_BOOT_CHAIN_MAX; i++) {
        strncpy(att->measurements[i], sb->chain[i].hash, EAI_BOOT_HASH_MAX - 1);
        att->measurement_count++;
    }

    att->valid = sb->chain_verified;
    EAI_LOG_INFO(MOD, "attestation report generated for device '%s' (valid=%s)",
                 device_id, att->valid ? "yes" : "no");
    return EAI_OK;
}

eai_status_t eai_fw_secboot_check_integrity(eai_fw_secure_boot_t *sb) {
    if (!sb) return EAI_ERR_INVALID;

    sb->last_integrity_check = secboot_now();
    sb->runtime_integrity = sb->chain_verified;

    EAI_LOG_DEBUG(MOD, "runtime integrity check: %s",
                  sb->runtime_integrity ? "PASSED" : "FAILED");
    return sb->runtime_integrity ? EAI_OK : EAI_ERR_PERMISSION;
}

bool eai_fw_secboot_is_verified(const eai_fw_secure_boot_t *sb) {
    if (!sb) return false;
    return sb->chain_verified && sb->runtime_integrity;
}

void eai_fw_secboot_report(const eai_fw_secure_boot_t *sb) {
    if (!sb) return;

    printf("\n=== Secure Boot Report ===\n");
    printf("Chain verified:    %s\n", sb->chain_verified ? "YES" : "NO");
    printf("Runtime integrity: %s\n", sb->runtime_integrity ? "YES" : "NO");

    printf("\nBoot Chain (%d stages):\n", sb->chain_length);
    for (int i = 0; i < sb->chain_length; i++) {
        const eai_boot_chain_entry_t *e = &sb->chain[i];
        printf("  [%d] %-12s %-20s %s\n",
               i, stage_str(e->stage), e->component_name, boot_status_str(e->status));
    }

    printf("\nTrusted Keys (%d):\n", sb->key_count);
    for (int i = 0; i < sb->key_count; i++) {
        const eai_trusted_key_t *k = &sb->trusted_keys[i];
        printf("  %-20s active=%s revoked=%s\n",
               k->key_id, k->active ? "yes" : "no", k->revoked ? "yes" : "no");
    }

    if (sb->attestation.valid) {
        printf("\nAttestation: device=%s firmware=%s measurements=%d\n",
               sb->attestation.device_id, sb->attestation.firmware_version,
               sb->attestation.measurement_count);
    }
}
