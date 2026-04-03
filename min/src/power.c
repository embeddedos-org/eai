// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/power.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>

#define MOD "power"

static const char *power_state_str(eai_power_state_t st) {
    switch (st) {
        case EAI_POWER_FULL:     return "FULL_POWER";
        case EAI_POWER_ECO:      return "ECO";
        case EAI_POWER_LOW:      return "LOW_POWER";
        case EAI_POWER_CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

static const char *power_source_str(eai_power_source_t src) {
    switch (src) {
        case EAI_POWER_AC:          return "AC";
        case EAI_POWER_BATTERY:     return "BATTERY";
        case EAI_POWER_SOLAR:       return "SOLAR";
        case EAI_POWER_UNKNOWN_SRC: return "UNKNOWN";
        default:                    return "?";
    }
}

eai_status_t eai_min_power_init(eai_min_power_t *pwr, const eai_power_config_t *cfg) {
    if (!pwr) return EAI_ERR_INVALID;
    memset(pwr, 0, sizeof(*pwr));

    if (cfg) {
        pwr->config = *cfg;
    } else {
        pwr->config.thermal_limit_c = 80.0f;
        pwr->config.eco_threshold_pct = 50.0f;
        pwr->config.low_threshold_pct = 20.0f;
        pwr->config.critical_threshold_pct = 5.0f;
        pwr->config.auto_throttle = true;
    }

    pwr->state = EAI_POWER_FULL;
    pwr->source = EAI_POWER_UNKNOWN_SRC;
    pwr->battery_pct = 100.0f;
    pwr->cpu_temp_c = 40.0f;
    pwr->max_tokens_override = 256;
    pwr->temperature_override = 0.7f;
    pwr->inference_budget_remaining = UINT32_MAX;
    pwr->throttled = false;
    pwr->initialized = true;

    EAI_LOG_INFO(MOD, "power manager initialized (thermal_limit=%.0fC, auto_throttle=%s)",
                 pwr->config.thermal_limit_c,
                 pwr->config.auto_throttle ? "on" : "off");
    return EAI_OK;
}

eai_status_t eai_min_power_update(eai_min_power_t *pwr) {
    if (!pwr || !pwr->initialized) return EAI_ERR_INVALID;

    eai_power_state_t prev_state = pwr->state;

    /* Determine power state from battery */
    if (pwr->source == EAI_POWER_AC) {
        pwr->state = EAI_POWER_FULL;
    } else if (pwr->battery_pct <= pwr->config.critical_threshold_pct) {
        pwr->state = EAI_POWER_CRITICAL;
    } else if (pwr->battery_pct <= pwr->config.low_threshold_pct) {
        pwr->state = EAI_POWER_LOW;
    } else if (pwr->battery_pct <= pwr->config.eco_threshold_pct) {
        pwr->state = EAI_POWER_ECO;
    } else {
        pwr->state = EAI_POWER_FULL;
    }

    /* Thermal throttling override */
    if (pwr->config.auto_throttle && pwr->cpu_temp_c >= pwr->config.thermal_limit_c) {
        if (pwr->state < EAI_POWER_LOW) {
            pwr->state = EAI_POWER_LOW;
        }
        pwr->throttled = true;
        EAI_LOG_WARN(MOD, "thermal throttling active (%.1fC >= %.1fC limit)",
                     pwr->cpu_temp_c, pwr->config.thermal_limit_c);
    } else {
        pwr->throttled = false;
    }

    /* Adjust inference parameters per power state */
    switch (pwr->state) {
        case EAI_POWER_FULL:
            pwr->max_tokens_override = 256;
            pwr->temperature_override = 0.7f;
            pwr->inference_budget_remaining = UINT32_MAX;
            break;
        case EAI_POWER_ECO:
            pwr->max_tokens_override = 128;
            pwr->temperature_override = 0.5f;
            pwr->inference_budget_remaining = 1000;
            break;
        case EAI_POWER_LOW:
            pwr->max_tokens_override = 64;
            pwr->temperature_override = 0.3f;
            pwr->inference_budget_remaining = 100;
            break;
        case EAI_POWER_CRITICAL:
            pwr->max_tokens_override = 32;
            pwr->temperature_override = 0.1f;
            pwr->inference_budget_remaining = 10;
            break;
    }

    if (pwr->state != prev_state) {
        EAI_LOG_INFO(MOD, "power state: %s -> %s (battery=%.0f%%, temp=%.1fC)",
                     power_state_str(prev_state), power_state_str(pwr->state),
                     pwr->battery_pct, pwr->cpu_temp_c);
    }

    return EAI_OK;
}

eai_power_state_t eai_min_power_state(const eai_min_power_t *pwr) {
    if (!pwr) return EAI_POWER_CRITICAL;
    return pwr->state;
}

eai_status_t eai_min_power_set_battery(eai_min_power_t *pwr, float pct,
                                        eai_power_source_t source) {
    if (!pwr) return EAI_ERR_INVALID;
    pwr->battery_pct = pct < 0.0f ? 0.0f : (pct > 100.0f ? 100.0f : pct);
    pwr->source = source;
    return eai_min_power_update(pwr);
}

uint32_t eai_min_power_max_tokens(const eai_min_power_t *pwr) {
    if (!pwr) return 32;
    return pwr->max_tokens_override;
}

float eai_min_power_temperature(const eai_min_power_t *pwr) {
    if (!pwr) return 0.1f;
    return pwr->temperature_override;
}

bool eai_min_power_should_infer(const eai_min_power_t *pwr) {
    if (!pwr || !pwr->initialized) return false;
    if (pwr->state == EAI_POWER_CRITICAL && pwr->inference_budget_remaining == 0) {
        EAI_LOG_WARN(MOD, "inference blocked: critical power, no budget remaining");
        return false;
    }
    return true;
}

eai_status_t eai_min_power_set_cpu_temp(eai_min_power_t *pwr, float temp_c) {
    if (!pwr) return EAI_ERR_INVALID;
    pwr->cpu_temp_c = temp_c;
    return eai_min_power_update(pwr);
}

void eai_min_power_report(const eai_min_power_t *pwr) {
    if (!pwr) return;

    printf("\n=== Power Manager Report ===\n");
    printf("State:      %s%s\n", power_state_str(pwr->state),
           pwr->throttled ? " [THROTTLED]" : "");
    printf("Source:     %s\n", power_source_str(pwr->source));
    printf("Battery:    %.0f%%\n", pwr->battery_pct);
    printf("CPU Temp:   %.1f C (limit: %.1f C)\n",
           pwr->cpu_temp_c, pwr->config.thermal_limit_c);
    printf("Max tokens: %u\n", pwr->max_tokens_override);
    printf("LLM temp:   %.2f\n", pwr->temperature_override);
    printf("Budget:     %u inferences remaining\n", pwr->inference_budget_remaining);
}
