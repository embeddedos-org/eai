// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/federated.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MOD "federated"

eai_status_t eai_fw_fed_init(eai_fw_federated_t *fed, eai_fed_role_t role,
                              const eai_fed_config_t *config) {
    if (!fed) return EAI_ERR_INVALID;
    memset(fed, 0, sizeof(*fed));
    fed->role = role;
    fed->state = EAI_FED_IDLE;

    if (config) {
        fed->config = *config;
    } else {
        fed->config.learning_rate = 0.001f;
        fed->config.local_epochs = 5;
        fed->config.min_participants = 2;
        fed->config.max_rounds = 100;
        fed->config.target_loss = 0.01f;
        fed->config.privacy.epsilon = 1.0f;
        fed->config.privacy.delta = 1e-5f;
        fed->config.privacy.enabled = false;
    }

    fed->privacy_budget = fed->config.privacy.epsilon * 10.0f;
    fed->global_loss = 1.0f;

    EAI_LOG_INFO(MOD, "federated learning initialized (role=%s, min_participants=%u)",
                 role == EAI_FED_COORDINATOR ? "coordinator" : "participant",
                 fed->config.min_participants);
    return EAI_OK;
}

eai_status_t eai_fw_fed_set_runtime(eai_fw_federated_t *fed, eai_runtime_t *rt) {
    if (!fed || !rt) return EAI_ERR_INVALID;
    fed->runtime = rt;
    return EAI_OK;
}

eai_status_t eai_fw_fed_add_participant(eai_fw_federated_t *fed, const char *device_id) {
    if (!fed || !device_id) return EAI_ERR_INVALID;
    if (fed->participant_count >= EAI_FED_MAX_PARTICIPANTS) return EAI_ERR_NOMEM;

    for (int i = 0; i < fed->participant_count; i++) {
        if (strcmp(fed->participants[i].device_id, device_id) == 0) {
            EAI_LOG_WARN(MOD, "participant '%s' already registered", device_id);
            return EAI_ERR_INVALID;
        }
    }

    eai_fed_participant_t *p = &fed->participants[fed->participant_count];
    memset(p, 0, sizeof(*p));
    strncpy(p->device_id, device_id, EAI_FED_DEVICE_ID_MAX - 1);
    p->device_id[EAI_FED_DEVICE_ID_MAX - 1] = '\0';
    p->active = true;
    fed->participant_count++;

    EAI_LOG_INFO(MOD, "added participant: %s (total: %d)", device_id, fed->participant_count);
    return EAI_OK;
}

eai_status_t eai_fw_fed_remove_participant(eai_fw_federated_t *fed, const char *device_id) {
    if (!fed || !device_id) return EAI_ERR_INVALID;

    for (int i = 0; i < fed->participant_count; i++) {
        if (strcmp(fed->participants[i].device_id, device_id) == 0) {
            fed->participants[i].active = false;
            EAI_LOG_INFO(MOD, "removed participant: %s", device_id);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_fed_start_round(eai_fw_federated_t *fed) {
    if (!fed) return EAI_ERR_INVALID;

    int active = 0;
    for (int i = 0; i < fed->participant_count; i++) {
        if (fed->participants[i].active) active++;
    }

    if ((uint32_t)active < fed->config.min_participants) {
        EAI_LOG_ERROR(MOD, "not enough participants: %d < %u",
                      active, fed->config.min_participants);
        return EAI_ERR_INVALID;
    }

    fed->current_round++;
    fed->state = EAI_FED_DISTRIBUTING;

    for (int i = 0; i < fed->participant_count; i++) {
        if (fed->participants[i].active) {
            fed->participants[i].submitted = false;
            fed->participants[i].round_number = fed->current_round;
        }
    }

    EAI_LOG_INFO(MOD, "starting round %u with %d participants",
                 fed->current_round, active);
    fed->state = EAI_FED_TRAINING;
    return EAI_OK;
}

eai_status_t eai_fw_fed_submit_update(eai_fw_federated_t *fed, const char *device_id,
                                       float loss, uint32_t samples) {
    if (!fed || !device_id) return EAI_ERR_INVALID;

    for (int i = 0; i < fed->participant_count; i++) {
        if (strcmp(fed->participants[i].device_id, device_id) == 0 &&
            fed->participants[i].active) {
            fed->participants[i].local_loss = loss;
            fed->participants[i].samples_trained = samples;
            fed->participants[i].submitted = true;
            EAI_LOG_INFO(MOD, "received update from %s: loss=%.4f samples=%u",
                         device_id, loss, samples);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_fed_aggregate(eai_fw_federated_t *fed) {
    if (!fed) return EAI_ERR_INVALID;

    fed->state = EAI_FED_AGGREGATING;

    float total_loss = 0.0f;
    uint32_t total_samples = 0;
    int submitted = 0;

    for (int i = 0; i < fed->participant_count; i++) {
        if (fed->participants[i].submitted) {
            total_loss += fed->participants[i].local_loss * fed->participants[i].samples_trained;
            total_samples += fed->participants[i].samples_trained;
            submitted++;
        }
    }

    if (submitted == 0 || total_samples == 0) {
        EAI_LOG_ERROR(MOD, "no updates to aggregate");
        fed->state = EAI_FED_ERROR;
        return EAI_ERR_INVALID;
    }

    fed->global_loss = total_loss / (float)total_samples;

    if (fed->config.privacy.enabled) {
        fed->cumulative_epsilon += fed->config.privacy.epsilon;
        EAI_LOG_INFO(MOD, "privacy budget: used %.2f / %.2f epsilon",
                     fed->cumulative_epsilon, fed->privacy_budget);
    }

    fed->state = EAI_FED_COMPLETE;
    EAI_LOG_INFO(MOD, "round %u aggregated: global_loss=%.4f (%d participants, %u samples)",
                 fed->current_round, fed->global_loss, submitted, total_samples);
    return EAI_OK;
}

eai_status_t eai_fw_fed_apply_privacy(eai_fw_federated_t *fed, float *weights, int count) {
    if (!fed || !weights || count <= 0) return EAI_ERR_INVALID;
    if (!fed->config.privacy.enabled) return EAI_OK;

    if (fed->cumulative_epsilon >= fed->privacy_budget) {
        EAI_LOG_ERROR(MOD, "privacy budget exhausted");
        return EAI_ERR_RESOURCE_BUDGET;
    }

    float sensitivity = 1.0f;
    float noise_scale = sensitivity / fed->config.privacy.epsilon;

    for (int i = 0; i < count; i++) {
        float u1 = ((float)(rand() % 10000) + 1.0f) / 10001.0f;
        float u2 = ((float)(rand() % 10000) + 1.0f) / 10001.0f;
        float gaussian = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
        weights[i] += gaussian * noise_scale;
    }

    EAI_LOG_DEBUG(MOD, "applied differential privacy noise (epsilon=%.2f, scale=%.4f)",
                  fed->config.privacy.epsilon, noise_scale);
    return EAI_OK;
}

bool eai_fw_fed_round_complete(const eai_fw_federated_t *fed) {
    if (!fed) return false;

    for (int i = 0; i < fed->participant_count; i++) {
        if (fed->participants[i].active && !fed->participants[i].submitted) {
            return false;
        }
    }
    return true;
}

bool eai_fw_fed_training_complete(const eai_fw_federated_t *fed) {
    if (!fed) return false;
    if (fed->current_round >= fed->config.max_rounds) return true;
    if (fed->global_loss <= fed->config.target_loss) return true;
    if (fed->config.privacy.enabled && fed->cumulative_epsilon >= fed->privacy_budget) return true;
    return false;
}

void eai_fw_fed_report(const eai_fw_federated_t *fed) {
    if (!fed) return;

    const char *state_str;
    switch (fed->state) {
        case EAI_FED_IDLE:         state_str = "IDLE";         break;
        case EAI_FED_PLANNING:     state_str = "PLANNING";     break;
        case EAI_FED_DISTRIBUTING: state_str = "DISTRIBUTING"; break;
        case EAI_FED_TRAINING:     state_str = "TRAINING";     break;
        case EAI_FED_COLLECTING:   state_str = "COLLECTING";   break;
        case EAI_FED_AGGREGATING:  state_str = "AGGREGATING";  break;
        case EAI_FED_COMPLETE:     state_str = "COMPLETE";     break;
        case EAI_FED_ERROR:        state_str = "ERROR";        break;
        default:                   state_str = "UNKNOWN";       break;
    }

    printf("\n=== Federated Learning Report ===\n");
    printf("Role:        %s\n", fed->role == EAI_FED_COORDINATOR ? "COORDINATOR" : "PARTICIPANT");
    printf("State:       %s\n", state_str);
    printf("Round:       %u / %u\n", fed->current_round, fed->config.max_rounds);
    printf("Global loss: %.6f (target: %.6f)\n", fed->global_loss, fed->config.target_loss);

    if (fed->config.privacy.enabled) {
        printf("Privacy:     epsilon=%.2f/%.2f delta=%.1e\n",
               fed->cumulative_epsilon, fed->privacy_budget, (double)fed->config.privacy.delta);
    }

    printf("\nParticipants (%d):\n", fed->participant_count);
    printf("%-20s %-8s %-10s %-10s %s\n", "Device", "Active", "Submitted", "Loss", "Samples");
    for (int i = 0; i < fed->participant_count; i++) {
        const eai_fed_participant_t *p = &fed->participants[i];
        printf("%-20s %-8s %-10s %-10.4f %u\n",
               p->device_id,
               p->active ? "yes" : "no",
               p->submitted ? "yes" : "no",
               p->local_loss,
               p->samples_trained);
    }
}
