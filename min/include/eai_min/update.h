// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_MIN_UPDATE_H
#define EAI_MIN_UPDATE_H

#include "eai/types.h"
#include "eai/manifest.h"

#define EAI_UPDATE_PATH_MAX  256
#define EAI_UPDATE_HASH_MAX  72

typedef enum {
    EAI_UPDATE_IDLE,
    EAI_UPDATE_DOWNLOADING,
    EAI_UPDATE_VERIFYING,
    EAI_UPDATE_APPLYING,
    EAI_UPDATE_DONE,
    EAI_UPDATE_FAILED,
    EAI_UPDATE_ROLLING_BACK,
} eai_update_state_t;

typedef enum {
    EAI_UPDATE_MODEL,
    EAI_UPDATE_ADAPTER,
    EAI_UPDATE_FIRMWARE,
    EAI_UPDATE_CONFIG,
} eai_update_type_t;

typedef struct {
    eai_update_state_t state;
    eai_update_type_t  type;
    char               current_path[EAI_UPDATE_PATH_MAX];
    char               pending_path[EAI_UPDATE_PATH_MAX];
    char               rollback_path[EAI_UPDATE_PATH_MAX];
    char               expected_hash[EAI_UPDATE_HASH_MAX];
    char               computed_hash[EAI_UPDATE_HASH_MAX];
    uint32_t           file_size_bytes;
    uint32_t           bytes_received;
    uint64_t           started_ts;
    uint64_t           completed_ts;
    bool               signature_valid;
    int                retry_count;
    int                max_retries;
} eai_min_update_t;

eai_status_t eai_min_update_init(eai_min_update_t *upd);
eai_status_t eai_min_update_prepare(eai_min_update_t *upd, eai_update_type_t type,
                                     const char *new_path, const char *expected_hash);
eai_status_t eai_min_update_verify(eai_min_update_t *upd);
eai_status_t eai_min_update_apply(eai_min_update_t *upd, const char *target_path);
eai_status_t eai_min_update_rollback(eai_min_update_t *upd);
eai_status_t eai_min_update_set_current(eai_min_update_t *upd, const char *path);
eai_update_state_t eai_min_update_state(const eai_min_update_t *upd);
float        eai_min_update_progress(const eai_min_update_t *upd);

#endif /* EAI_MIN_UPDATE_H */
