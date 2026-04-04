// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/update.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "update"

static uint64_t upd_now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

static const char *update_state_str(eai_update_state_t st) {
    switch (st) {
        case EAI_UPDATE_IDLE:         return "IDLE";
        case EAI_UPDATE_DOWNLOADING:  return "DOWNLOADING";
        case EAI_UPDATE_VERIFYING:    return "VERIFYING";
        case EAI_UPDATE_APPLYING:     return "APPLYING";
        case EAI_UPDATE_DONE:         return "DONE";
        case EAI_UPDATE_FAILED:       return "FAILED";
        case EAI_UPDATE_ROLLING_BACK: return "ROLLING_BACK";
        default:                      return "UNKNOWN";
    }
}

static const char *update_type_str(eai_update_type_t t) {
    switch (t) {
        case EAI_UPDATE_MODEL:    return "model";
        case EAI_UPDATE_ADAPTER:  return "adapter";
        case EAI_UPDATE_FIRMWARE: return "firmware";
        case EAI_UPDATE_CONFIG:   return "config";
        default:                  return "unknown";
    }
}

eai_status_t eai_min_update_init(eai_min_update_t *upd) {
    if (!upd) return EAI_ERR_INVALID;
    memset(upd, 0, sizeof(*upd));
    upd->state = EAI_UPDATE_IDLE;
    upd->max_retries = 3;
    EAI_LOG_INFO(MOD, "update manager initialized");
    return EAI_OK;
}

eai_status_t eai_min_update_prepare(eai_min_update_t *upd, eai_update_type_t type,
                                     const char *new_path, const char *expected_hash) {
    if (!upd || !new_path) return EAI_ERR_INVALID;
    if (upd->state != EAI_UPDATE_IDLE && upd->state != EAI_UPDATE_DONE &&
        upd->state != EAI_UPDATE_FAILED) {
        EAI_LOG_ERROR(MOD, "update already in progress (state=%s)",
                      update_state_str(upd->state));
        return EAI_ERR_INVALID;
    }

    upd->type = type;
    strncpy(upd->pending_path, new_path, EAI_UPDATE_PATH_MAX - 1);
    upd->pending_path[EAI_UPDATE_PATH_MAX - 1] = '\0';

    if (expected_hash) {
        strncpy(upd->expected_hash, expected_hash, EAI_UPDATE_HASH_MAX - 1);
        upd->expected_hash[EAI_UPDATE_HASH_MAX - 1] = '\0';
    } else {
        upd->expected_hash[0] = '\0';
    }

    upd->state = EAI_UPDATE_DOWNLOADING;
    upd->started_ts = upd_now_ms();
    upd->retry_count = 0;
    upd->signature_valid = false;

    EAI_LOG_INFO(MOD, "update prepared: type=%s path=%s",
                 update_type_str(type), new_path);
    return EAI_OK;
}

eai_status_t eai_min_update_verify(eai_min_update_t *upd) {
    if (!upd) return EAI_ERR_INVALID;

    upd->state = EAI_UPDATE_VERIFYING;
    EAI_LOG_INFO(MOD, "verifying update: %s", upd->pending_path);

    if (strlen(upd->expected_hash) == 0) {
        EAI_LOG_WARN(MOD, "no hash provided, skipping verification");
        upd->signature_valid = false;
        upd->state = EAI_UPDATE_APPLYING;
        return EAI_OK;
    }

    /* Compute SHA-256 hash of the update file for verification */
    FILE *f = fopen(upd->pending_path, "rb");
    if (!f) {
        EAI_LOG_ERROR(MOD, "cannot open update file: %s", upd->pending_path);
        upd->state = EAI_UPDATE_FAILED;
        return EAI_ERR_NOT_FOUND;
    }

    /* Simple SHA-256 computation using incremental hashing.
     * We compute a hex digest string for comparison with expected_hash. */
    unsigned char buf[4096];
    uint32_t hash[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    static const uint32_t sha256_k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    #define SHA_ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
    #define SHA_CH(x,y,z) (((x)&(y))^(~(x)&(z)))
    #define SHA_MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
    #define SHA_EP0(x) (SHA_ROTR(x,2)^SHA_ROTR(x,13)^SHA_ROTR(x,22))
    #define SHA_EP1(x) (SHA_ROTR(x,6)^SHA_ROTR(x,11)^SHA_ROTR(x,25))
    #define SHA_SIG0(x) (SHA_ROTR(x,7)^SHA_ROTR(x,18)^((x)>>3))
    #define SHA_SIG1(x) (SHA_ROTR(x,17)^SHA_ROTR(x,19)^((x)>>10))

    unsigned char block[64];
    size_t total_bytes = 0;
    size_t block_pos = 0;

    while (!feof(f)) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        for (size_t i = 0; i < n; i++) {
            block[block_pos++] = buf[i];
            total_bytes++;
            if (block_pos == 64) {
                /* Process block */
                uint32_t w[64];
                for (int j = 0; j < 16; j++)
                    w[j] = ((uint32_t)block[j*4]<<24)|((uint32_t)block[j*4+1]<<16)|
                            ((uint32_t)block[j*4+2]<<8)|block[j*4+3];
                for (int j = 16; j < 64; j++)
                    w[j] = SHA_SIG1(w[j-2]) + w[j-7] + SHA_SIG0(w[j-15]) + w[j-16];

                uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3];
                uint32_t e=hash[4],ff=hash[5],g=hash[6],h=hash[7];
                for (int j = 0; j < 64; j++) {
                    uint32_t t1 = h + SHA_EP1(e) + SHA_CH(e,ff,g) + sha256_k[j] + w[j];
                    uint32_t t2 = SHA_EP0(a) + SHA_MAJ(a,b,c);
                    h=g; g=ff; ff=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
                }
                hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;
                hash[4]+=e;hash[5]+=ff;hash[6]+=g;hash[7]+=h;
                block_pos = 0;
            }
        }
    }
    fclose(f);

    /* Padding */
    block[block_pos++] = 0x80;
    if (block_pos > 56) {
        while (block_pos < 64) block[block_pos++] = 0;
        /* Process block */
        uint32_t w[64];
        for (int j = 0; j < 16; j++)
            w[j] = ((uint32_t)block[j*4]<<24)|((uint32_t)block[j*4+1]<<16)|
                    ((uint32_t)block[j*4+2]<<8)|block[j*4+3];
        for (int j = 16; j < 64; j++)
            w[j] = SHA_SIG1(w[j-2]) + w[j-7] + SHA_SIG0(w[j-15]) + w[j-16];
        uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3];
        uint32_t e=hash[4],ff=hash[5],g=hash[6],h=hash[7];
        for (int j = 0; j < 64; j++) {
            uint32_t t1 = h + SHA_EP1(e) + SHA_CH(e,ff,g) + sha256_k[j] + w[j];
            uint32_t t2 = SHA_EP0(a) + SHA_MAJ(a,b,c);
            h=g; g=ff; ff=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;
        hash[4]+=e;hash[5]+=ff;hash[6]+=g;hash[7]+=h;
        block_pos = 0;
    }
    while (block_pos < 56) block[block_pos++] = 0;
    uint64_t bits = (uint64_t)total_bytes * 8;
    for (int j = 7; j >= 0; j--) block[block_pos++] = (unsigned char)(bits >> (j*8));
    {
        uint32_t w[64];
        for (int j = 0; j < 16; j++)
            w[j] = ((uint32_t)block[j*4]<<24)|((uint32_t)block[j*4+1]<<16)|
                    ((uint32_t)block[j*4+2]<<8)|block[j*4+3];
        for (int j = 16; j < 64; j++)
            w[j] = SHA_SIG1(w[j-2]) + w[j-7] + SHA_SIG0(w[j-15]) + w[j-16];
        uint32_t a=hash[0],b=hash[1],c=hash[2],d=hash[3];
        uint32_t e=hash[4],ff=hash[5],g=hash[6],h=hash[7];
        for (int j = 0; j < 64; j++) {
            uint32_t t1 = h + SHA_EP1(e) + SHA_CH(e,ff,g) + sha256_k[j] + w[j];
            uint32_t t2 = SHA_EP0(a) + SHA_MAJ(a,b,c);
            h=g; g=ff; ff=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        hash[0]+=a;hash[1]+=b;hash[2]+=c;hash[3]+=d;
        hash[4]+=e;hash[5]+=ff;hash[6]+=g;hash[7]+=h;
    }

    #undef SHA_ROTR
    #undef SHA_CH
    #undef SHA_MAJ
    #undef SHA_EP0
    #undef SHA_EP1
    #undef SHA_SIG0
    #undef SHA_SIG1

    /* Convert to hex string */
    for (int i = 0; i < 8; i++) {
        snprintf(&upd->computed_hash[i*8], 9, "%08x", hash[i]);
    }

    if (strcmp(upd->expected_hash, upd->computed_hash) == 0) {
        upd->signature_valid = true;
        EAI_LOG_INFO(MOD, "hash verification PASSED");
        upd->state = EAI_UPDATE_APPLYING;
        return EAI_OK;
    }

    EAI_LOG_ERROR(MOD, "hash mismatch: expected=%s computed=%s",
                  upd->expected_hash, upd->computed_hash);
    upd->signature_valid = false;
    upd->state = EAI_UPDATE_FAILED;
    return EAI_ERR_PERMISSION;
}

eai_status_t eai_min_update_apply(eai_min_update_t *upd, const char *target_path) {
    if (!upd || !target_path) return EAI_ERR_INVALID;
    if (upd->state != EAI_UPDATE_APPLYING && upd->state != EAI_UPDATE_VERIFYING) {
        EAI_LOG_ERROR(MOD, "cannot apply update in state=%s",
                      update_state_str(upd->state));
        return EAI_ERR_INVALID;
    }

    upd->state = EAI_UPDATE_APPLYING;

    /* Save current as rollback */
    if (strlen(upd->current_path) > 0) {
        strncpy(upd->rollback_path, upd->current_path, EAI_UPDATE_PATH_MAX - 1);
        upd->rollback_path[EAI_UPDATE_PATH_MAX - 1] = '\0';
    }

    /* Atomically apply the update by copying the pending file to target.
     * On POSIX systems, rename() is atomic within the same filesystem.
     * For cross-filesystem updates, copy then rename for safety. */

    /* Try atomic rename first */
    if (rename(upd->pending_path, target_path) == 0) {
        EAI_LOG_INFO(MOD, "update applied via rename: %s -> %s",
                     upd->pending_path, target_path);
    } else {
        /* Cross-filesystem: copy then rename */
        FILE *src = fopen(upd->pending_path, "rb");
        if (!src) {
            EAI_LOG_ERROR(MOD, "cannot open pending update: %s", upd->pending_path);
            upd->state = EAI_UPDATE_FAILED;
            return EAI_ERR_NOT_FOUND;
        }

        char tmp_path[EAI_UPDATE_PATH_MAX];
        snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", target_path);
        FILE *dst = fopen(tmp_path, "wb");
        if (!dst) {
            fclose(src);
            EAI_LOG_ERROR(MOD, "cannot create temp file: %s", tmp_path);
            upd->state = EAI_UPDATE_FAILED;
            return EAI_ERR_PERMISSION;
        }

        char copy_buf[4096];
        size_t n;
        while ((n = fread(copy_buf, 1, sizeof(copy_buf), src)) > 0) {
            if (fwrite(copy_buf, 1, n, dst) != n) {
                fclose(src);
                fclose(dst);
                remove(tmp_path);
                EAI_LOG_ERROR(MOD, "write error during update copy");
                upd->state = EAI_UPDATE_FAILED;
                return EAI_ERR_GENERIC;
            }
        }
        fclose(src);
        fclose(dst);

        /* Rename temp to target (atomic on same filesystem) */
        remove(target_path); /* Remove old file first (required on Windows) */
        if (rename(tmp_path, target_path) != 0) {
            remove(tmp_path);
            EAI_LOG_ERROR(MOD, "rename failed: %s -> %s", tmp_path, target_path);
            upd->state = EAI_UPDATE_FAILED;
            return EAI_ERR_GENERIC;
        }

        EAI_LOG_INFO(MOD, "update applied via copy: %s -> %s",
                     upd->pending_path, target_path);
    }
    strncpy(upd->current_path, target_path, EAI_UPDATE_PATH_MAX - 1);
    upd->current_path[EAI_UPDATE_PATH_MAX - 1] = '\0';

    upd->completed_ts = upd_now_ms();
    upd->state = EAI_UPDATE_DONE;

    EAI_LOG_INFO(MOD, "update applied successfully: %s -> %s",
                 upd->pending_path, target_path);
    return EAI_OK;
}

eai_status_t eai_min_update_rollback(eai_min_update_t *upd) {
    if (!upd) return EAI_ERR_INVALID;

    if (strlen(upd->rollback_path) == 0) {
        EAI_LOG_ERROR(MOD, "no rollback path available");
        return EAI_ERR_NOT_FOUND;
    }

    upd->state = EAI_UPDATE_ROLLING_BACK;
    EAI_LOG_WARN(MOD, "rolling back to: %s", upd->rollback_path);

    strncpy(upd->current_path, upd->rollback_path, EAI_UPDATE_PATH_MAX - 1);
    upd->current_path[EAI_UPDATE_PATH_MAX - 1] = '\0';
    upd->rollback_path[0] = '\0';
    upd->state = EAI_UPDATE_DONE;

    EAI_LOG_INFO(MOD, "rollback completed");
    return EAI_OK;
}

eai_status_t eai_min_update_set_current(eai_min_update_t *upd, const char *path) {
    if (!upd || !path) return EAI_ERR_INVALID;
    strncpy(upd->current_path, path, EAI_UPDATE_PATH_MAX - 1);
    upd->current_path[EAI_UPDATE_PATH_MAX - 1] = '\0';
    return EAI_OK;
}

eai_update_state_t eai_min_update_state(const eai_min_update_t *upd) {
    if (!upd) return EAI_UPDATE_IDLE;
    return upd->state;
}

float eai_min_update_progress(const eai_min_update_t *upd) {
    if (!upd || upd->file_size_bytes == 0) return 0.0f;
    float pct = (float)upd->bytes_received / (float)upd->file_size_bytes * 100.0f;
    return pct > 100.0f ? 100.0f : pct;
}
