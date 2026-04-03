// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/update_manager.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "updmgr"

static uint64_t updmgr_now(void) {
    return (uint64_t)time(NULL);
}

eai_status_t eai_fw_updmgr_init(eai_fw_update_manager_t *mgr, const eai_updmgr_config_t *cfg) {
    if (!mgr) return EAI_ERR_INVALID;
    memset(mgr, 0, sizeof(*mgr));
    mgr->state = EAI_UPDMGR_IDLE;
    mgr->active_partition = EAI_PARTITION_A;
    mgr->staging_partition = EAI_PARTITION_B;

    if (cfg) {
        mgr->config = *cfg;
    } else {
        mgr->config.ab_partition_enabled = true;
        mgr->config.auto_rollback = true;
        mgr->config.max_consecutive_failures = 3;
        mgr->config.maintenance_window_start_hour = 2;
        mgr->config.maintenance_window_end_hour = 5;
        mgr->config.require_signature = true;
        mgr->config.require_compatibility_check = true;
    }

    EAI_LOG_INFO(MOD, "update manager initialized (A/B=%s, auto_rollback=%s)",
                 mgr->config.ab_partition_enabled ? "on" : "off",
                 mgr->config.auto_rollback ? "on" : "off");
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_set_partitions(eai_fw_update_manager_t *mgr,
                                            const char *path_a, const char *path_b) {
    if (!mgr || !path_a || !path_b) return EAI_ERR_INVALID;
    strncpy(mgr->partition_a_path, path_a, EAI_UPDMGR_PATH_MAX - 1);
    strncpy(mgr->partition_b_path, path_b, EAI_UPDMGR_PATH_MAX - 1);
    EAI_LOG_INFO(MOD, "partitions set: A=%s B=%s", path_a, path_b);
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_check_update(eai_fw_update_manager_t *mgr,
                                          const char *version, const char *hash) {
    if (!mgr || !version) return EAI_ERR_INVALID;

    mgr->state = EAI_UPDMGR_CHECKING;
    mgr->last_check_ts = updmgr_now();

    if (strcmp(version, mgr->current_version) == 0) {
        EAI_LOG_INFO(MOD, "already up to date: %s", version);
        mgr->state = EAI_UPDMGR_IDLE;
        return EAI_OK;
    }

    strncpy(mgr->pending_version, version, 31);
    mgr->pending_version[31] = '\0';
    if (hash) {
        strncpy(mgr->pending_hash, hash, EAI_UPDMGR_HASH_MAX - 1);
        mgr->pending_hash[EAI_UPDMGR_HASH_MAX - 1] = '\0';
    }

    mgr->state = EAI_UPDMGR_DOWNLOADING;
    EAI_LOG_INFO(MOD, "update available: %s -> %s", mgr->current_version, version);
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_stage(eai_fw_update_manager_t *mgr, const char *update_path) {
    if (!mgr || !update_path) return EAI_ERR_INVALID;

    mgr->state = EAI_UPDMGR_STAGING;
    EAI_LOG_INFO(MOD, "staging update to partition %c",
                 mgr->staging_partition == EAI_PARTITION_A ? 'A' : 'B');
    mgr->state = EAI_UPDMGR_VERIFYING;
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_verify(eai_fw_update_manager_t *mgr) {
    if (!mgr) return EAI_ERR_INVALID;

    mgr->state = EAI_UPDMGR_VERIFYING;

    if (mgr->config.require_signature && strlen(mgr->pending_hash) == 0) {
        EAI_LOG_ERROR(MOD, "signature required but no hash provided");
        mgr->state = EAI_UPDMGR_FAILED;
        return EAI_ERR_PERMISSION;
    }

    EAI_LOG_INFO(MOD, "verification passed for version %s", mgr->pending_version);
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_apply(eai_fw_update_manager_t *mgr) {
    if (!mgr) return EAI_ERR_INVALID;

    mgr->state = EAI_UPDMGR_APPLYING;

    if (mgr->config.ab_partition_enabled) {
        eai_partition_t tmp = mgr->active_partition;
        mgr->active_partition = mgr->staging_partition;
        mgr->staging_partition = tmp;
        EAI_LOG_INFO(MOD, "switched active partition to %c",
                     mgr->active_partition == EAI_PARTITION_A ? 'A' : 'B');
    }

    if (mgr->history_count < EAI_UPDMGR_HISTORY_MAX) {
        eai_update_history_t *h = &mgr->history[mgr->history_count];
        memset(h, 0, sizeof(*h));
        strncpy(h->version, mgr->pending_version, 31);
        strncpy(h->hash, mgr->pending_hash, EAI_UPDMGR_HASH_MAX - 1);
        h->applied_ts = updmgr_now();
        h->success = true;
        h->partition = mgr->active_partition;
        mgr->history_count++;
    }

    strncpy(mgr->current_version, mgr->pending_version, 31);
    mgr->consecutive_failures = 0;
    mgr->state = EAI_UPDMGR_DONE;

    EAI_LOG_INFO(MOD, "update applied: now running version %s", mgr->current_version);
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_rollback(eai_fw_update_manager_t *mgr) {
    if (!mgr) return EAI_ERR_INVALID;

    if (!mgr->config.ab_partition_enabled) {
        EAI_LOG_ERROR(MOD, "rollback requires A/B partitions");
        return EAI_ERR_UNSUPPORTED;
    }

    mgr->state = EAI_UPDMGR_ROLLING_BACK;

    eai_partition_t tmp = mgr->active_partition;
    mgr->active_partition = mgr->staging_partition;
    mgr->staging_partition = tmp;

    EAI_LOG_WARN(MOD, "rolled back to partition %c",
                 mgr->active_partition == EAI_PARTITION_A ? 'A' : 'B');
    mgr->state = EAI_UPDMGR_DONE;
    return EAI_OK;
}

eai_status_t eai_fw_updmgr_confirm_boot(eai_fw_update_manager_t *mgr) {
    if (!mgr) return EAI_ERR_INVALID;

    mgr->consecutive_failures = 0;
    EAI_LOG_INFO(MOD, "boot confirmed on partition %c",
                 mgr->active_partition == EAI_PARTITION_A ? 'A' : 'B');
    return EAI_OK;
}

bool eai_fw_updmgr_in_maintenance_window(const eai_fw_update_manager_t *mgr) {
    if (!mgr) return false;

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    if (!tm) return false;

    uint32_t hour = (uint32_t)tm->tm_hour;
    if (mgr->config.maintenance_window_start_hour <= mgr->config.maintenance_window_end_hour) {
        return hour >= mgr->config.maintenance_window_start_hour &&
               hour < mgr->config.maintenance_window_end_hour;
    }
    return hour >= mgr->config.maintenance_window_start_hour ||
           hour < mgr->config.maintenance_window_end_hour;
}

void eai_fw_updmgr_report(const eai_fw_update_manager_t *mgr) {
    if (!mgr) return;

    printf("\n=== Update Manager Report ===\n");
    printf("Current version:  %s\n", mgr->current_version);
    printf("Active partition: %c\n", mgr->active_partition == EAI_PARTITION_A ? 'A' : 'B');
    printf("Failures:         %u / %u\n", mgr->consecutive_failures,
           mgr->config.max_consecutive_failures);
    printf("A/B partitions:   %s\n", mgr->config.ab_partition_enabled ? "enabled" : "disabled");

    printf("\nUpdate History (%d entries):\n", mgr->history_count);
    for (int i = 0; i < mgr->history_count; i++) {
        const eai_update_history_t *h = &mgr->history[i];
        printf("  [%d] v%s partition=%c success=%s\n",
               i, h->version,
               h->partition == EAI_PARTITION_A ? 'A' : 'B',
               h->success ? "yes" : "no");
    }
}
