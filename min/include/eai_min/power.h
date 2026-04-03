// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_MIN_POWER_H
#define EAI_MIN_POWER_H

#include "eai/types.h"

typedef enum {
    EAI_POWER_FULL,
    EAI_POWER_ECO,
    EAI_POWER_LOW,
    EAI_POWER_CRITICAL,
} eai_power_state_t;

typedef enum {
    EAI_POWER_AC,
    EAI_POWER_BATTERY,
    EAI_POWER_SOLAR,
    EAI_POWER_UNKNOWN_SRC,
} eai_power_source_t;

typedef struct {
    float thermal_limit_c;
    float eco_threshold_pct;
    float low_threshold_pct;
    float critical_threshold_pct;
    bool  auto_throttle;
} eai_power_config_t;

typedef struct {
    eai_power_state_t  state;
    eai_power_source_t source;
    eai_power_config_t config;
    float              battery_pct;
    float              cpu_temp_c;
    uint32_t           max_tokens_override;
    float              temperature_override;
    uint32_t           inference_budget_remaining;
    uint64_t           last_check_ts;
    bool               throttled;
    bool               initialized;
} eai_min_power_t;

eai_status_t      eai_min_power_init(eai_min_power_t *pwr, const eai_power_config_t *cfg);
eai_status_t      eai_min_power_update(eai_min_power_t *pwr);
eai_power_state_t eai_min_power_state(const eai_min_power_t *pwr);
eai_status_t      eai_min_power_set_battery(eai_min_power_t *pwr, float pct,
                                             eai_power_source_t source);
uint32_t          eai_min_power_max_tokens(const eai_min_power_t *pwr);
float             eai_min_power_temperature(const eai_min_power_t *pwr);
bool              eai_min_power_should_infer(const eai_min_power_t *pwr);
eai_status_t      eai_min_power_set_cpu_temp(eai_min_power_t *pwr, float temp_c);
void              eai_min_power_report(const eai_min_power_t *pwr);

#endif /* EAI_MIN_POWER_H */
