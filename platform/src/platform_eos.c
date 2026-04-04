// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai/platform.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(EAI_PLATFORM_EOS_ENABLED)

static eai_status_t eos_init(eai_platform_t *plat) {
    (void)plat;
    return EAI_OK;
}

static eai_status_t eos_get_device_info(eai_platform_t *plat, char *buf, size_t buf_size) {
    (void)plat;
    snprintf(buf, buf_size, "EoS Device (embedded)");
    return EAI_OK;
}

static eai_status_t eos_read_gpio(eai_platform_t *plat, int pin, int *value) {
    (void)plat; (void)pin;
    if (!value) return EAI_ERR_INVALID;
    *value = 0;
    return EAI_ERR_UNSUPPORTED;
}

static eai_status_t eos_write_gpio(eai_platform_t *plat, int pin, int value) {
    (void)plat; (void)pin; (void)value;
    return EAI_ERR_UNSUPPORTED;
}

static eai_status_t eos_get_memory_info(eai_platform_t *plat, uint64_t *total, uint64_t *available) {
    (void)plat;
    if (total) *total = 256 * 1024 * 1024ULL;
    if (available) *available = 128 * 1024 * 1024ULL;
    return EAI_OK;
}

static eai_status_t eos_get_cpu_temp(eai_platform_t *plat, float *temp_c) {
    (void)plat;
    if (!temp_c) return EAI_ERR_INVALID;
    *temp_c = 0.0f;
    return EAI_ERR_UNSUPPORTED;
}

static void eos_shutdown(eai_platform_t *plat) {
    (void)plat;
}

const eai_platform_ops_t eai_platform_eos_ops = {
    .name            = "eos",
    .init            = eos_init,
    .get_device_info = eos_get_device_info,
    .read_gpio       = eos_read_gpio,
    .write_gpio      = eos_write_gpio,
    .get_memory_info = eos_get_memory_info,
    .get_cpu_temp    = eos_get_cpu_temp,
    .shutdown        = eos_shutdown,
};

#endif
