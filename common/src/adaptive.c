// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai/adaptive.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ========== Preference Store ========== */

eai_status_t eai_pref_init(eai_preference_store_t *store, const char *storage_path)
{
    if (!store) return EAI_ERR_INVALID;
    memset(store, 0, sizeof(*store));
    if (storage_path) {
        strncpy(store->storage_path, storage_path, sizeof(store->storage_path) - 1);
    }
    return EAI_OK;
}

static int pref_find_index(const eai_preference_store_t *store, const char *key)
{
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].key, key) == 0)
            return i;
    }
    return -1;
}

eai_status_t eai_pref_set(eai_preference_store_t *store, const char *key, const char *value,
                           eai_preference_type_t type, float weight)
{
    if (!store || !key || !value) return EAI_ERR_INVALID;

    int idx = pref_find_index(store, key);
    if (idx >= 0) {
        strncpy(store->entries[idx].value, value, EAI_PREF_VALUE_MAX - 1);
        store->entries[idx].value[EAI_PREF_VALUE_MAX - 1] = '\0';
        store->entries[idx].type = type;
        store->entries[idx].weight = weight;
        store->entries[idx].timestamp = (uint64_t)time(NULL);
        return EAI_OK;
    }

    if (store->count >= EAI_PREF_MAX_ENTRIES) {
        /* evict lowest-weight entry */
        int min_idx = 0;
        float min_weight = store->entries[0].weight;
        for (int i = 1; i < store->count; i++) {
            if (store->entries[i].weight < min_weight) {
                min_weight = store->entries[i].weight;
                min_idx = i;
            }
        }
        idx = min_idx;
    } else {
        idx = store->count;
        store->count++;
    }

    strncpy(store->entries[idx].key, key, EAI_PREF_KEY_MAX - 1);
    store->entries[idx].key[EAI_PREF_KEY_MAX - 1] = '\0';
    strncpy(store->entries[idx].value, value, EAI_PREF_VALUE_MAX - 1);
    store->entries[idx].value[EAI_PREF_VALUE_MAX - 1] = '\0';
    store->entries[idx].type = type;
    store->entries[idx].weight = weight;
    store->entries[idx].decay_rate = 0.98f;
    store->entries[idx].timestamp = (uint64_t)time(NULL);

    return EAI_OK;
}

const char *eai_pref_get(const eai_preference_store_t *store, const char *key)
{
    if (!store || !key) return NULL;
    int idx = pref_find_index(store, key);
    if (idx < 0) return NULL;
    return store->entries[idx].value;
}

float eai_pref_get_weight(const eai_preference_store_t *store, const char *key)
{
    if (!store || !key) return 0.0f;
    int idx = pref_find_index(store, key);
    if (idx < 0) return 0.0f;
    return store->entries[idx].weight;
}

eai_status_t eai_pref_delete(eai_preference_store_t *store, const char *key)
{
    if (!store || !key) return EAI_ERR_INVALID;
    int idx = pref_find_index(store, key);
    if (idx < 0) return EAI_ERR_NOT_FOUND;

    if (idx < store->count - 1) {
        memmove(&store->entries[idx], &store->entries[idx + 1],
                (size_t)(store->count - idx - 1) * sizeof(eai_preference_t));
    }
    store->count--;
    return EAI_OK;
}

eai_status_t eai_pref_decay(eai_preference_store_t *store, float elapsed_days)
{
    if (!store) return EAI_ERR_INVALID;
    if (elapsed_days <= 0.0f) return EAI_OK;

    for (int i = 0; i < store->count; i++) {
        float factor = powf(store->entries[i].decay_rate, elapsed_days);
        store->entries[i].weight *= factor;
    }

    /* remove entries with negligible weight */
    int write = 0;
    for (int read = 0; read < store->count; read++) {
        if (store->entries[read].weight >= 0.01f) {
            if (write != read) {
                memcpy(&store->entries[write], &store->entries[read], sizeof(eai_preference_t));
            }
            write++;
        }
    }
    store->count = write;
    return EAI_OK;
}

eai_status_t eai_pref_save(const eai_preference_store_t *store)
{
    if (!store || store->storage_path[0] == '\0') return EAI_ERR_INVALID;

    FILE *fp = fopen(store->storage_path, "w");
    if (!fp) return EAI_ERR_IO;

    for (int i = 0; i < store->count; i++) {
        const eai_preference_t *p = &store->entries[i];
        fprintf(fp, "%s|%s|%d|%.6f|%.6f\n",
                p->key, p->value, (int)p->type, p->weight, p->decay_rate);
    }

    fclose(fp);
    return EAI_OK;
}

eai_status_t eai_pref_load(eai_preference_store_t *store)
{
    if (!store || store->storage_path[0] == '\0') return EAI_ERR_INVALID;

    FILE *fp = fopen(store->storage_path, "r");
    if (!fp) return EAI_ERR_IO;

    store->count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp) && store->count < EAI_PREF_MAX_ENTRIES) {
        /* strip newline */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        eai_preference_t *p = &store->entries[store->count];
        char key[EAI_PREF_KEY_MAX], value[EAI_PREF_VALUE_MAX];
        int type_int;
        float weight, decay;

        if (sscanf(line, "%63[^|]|%511[^|]|%d|%f|%f",
                   key, value, &type_int, &weight, &decay) == 5) {
            strncpy(p->key, key, EAI_PREF_KEY_MAX - 1);
            p->key[EAI_PREF_KEY_MAX - 1] = '\0';
            strncpy(p->value, value, EAI_PREF_VALUE_MAX - 1);
            p->value[EAI_PREF_VALUE_MAX - 1] = '\0';
            p->type = (eai_preference_type_t)type_int;
            p->weight = weight;
            p->decay_rate = decay;
            p->timestamp = (uint64_t)time(NULL);
            store->count++;
        }
    }

    fclose(fp);
    return EAI_OK;
}

void eai_pref_clear(eai_preference_store_t *store)
{
    if (!store) return;
    store->count = 0;
}

int eai_pref_count(const eai_preference_store_t *store)
{
    if (!store) return 0;
    return store->count;
}

/* ========== Feedback Buffer ========== */

eai_status_t eai_feedback_init(eai_feedback_buffer_t *buf)
{
    if (!buf) return EAI_ERR_INVALID;
    memset(buf, 0, sizeof(*buf));
    return EAI_OK;
}

eai_status_t eai_feedback_record(eai_feedback_buffer_t *buf, float score, const char *context,
                                  uint32_t input_hash, uint32_t output_hash)
{
    if (!buf) return EAI_ERR_INVALID;
    if (score < -1.0f || score > 1.0f) return EAI_ERR_INVALID;

    eai_feedback_t *entry = &buf->entries[buf->head];
    entry->input_hash = input_hash;
    entry->output_hash = output_hash;
    entry->score = score;
    entry->timestamp = (uint64_t)time(NULL);

    if (context) {
        strncpy(entry->context, context, sizeof(entry->context) - 1);
        entry->context[sizeof(entry->context) - 1] = '\0';
    } else {
        entry->context[0] = '\0';
    }

    buf->head = (buf->head + 1) % EAI_FEEDBACK_MAX;
    if (buf->count < EAI_FEEDBACK_MAX) buf->count++;

    return EAI_OK;
}

int eai_feedback_get_recent(const eai_feedback_buffer_t *buf, eai_feedback_t *out, int max_count)
{
    if (!buf || !out || max_count <= 0) return 0;

    int n = buf->count < max_count ? buf->count : max_count;
    for (int i = 0; i < n; i++) {
        int idx = (buf->head - 1 - i + EAI_FEEDBACK_MAX) % EAI_FEEDBACK_MAX;
        memcpy(&out[i], &buf->entries[idx], sizeof(eai_feedback_t));
    }
    return n;
}

float eai_feedback_avg_score(const eai_feedback_buffer_t *buf, int recent_n)
{
    if (!buf || buf->count == 0) return 0.0f;

    int n = buf->count < recent_n ? buf->count : recent_n;
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = (buf->head - 1 - i + EAI_FEEDBACK_MAX) % EAI_FEEDBACK_MAX;
        sum += buf->entries[idx].score;
    }
    return sum / (float)n;
}

void eai_feedback_clear(eai_feedback_buffer_t *buf)
{
    if (!buf) return;
    buf->count = 0;
    buf->head = 0;
}

/* ========== Training Sample Buffer ========== */

eai_status_t eai_training_buf_init(eai_training_buffer_t *buf)
{
    if (!buf) return EAI_ERR_INVALID;
    memset(buf, 0, sizeof(*buf));
    return EAI_OK;
}

eai_status_t eai_training_buf_add(eai_training_buffer_t *buf, const char *input,
                                   const char *target, float score)
{
    if (!buf || !input || !target) return EAI_ERR_INVALID;

    eai_training_sample_t *sample = &buf->entries[buf->head];
    strncpy(sample->input_text, input, EAI_TRAINING_TEXT_MAX - 1);
    sample->input_text[EAI_TRAINING_TEXT_MAX - 1] = '\0';
    strncpy(sample->target_text, target, EAI_TRAINING_TEXT_MAX - 1);
    sample->target_text[EAI_TRAINING_TEXT_MAX - 1] = '\0';
    sample->feedback_score = score;
    sample->weight = (score > 0.0f) ? score : 0.1f;
    sample->timestamp = (uint64_t)time(NULL);

    buf->head = (buf->head + 1) % EAI_TRAINING_BUF_MAX;
    if (buf->count < EAI_TRAINING_BUF_MAX) buf->count++;

    return EAI_OK;
}

int eai_training_buf_get_batch(const eai_training_buffer_t *buf,
                                eai_training_sample_t *out, int max_count)
{
    if (!buf || !out || max_count <= 0) return 0;

    int n = buf->count < max_count ? buf->count : max_count;
    for (int i = 0; i < n; i++) {
        int idx = (buf->head - 1 - i + EAI_TRAINING_BUF_MAX) % EAI_TRAINING_BUF_MAX;
        memcpy(&out[i], &buf->entries[idx], sizeof(eai_training_sample_t));
    }
    return n;
}

void eai_training_buf_clear(eai_training_buffer_t *buf)
{
    if (!buf) return;
    buf->count = 0;
    buf->head = 0;
}

int eai_training_buf_count(const eai_training_buffer_t *buf)
{
    if (!buf) return 0;
    return buf->count;
}

/* ========== Adaptive Config ========== */

eai_status_t eai_adaptive_config_defaults(eai_adaptive_config_t *cfg)
{
    if (!cfg) return EAI_ERR_INVALID;
    cfg->enable_learning = true;
    cfg->enable_preferences = true;
    cfg->learning_rate = 0.0001f;
    cfg->lora_rank = 8;
    cfg->preference_decay_days = 30.0f;
    cfg->max_training_memory_mb = 512;
    cfg->train_during_idle = true;
    cfg->max_training_steps_per_cycle = 100;
    return EAI_OK;
}
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai/adaptive.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#define LOG_MOD "adaptive"

/* ========== Adaptive Config Defaults ========== */

eai_status_t eai_adaptive_config_defaults(eai_adaptive_config_t *cfg)
{
    if (!cfg) return EAI_ERR_INVALID;
    cfg->enable_learning            = true;
    cfg->enable_preferences         = true;
    cfg->learning_rate              = 0.0001f;
    cfg->lora_rank                  = 8;
    cfg->preference_decay_days      = 30.0f;
    cfg->max_training_memory_mb     = 512;
    cfg->train_during_idle          = true;
    cfg->max_training_steps_per_cycle = 100;
    return EAI_OK;
}

/* ========== Preference Store ========== */

eai_status_t eai_pref_init(eai_preference_store_t *store, const char *storage_path)
{
    if (!store) return EAI_ERR_INVALID;
    memset(store, 0, sizeof(*store));
    if (storage_path) {
        strncpy(store->storage_path, storage_path, sizeof(store->storage_path) - 1);
    }
    return EAI_OK;
}

static int pref_find_index(const eai_preference_store_t *store, const char *key)
{
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->entries[i].key, key) == 0) return i;
    }
    return -1;
}

eai_status_t eai_pref_set(eai_preference_store_t *store, const char *key, const char *value,
                           eai_preference_type_t type, float weight)
{
    if (!store || !key || !value) return EAI_ERR_INVALID;
    if (weight < 0.0f || weight > 1.0f) return EAI_ERR_INVALID;

    int idx = pref_find_index(store, key);
    if (idx < 0) {
        if (store->count >= EAI_PREF_MAX_ENTRIES) {
            /* evict lowest-weight entry */
            int min_idx = 0;
            float min_w = store->entries[0].weight;
            for (int i = 1; i < store->count; i++) {
                if (store->entries[i].weight < min_w) {
                    min_w = store->entries[i].weight;
                    min_idx = i;
                }
            }
            if (weight <= min_w) return EAI_ERR_NOMEM;
            idx = min_idx;
            EAI_LOG_DEBUG(LOG_MOD, "evicting preference '%s' (weight=%.2f)",
                          store->entries[idx].key, min_w);
        } else {
            idx = store->count++;
        }
    }

    eai_preference_t *p = &store->entries[idx];
    strncpy(p->key, key, EAI_PREF_KEY_MAX - 1);
    p->key[EAI_PREF_KEY_MAX - 1] = '\0';
    strncpy(p->value, value, EAI_PREF_VALUE_MAX - 1);
    p->value[EAI_PREF_VALUE_MAX - 1] = '\0';
    p->type = type;
    p->weight = weight;
    p->decay_rate = 0.98f;
    p->timestamp = (uint64_t)time(NULL);

    return EAI_OK;
}

const char *eai_pref_get(const eai_preference_store_t *store, const char *key)
{
    if (!store || !key) return NULL;
    int idx = pref_find_index(store, key);
    if (idx < 0) return NULL;
    return store->entries[idx].value;
}

float eai_pref_get_weight(const eai_preference_store_t *store, const char *key)
{
    if (!store || !key) return 0.0f;
    int idx = pref_find_index(store, key);
    if (idx < 0) return 0.0f;
    return store->entries[idx].weight;
}

eai_status_t eai_pref_delete(eai_preference_store_t *store, const char *key)
{
    if (!store || !key) return EAI_ERR_INVALID;
    int idx = pref_find_index(store, key);
    if (idx < 0) return EAI_ERR_NOT_FOUND;

    if (idx < store->count - 1) {
        memmove(&store->entries[idx], &store->entries[idx + 1],
                sizeof(eai_preference_t) * (size_t)(store->count - idx - 1));
    }
    store->count--;
    return EAI_OK;
}

eai_status_t eai_pref_decay(eai_preference_store_t *store, float elapsed_days)
{
    if (!store || elapsed_days <= 0.0f) return EAI_ERR_INVALID;

    for (int i = store->count - 1; i >= 0; i--) {
        eai_preference_t *p = &store->entries[i];
        p->weight *= powf(p->decay_rate, elapsed_days);
        if (p->weight < 0.01f) {
            EAI_LOG_DEBUG(LOG_MOD, "decayed preference '%s' removed (weight=%.4f)", p->key, p->weight);
            eai_pref_delete(store, p->key);
        }
    }
    return EAI_OK;
}

eai_status_t eai_pref_save(const eai_preference_store_t *store)
{
    if (!store || store->storage_path[0] == '\0') return EAI_ERR_INVALID;

    FILE *fp = fopen(store->storage_path, "w");
    if (!fp) return EAI_ERR_IO;

    for (int i = 0; i < store->count; i++) {
        const eai_preference_t *p = &store->entries[i];
        fprintf(fp, "%s|%s|%d|%.4f|%.4f\n",
                p->key, p->value, (int)p->type, p->weight, p->decay_rate);
    }
    fclose(fp);
    EAI_LOG_INFO(LOG_MOD, "saved %d preferences to %s", store->count, store->storage_path);
    return EAI_OK;
}

eai_status_t eai_pref_load(eai_preference_store_t *store)
{
    if (!store || store->storage_path[0] == '\0') return EAI_ERR_INVALID;

    FILE *fp = fopen(store->storage_path, "r");
    if (!fp) return EAI_ERR_IO;

    store->count = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp) && store->count < EAI_PREF_MAX_ENTRIES) {
        eai_preference_t *p = &store->entries[store->count];
        int type_int = 0;
        if (sscanf(line, "%63[^|]|%511[^|]|%d|%f|%f",
                   p->key, p->value, &type_int, &p->weight, &p->decay_rate) >= 3) {
            p->type = (eai_preference_type_t)type_int;
            p->timestamp = (uint64_t)time(NULL);
            store->count++;
        }
    }
    fclose(fp);
    EAI_LOG_INFO(LOG_MOD, "loaded %d preferences from %s", store->count, store->storage_path);
    return EAI_OK;
}

void eai_pref_clear(eai_preference_store_t *store)
{
    if (!store) return;
    store->count = 0;
}

int eai_pref_count(const eai_preference_store_t *store)
{
    return store ? store->count : 0;
}

/* ========== Feedback Buffer ========== */

eai_status_t eai_feedback_init(eai_feedback_buffer_t *buf)
{
    if (!buf) return EAI_ERR_INVALID;
    memset(buf, 0, sizeof(*buf));
    return EAI_OK;
}

eai_status_t eai_feedback_record(eai_feedback_buffer_t *buf, float score, const char *context,
                                  uint32_t input_hash, uint32_t output_hash)
{
    if (!buf) return EAI_ERR_INVALID;
    if (score < -1.0f || score > 1.0f) return EAI_ERR_INVALID;

    eai_feedback_t *entry = &buf->entries[buf->head];
    entry->input_hash = input_hash;
    entry->output_hash = output_hash;
    entry->score = score;
    entry->timestamp = (uint64_t)time(NULL);
    if (context) {
        strncpy(entry->context, context, sizeof(entry->context) - 1);
        entry->context[sizeof(entry->context) - 1] = '\0';
    } else {
        entry->context[0] = '\0';
    }

    buf->head = (buf->head + 1) % EAI_FEEDBACK_MAX;
    if (buf->count < EAI_FEEDBACK_MAX) buf->count++;

    return EAI_OK;
}

int eai_feedback_get_recent(const eai_feedback_buffer_t *buf, eai_feedback_t *out, int max_count)
{
    if (!buf || !out || max_count <= 0) return 0;
    int n = (max_count < buf->count) ? max_count : buf->count;
    for (int i = 0; i < n; i++) {
        int idx = (buf->head - 1 - i + EAI_FEEDBACK_MAX) % EAI_FEEDBACK_MAX;
        memcpy(&out[i], &buf->entries[idx], sizeof(eai_feedback_t));
    }
    return n;
}

float eai_feedback_avg_score(const eai_feedback_buffer_t *buf, int recent_n)
{
    if (!buf || buf->count == 0 || recent_n <= 0) return 0.0f;
    int n = (recent_n < buf->count) ? recent_n : buf->count;
    float sum = 0.0f;
    for (int i = 0; i < n; i++) {
        int idx = (buf->head - 1 - i + EAI_FEEDBACK_MAX) % EAI_FEEDBACK_MAX;
        sum += buf->entries[idx].score;
    }
    return sum / (float)n;
}

void eai_feedback_clear(eai_feedback_buffer_t *buf)
{
    if (!buf) return;
    buf->count = 0;
    buf->head = 0;
}

/* ========== Training Sample Buffer ========== */

eai_status_t eai_training_buf_init(eai_training_buffer_t *buf)
{
    if (!buf) return EAI_ERR_INVALID;
    memset(buf, 0, sizeof(*buf));
    return EAI_OK;
}

eai_status_t eai_training_buf_add(eai_training_buffer_t *buf, const char *input,
                                   const char *target, float score)
{
    if (!buf || !input || !target) return EAI_ERR_INVALID;

    eai_training_sample_t *s = &buf->entries[buf->head];
    strncpy(s->input_text, input, EAI_TRAINING_TEXT_MAX - 1);
    s->input_text[EAI_TRAINING_TEXT_MAX - 1] = '\0';
    strncpy(s->target_text, target, EAI_TRAINING_TEXT_MAX - 1);
    s->target_text[EAI_TRAINING_TEXT_MAX - 1] = '\0';
    s->feedback_score = score;
    s->weight = (score + 1.0f) / 2.0f;  /* normalize [-1,1] → [0,1] */
    s->timestamp = (uint64_t)time(NULL);

    buf->head = (buf->head + 1) % EAI_TRAINING_BUF_MAX;
    if (buf->count < EAI_TRAINING_BUF_MAX) buf->count++;

    return EAI_OK;
}

int eai_training_buf_get_batch(const eai_training_buffer_t *buf,
                                eai_training_sample_t *out, int max_count)
{
    if (!buf || !out || max_count <= 0) return 0;
    int n = (max_count < buf->count) ? max_count : buf->count;
    for (int i = 0; i < n; i++) {
        int idx = (buf->head - 1 - i + EAI_TRAINING_BUF_MAX) % EAI_TRAINING_BUF_MAX;
        memcpy(&out[i], &buf->entries[idx], sizeof(eai_training_sample_t));
    }
    return n;
}

void eai_training_buf_clear(eai_training_buffer_t *buf)
{
    if (!buf) return;
    buf->count = 0;
    buf->head = 0;
}

int eai_training_buf_count(const eai_training_buffer_t *buf)
{
    return buf ? buf->count : 0;
}
