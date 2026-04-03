// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_FEDERATED_H
#define EAI_FW_FEDERATED_H

#include "eai/types.h"
#include "eai/runtime_contract.h"

#define EAI_FED_MAX_PARTICIPANTS 32
#define EAI_FED_DEVICE_ID_MAX   64

typedef enum {
    EAI_FED_COORDINATOR,
    EAI_FED_PARTICIPANT,
} eai_fed_role_t;

typedef enum {
    EAI_FED_IDLE,
    EAI_FED_PLANNING,
    EAI_FED_DISTRIBUTING,
    EAI_FED_TRAINING,
    EAI_FED_COLLECTING,
    EAI_FED_AGGREGATING,
    EAI_FED_COMPLETE,
    EAI_FED_ERROR,
} eai_fed_state_t;

typedef struct {
    float epsilon;
    float delta;
    bool  enabled;
} eai_differential_privacy_t;

typedef struct {
    char     device_id[EAI_FED_DEVICE_ID_MAX];
    bool     active;
    bool     submitted;
    float    local_loss;
    uint32_t samples_trained;
    uint32_t round_number;
} eai_fed_participant_t;

typedef struct {
    float    learning_rate;
    uint32_t local_epochs;
    uint32_t min_participants;
    uint32_t max_rounds;
    float    target_loss;
    eai_differential_privacy_t privacy;
} eai_fed_config_t;

typedef struct {
    eai_fed_role_t        role;
    eai_fed_state_t       state;
    eai_fed_config_t      config;
    eai_fed_participant_t participants[EAI_FED_MAX_PARTICIPANTS];
    int                   participant_count;
    uint32_t              current_round;
    float                 global_loss;
    float                 cumulative_epsilon;
    float                 privacy_budget;
    eai_runtime_t        *runtime;
    char                  model_id[64];
} eai_fw_federated_t;

eai_status_t eai_fw_fed_init(eai_fw_federated_t *fed, eai_fed_role_t role,
                              const eai_fed_config_t *config);
eai_status_t eai_fw_fed_set_runtime(eai_fw_federated_t *fed, eai_runtime_t *rt);
eai_status_t eai_fw_fed_add_participant(eai_fw_federated_t *fed, const char *device_id);
eai_status_t eai_fw_fed_remove_participant(eai_fw_federated_t *fed, const char *device_id);
eai_status_t eai_fw_fed_start_round(eai_fw_federated_t *fed);
eai_status_t eai_fw_fed_submit_update(eai_fw_federated_t *fed, const char *device_id,
                                       float loss, uint32_t samples);
eai_status_t eai_fw_fed_aggregate(eai_fw_federated_t *fed);
eai_status_t eai_fw_fed_apply_privacy(eai_fw_federated_t *fed, float *weights, int count);
bool         eai_fw_fed_round_complete(const eai_fw_federated_t *fed);
bool         eai_fw_fed_training_complete(const eai_fw_federated_t *fed);
void         eai_fw_fed_report(const eai_fw_federated_t *fed);

#endif /* EAI_FW_FEDERATED_H */
