// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/sensor_fusion.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define MOD "fusion"

static uint64_t fusion_now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

static eai_fusion_group_t *find_group(eai_fw_sensor_fusion_t *sf, const char *name) {
    for (int i = 0; i < sf->group_count; i++) {
        if (strcmp(sf->groups[i].name, name) == 0) return &sf->groups[i];
    }
    return NULL;
}

static const eai_fusion_group_t *find_group_const(const eai_fw_sensor_fusion_t *sf, const char *name) {
    for (int i = 0; i < sf->group_count; i++) {
        if (strcmp(sf->groups[i].name, name) == 0) return &sf->groups[i];
    }
    return NULL;
}

static eai_fusion_source_t *find_source(eai_fusion_group_t *g, const char *name) {
    for (int i = 0; i < g->source_count; i++) {
        if (strcmp(g->sources[i].name, name) == 0) return &g->sources[i];
    }
    return NULL;
}

eai_status_t eai_fw_fusion_init(eai_fw_sensor_fusion_t *sf) {
    if (!sf) return EAI_ERR_INVALID;
    memset(sf, 0, sizeof(*sf));
    EAI_LOG_INFO(MOD, "sensor fusion engine initialized");
    return EAI_OK;
}

eai_status_t eai_fw_fusion_create_group(eai_fw_sensor_fusion_t *sf, const char *name,
                                         eai_fusion_algorithm_t algo) {
    if (!sf || !name) return EAI_ERR_INVALID;
    if (sf->group_count >= EAI_FUSION_MAX_GROUPS) return EAI_ERR_NOMEM;
    if (find_group(sf, name)) return EAI_ERR_INVALID;

    eai_fusion_group_t *g = &sf->groups[sf->group_count];
    memset(g, 0, sizeof(*g));
    strncpy(g->name, name, EAI_FUSION_NAME_MAX - 1);
    g->algorithm = algo;
    g->active = true;
    g->kalman.error_covariance = 1.0f;
    g->kalman.process_noise = 0.01f;
    g->kalman.measurement_noise = 0.1f;
    sf->group_count++;

    EAI_LOG_INFO(MOD, "created fusion group '%s'", name);
    return EAI_OK;
}

eai_status_t eai_fw_fusion_add_source(eai_fw_sensor_fusion_t *sf, const char *group,
                                       const char *source_name, float weight) {
    if (!sf || !group || !source_name) return EAI_ERR_INVALID;

    eai_fusion_group_t *g = find_group(sf, group);
    if (!g) return EAI_ERR_NOT_FOUND;
    if (g->source_count >= EAI_FUSION_MAX_SOURCES) return EAI_ERR_NOMEM;

    eai_fusion_source_t *s = &g->sources[g->source_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, source_name, EAI_FUSION_NAME_MAX - 1);
    s->weight = weight > 0.0f ? weight : 1.0f;
    s->stale_timeout_ms = 5000;
    s->status = EAI_FUSION_SRC_OK;
    s->active = true;
    g->source_count++;

    EAI_LOG_INFO(MOD, "added source '%s' to group '%s' (weight=%.2f)",
                 source_name, group, s->weight);
    return EAI_OK;
}

eai_status_t eai_fw_fusion_update_source(eai_fw_sensor_fusion_t *sf, const char *group,
                                          const char *source_name, float value) {
    if (!sf || !group || !source_name) return EAI_ERR_INVALID;

    eai_fusion_group_t *g = find_group(sf, group);
    if (!g) return EAI_ERR_NOT_FOUND;

    eai_fusion_source_t *s = find_source(g, source_name);
    if (!s) return EAI_ERR_NOT_FOUND;

    s->last_value = value;
    s->last_update_ts = fusion_now_ms();
    s->status = EAI_FUSION_SRC_OK;
    return EAI_OK;
}

static void check_stale_sources(eai_fusion_group_t *g) {
    uint64_t now = fusion_now_ms();
    for (int i = 0; i < g->source_count; i++) {
        eai_fusion_source_t *s = &g->sources[i];
        if (s->active && s->last_update_ts > 0 && s->stale_timeout_ms > 0) {
            if ((now - s->last_update_ts) > s->stale_timeout_ms) {
                s->status = EAI_FUSION_SRC_STALE;
            }
        }
    }
}

static float fuse_weighted_avg(eai_fusion_group_t *g) {
    float sum = 0.0f, weight_sum = 0.0f;
    for (int i = 0; i < g->source_count; i++) {
        if (g->sources[i].active && g->sources[i].status == EAI_FUSION_SRC_OK) {
            sum += g->sources[i].last_value * g->sources[i].weight;
            weight_sum += g->sources[i].weight;
        }
    }
    return weight_sum > 0.0f ? sum / weight_sum : 0.0f;
}

static float fuse_kalman(eai_fusion_group_t *g) {
    eai_kalman_state_t *k = &g->kalman;

    for (int i = 0; i < g->source_count; i++) {
        if (g->sources[i].active && g->sources[i].status == EAI_FUSION_SRC_OK) {
            float predicted_cov = k->error_covariance + k->process_noise;
            k->kalman_gain = predicted_cov / (predicted_cov + k->measurement_noise);
            k->estimate = k->estimate + k->kalman_gain * (g->sources[i].last_value - k->estimate);
            k->error_covariance = (1.0f - k->kalman_gain) * predicted_cov;
        }
    }
    return k->estimate;
}

static float fuse_vote(eai_fusion_group_t *g, bool require_unanimous) {
    float values[EAI_FUSION_MAX_SOURCES];
    int count = 0;

    for (int i = 0; i < g->source_count; i++) {
        if (g->sources[i].active && g->sources[i].status == EAI_FUSION_SRC_OK) {
            values[count++] = g->sources[i].last_value;
        }
    }
    if (count == 0) return 0.0f;

    /* Simple majority: find value closest to median */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (values[j] < values[i]) {
                float tmp = values[i];
                values[i] = values[j];
                values[j] = tmp;
            }
        }
    }

    if (require_unanimous) {
        float range = values[count - 1] - values[0];
        float mean = (values[0] + values[count - 1]) / 2.0f;
        if (range > fabsf(mean) * 0.1f) {
            return 0.0f;
        }
    }

    return values[count / 2];
}

eai_status_t eai_fw_fusion_fuse(eai_fw_sensor_fusion_t *sf, const char *group) {
    if (!sf || !group) return EAI_ERR_INVALID;

    eai_fusion_group_t *g = find_group(sf, group);
    if (!g) return EAI_ERR_NOT_FOUND;

    check_stale_sources(g);

    int active = eai_fw_fusion_active_sources(sf, group);
    if (active == 0) {
        g->confidence = 0.0f;
        return EAI_ERR_NOT_FOUND;
    }

    switch (g->algorithm) {
        case EAI_FUSION_WEIGHTED_AVG:
            g->fused_value = fuse_weighted_avg(g);
            break;
        case EAI_FUSION_KALMAN:
            g->fused_value = fuse_kalman(g);
            break;
        case EAI_FUSION_VOTE_MAJORITY:
            g->fused_value = fuse_vote(g, false);
            break;
        case EAI_FUSION_VOTE_UNANIMOUS:
            g->fused_value = fuse_vote(g, true);
            break;
        case EAI_FUSION_MAX_VALUE: {
            float max = -1e30f;
            for (int i = 0; i < g->source_count; i++) {
                if (g->sources[i].active && g->sources[i].status == EAI_FUSION_SRC_OK) {
                    if (g->sources[i].last_value > max) max = g->sources[i].last_value;
                }
            }
            g->fused_value = max;
            break;
        }
        case EAI_FUSION_MIN_VALUE: {
            float min = 1e30f;
            for (int i = 0; i < g->source_count; i++) {
                if (g->sources[i].active && g->sources[i].status == EAI_FUSION_SRC_OK) {
                    if (g->sources[i].last_value < min) min = g->sources[i].last_value;
                }
            }
            g->fused_value = min;
            break;
        }
    }

    g->confidence = (float)active / (float)g->source_count;
    g->last_fused_ts = fusion_now_ms();
    return EAI_OK;
}

eai_status_t eai_fw_fusion_get(const eai_fw_sensor_fusion_t *sf, const char *group,
                                float *value, float *confidence) {
    if (!sf || !group) return EAI_ERR_INVALID;

    const eai_fusion_group_t *g = find_group_const(sf, group);
    if (!g) return EAI_ERR_NOT_FOUND;

    if (value) *value = g->fused_value;
    if (confidence) *confidence = g->confidence;
    return EAI_OK;
}

eai_status_t eai_fw_fusion_set_stale_timeout(eai_fw_sensor_fusion_t *sf, const char *group,
                                              const char *source_name, uint32_t timeout_ms) {
    if (!sf || !group || !source_name) return EAI_ERR_INVALID;

    eai_fusion_group_t *g = find_group(sf, group);
    if (!g) return EAI_ERR_NOT_FOUND;

    eai_fusion_source_t *s = find_source(g, source_name);
    if (!s) return EAI_ERR_NOT_FOUND;

    s->stale_timeout_ms = timeout_ms;
    return EAI_OK;
}

int eai_fw_fusion_active_sources(const eai_fw_sensor_fusion_t *sf, const char *group) {
    if (!sf || !group) return 0;

    const eai_fusion_group_t *g = find_group_const(sf, group);
    if (!g) return 0;

    int count = 0;
    for (int i = 0; i < g->source_count; i++) {
        if (g->sources[i].active && g->sources[i].status == EAI_FUSION_SRC_OK) count++;
    }
    return count;
}

void eai_fw_fusion_report(const eai_fw_sensor_fusion_t *sf) {
    if (!sf) return;

    printf("\n=== Sensor Fusion Report ===\n");
    for (int i = 0; i < sf->group_count; i++) {
        const eai_fusion_group_t *g = &sf->groups[i];
        printf("\nGroup '%s': value=%.4f confidence=%.0f%%\n",
               g->name, g->fused_value, g->confidence * 100.0f);

        for (int j = 0; j < g->source_count; j++) {
            const eai_fusion_source_t *s = &g->sources[j];
            const char *st;
            switch (s->status) {
                case EAI_FUSION_SRC_OK:       st = "OK";       break;
                case EAI_FUSION_SRC_STALE:    st = "STALE";    break;
                case EAI_FUSION_SRC_FAILED:   st = "FAILED";   break;
                case EAI_FUSION_SRC_EXCLUDED: st = "EXCLUDED"; break;
                default:                      st = "?";        break;
            }
            printf("  %-20s val=%-10.3f weight=%-6.2f status=%s\n",
                   s->name, s->last_value, s->weight, st);
        }
    }
}
