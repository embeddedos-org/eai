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

    /* Stub: In production, compute SHA-256 of the file and compare.
       For now, we simulate a successful verification. */
    strncpy(upd->computed_hash, upd->expected_hash, EAI_UPDATE_HASH_MAX - 1);
    upd->computed_hash[EAI_UPDATE_HASH_MAX - 1] = '\0';

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

    /* Stub: In production, this would rename/copy files atomically.
       For now, we simulate success. */
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
