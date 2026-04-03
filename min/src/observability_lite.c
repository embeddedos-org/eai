// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/observability_lite.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "obs-lite"

static uint64_t obs_now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

eai_status_t eai_min_obs_init(eai_min_obs_lite_t *obs) {
    if (!obs) return EAI_ERR_INVALID;
    memset(obs, 0, sizeof(*obs));
    obs->start_time_ms = obs_now_ms();
    obs->inference_latency.min_us = UINT64_MAX;
    obs->initialized = true;
    EAI_LOG_INFO(MOD, "observability lite initialized");
    return EAI_OK;
}

eai_status_t eai_min_obs_counter_inc(eai_min_obs_lite_t *obs, const char *name, uint64_t val) {
    if (!obs || !name) return EAI_ERR_INVALID;

    for (int i = 0; i < obs->counter_count; i++) {
        if (strcmp(obs->counters[i].name, name) == 0) {
            obs->counters[i].value += val;
            return EAI_OK;
        }
    }

    if (obs->counter_count >= EAI_OBS_LITE_MAX_COUNTERS) return EAI_ERR_NOMEM;

    strncpy(obs->counters[obs->counter_count].name, name, 63);
    obs->counters[obs->counter_count].name[63] = '\0';
    obs->counters[obs->counter_count].value = val;
    obs->counter_count++;
    return EAI_OK;
}

uint64_t eai_min_obs_counter_get(const eai_min_obs_lite_t *obs, const char *name) {
    if (!obs || !name) return 0;

    for (int i = 0; i < obs->counter_count; i++) {
        if (strcmp(obs->counters[i].name, name) == 0) {
            return obs->counters[i].value;
        }
    }
    return 0;
}

eai_status_t eai_min_obs_record_latency(eai_min_obs_lite_t *obs, uint64_t latency_us) {
    if (!obs) return EAI_ERR_INVALID;

    eai_latency_tracker_t *lt = &obs->inference_latency;
    lt->last_us = latency_us;
    lt->total_us += latency_us;
    lt->sample_count++;
    lt->avg_us = lt->total_us / lt->sample_count;
    if (latency_us > lt->max_us) lt->max_us = latency_us;
    if (latency_us < lt->min_us) lt->min_us = latency_us;

    eai_min_obs_counter_inc(obs, "inference_count", 1);
    return EAI_OK;
}

eai_status_t eai_min_obs_update_system(eai_min_obs_lite_t *obs) {
    if (!obs) return EAI_ERR_INVALID;

    /* Platform-specific memory and temperature would be fetched here.
       For portability, we leave values at their last-set state. */
    EAI_LOG_TRACE(MOD, "system metrics updated");
    return EAI_OK;
}

eai_health_status_t eai_min_obs_health_check(const eai_min_obs_lite_t *obs) {
    if (!obs || !obs->initialized) return EAI_HEALTH_UNKNOWN;

    if (obs->cpu_temp_c > 90.0f) return EAI_HEALTH_CRITICAL;

    if (obs->memory_total_bytes > 0) {
        float usage_pct = (float)obs->memory_used_bytes / (float)obs->memory_total_bytes * 100.0f;
        if (usage_pct > 95.0f) return EAI_HEALTH_CRITICAL;
        if (usage_pct > 80.0f) return EAI_HEALTH_DEGRADED;
    }

    if (obs->cpu_temp_c > 75.0f) return EAI_HEALTH_DEGRADED;

    uint64_t error_count = eai_min_obs_counter_get(obs, "error_count");
    uint64_t infer_count = eai_min_obs_counter_get(obs, "inference_count");
    if (infer_count > 10 && error_count * 10 > infer_count) {
        return EAI_HEALTH_DEGRADED;
    }

    return EAI_HEALTH_OK;
}

uint64_t eai_min_obs_uptime_ms(const eai_min_obs_lite_t *obs) {
    if (!obs || !obs->initialized) return 0;
    return obs_now_ms() - obs->start_time_ms;
}

void eai_min_obs_dump(const eai_min_obs_lite_t *obs) {
    if (!obs) return;

    printf("\n=== Observability Lite Report ===\n");
    printf("Uptime: %llu ms\n", (unsigned long long)eai_min_obs_uptime_ms(obs));

    const eai_latency_tracker_t *lt = &obs->inference_latency;
    printf("\nInference Latency:\n");
    printf("  Last: %llu us | Avg: %llu us | Min: %llu us | Max: %llu us | Samples: %u\n",
           (unsigned long long)lt->last_us,
           (unsigned long long)lt->avg_us,
           (unsigned long long)(lt->min_us == UINT64_MAX ? 0 : lt->min_us),
           (unsigned long long)lt->max_us,
           lt->sample_count);

    printf("\nSystem:\n");
    printf("  Memory: %llu / %llu bytes (%.1f%%)\n",
           (unsigned long long)obs->memory_used_bytes,
           (unsigned long long)obs->memory_total_bytes,
           obs->memory_total_bytes > 0 ?
               (float)obs->memory_used_bytes / (float)obs->memory_total_bytes * 100.0f : 0.0f);
    printf("  CPU Temp: %.1f C\n", obs->cpu_temp_c);

    const char *health_str;
    switch (eai_min_obs_health_check(obs)) {
        case EAI_HEALTH_OK:       health_str = "OK";       break;
        case EAI_HEALTH_DEGRADED: health_str = "DEGRADED"; break;
        case EAI_HEALTH_CRITICAL: health_str = "CRITICAL"; break;
        default:                  health_str = "UNKNOWN";   break;
    }
    printf("  Health: %s\n", health_str);

    printf("\nCounters (%d):\n", obs->counter_count);
    for (int i = 0; i < obs->counter_count; i++) {
        printf("  %-32s %llu\n", obs->counters[i].name,
               (unsigned long long)obs->counters[i].value);
    }
}
