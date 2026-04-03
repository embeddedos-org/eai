// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_MIN_OBSERVABILITY_LITE_H
#define EAI_MIN_OBSERVABILITY_LITE_H

#include "eai/types.h"

#define EAI_OBS_LITE_MAX_COUNTERS 32

typedef enum {
    EAI_HEALTH_OK,
    EAI_HEALTH_DEGRADED,
    EAI_HEALTH_CRITICAL,
    EAI_HEALTH_UNKNOWN,
} eai_health_status_t;

typedef struct {
    char     name[64];
    uint64_t value;
} eai_lite_counter_t;

typedef struct {
    uint64_t last_us;
    uint64_t avg_us;
    uint64_t max_us;
    uint64_t min_us;
    uint64_t total_us;
    uint32_t sample_count;
} eai_latency_tracker_t;

typedef struct {
    eai_lite_counter_t   counters[EAI_OBS_LITE_MAX_COUNTERS];
    int                  counter_count;
    eai_latency_tracker_t inference_latency;
    uint64_t             start_time_ms;
    uint64_t             memory_used_bytes;
    uint64_t             memory_total_bytes;
    float                cpu_temp_c;
    bool                 initialized;
} eai_min_obs_lite_t;

eai_status_t       eai_min_obs_init(eai_min_obs_lite_t *obs);
eai_status_t       eai_min_obs_counter_inc(eai_min_obs_lite_t *obs, const char *name, uint64_t val);
uint64_t           eai_min_obs_counter_get(const eai_min_obs_lite_t *obs, const char *name);
eai_status_t       eai_min_obs_record_latency(eai_min_obs_lite_t *obs, uint64_t latency_us);
eai_status_t       eai_min_obs_update_system(eai_min_obs_lite_t *obs);
eai_health_status_t eai_min_obs_health_check(const eai_min_obs_lite_t *obs);
uint64_t           eai_min_obs_uptime_ms(const eai_min_obs_lite_t *obs);
void               eai_min_obs_dump(const eai_min_obs_lite_t *obs);

#endif /* EAI_MIN_OBSERVABILITY_LITE_H */
