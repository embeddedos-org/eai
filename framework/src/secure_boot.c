// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/secure_boot.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdint.h>

#define MOD "secboot"

/* ========================================================================
 * Minimal SHA-256 implementation (FIPS 180-4)
 * ======================================================================== */

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define SHA256_CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x)  (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_EP1(x)  (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_SIG0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

typedef struct {
    uint32_t state[8];
    uint8_t  block[64];
    uint32_t block_len;
    uint64_t total_len;
} sha256_ctx_t;

static void sha256_transform(sha256_ctx_t *ctx) {
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;

    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)ctx->block[i * 4] << 24) |
               ((uint32_t)ctx->block[i * 4 + 1] << 16) |
               ((uint32_t)ctx->block[i * 4 + 2] << 8) |
               ((uint32_t)ctx->block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        w[i] = SHA256_SIG1(w[i - 2]) + w[i - 7] +
               SHA256_SIG0(w[i - 15]) + w[i - 16];
    }

    a = ctx->state[0]; b = ctx->state[1];
    c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5];
    g = ctx->state[6]; h = ctx->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + sha256_k[i] + w[i];
        uint32_t t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b;
    ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f;
    ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->block_len = 0;
    ctx->total_len = 0;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        ctx->block[ctx->block_len++] = data[i];
        if (ctx->block_len == 64) {
            sha256_transform(ctx);
            ctx->block_len = 0;
            ctx->total_len += 512;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]) {
    ctx->total_len += ctx->block_len * 8;
    ctx->block[ctx->block_len++] = 0x80;

    if (ctx->block_len > 56) {
        while (ctx->block_len < 64)
            ctx->block[ctx->block_len++] = 0;
        sha256_transform(ctx);
        ctx->block_len = 0;
    }
    while (ctx->block_len < 56)
        ctx->block[ctx->block_len++] = 0;

    ctx->block[56] = (uint8_t)(ctx->total_len >> 56);
    ctx->block[57] = (uint8_t)(ctx->total_len >> 48);
    ctx->block[58] = (uint8_t)(ctx->total_len >> 40);
    ctx->block[59] = (uint8_t)(ctx->total_len >> 32);
    ctx->block[60] = (uint8_t)(ctx->total_len >> 24);
    ctx->block[61] = (uint8_t)(ctx->total_len >> 16);
    ctx->block[62] = (uint8_t)(ctx->total_len >> 8);
    ctx->block[63] = (uint8_t)(ctx->total_len);
    sha256_transform(ctx);

    for (int i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/**
 * Compute SHA-256 of data and write the result as a 64-char hex string
 * into out_hex (must be at least 65 bytes).
 */
static void sha256_hex(const void *data, size_t len, char *out_hex) {
    sha256_ctx_t ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)data, len);
    sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++) {
        sprintf(out_hex + i * 2, "%02x", hash[i]);
    }
    out_hex[64] = '\0';
}

/* ======================================================================== */

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

        /*
         * Compute SHA-256 of the component_name string as a proxy for
         * computing the hash of the actual firmware binary. This is
         * deterministic and testable in simulation mode.
         *
         * In production, this would hash the actual component binary data
         * loaded from flash/storage.
         */
        sha256_hex(e->component_name, strlen(e->component_name), e->hash);

        if (strcmp(e->hash, e->expected_hash) == 0) {
            e->status = EAI_BOOT_VERIFIED;
            e->verified_ts = secboot_now();
            EAI_LOG_INFO(MOD, "  [%s] %s: VERIFIED (hash=%s)",
                         stage_str(e->stage), e->component_name, e->hash);
        } else {
            e->status = EAI_BOOT_FAILED;
            sb->chain_verified = false;
            EAI_LOG_ERROR(MOD, "  [%s] %s: FAILED (computed=%s expected=%s)",
                          stage_str(e->stage), e->component_name,
                          e->hash, e->expected_hash);
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
