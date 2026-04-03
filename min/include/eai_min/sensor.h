// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_MIN_SENSOR_H
#define EAI_MIN_SENSOR_H

#include "eai/types.h"

#define EAI_SENSOR_MAX           32
#define EAI_SENSOR_NAME_MAX      64
#define EAI_SENSOR_FILTER_WINDOW 8

typedef enum {
    EAI_SENSOR_TEMPERATURE,
    EAI_SENSOR_PRESSURE,
    EAI_SENSOR_HUMIDITY,
    EAI_SENSOR_IMU_ACCEL,
    EAI_SENSOR_IMU_GYRO,
    EAI_SENSOR_IMU_MAG,
    EAI_SENSOR_CAMERA,
    EAI_SENSOR_LIDAR,
    EAI_SENSOR_PROXIMITY,
    EAI_SENSOR_LIGHT,
    EAI_SENSOR_GAS,
    EAI_SENSOR_CURRENT,
    EAI_SENSOR_VOLTAGE,
    EAI_SENSOR_CUSTOM,
} eai_sensor_type_t;

typedef enum {
    EAI_SENSOR_OK,
    EAI_SENSOR_STALE,
    EAI_SENSOR_FAULT,
    EAI_SENSOR_OFFLINE,
} eai_sensor_status_t;

typedef struct {
    float  offset;
    float  scale;
    float  min_valid;
    float  max_valid;
} eai_sensor_calibration_t;

typedef eai_status_t (*eai_sensor_read_fn)(int sensor_id, float *value, void *user_data);

typedef struct {
    char                    name[EAI_SENSOR_NAME_MAX];
    eai_sensor_type_t       type;
    eai_sensor_status_t     status;
    eai_sensor_calibration_t calibration;
    eai_sensor_read_fn       read_fn;
    void                    *user_data;
    float                   last_value;
    float                   filtered_value;
    float                   filter_buf[EAI_SENSOR_FILTER_WINDOW];
    int                     filter_idx;
    int                     filter_count;
    uint64_t                last_read_ts;
    uint32_t                stale_timeout_ms;
    bool                    active;
} eai_sensor_t;

typedef struct {
    eai_sensor_t sensors[EAI_SENSOR_MAX];
    int          count;
} eai_sensor_registry_t;

eai_status_t eai_sensor_registry_init(eai_sensor_registry_t *reg);
eai_status_t eai_sensor_register(eai_sensor_registry_t *reg, const char *name,
                                  eai_sensor_type_t type, eai_sensor_read_fn read_fn,
                                  void *user_data);
eai_status_t eai_sensor_calibrate(eai_sensor_registry_t *reg, const char *name,
                                   const eai_sensor_calibration_t *cal);
eai_status_t eai_sensor_read(eai_sensor_registry_t *reg, const char *name, float *value);
eai_status_t eai_sensor_read_filtered(eai_sensor_registry_t *reg, const char *name, float *value);
eai_status_t eai_sensor_set_stale_timeout(eai_sensor_registry_t *reg, const char *name,
                                           uint32_t timeout_ms);
eai_sensor_t *eai_sensor_find(eai_sensor_registry_t *reg, const char *name);
int           eai_sensor_count_active(const eai_sensor_registry_t *reg);
void          eai_sensor_list(const eai_sensor_registry_t *reg);

#endif /* EAI_MIN_SENSOR_H */
