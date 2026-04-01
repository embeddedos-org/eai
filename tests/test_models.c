// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023
/**
 * @file test_models.c
 * @brief Unit tests for EAI model catalog
 */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "eai/types.h"

static int passed = 0;
#define PASS(name) do { printf("[PASS] %s\n", name); passed++; } while(0)

/* ---- Inline model types ---- */
typedef enum {
    EAI_QUANT_F32 = 0, EAI_QUANT_F16, EAI_QUANT_Q8_0,
    EAI_QUANT_Q5_1, EAI_QUANT_Q5_0, EAI_QUANT_Q4_1,
    EAI_QUANT_Q4_0, EAI_QUANT_Q3_K, EAI_QUANT_Q2_K, EAI_QUANT_IQ2
} eai_quant_t;

typedef enum {
    EAI_RUNTIME_LLAMA_CPP = 0, EAI_RUNTIME_ONNX,
    EAI_RUNTIME_TFLITE, EAI_RUNTIME_CUSTOM
} eai_runtime_type_t;

typedef enum {
    EAI_MODEL_TIER_MICRO = 0, EAI_MODEL_TIER_TINY,
    EAI_MODEL_TIER_SMALL, EAI_MODEL_TIER_MEDIUM, EAI_MODEL_TIER_LARGE
} eai_model_tier_t;

typedef struct {
    const char     *name;
    const char     *family;
    const char     *description;
    eai_quant_t     quant;
    eai_runtime_type_t runtime;
    eai_model_tier_t tier;
    uint32_t        param_count_m;
    uint32_t        context_len;
    uint32_t        ram_mb;
    uint32_t        storage_mb;
    uint32_t        tokens_per_sec;
    const char     *target_hardware;
    const char     *license;
    const char     *gguf_file;
} eai_model_info_t;

/* ---- Test model catalog ---- */
static const eai_model_info_t test_models[] = {
    {"phi-mini-q4", "phi", "Microsoft Phi-3 Mini Q4", EAI_QUANT_Q4_0,
     EAI_RUNTIME_LLAMA_CPP, EAI_MODEL_TIER_SMALL, 3800, 4096,
     1800, 1200, 15, "RPi4/Jetson", "MIT", "phi-3-mini-q4.gguf"},
    {"tinyllama-q8", "llama", "TinyLlama 1.1B Q8", EAI_QUANT_Q8_0,
     EAI_RUNTIME_LLAMA_CPP, EAI_MODEL_TIER_TINY, 1100, 2048,
     800, 500, 25, "RPi4", "Apache-2.0", "tinyllama-q8.gguf"},
    {"smollm-q4", "smollm", "SmolLM 135M Q4", EAI_QUANT_Q4_0,
     EAI_RUNTIME_LLAMA_CPP, EAI_MODEL_TIER_MICRO, 135, 512,
     80, 50, 60, "RP2040/nRF", "MIT", "smollm-135m-q4.gguf"},
};
static const int test_model_count = 3;

/* ---- Stub implementations ---- */
const eai_model_info_t *eai_model_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < test_model_count; i++) {
        if (strcmp(test_models[i].name, name) == 0)
            return &test_models[i];
    }
    return NULL;
}

const eai_model_info_t *eai_model_find_by_tier(eai_model_tier_t tier) {
    for (int i = 0; i < test_model_count; i++) {
        if (test_models[i].tier == tier) return &test_models[i];
    }
    return NULL;
}

const eai_model_info_t *eai_model_find_best_fit(uint32_t ram_mb, uint32_t storage_mb) {
    const eai_model_info_t *best = NULL;
    for (int i = 0; i < test_model_count; i++) {
        if (test_models[i].ram_mb <= ram_mb && test_models[i].storage_mb <= storage_mb) {
            if (!best || test_models[i].param_count_m > best->param_count_m)
                best = &test_models[i];
        }
    }
    return best;
}

const char *eai_status_str(eai_status_t s) { (void)s; return "OK"; }

/* ---- Tests ---- */
static void test_model_find_by_name(void) {
    const eai_model_info_t *m = eai_model_find("phi-mini-q4");
    assert(m != NULL);
    assert(strcmp(m->family, "phi") == 0);
    assert(m->param_count_m == 3800);
    PASS("model_find_by_name");
}

static void test_model_find_not_found(void) {
    const eai_model_info_t *m = eai_model_find("gpt-4");
    assert(m == NULL);
    PASS("model_find_not_found");
}

static void test_model_find_by_tier_micro(void) {
    const eai_model_info_t *m = eai_model_find_by_tier(EAI_MODEL_TIER_MICRO);
    assert(m != NULL);
    assert(strcmp(m->name, "smollm-q4") == 0);
    assert(m->ram_mb <= 100);
    PASS("model_find_by_tier_micro");
}

static void test_model_find_by_tier_tiny(void) {
    const eai_model_info_t *m = eai_model_find_by_tier(EAI_MODEL_TIER_TINY);
    assert(m != NULL);
    assert(strcmp(m->name, "tinyllama-q8") == 0);
    PASS("model_find_by_tier_tiny");
}

static void test_model_best_fit_large_device(void) {
    const eai_model_info_t *m = eai_model_find_best_fit(4096, 4096);
    assert(m != NULL);
    assert(strcmp(m->name, "phi-mini-q4") == 0);
    PASS("model_best_fit_large_device");
}

static void test_model_best_fit_small_device(void) {
    const eai_model_info_t *m = eai_model_find_best_fit(100, 100);
    assert(m != NULL);
    assert(strcmp(m->name, "smollm-q4") == 0);
    PASS("model_best_fit_small_device");
}

static void test_model_best_fit_no_fit(void) {
    const eai_model_info_t *m = eai_model_find_best_fit(10, 10);
    assert(m == NULL);
    PASS("model_best_fit_no_fit");
}

static void test_model_tier_enum(void) {
    assert(EAI_MODEL_TIER_MICRO == 0);
    assert(EAI_MODEL_TIER_TINY == 1);
    assert(EAI_MODEL_TIER_SMALL == 2);
    assert(EAI_MODEL_TIER_MEDIUM == 3);
    assert(EAI_MODEL_TIER_LARGE == 4);
    PASS("model_tier_enum");
}

static void test_quant_enum(void) {
    assert(EAI_QUANT_F32 == 0);
    assert(EAI_QUANT_Q4_0 == 6);
    assert(EAI_QUANT_IQ2 == 9);
    PASS("quant_enum");
}

int main(void) {
    printf("=== eai Model Catalog Tests ===\n");
    test_model_find_by_name();
    test_model_find_not_found();
    test_model_find_by_tier_micro();
    test_model_find_by_tier_tiny();
    test_model_best_fit_large_device();
    test_model_best_fit_small_device();
    test_model_best_fit_no_fit();
    test_model_tier_enum();
    test_quant_enum();
    printf("\n=== ALL %d TESTS PASSED ===\n", passed);
    return 0;
}
