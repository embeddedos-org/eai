// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai/adaptive.h"
#include "eai/config.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define TEST(name) static int name(void)
#define RUN(fn) do { printf("  %-50s", #fn); if (fn() == 0) { printf("PASS\n"); pass++; } else { printf("FAIL\n"); fail++; } } while(0)

/* ========== Preference Store Tests ========== */

TEST(test_pref_init)
{
    eai_preference_store_t store;
    assert(eai_pref_init(&store, "/tmp/test_prefs.dat") == EAI_OK);
    assert(store.count == 0);
    assert(strcmp(store.storage_path, "/tmp/test_prefs.dat") == 0);
    return 0;
}

TEST(test_pref_set_get)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    assert(eai_pref_set(&store, "language", "English", EAI_PREF_STRING, 0.9f) == EAI_OK);
    assert(store.count == 1);

    const char *val = eai_pref_get(&store, "language");
    assert(val != NULL);
    assert(strcmp(val, "English") == 0);

    float w = eai_pref_get_weight(&store, "language");
    assert(w > 0.89f && w < 0.91f);
    return 0;
}

TEST(test_pref_update_existing)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_pref_set(&store, "theme", "dark", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "theme", "light", EAI_PREF_STRING, 0.8f);
    assert(store.count == 1);
    assert(strcmp(eai_pref_get(&store, "theme"), "light") == 0);
    assert(eai_pref_get_weight(&store, "theme") > 0.79f);
    return 0;
}

TEST(test_pref_delete)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_pref_set(&store, "key1", "val1", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "key2", "val2", EAI_PREF_STRING, 0.5f);
    assert(store.count == 2);

    assert(eai_pref_delete(&store, "key1") == EAI_OK);
    assert(store.count == 1);
    assert(eai_pref_get(&store, "key1") == NULL);
    assert(eai_pref_get(&store, "key2") != NULL);
    return 0;
}

TEST(test_pref_not_found)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);
    assert(eai_pref_get(&store, "nonexistent") == NULL);
    assert(eai_pref_get_weight(&store, "nonexistent") == 0.0f);
    assert(eai_pref_delete(&store, "nonexistent") == EAI_ERR_NOT_FOUND);
    return 0;
}

TEST(test_pref_decay)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_pref_set(&store, "strong", "val", EAI_PREF_STRING, 1.0f);
    eai_pref_set(&store, "weak", "val", EAI_PREF_STRING, 0.02f);

    assert(eai_pref_decay(&store, 30.0f) == EAI_OK);

    /* strong pref should survive decay */
    assert(eai_pref_get(&store, "strong") != NULL);
    float w = eai_pref_get_weight(&store, "strong");
    assert(w < 1.0f);
    assert(w > 0.0f);

    /* weak pref should be evicted (below 0.01 threshold) */
    assert(eai_pref_get(&store, "weak") == NULL);
    return 0;
}

TEST(test_pref_save_load)
{
    const char *path = "test_prefs_persist.dat";

    /* save */
    eai_preference_store_t store1;
    eai_pref_init(&store1, path);
    eai_pref_set(&store1, "color", "blue", EAI_PREF_STRING, 0.7f);
    eai_pref_set(&store1, "size", "42", EAI_PREF_INT, 0.9f);
    assert(eai_pref_save(&store1) == EAI_OK);

    /* load into new store */
    eai_preference_store_t store2;
    eai_pref_init(&store2, path);
    assert(eai_pref_load(&store2) == EAI_OK);
    assert(store2.count == 2);
    assert(strcmp(eai_pref_get(&store2, "color"), "blue") == 0);
    assert(strcmp(eai_pref_get(&store2, "size"), "42") == 0);

    /* cleanup */
    remove(path);
    return 0;
}

TEST(test_pref_clear)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);
    eai_pref_set(&store, "k1", "v1", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "k2", "v2", EAI_PREF_STRING, 0.5f);
    assert(store.count == 2);

    eai_pref_clear(&store);
    assert(store.count == 0);
    assert(eai_pref_get(&store, "k1") == NULL);
    return 0;
}

TEST(test_pref_eviction)
{
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    /* fill store to max */
    char key[64];
    for (int i = 0; i < EAI_PREF_MAX_ENTRIES; i++) {
        snprintf(key, sizeof(key), "key_%d", i);
        eai_pref_set(&store, key, "value", EAI_PREF_STRING, 0.5f);
    }
    assert(store.count == EAI_PREF_MAX_ENTRIES);

    /* set one with very low weight */
    eai_pref_set(&store, "key_0", "value", EAI_PREF_STRING, 0.001f);

    /* adding new entry should evict lowest weight */
    assert(eai_pref_set(&store, "new_key", "new_val", EAI_PREF_STRING, 1.0f) == EAI_OK);
    assert(store.count == EAI_PREF_MAX_ENTRIES);
    assert(eai_pref_get(&store, "new_key") != NULL);
    return 0;
}

/* ========== Feedback Buffer Tests ========== */

TEST(test_feedback_init)
{
    eai_feedback_buffer_t buf;
    assert(eai_feedback_init(&buf) == EAI_OK);
    assert(buf.count == 0);
    assert(buf.head == 0);
    return 0;
}

TEST(test_feedback_record)
{
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    assert(eai_feedback_record(&buf, 0.8f, "good response", 123, 456) == EAI_OK);
    assert(buf.count == 1);

    eai_feedback_t out[1];
    int n = eai_feedback_get_recent(&buf, out, 1);
    assert(n == 1);
    assert(out[0].score > 0.79f && out[0].score < 0.81f);
    assert(out[0].input_hash == 123);
    assert(out[0].output_hash == 456);
    assert(strcmp(out[0].context, "good response") == 0);
    return 0;
}

TEST(test_feedback_invalid_score)
{
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    assert(eai_feedback_record(&buf, 1.5f, "too high", 0, 0) == EAI_ERR_INVALID);
    assert(eai_feedback_record(&buf, -1.5f, "too low", 0, 0) == EAI_ERR_INVALID);
    assert(buf.count == 0);
    return 0;
}

TEST(test_feedback_ring_buffer)
{
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    /* fill past capacity to test wraparound */
    for (int i = 0; i < EAI_FEEDBACK_MAX + 10; i++) {
        eai_feedback_record(&buf, (float)i / (float)(EAI_FEEDBACK_MAX + 10), NULL, 0, 0);
    }
    assert(buf.count == EAI_FEEDBACK_MAX);
    return 0;
}

TEST(test_feedback_avg_score)
{
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    eai_feedback_record(&buf, 0.5f, NULL, 0, 0);
    eai_feedback_record(&buf, 1.0f, NULL, 0, 0);
    eai_feedback_record(&buf, 0.0f, NULL, 0, 0);

    float avg = eai_feedback_avg_score(&buf, 3);
    assert(avg > 0.49f && avg < 0.51f);
    return 0;
}

/* ========== Training Buffer Tests ========== */

TEST(test_training_buf_init)
{
    eai_training_buffer_t buf;
    assert(eai_training_buf_init(&buf) == EAI_OK);
    assert(buf.count == 0);
    return 0;
}

TEST(test_training_buf_add)
{
    eai_training_buffer_t buf;
    eai_training_buf_init(&buf);

    assert(eai_training_buf_add(&buf, "input text", "target text", 0.9f) == EAI_OK);
    assert(buf.count == 1);

    eai_training_sample_t out[1];
    int n = eai_training_buf_get_batch(&buf, out, 1);
    assert(n == 1);
    assert(strcmp(out[0].input_text, "input text") == 0);
    assert(strcmp(out[0].target_text, "target text") == 0);
    return 0;
}

TEST(test_training_buf_ring)
{
    eai_training_buffer_t buf;
    eai_training_buf_init(&buf);

    for (int i = 0; i < EAI_TRAINING_BUF_MAX + 5; i++) {
        eai_training_buf_add(&buf, "in", "out", 0.5f);
    }
    assert(buf.count == EAI_TRAINING_BUF_MAX);
    assert(eai_training_buf_count(&buf) == EAI_TRAINING_BUF_MAX);
    return 0;
}

TEST(test_training_buf_clear)
{
    eai_training_buffer_t buf;
    eai_training_buf_init(&buf);

    eai_training_buf_add(&buf, "in", "out", 0.5f);
    assert(buf.count == 1);

    eai_training_buf_clear(&buf);
    assert(buf.count == 0);
    assert(eai_training_buf_count(&buf) == 0);
    return 0;
}

/* ========== Adaptive Config Tests ========== */

TEST(test_adaptive_config_defaults)
{
    eai_adaptive_config_t cfg;
    assert(eai_adaptive_config_defaults(&cfg) == EAI_OK);
    assert(cfg.enable_learning == true);
    assert(cfg.enable_preferences == true);
    assert(cfg.learning_rate > 0.00009f && cfg.learning_rate < 0.00011f);
    assert(cfg.lora_rank == 8);
    assert(cfg.preference_decay_days > 29.0f && cfg.preference_decay_days < 31.0f);
    assert(cfg.max_training_memory_mb == 512);
    assert(cfg.train_during_idle == true);
    assert(cfg.max_training_steps_per_cycle == 100);
    return 0;
}

TEST(test_adaptive_profile_loading)
{
    eai_config_t cfg;
    eai_config_init(&cfg);
    assert(eai_config_load_profile(&cfg, "adaptive-edge") == EAI_OK);
    assert(cfg.adaptive.enable_learning == true);
    assert(cfg.adaptive.lora_rank == 8);
    assert(cfg.adaptive.max_training_memory_mb == 512);
    assert(cfg.tool_count == 7);
    return 0;
}

/* ========== Main ========== */

int main(void)
{
    int pass = 0, fail = 0;
    printf("=== EAI Adaptive Tests ===\n\n");

    printf("[Preference Store]\n");
    RUN(test_pref_init);
    RUN(test_pref_set_get);
    RUN(test_pref_update_existing);
    RUN(test_pref_delete);
    RUN(test_pref_not_found);
    RUN(test_pref_decay);
    RUN(test_pref_save_load);
    RUN(test_pref_clear);
    RUN(test_pref_eviction);

    printf("\n[Feedback Buffer]\n");
    RUN(test_feedback_init);
    RUN(test_feedback_record);
    RUN(test_feedback_invalid_score);
    RUN(test_feedback_ring_buffer);
    RUN(test_feedback_avg_score);

    printf("\n[Training Buffer]\n");
    RUN(test_training_buf_init);
    RUN(test_training_buf_add);
    RUN(test_training_buf_ring);
    RUN(test_training_buf_clear);

    printf("\n[Adaptive Config]\n");
    RUN(test_adaptive_config_defaults);
    RUN(test_adaptive_profile_loading);

    printf("\n=== Results: %d passed, %d failed ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
