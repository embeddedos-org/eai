// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/sensor.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "sensor"

static uint64_t sensor_now_ms(void) {
    return (uint64_t)time(NULL) * 1000;
}

static const char *sensor_type_str(eai_sensor_type_t type) {
    switch (type) {
        case EAI_SENSOR_TEMPERATURE: return "temperature";
        case EAI_SENSOR_PRESSURE:    return "pressure";
        case EAI_SENSOR_HUMIDITY:    return "humidity";
        case EAI_SENSOR_IMU_ACCEL:   return "imu_accel";
        case EAI_SENSOR_IMU_GYRO:    return "imu_gyro";
        case EAI_SENSOR_IMU_MAG:     return "imu_mag";
        case EAI_SENSOR_CAMERA:      return "camera";
        case EAI_SENSOR_LIDAR:       return "lidar";
        case EAI_SENSOR_PROXIMITY:   return "proximity";
        case EAI_SENSOR_LIGHT:       return "light";
        case EAI_SENSOR_GAS:         return "gas";
        case EAI_SENSOR_CURRENT:     return "current";
        case EAI_SENSOR_VOLTAGE:     return "voltage";
        case EAI_SENSOR_CUSTOM:      return "custom";
        default:                     return "unknown";
    }
}

eai_status_t eai_sensor_registry_init(eai_sensor_registry_t *reg) {
    if (!reg) return EAI_ERR_INVALID;
    memset(reg, 0, sizeof(*reg));
    EAI_LOG_INFO(MOD, "sensor registry initialized");
    return EAI_OK;
}

eai_status_t eai_sensor_register(eai_sensor_registry_t *reg, const char *name,
                                  eai_sensor_type_t type, eai_sensor_read_fn read_fn,
                                  void *user_data) {
    if (!reg || !name || !read_fn) return EAI_ERR_INVALID;
    if (reg->count >= EAI_SENSOR_MAX) return EAI_ERR_NOMEM;

    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->sensors[i].name, name) == 0) {
            EAI_LOG_WARN(MOD, "sensor '%s' already registered", name);
            return EAI_ERR_INVALID;
        }
    }

    eai_sensor_t *s = &reg->sensors[reg->count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, EAI_SENSOR_NAME_MAX - 1);
    s->name[EAI_SENSOR_NAME_MAX - 1] = '\0';
    s->type = type;
    s->status = EAI_SENSOR_OK;
    s->read_fn = read_fn;
    s->user_data = user_data;
    s->calibration.offset = 0.0f;
    s->calibration.scale = 1.0f;
    s->calibration.min_valid = -1e9f;
    s->calibration.max_valid = 1e9f;
    s->stale_timeout_ms = 5000;
    s->active = true;
    reg->count++;

    EAI_LOG_INFO(MOD, "registered sensor '%s' type=%s", name, sensor_type_str(type));
    return EAI_OK;
}

eai_status_t eai_sensor_calibrate(eai_sensor_registry_t *reg, const char *name,
                                   const eai_sensor_calibration_t *cal) {
    if (!reg || !name || !cal) return EAI_ERR_INVALID;

    eai_sensor_t *s = eai_sensor_find(reg, name);
    if (!s) return EAI_ERR_NOT_FOUND;

    s->calibration = *cal;
    EAI_LOG_INFO(MOD, "calibrated sensor '%s': offset=%.3f scale=%.3f range=[%.1f, %.1f]",
                 name, cal->offset, cal->scale, cal->min_valid, cal->max_valid);
    return EAI_OK;
}

static float apply_calibration(const eai_sensor_calibration_t *cal, float raw) {
    float calibrated = (raw + cal->offset) * cal->scale;
    if (calibrated < cal->min_valid) calibrated = cal->min_valid;
    if (calibrated > cal->max_valid) calibrated = cal->max_valid;
    return calibrated;
}

static void update_filter(eai_sensor_t *s, float value) {
    s->filter_buf[s->filter_idx] = value;
    s->filter_idx = (s->filter_idx + 1) % EAI_SENSOR_FILTER_WINDOW;
    if (s->filter_count < EAI_SENSOR_FILTER_WINDOW) s->filter_count++;

    float sum = 0.0f;
    for (int i = 0; i < s->filter_count; i++) {
        sum += s->filter_buf[i];
    }
    s->filtered_value = sum / (float)s->filter_count;
}

eai_status_t eai_sensor_read(eai_sensor_registry_t *reg, const char *name, float *value) {
    if (!reg || !name || !value) return EAI_ERR_INVALID;

    eai_sensor_t *s = eai_sensor_find(reg, name);
    if (!s) return EAI_ERR_NOT_FOUND;
    if (!s->active) return EAI_ERR_UNSUPPORTED;

    float raw = 0.0f;
    eai_status_t st = s->read_fn(0, &raw, s->user_data);
    if (st != EAI_OK) {
        s->status = EAI_SENSOR_FAULT;
        return st;
    }

    float calibrated = apply_calibration(&s->calibration, raw);
    s->last_value = calibrated;
    s->last_read_ts = sensor_now_ms();
    s->status = EAI_SENSOR_OK;

    update_filter(s, calibrated);

    *value = calibrated;
    return EAI_OK;
}

eai_status_t eai_sensor_read_filtered(eai_sensor_registry_t *reg, const char *name, float *value) {
    if (!reg || !name || !value) return EAI_ERR_INVALID;

    eai_sensor_t *s = eai_sensor_find(reg, name);
    if (!s) return EAI_ERR_NOT_FOUND;

    if (s->filter_count == 0) {
        return eai_sensor_read(reg, name, value);
    }

    uint64_t now = sensor_now_ms();
    if (s->stale_timeout_ms > 0 && s->last_read_ts > 0 &&
        (now - s->last_read_ts) > s->stale_timeout_ms) {
        s->status = EAI_SENSOR_STALE;
        EAI_LOG_WARN(MOD, "sensor '%s' data is stale", name);
    }

    *value = s->filtered_value;
    return EAI_OK;
}

eai_status_t eai_sensor_set_stale_timeout(eai_sensor_registry_t *reg, const char *name,
                                           uint32_t timeout_ms) {
    if (!reg || !name) return EAI_ERR_INVALID;

    eai_sensor_t *s = eai_sensor_find(reg, name);
    if (!s) return EAI_ERR_NOT_FOUND;

    s->stale_timeout_ms = timeout_ms;
    return EAI_OK;
}

eai_sensor_t *eai_sensor_find(eai_sensor_registry_t *reg, const char *name) {
    if (!reg || !name) return NULL;

    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->sensors[i].name, name) == 0) {
            return &reg->sensors[i];
        }
    }
    return NULL;
}

int eai_sensor_count_active(const eai_sensor_registry_t *reg) {
    if (!reg) return 0;
    int count = 0;
    for (int i = 0; i < reg->count; i++) {
        if (reg->sensors[i].active) count++;
    }
    return count;
}

void eai_sensor_list(const eai_sensor_registry_t *reg) {
    if (!reg) return;

    printf("\n=== Sensor Registry (%d sensors) ===\n", reg->count);
    printf("%-20s %-14s %-10s %-12s %s\n", "Name", "Type", "Status", "Last Value", "Filtered");
    printf("-----------------------------------------------------------------------\n");

    for (int i = 0; i < reg->count; i++) {
        const eai_sensor_t *s = &reg->sensors[i];
        const char *status_str;
        switch (s->status) {
            case EAI_SENSOR_OK:      status_str = "OK";      break;
            case EAI_SENSOR_STALE:   status_str = "STALE";   break;
            case EAI_SENSOR_FAULT:   status_str = "FAULT";   break;
            case EAI_SENSOR_OFFLINE: status_str = "OFFLINE"; break;
            default:                 status_str = "?";        break;
        }
        printf("%-20s %-14s %-10s %-12.3f %.3f\n",
               s->name, sensor_type_str(s->type), status_str,
               s->last_value, s->filtered_value);
    }
}
