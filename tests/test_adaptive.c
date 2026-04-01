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
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// Adaptive / Personalized AI — Unit Tests

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* Inline stubs to avoid linking full eai_common */
#include "eai/types.h"

void eai_log_set_level(eai_log_level_t level) { (void)level; }
void eai_log_set_output(FILE *fp) { (void)fp; }
void eai_log_write(eai_log_level_t level, const char *mod, const char *fmt, ...) {
    (void)level; (void)mod; (void)fmt;
}

const char *eai_status_str(eai_status_t s) {
    (void)s; return "OK";
}

#include "eai/adaptive.h"

/* Need pow() from math — included via adaptive.c */

static int tests_run = 0;
static int tests_pass = 0;

#define TEST(name) do { tests_run++; printf("  [TEST] %-50s", name); } while(0)
#define PASS()     do { tests_pass++; printf(" PASS\n"); } while(0)
#define FAIL(msg)  do { printf(" FAIL: %s\n", msg); } while(0)

/* ========== Preference Store Tests ========== */

static void test_pref_init(void) {
    TEST("pref_init");
    eai_preference_store_t store;
    eai_status_t s = eai_pref_init(&store, "test_prefs.dat");
    assert(s == EAI_OK);
    assert(store.count == 0);
    assert(strcmp(store.storage_path, "test_prefs.dat") == 0);
    PASS();
}

static void test_pref_set_get(void) {
    TEST("pref_set_get");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_status_t s = eai_pref_set(&store, "language", "English", EAI_PREF_STRING, 0.9f);
    assert(s == EAI_OK);
    assert(store.count == 1);

    const char *val = eai_pref_get(&store, "language");
    assert(val != NULL);
    assert(strcmp(val, "English") == 0);

    float w = eai_pref_get_weight(&store, "language");
    assert(fabsf(w - 0.9f) < 0.001f);
    PASS();
}

static void test_pref_update(void) {
    TEST("pref_update_existing_key");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_pref_set(&store, "theme", "dark", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "theme", "light", EAI_PREF_STRING, 0.8f);

    assert(store.count == 1);
    const char *val = eai_pref_get(&store, "theme");
    assert(strcmp(val, "light") == 0);
    assert(fabsf(eai_pref_get_weight(&store, "theme") - 0.8f) < 0.001f);
    PASS();
}

static void test_pref_delete(void) {
    TEST("pref_delete");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_pref_set(&store, "a", "1", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "b", "2", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "c", "3", EAI_PREF_STRING, 0.5f);
    assert(store.count == 3);

    eai_status_t s = eai_pref_delete(&store, "b");
    assert(s == EAI_OK);
    assert(store.count == 2);
    assert(eai_pref_get(&store, "b") == NULL);
    assert(eai_pref_get(&store, "a") != NULL);
    assert(eai_pref_get(&store, "c") != NULL);
    PASS();
}

static void test_pref_delete_not_found(void) {
    TEST("pref_delete_not_found");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_status_t s = eai_pref_delete(&store, "nonexistent");
    assert(s == EAI_ERR_NOT_FOUND);
    PASS();
}

static void test_pref_clear(void) {
    TEST("pref_clear");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    eai_pref_set(&store, "x", "1", EAI_PREF_STRING, 0.5f);
    eai_pref_set(&store, "y", "2", EAI_PREF_STRING, 0.5f);
    assert(store.count == 2);

    eai_pref_clear(&store);
    assert(store.count == 0);
    assert(eai_pref_get(&store, "x") == NULL);
    PASS();
}

static void test_pref_invalid_weight(void) {
    TEST("pref_invalid_weight");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    assert(eai_pref_set(&store, "k", "v", EAI_PREF_STRING, -0.1f) == EAI_ERR_INVALID);
    assert(eai_pref_set(&store, "k", "v", EAI_PREF_STRING, 1.1f) == EAI_ERR_INVALID);
    assert(store.count == 0);
    PASS();
}

static void test_pref_count(void) {
    TEST("pref_count");
    eai_preference_store_t store;
    eai_pref_init(&store, NULL);

    assert(eai_pref_count(&store) == 0);
    eai_pref_set(&store, "a", "1", EAI_PREF_STRING, 0.5f);
    assert(eai_pref_count(&store) == 1);
    eai_pref_set(&store, "b", "2", EAI_PREF_STRING, 0.5f);
    assert(eai_pref_count(&store) == 2);
    PASS();
}

/* ========== Feedback Buffer Tests ========== */

static void test_feedback_init(void) {
    TEST("feedback_init");
    eai_feedback_buffer_t buf;
    assert(eai_feedback_init(&buf) == EAI_OK);
    assert(buf.count == 0);
    assert(buf.head == 0);
    PASS();
}

static void test_feedback_record(void) {
    TEST("feedback_record");
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    assert(eai_feedback_record(&buf, 0.8f, "good response", 123, 456) == EAI_OK);
    assert(buf.count == 1);
    assert(buf.head == 1);
    PASS();
}

static void test_feedback_invalid_score(void) {
    TEST("feedback_invalid_score");
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    assert(eai_feedback_record(&buf, -1.5f, NULL, 0, 0) == EAI_ERR_INVALID);
    assert(eai_feedback_record(&buf, 1.5f, NULL, 0, 0) == EAI_ERR_INVALID);
    assert(buf.count == 0);
    PASS();
}

static void test_feedback_get_recent(void) {
    TEST("feedback_get_recent");
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    eai_feedback_record(&buf, 0.5f, "ok", 1, 1);
    eai_feedback_record(&buf, 0.9f, "great", 2, 2);
    eai_feedback_record(&buf, -0.3f, "bad", 3, 3);

    eai_feedback_t recent[5];
    int n = eai_feedback_get_recent(&buf, recent, 5);
    assert(n == 3);
    /* most recent first */
    assert(fabsf(recent[0].score - (-0.3f)) < 0.001f);
    assert(fabsf(recent[1].score - 0.9f) < 0.001f);
    assert(fabsf(recent[2].score - 0.5f) < 0.001f);
    PASS();
}

static void test_feedback_avg_score(void) {
    TEST("feedback_avg_score");
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    eai_feedback_record(&buf, 0.8f, NULL, 0, 0);
    eai_feedback_record(&buf, 0.6f, NULL, 0, 0);
    eai_feedback_record(&buf, 0.4f, NULL, 0, 0);

    float avg = eai_feedback_avg_score(&buf, 3);
    assert(fabsf(avg - 0.6f) < 0.001f);
    PASS();
}

static void test_feedback_ring_buffer(void) {
    TEST("feedback_ring_buffer_wraparound");
    eai_feedback_buffer_t buf;
    eai_feedback_init(&buf);

    for (int i = 0; i < EAI_FEEDBACK_MAX + 10; i++) {
        eai_feedback_record(&buf, (float)i / (float)(EAI_FEEDBACK_MAX + 10), NULL, (uint32_t)i, 0);
    }
    assert(buf.count == EAI_FEEDBACK_MAX);
    PASS();
}

/* ========== Training Buffer Tests ========== */

static void test_training_buf_init(void) {
    TEST("training_buf_init");
    eai_training_buffer_t buf;
    assert(eai_training_buf_init(&buf) == EAI_OK);
    assert(buf.count == 0);
    PASS();
}

static void test_training_buf_add(void) {
    TEST("training_buf_add");
    eai_training_buffer_t buf;
    eai_training_buf_init(&buf);

    assert(eai_training_buf_add(&buf, "hello", "world", 0.8f) == EAI_OK);
    assert(buf.count == 1);
    assert(eai_training_buf_count(&buf) == 1);
    PASS();
}

static void test_training_buf_get_batch(void) {
    TEST("training_buf_get_batch");
    eai_training_buffer_t buf;
    eai_training_buf_init(&buf);

    eai_training_buf_add(&buf, "in1", "out1", 0.5f);
    eai_training_buf_add(&buf, "in2", "out2", 0.9f);

    eai_training_sample_t batch[5];
    int n = eai_training_buf_get_batch(&buf, batch, 5);
    assert(n == 2);
    assert(strcmp(batch[0].input_text, "in2") == 0);
    assert(strcmp(batch[1].input_text, "in1") == 0);
    PASS();
}

static void test_training_buf_clear(void) {
    TEST("training_buf_clear");
    eai_training_buffer_t buf;
    eai_training_buf_init(&buf);

    eai_training_buf_add(&buf, "a", "b", 0.5f);
    eai_training_buf_clear(&buf);
    assert(eai_training_buf_count(&buf) == 0);
    PASS();
}

/* ========== Adaptive Config Tests ========== */

static void test_adaptive_config_defaults(void) {
    TEST("adaptive_config_defaults");
    eai_adaptive_config_t cfg;
    assert(eai_adaptive_config_defaults(&cfg) == EAI_OK);
    assert(cfg.enable_learning == true);
    assert(cfg.enable_preferences == true);
    assert(fabsf(cfg.learning_rate - 0.0001f) < 0.00001f);
    assert(cfg.lora_rank == 8);
    assert(cfg.max_training_memory_mb == 512);
    assert(cfg.max_training_steps_per_cycle == 100);
    PASS();
}

int main(void) {
    printf("EAI Adaptive/Personalized AI — Unit Tests\n");
    printf("==========================================\n\n");

    /* Preference Store */
    test_pref_init();
    test_pref_set_get();
    test_pref_update();
    test_pref_delete();
    test_pref_delete_not_found();
    test_pref_clear();
    test_pref_invalid_weight();
    test_pref_count();

    /* Feedback Buffer */
    test_feedback_init();
    test_feedback_record();
    test_feedback_invalid_score();
    test_feedback_get_recent();
    test_feedback_avg_score();
    test_feedback_ring_buffer();

    /* Training Buffer */
    test_training_buf_init();
    test_training_buf_add();
    test_training_buf_get_batch();
    test_training_buf_clear();

    /* Config */
    test_adaptive_config_defaults();

    printf("\n==========================================\n");
    printf("Results: %d/%d passed\n", tests_pass, tests_run);
    return (tests_pass == tests_run) ? 0 : 1;
}
