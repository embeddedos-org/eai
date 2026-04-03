// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_UPDATE_MANAGER_H
#define EAI_FW_UPDATE_MANAGER_H

#include "eai/types.h"
#include "eai/manifest.h"

#define EAI_UPDMGR_PATH_MAX     256
#define EAI_UPDMGR_HASH_MAX     72
#define EAI_UPDMGR_HISTORY_MAX  16
#define EAI_UPDMGR_CHANGELOG_MAX 512

typedef enum {
    EAI_UPDMGR_IDLE,
    EAI_UPDMGR_CHECKING,
    EAI_UPDMGR_DOWNLOADING,
    EAI_UPDMGR_VERIFYING,
    EAI_UPDMGR_STAGING,
    EAI_UPDMGR_APPLYING,
    EAI_UPDMGR_REBOOTING,
    EAI_UPDMGR_DONE,
    EAI_UPDMGR_FAILED,
    EAI_UPDMGR_ROLLING_BACK,
} eai_updmgr_state_t;

typedef enum {
    EAI_UPDMGR_MODEL,
    EAI_UPDMGR_ADAPTER,
    EAI_UPDMGR_FIRMWARE,
    EAI_UPDMGR_RUNTIME,
    EAI_UPDMGR_CONFIG,
    EAI_UPDMGR_SYSTEM,
} eai_updmgr_type_t;

typedef enum {
    EAI_PARTITION_A,
    EAI_PARTITION_B,
} eai_partition_t;

typedef struct {
    char               version[32];
    char               hash[EAI_UPDMGR_HASH_MAX];
    char               changelog[EAI_UPDMGR_CHANGELOG_MAX];
    eai_updmgr_type_t  type;
    uint64_t           applied_ts;
    bool               success;
    eai_partition_t    partition;
} eai_update_history_t;

typedef struct {
    bool     ab_partition_enabled;
    bool     auto_rollback;
    uint32_t max_consecutive_failures;
    uint32_t maintenance_window_start_hour;
    uint32_t maintenance_window_end_hour;
    bool     require_signature;
    bool     require_compatibility_check;
} eai_updmgr_config_t;

typedef struct {
    eai_updmgr_state_t   state;
    eai_updmgr_config_t  config;
    eai_partition_t      active_partition;
    eai_partition_t      staging_partition;
    char                 partition_a_path[EAI_UPDMGR_PATH_MAX];
    char                 partition_b_path[EAI_UPDMGR_PATH_MAX];
    char                 pending_version[32];
    char                 pending_hash[EAI_UPDMGR_HASH_MAX];
    char                 current_version[32];
    uint32_t             consecutive_failures;
    eai_update_history_t history[EAI_UPDMGR_HISTORY_MAX];
    int                  history_count;
    uint64_t             last_check_ts;
} eai_fw_update_manager_t;

eai_status_t eai_fw_updmgr_init(eai_fw_update_manager_t *mgr, const eai_updmgr_config_t *cfg);
eai_status_t eai_fw_updmgr_set_partitions(eai_fw_update_manager_t *mgr,
                                            const char *path_a, const char *path_b);
eai_status_t eai_fw_updmgr_check_update(eai_fw_update_manager_t *mgr,
                                          const char *version, const char *hash);
eai_status_t eai_fw_updmgr_stage(eai_fw_update_manager_t *mgr, const char *update_path);
eai_status_t eai_fw_updmgr_verify(eai_fw_update_manager_t *mgr);
eai_status_t eai_fw_updmgr_apply(eai_fw_update_manager_t *mgr);
eai_status_t eai_fw_updmgr_rollback(eai_fw_update_manager_t *mgr);
eai_status_t eai_fw_updmgr_confirm_boot(eai_fw_update_manager_t *mgr);
bool         eai_fw_updmgr_in_maintenance_window(const eai_fw_update_manager_t *mgr);
void         eai_fw_updmgr_report(const eai_fw_update_manager_t *mgr);

#endif /* EAI_FW_UPDATE_MANAGER_H */
