// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_min/compression.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>

#define MOD "compress"

static const float QUANT_SIZE_RATIOS[] = {
    1.000f, /* F32   */
    0.500f, /* F16   */
    0.250f, /* Q8_0  */
    0.200f, /* Q5_1  */
    0.188f, /* Q5_0  */
    0.163f, /* Q4_1  */
    0.150f, /* Q4_0  */
    0.125f, /* Q3_K  */
    0.100f, /* Q2_K  */
    0.080f, /* IQ2   */
};

static const float QUANT_QUALITY_RATIOS[] = {
    1.000f, /* F32   */
    0.998f, /* F16   */
    0.985f, /* Q8_0  */
    0.960f, /* Q5_1  */
    0.950f, /* Q5_0  */
    0.930f, /* Q4_1  */
    0.920f, /* Q4_0  */
    0.880f, /* Q3_K  */
    0.830f, /* Q2_K  */
    0.780f, /* IQ2   */
};

static const float QUANT_SPEED_RATIOS[] = {
    1.0f, /* F32   */
    1.5f, /* F16   */
    2.5f, /* Q8_0  */
    3.0f, /* Q5_1  */
    3.2f, /* Q5_0  */
    3.8f, /* Q4_1  */
    4.0f, /* Q4_0  */
    4.5f, /* Q3_K  */
    5.0f, /* Q2_K  */
    5.5f, /* IQ2   */
};

const char *eai_quant_level_str(eai_quant_level_t level) {
    switch (level) {
        case EAI_QUANT_LEVEL_F32:  return "F32";
        case EAI_QUANT_LEVEL_F16:  return "F16";
        case EAI_QUANT_LEVEL_Q8_0: return "Q8_0";
        case EAI_QUANT_LEVEL_Q5_1: return "Q5_1";
        case EAI_QUANT_LEVEL_Q5_0: return "Q5_0";
        case EAI_QUANT_LEVEL_Q4_1: return "Q4_1";
        case EAI_QUANT_LEVEL_Q4_0: return "Q4_0";
        case EAI_QUANT_LEVEL_Q3_K: return "Q3_K";
        case EAI_QUANT_LEVEL_Q2_K: return "Q2_K";
        case EAI_QUANT_LEVEL_IQ2:  return "IQ2";
        default:                   return "UNKNOWN";
    }
}

eai_status_t eai_min_compress_init(eai_min_compression_t *comp) {
    if (!comp) return EAI_ERR_INVALID;
    memset(comp, 0, sizeof(*comp));
    comp->recommended_quant = EAI_QUANT_LEVEL_Q4_0;
    comp->pruning_threshold = 0.0f;
    EAI_LOG_INFO(MOD, "compression analyzer initialized");
    return EAI_OK;
}

eai_quant_level_t eai_min_compress_recommend_quant(uint32_t available_ram_mb,
                                                    uint32_t param_count_m) {
    /* Estimate F32 model size: ~4 bytes per parameter */
    uint32_t f32_size_mb = param_count_m * 4;

    for (int level = 0; level <= (int)EAI_QUANT_LEVEL_IQ2; level++) {
        uint32_t estimated = (uint32_t)(f32_size_mb * QUANT_SIZE_RATIOS[level]);
        /* Add ~20% overhead for runtime buffers */
        uint32_t with_overhead = estimated + estimated / 5;
        if (with_overhead <= available_ram_mb) {
            EAI_LOG_INFO(MOD, "recommended quant for %uM params with %uMB RAM: %s",
                         param_count_m, available_ram_mb, eai_quant_level_str(level));
            return (eai_quant_level_t)level;
        }
    }

    EAI_LOG_WARN(MOD, "model too large even at max compression, suggesting IQ2");
    return EAI_QUANT_LEVEL_IQ2;
}

uint32_t eai_min_compress_estimate_size(uint32_t param_count_m, eai_quant_level_t quant) {
    if (quant > EAI_QUANT_LEVEL_IQ2) quant = EAI_QUANT_LEVEL_Q4_0;
    uint32_t f32_size = param_count_m * 4;
    return (uint32_t)(f32_size * QUANT_SIZE_RATIOS[quant]);
}

float eai_min_compress_estimate_quality(eai_quant_level_t quant) {
    if (quant > EAI_QUANT_LEVEL_IQ2) return 0.0f;
    return QUANT_QUALITY_RATIOS[quant];
}

float eai_min_compress_estimate_speedup(eai_quant_level_t quant) {
    if (quant > EAI_QUANT_LEVEL_IQ2) return 1.0f;
    return QUANT_SPEED_RATIOS[quant];
}

float eai_min_compress_pruning_threshold(uint32_t available_ram_mb, uint32_t model_ram_mb) {
    if (model_ram_mb == 0 || available_ram_mb >= model_ram_mb) return 0.0f;

    float reduction_needed = 1.0f - ((float)available_ram_mb / (float)model_ram_mb);
    float threshold = reduction_needed * 0.8f;
    if (threshold > 0.9f) threshold = 0.9f;

    EAI_LOG_INFO(MOD, "pruning threshold: %.2f (need %.0f%% reduction)",
                 threshold, reduction_needed * 100.0f);
    return threshold;
}

eai_status_t eai_min_compress_analyze(eai_min_compression_t *comp,
                                       uint32_t param_count_m,
                                       uint32_t available_ram_mb,
                                       uint32_t available_storage_mb) {
    if (!comp) return EAI_ERR_INVALID;

    comp->param_count_m = param_count_m;
    comp->available_ram_mb = available_ram_mb;
    comp->available_storage_mb = available_storage_mb;
    comp->original_size_mb = param_count_m * 4;

    comp->recommended_quant = eai_min_compress_recommend_quant(available_ram_mb, param_count_m);

    uint32_t compressed_size = eai_min_compress_estimate_size(param_count_m, comp->recommended_quant);
    comp->stats.size_ratio = (float)compressed_size / (float)comp->original_size_mb;
    comp->stats.quality_ratio = eai_min_compress_estimate_quality(comp->recommended_quant);
    comp->stats.speed_ratio = eai_min_compress_estimate_speedup(comp->recommended_quant);

    comp->pruning_threshold = eai_min_compress_pruning_threshold(available_ram_mb, compressed_size);

    EAI_LOG_INFO(MOD, "analysis complete: %s quant, %.0f%% size, %.0f%% quality, %.1fx speed",
                 eai_quant_level_str(comp->recommended_quant),
                 comp->stats.size_ratio * 100.0f,
                 comp->stats.quality_ratio * 100.0f,
                 comp->stats.speed_ratio);
    return EAI_OK;
}

void eai_min_compress_report(const eai_min_compression_t *comp) {
    if (!comp) return;

    printf("\n=== Model Compression Analysis ===\n");
    printf("Parameters:        %u M\n", comp->param_count_m);
    printf("Original size:     %u MB (F32)\n", comp->original_size_mb);
    printf("Available RAM:     %u MB\n", comp->available_ram_mb);
    printf("Available storage: %u MB\n", comp->available_storage_mb);
    printf("\nRecommended:\n");
    printf("  Quantization: %s\n", eai_quant_level_str(comp->recommended_quant));
    printf("  Est. size:    %u MB (%.0f%% of original)\n",
           eai_min_compress_estimate_size(comp->param_count_m, comp->recommended_quant),
           comp->stats.size_ratio * 100.0f);
    printf("  Quality:      %.1f%%\n", comp->stats.quality_ratio * 100.0f);
    printf("  Speed:        %.1fx vs F32\n", comp->stats.speed_ratio);
    if (comp->pruning_threshold > 0.0f) {
        printf("  Pruning:      %.0f%% threshold recommended\n",
               comp->pruning_threshold * 100.0f);
    }
}
