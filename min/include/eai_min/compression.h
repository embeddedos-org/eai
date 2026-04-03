// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_MIN_COMPRESSION_H
#define EAI_MIN_COMPRESSION_H

#include "eai/types.h"

typedef enum {
    EAI_COMPRESS_NONE,
    EAI_COMPRESS_QUANTIZE,
    EAI_COMPRESS_PRUNE,
    EAI_COMPRESS_DISTILL,
} eai_compression_method_t;

typedef enum {
    EAI_QUANT_LEVEL_F32   = 0,
    EAI_QUANT_LEVEL_F16   = 1,
    EAI_QUANT_LEVEL_Q8_0  = 2,
    EAI_QUANT_LEVEL_Q5_1  = 3,
    EAI_QUANT_LEVEL_Q5_0  = 4,
    EAI_QUANT_LEVEL_Q4_1  = 5,
    EAI_QUANT_LEVEL_Q4_0  = 6,
    EAI_QUANT_LEVEL_Q3_K  = 7,
    EAI_QUANT_LEVEL_Q2_K  = 8,
    EAI_QUANT_LEVEL_IQ2   = 9,
} eai_quant_level_t;

typedef struct {
    float size_ratio;
    float speed_ratio;
    float quality_ratio;
} eai_compression_stats_t;

typedef struct {
    uint32_t            available_ram_mb;
    uint32_t            available_storage_mb;
    uint32_t            original_size_mb;
    uint32_t            param_count_m;
    eai_quant_level_t   recommended_quant;
    float               pruning_threshold;
    eai_compression_stats_t stats;
} eai_min_compression_t;

eai_status_t eai_min_compress_init(eai_min_compression_t *comp);
eai_quant_level_t eai_min_compress_recommend_quant(uint32_t available_ram_mb,
                                                    uint32_t param_count_m);
uint32_t     eai_min_compress_estimate_size(uint32_t param_count_m, eai_quant_level_t quant);
float        eai_min_compress_estimate_quality(eai_quant_level_t quant);
float        eai_min_compress_estimate_speedup(eai_quant_level_t quant);
float        eai_min_compress_pruning_threshold(uint32_t available_ram_mb,
                                                 uint32_t model_ram_mb);
eai_status_t eai_min_compress_analyze(eai_min_compression_t *comp,
                                       uint32_t param_count_m,
                                       uint32_t available_ram_mb,
                                       uint32_t available_storage_mb);
void         eai_min_compress_report(const eai_min_compression_t *comp);
const char  *eai_quant_level_str(eai_quant_level_t level);

#endif /* EAI_MIN_COMPRESSION_H */
