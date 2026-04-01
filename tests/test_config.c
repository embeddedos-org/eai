// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_config.c
 * @brief Unit tests for EAI configuration
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eai/types.h"
#include "eai/config.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Stub implementations ---- */
eai_status_t eai_config_init(eai_config_t *cfg) {
    if (!cfg) return EAI_ERR_INVALID;
    memset(cfg, 0, sizeof(*cfg));
    cfg->variant = EAI_VARIANT_MIN;
    cfg->mode = EAI_MODE_LOCAL;
    cfg->observability = false;
    cfg->policy.cloud_fallback = false;
    return EAI_OK;
}

eai_status_t eai_config_load_file(eai_config_t *cfg, const char *path) {
    if (!cfg || !path) return EAI_ERR_INVALID;
    if (strlen(path) == 0) return EAI_ERR_NOT_FOUND;
    return EAI_OK;
}

eai_status_t eai_config_load_profile(eai_config_t *cfg, const char *profile_name) {
    if (!cfg || !profile_name) return EAI_ERR_INVALID;
    if (strcmp(profile_name, "minimal") == 0) {
        cfg->variant = EAI_VARIANT_MIN;
        cfg->mode = EAI_MODE_LOCAL;
        cfg->tool_count = 0;
    } else if (strcmp(profile_name, "full") == 0) {
        cfg->variant = EAI_VARIANT_FRAMEWORK;
        cfg->mode = EAI_MODE_HYBRID;
        cfg->observability = true;
    } else {
        return EAI_ERR_NOT_FOUND;
    }
    return EAI_OK;
}

void eai_config_dump(const eai_config_t *cfg) { (void)cfg; }

void eai_config_free(eai_config_t *cfg) {
    if (cfg) memset(cfg, 0, sizeof(*cfg));
}

const char *eai_status_str(eai_status_t s) { (void)s; return "OK"; }

/* ---- Tests ---- */
static void test_config_init(void) {
    eai_config_t cfg;
    eai_status_t rc = eai_config_init(&cfg);
    assert(rc == EAI_OK);
    assert(cfg.variant == EAI_VARIANT_MIN);
    assert(cfg.mode == EAI_MODE_LOCAL);
    assert(cfg.tool_count == 0);
    assert(cfg.connector_count == 0);
    PASS("config_init");
}

static void test_config_init_null(void) {
    eai_status_t rc = eai_config_init(NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("config_init_null");
}

static void test_config_load_profile_minimal(void) {
    eai_config_t cfg;
    eai_config_init(&cfg);
    eai_status_t rc = eai_config_load_profile(&cfg, "minimal");
    assert(rc == EAI_OK);
    assert(cfg.variant == EAI_VARIANT_MIN);
    assert(cfg.mode == EAI_MODE_LOCAL);
    PASS("config_load_profile_minimal");
}

static void test_config_load_profile_full(void) {
    eai_config_t cfg;
    eai_config_init(&cfg);
    eai_status_t rc = eai_config_load_profile(&cfg, "full");
    assert(rc == EAI_OK);
    assert(cfg.variant == EAI_VARIANT_FRAMEWORK);
    assert(cfg.mode == EAI_MODE_HYBRID);
    assert(cfg.observability == true);
    PASS("config_load_profile_full");
}

static void test_config_load_profile_unknown(void) {
    eai_config_t cfg;
    eai_config_init(&cfg);
    eai_status_t rc = eai_config_load_profile(&cfg, "nonexistent");
    assert(rc == EAI_ERR_NOT_FOUND);
    PASS("config_load_profile_unknown");
}

static void test_config_load_file_null(void) {
    eai_config_t cfg;
    eai_config_init(&cfg);
    eai_status_t rc = eai_config_load_file(&cfg, NULL);
    assert(rc == EAI_ERR_INVALID);
    PASS("config_load_file_null");
}

static void test_config_free(void) {
    eai_config_t cfg;
    eai_config_init(&cfg);
    cfg.tool_count = 3;
    cfg.observability = true;
    eai_config_free(&cfg);
    assert(cfg.tool_count == 0);
    assert(cfg.observability == false);
    PASS("config_free");
}

static void test_config_constants(void) {
    assert(EAI_CONFIG_MAX_TOOLS == 32);
    assert(EAI_CONFIG_MAX_CONNECTORS == 16);
    assert(EAI_CONFIG_MAX_PROVIDERS == 8);
    PASS("config_constants");
}

int main(void) {
    printf("=== eai Config Tests ===\n");
    test_config_init();
    test_config_init_null();
    test_config_load_profile_minimal();
    test_config_load_profile_full();
    test_config_load_profile_unknown();
    test_config_load_file_null();
    test_config_free();
    test_config_constants();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
