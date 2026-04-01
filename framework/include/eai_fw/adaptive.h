// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_ADAPTIVE_H
#define EAI_FW_ADAPTIVE_H

#include "eai/types.h"
#include "eai/adaptive.h"
#include "eai/runtime_contract.h"
#include "eai_fw/policy.h"

typedef struct {
    eai_preference_store_t *prefs;
    eai_feedback_buffer_t  *feedback;
    eai_training_buffer_t  *training;
    eai_runtime_t          *runtime;
    eai_fw_policy_t        *policy;
    eai_adaptive_config_t   config;
    uint32_t                cycles_run;
    float                   last_loss;
} eai_fw_adaptive_t;

eai_status_t eai_fw_adaptive_init(eai_fw_adaptive_t *adaptive,
                                   eai_runtime_t *runtime,
                                   eai_fw_policy_t *policy);
void         eai_fw_adaptive_shutdown(eai_fw_adaptive_t *adaptive);

bool         eai_fw_adaptive_should_train(const eai_fw_adaptive_t *adaptive,
                                           uint32_t available_ram_mb,
                                           float cpu_temp_c);
eai_status_t eai_fw_adaptive_run_training_cycle(eai_fw_adaptive_t *adaptive);
eai_status_t eai_fw_adaptive_update_preferences(eai_fw_adaptive_t *adaptive, float elapsed_days);
eai_status_t eai_fw_adaptive_record_feedback(eai_fw_adaptive_t *adaptive,
                                              float score, const char *context,
                                              const char *input, const char *output);

#endif /* EAI_FW_ADAPTIVE_H */
