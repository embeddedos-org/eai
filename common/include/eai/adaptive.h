// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_ADAPTIVE_H
#define EAI_ADAPTIVE_H

#include "eai/types.h"
#include <time.h>

#define EAI_PREF_MAX_ENTRIES    256
#define EAI_PREF_KEY_MAX        64
#define EAI_PREF_VALUE_MAX      512
#define EAI_FEEDBACK_MAX        512
#define EAI_TRAINING_BUF_MAX    256
#define EAI_TRAINING_TEXT_MAX   1024

/* ========== Preference Store ========== */

typedef enum {
    EAI_PREF_STRING,
    EAI_PREF_INT,
    EAI_PREF_FLOAT,
    EAI_PREF_BOOL,
    EAI_PREF_JSON,
} eai_preference_type_t;

typedef struct {
    char                   key[EAI_PREF_KEY_MAX];
    char                   value[EAI_PREF_VALUE_MAX];
    eai_preference_type_t  type;
    float                  weight;      /* 0.0 - 1.0, higher = more important */
    float                  decay_rate;  /* per-day decay multiplier (0.0 - 1.0) */
    uint64_t               timestamp;   /* Unix timestamp of last update */
} eai_preference_t;

typedef struct {
    eai_preference_t entries[EAI_PREF_MAX_ENTRIES];
    int              count;
    char             storage_path[256];
} eai_preference_store_t;

eai_status_t eai_pref_init(eai_preference_store_t *store, const char *storage_path);
eai_status_t eai_pref_set(eai_preference_store_t *store, const char *key, const char *value,
                           eai_preference_type_t type, float weight);
const char  *eai_pref_get(const eai_preference_store_t *store, const char *key);
float        eai_pref_get_weight(const eai_preference_store_t *store, const char *key);
eai_status_t eai_pref_delete(eai_preference_store_t *store, const char *key);
eai_status_t eai_pref_decay(eai_preference_store_t *store, float elapsed_days);
eai_status_t eai_pref_save(const eai_preference_store_t *store);
eai_status_t eai_pref_load(eai_preference_store_t *store);
void         eai_pref_clear(eai_preference_store_t *store);
int          eai_pref_count(const eai_preference_store_t *store);

/* ========== Feedback Buffer ========== */

typedef struct {
    uint32_t input_hash;
    uint32_t output_hash;
    float    score;        /* -1.0 (bad) to +1.0 (good) */
    char     context[256];
    uint64_t timestamp;
} eai_feedback_t;

typedef struct {
    eai_feedback_t entries[EAI_FEEDBACK_MAX];
    int            count;
    int            head;    /* ring buffer write position */
} eai_feedback_buffer_t;

eai_status_t eai_feedback_init(eai_feedback_buffer_t *buf);
eai_status_t eai_feedback_record(eai_feedback_buffer_t *buf, float score, const char *context,
                                  uint32_t input_hash, uint32_t output_hash);
int          eai_feedback_get_recent(const eai_feedback_buffer_t *buf, eai_feedback_t *out, int max_count);
float        eai_feedback_avg_score(const eai_feedback_buffer_t *buf, int recent_n);
void         eai_feedback_clear(eai_feedback_buffer_t *buf);

/* ========== Training Sample Buffer ========== */

typedef struct {
    char     input_text[EAI_TRAINING_TEXT_MAX];
    char     target_text[EAI_TRAINING_TEXT_MAX];
    float    feedback_score;
    float    weight;
    uint64_t timestamp;
} eai_training_sample_t;

typedef struct {
    eai_training_sample_t entries[EAI_TRAINING_BUF_MAX];
    int                   count;
    int                   head;
} eai_training_buffer_t;

eai_status_t eai_training_buf_init(eai_training_buffer_t *buf);
eai_status_t eai_training_buf_add(eai_training_buffer_t *buf, const char *input,
                                   const char *target, float score);
int          eai_training_buf_get_batch(const eai_training_buffer_t *buf,
                                        eai_training_sample_t *out, int max_count);
void         eai_training_buf_clear(eai_training_buffer_t *buf);
int          eai_training_buf_count(const eai_training_buffer_t *buf);

/* ========== Adaptive Config ========== */

typedef struct {
    bool     enable_learning;
    bool     enable_preferences;
    float    learning_rate;
    int      lora_rank;
    float    preference_decay_days;
    uint32_t max_training_memory_mb;
    bool     train_during_idle;
    uint32_t max_training_steps_per_cycle;
} eai_adaptive_config_t;

eai_status_t eai_adaptive_config_defaults(eai_adaptive_config_t *cfg);

#endif /* EAI_ADAPTIVE_H */
