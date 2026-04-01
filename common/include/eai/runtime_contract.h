// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_RUNTIME_CONTRACT_H
#define EAI_RUNTIME_CONTRACT_H

#include "eai/types.h"
#include "eai/manifest.h"

typedef struct {
    const char *text;
    size_t      text_len;
    const void *binary;
    size_t      binary_len;
} eai_inference_input_t;

typedef struct {
    char     text[4096];
    size_t   text_len;
    float    confidence;
    uint32_t tokens_used;
    uint32_t latency_ms;
} eai_inference_output_t;

/* ========== Training I/O Types ========== */

typedef struct {
    const char *input_text;
    const char *target_text;
    float       learning_rate;
    int         lora_rank;
} eai_training_input_t;

typedef struct {
    float    loss;
    uint32_t tokens_processed;
    uint32_t step_number;
    uint32_t latency_ms;
    uint32_t memory_used_mb;
} eai_training_output_t;

typedef struct {
    uint32_t total_steps;
    float    current_loss;
    bool     adapter_loaded;
    char     adapter_path[256];
    uint32_t memory_used_mb;
} eai_training_state_t;

typedef struct eai_runtime_s eai_runtime_t;

typedef struct {
    const char *name;
    eai_status_t (*init)(eai_runtime_t *rt);
    eai_status_t (*load_model)(eai_runtime_t *rt, const eai_model_manifest_t *manifest, const char *model_path);
    eai_status_t (*infer)(eai_runtime_t *rt, const eai_inference_input_t *in, eai_inference_output_t *out);
    eai_status_t (*unload_model)(eai_runtime_t *rt);
    void         (*shutdown)(eai_runtime_t *rt);

    /* Adaptive training ops (nullable — NULL means not supported) */
    eai_status_t (*load_adapter)(eai_runtime_t *rt, const char *adapter_path);
    eai_status_t (*unload_adapter)(eai_runtime_t *rt);
    eai_status_t (*train_step)(eai_runtime_t *rt, const eai_training_input_t *in, eai_training_output_t *out);
    eai_status_t (*save_checkpoint)(eai_runtime_t *rt, const char *path);
    eai_status_t (*get_training_state)(eai_runtime_t *rt, eai_training_state_t *state);
} eai_runtime_ops_t;

struct eai_runtime_s {
    const eai_runtime_ops_t *ops;
    void                    *ctx;
    bool                     loaded;
    eai_model_manifest_t     manifest;
};

eai_status_t eai_runtime_init(eai_runtime_t *rt, const eai_runtime_ops_t *ops);
eai_status_t eai_runtime_load(eai_runtime_t *rt, const eai_model_manifest_t *m, const char *path);
eai_status_t eai_runtime_infer(eai_runtime_t *rt, const eai_inference_input_t *in, eai_inference_output_t *out);
eai_status_t eai_runtime_unload(eai_runtime_t *rt);
void         eai_runtime_shutdown(eai_runtime_t *rt);

/* Adaptive runtime operations */
eai_status_t eai_runtime_load_adapter(eai_runtime_t *rt, const char *adapter_path);
eai_status_t eai_runtime_unload_adapter(eai_runtime_t *rt);
eai_status_t eai_runtime_train_step(eai_runtime_t *rt, const eai_training_input_t *in, eai_training_output_t *out);
eai_status_t eai_runtime_save_checkpoint(eai_runtime_t *rt, const char *path);
eai_status_t eai_runtime_get_training_state(eai_runtime_t *rt, eai_training_state_t *state);

#endif /* EAI_RUNTIME_CONTRACT_H */

typedef struct {
    const char *name;
    eai_status_t (*init)(eai_runtime_t *rt);
    eai_status_t (*load_model)(eai_runtime_t *rt, const eai_model_manifest_t *manifest, const char *model_path);
    eai_status_t (*infer)(eai_runtime_t *rt, const eai_inference_input_t *in, eai_inference_output_t *out);
    eai_status_t (*unload_model)(eai_runtime_t *rt);
    void         (*shutdown)(eai_runtime_t *rt);

    /* Adaptive/training ops — nullable, backends set to NULL if unsupported */
    eai_status_t (*load_adapter)(eai_runtime_t *rt, const char *adapter_path);
    eai_status_t (*unload_adapter)(eai_runtime_t *rt);
    eai_status_t (*train_step)(eai_runtime_t *rt, const eai_training_input_t *in, eai_training_output_t *out);
    eai_status_t (*save_checkpoint)(eai_runtime_t *rt, const char *path);
    eai_status_t (*get_training_state)(eai_runtime_t *rt, eai_training_state_t *state);
} eai_runtime_ops_t;

struct eai_runtime_s {
    const eai_runtime_ops_t *ops;
    void                    *ctx;        /* runtime-specific context */
    bool                     loaded;
    eai_model_manifest_t     manifest;
};

eai_status_t eai_runtime_init(eai_runtime_t *rt, const eai_runtime_ops_t *ops);
eai_status_t eai_runtime_load(eai_runtime_t *rt, const eai_model_manifest_t *m, const char *path);
eai_status_t eai_runtime_infer(eai_runtime_t *rt, const eai_inference_input_t *in, eai_inference_output_t *out);
eai_status_t eai_runtime_unload(eai_runtime_t *rt);
void         eai_runtime_shutdown(eai_runtime_t *rt);

/* Adaptive runtime operations */
eai_status_t eai_runtime_load_adapter(eai_runtime_t *rt, const char *adapter_path);
eai_status_t eai_runtime_unload_adapter(eai_runtime_t *rt);
eai_status_t eai_runtime_train_step(eai_runtime_t *rt, const eai_training_input_t *in, eai_training_output_t *out);
eai_status_t eai_runtime_save_checkpoint(eai_runtime_t *rt, const char *path);
eai_status_t eai_runtime_get_training_state(eai_runtime_t *rt, eai_training_state_t *state);

#endif /* EAI_RUNTIME_CONTRACT_H */
