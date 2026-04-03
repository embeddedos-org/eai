// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_SENSOR_FUSION_H
#define EAI_FW_SENSOR_FUSION_H

#include "eai/types.h"

#define EAI_FUSION_MAX_GROUPS   8
#define EAI_FUSION_MAX_SOURCES  8
#define EAI_FUSION_NAME_MAX     64

typedef enum {
    EAI_FUSION_WEIGHTED_AVG,
    EAI_FUSION_KALMAN,
    EAI_FUSION_VOTE_MAJORITY,
    EAI_FUSION_VOTE_UNANIMOUS,
    EAI_FUSION_MAX_VALUE,
    EAI_FUSION_MIN_VALUE,
} eai_fusion_algorithm_t;

typedef enum {
    EAI_FUSION_SRC_OK,
    EAI_FUSION_SRC_STALE,
    EAI_FUSION_SRC_FAILED,
    EAI_FUSION_SRC_EXCLUDED,
} eai_fusion_src_status_t;

typedef struct {
    char                    name[EAI_FUSION_NAME_MAX];
    float                   last_value;
    float                   weight;
    uint64_t                last_update_ts;
    uint32_t                stale_timeout_ms;
    eai_fusion_src_status_t status;
    bool                    active;
} eai_fusion_source_t;

typedef struct {
    float estimate;
    float error_covariance;
    float process_noise;
    float measurement_noise;
    float kalman_gain;
} eai_kalman_state_t;

typedef struct {
    char                  name[EAI_FUSION_NAME_MAX];
    eai_fusion_algorithm_t algorithm;
    eai_fusion_source_t   sources[EAI_FUSION_MAX_SOURCES];
    int                   source_count;
    eai_kalman_state_t    kalman;
    float                 fused_value;
    float                 confidence;
    uint64_t              last_fused_ts;
    bool                  active;
} eai_fusion_group_t;

typedef struct {
    eai_fusion_group_t groups[EAI_FUSION_MAX_GROUPS];
    int                group_count;
} eai_fw_sensor_fusion_t;

eai_status_t eai_fw_fusion_init(eai_fw_sensor_fusion_t *sf);
eai_status_t eai_fw_fusion_create_group(eai_fw_sensor_fusion_t *sf, const char *name,
                                         eai_fusion_algorithm_t algo);
eai_status_t eai_fw_fusion_add_source(eai_fw_sensor_fusion_t *sf, const char *group,
                                       const char *source_name, float weight);
eai_status_t eai_fw_fusion_update_source(eai_fw_sensor_fusion_t *sf, const char *group,
                                          const char *source_name, float value);
eai_status_t eai_fw_fusion_fuse(eai_fw_sensor_fusion_t *sf, const char *group);
eai_status_t eai_fw_fusion_get(const eai_fw_sensor_fusion_t *sf, const char *group,
                                float *value, float *confidence);
eai_status_t eai_fw_fusion_set_stale_timeout(eai_fw_sensor_fusion_t *sf, const char *group,
                                              const char *source_name, uint32_t timeout_ms);
int          eai_fw_fusion_active_sources(const eai_fw_sensor_fusion_t *sf, const char *group);
void         eai_fw_fusion_report(const eai_fw_sensor_fusion_t *sf);

#endif /* EAI_FW_SENSOR_FUSION_H */
