// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai/config.h"
#include "eai/adaptive.h"
#include "eai/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LOG_MOD "config"

eai_status_t eai_config_init(eai_config_t *cfg)
{
    if (!cfg) return EAI_ERR_INVALID;
    memset(cfg, 0, sizeof(*cfg));
    cfg->variant = EAI_VARIANT_MIN;
    cfg->mode    = EAI_MODE_LOCAL;
    cfg->policy.cloud_fallback = false;
    eai_adaptive_config_defaults(&cfg->adaptive);
    cfg->observability = false;
    return EAI_OK;
}

eai_status_t eai_config_load_file(eai_config_t *cfg, const char *path)
{
    if (!cfg || !path) return EAI_ERR_INVALID;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        EAI_LOG_ERROR(LOG_MOD, "cannot open config: %s", path);
        return EAI_ERR_IO;
    }

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char val[256];
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == '\n' || *p == '\0') continue;

        if (sscanf(p, "variant: %255s", val) == 1) {
            if (strcmp(val, "min") == 0)       cfg->variant = EAI_VARIANT_MIN;
            else if (strcmp(val, "framework") == 0) cfg->variant = EAI_VARIANT_FRAMEWORK;
        }
        else if (sscanf(p, "mode: %255s", val) == 1) {
            if (strcmp(val, "local") == 0)        cfg->mode = EAI_MODE_LOCAL;
            else if (strcmp(val, "cloud") == 0)   cfg->mode = EAI_MODE_CLOUD;
            else if (strcmp(val, "hybrid") == 0)  cfg->mode = EAI_MODE_HYBRID;
            else if (strcmp(val, "local-first") == 0) cfg->mode = EAI_MODE_LOCAL;
        }
        else if (sscanf(p, "provider: %255s", val) == 1) {
            cfg->runtime_single.provider = strdup(val);
        }
        else if (sscanf(p, "cloud_fallback: %255s", val) == 1) {
            cfg->policy.cloud_fallback = (strcmp(val, "true") == 0);
        }
        else if (sscanf(p, "observability: %255s", val) == 1) {
            cfg->observability = (strcmp(val, "true") == 0);
        }
        else if (sscanf(p, "adaptive.enabled: %255s", val) == 1) {
            cfg->adaptive.enable_learning = (strcmp(val, "true") == 0);
            cfg->adaptive.enable_preferences = cfg->adaptive.enable_learning;
        }
        else if (sscanf(p, "adaptive.learning_rate: %255s", val) == 1) {
            cfg->adaptive.learning_rate = (float)atof(val);
        }
        else if (sscanf(p, "adaptive.lora_rank: %255s", val) == 1) {
            cfg->adaptive.lora_rank = atoi(val);
        }
        else if (sscanf(p, "adaptive.preference_decay_days: %255s", val) == 1) {
            cfg->adaptive.preference_decay_days = (float)atof(val);
        }
        else if (sscanf(p, "adaptive.max_training_memory_mb: %255s", val) == 1) {
            cfg->adaptive.max_training_memory_mb = (uint32_t)atoi(val);
        }
        else if (sscanf(p, "adaptive.train_during_idle: %255s", val) == 1) {
            cfg->adaptive.train_during_idle = (strcmp(val, "true") == 0);
        }
        else if (sscanf(p, "- %255s", val) == 1) {
            if (cfg->tool_count < EAI_CONFIG_MAX_TOOLS) {
                cfg->tools[cfg->tool_count++] = strdup(val);
            }
        }
    }

    fclose(fp);
    EAI_LOG_INFO(LOG_MOD, "loaded config from %s", path);
    return EAI_OK;
}

eai_status_t eai_config_load_profile(eai_config_t *cfg, const char *profile_name)
{
    if (!cfg || !profile_name) return EAI_ERR_INVALID;

    if (strcmp(profile_name, "smart-camera") == 0) {
        cfg->variant = EAI_VARIANT_FRAMEWORK;
        cfg->mode    = EAI_MODE_LOCAL;
        cfg->runtime_multi.providers[0] = "onnxruntime";
        cfg->runtime_multi.provider_count = 1;
        cfg->tools[0] = "device.read_sensor";
        cfg->tools[1] = "http.get";
        cfg->tool_count = 2;
        cfg->observability = true;
    }
    else if (strcmp(profile_name, "industrial-gateway") == 0) {
        cfg->variant = EAI_VARIANT_FRAMEWORK;
        cfg->mode    = EAI_MODE_LOCAL;
        cfg->runtime_multi.providers[0] = "onnxruntime";
        cfg->runtime_multi.providers[1] = "llama.cpp";
        cfg->runtime_multi.provider_count = 2;
        cfg->connectors[0] = "mqtt";
        cfg->connectors[1] = "opcua";
        cfg->connectors[2] = "modbus";
        cfg->connector_count = 3;
        cfg->tools[0] = "mqtt.publish";
        cfg->tools[1] = "device.read_sensor";
        cfg->tools[2] = "http.get";
        cfg->tool_count = 3;
        cfg->observability = true;
    }
    else if (strcmp(profile_name, "robot-controller") == 0) {
        cfg->variant = EAI_VARIANT_FRAMEWORK;
        cfg->mode    = EAI_MODE_LOCAL;
        cfg->runtime_multi.providers[0] = "onnxruntime";
        cfg->runtime_multi.providers[1] = "tflite";
        cfg->runtime_multi.provider_count = 2;
        cfg->connectors[0] = "can";
        cfg->connectors[1] = "mqtt";
        cfg->connector_count = 2;
        cfg->tools[0] = "device.read_sensor";
        cfg->tools[1] = "mqtt.publish";
        cfg->tool_count = 2;
        cfg->observability = true;
    }
    else if (strcmp(profile_name, "mobile-edge") == 0) {
        cfg->variant = EAI_VARIANT_MIN;
        cfg->mode    = EAI_MODE_HYBRID;
        cfg->runtime_single.provider = "llama.cpp";
        cfg->tools[0] = "http.get";
        cfg->tools[1] = "device.read_sensor";
        cfg->tool_count = 2;
        cfg->observability = false;
    }
    else if (strcmp(profile_name, "adaptive-edge") == 0) {
        cfg->variant = EAI_VARIANT_FRAMEWORK;
        cfg->mode    = EAI_MODE_HYBRID;
        cfg->runtime_multi.providers[0] = "llama.cpp";
        cfg->runtime_multi.provider_count = 1;
        cfg->connectors[0] = "mqtt";
        cfg->connector_count = 1;
        cfg->tools[0] = "device.read_sensor";
        cfg->tools[1] = "http.get";
        cfg->tools[2] = "mqtt.publish";
        cfg->tools[3] = "preference.set";
        cfg->tools[4] = "preference.get";
        cfg->tools[5] = "feedback.record";
        cfg->tools[6] = "model.status";
        cfg->tool_count = 7;
        cfg->observability = true;
        cfg->adaptive.enable_learning = true;
        cfg->adaptive.enable_preferences = true;
        cfg->adaptive.learning_rate = 0.0001f;
        cfg->adaptive.lora_rank = 8;
        cfg->adaptive.preference_decay_days = 30.0f;
        cfg->adaptive.max_training_memory_mb = 512;
        cfg->adaptive.train_during_idle = true;
        cfg->adaptive.max_training_steps_per_cycle = 100;
    }
    else {
        EAI_LOG_WARN(LOG_MOD, "unknown profile: %s", profile_name);
        return EAI_ERR_NOT_FOUND;
    }

    EAI_LOG_INFO(LOG_MOD, "loaded profile: %s", profile_name);
    return EAI_OK;
}

void eai_config_dump(const eai_config_t *cfg)
{
    if (!cfg) return;
    const char *variant_str = cfg->variant == EAI_VARIANT_MIN ? "min" : "framework";
    const char *mode_str = cfg->mode == EAI_MODE_LOCAL ? "local"
                         : cfg->mode == EAI_MODE_CLOUD ? "cloud" : "hybrid";

    printf("EAI Config:\n");
    printf("  variant:        %s\n", variant_str);
    printf("  mode:           %s\n", mode_str);
    printf("  observability:  %s\n", cfg->observability ? "true" : "false");
    printf("  cloud_fallback: %s\n", cfg->policy.cloud_fallback ? "true" : "false");
    printf("  tools (%d):\n", cfg->tool_count);
    for (int i = 0; i < cfg->tool_count; i++)
        printf("    - %s\n", cfg->tools[i]);
    printf("  connectors (%d):\n", cfg->connector_count);
    for (int i = 0; i < cfg->connector_count; i++)
        printf("    - %s\n", cfg->connectors[i]);
    printf("  adaptive:\n");
    printf("    learning:     %s\n", cfg->adaptive.enable_learning ? "true" : "false");
    printf("    preferences:  %s\n", cfg->adaptive.enable_preferences ? "true" : "false");
    printf("    lr:           %.6f\n", cfg->adaptive.learning_rate);
    printf("    lora_rank:    %d\n", cfg->adaptive.lora_rank);
    printf("    decay_days:   %.1f\n", cfg->adaptive.preference_decay_days);
    printf("    train_mem_mb: %u\n", cfg->adaptive.max_training_memory_mb);
    printf("    idle_train:   %s\n", cfg->adaptive.train_during_idle ? "true" : "false");
}

void eai_config_free(eai_config_t *cfg)
{
    if (!cfg) return;
    if (cfg->runtime_single.provider) {
        free((void *)cfg->runtime_single.provider);
        cfg->runtime_single.provider = NULL;
    }
    for (int i = 0; i < cfg->tool_count; i++) {
        if (cfg->tools[i]) {
            free((void *)cfg->tools[i]);
            cfg->tools[i] = NULL;
        }
    }
    cfg->tool_count = 0;
    cfg->connector_count = 0;
}
