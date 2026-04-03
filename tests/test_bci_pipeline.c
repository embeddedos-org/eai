// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include <stdio.h>
#include <string.h>
#include "eai/common.h"
#include "eai_bci/eai_bci.h"

static int tests_run = 0, tests_passed = 0, tests_failed = 0;
#define TEST(name) do { tests_run++; printf("  TEST %-40s ", #name); } while(0)
#define PASS() do { tests_passed++; printf("[PASS]\n"); } while(0)
#define FAIL(msg) do { tests_failed++; printf("[FAIL] %s\n", msg); } while(0)

static int observer_call_count = 0;
static eai_bci_intent_t last_observed_intent;

static void test_observer(const eai_bci_intent_t *intent, void *user_data)
{
    (void)user_data;
    observer_call_count++;
    last_observed_intent = *intent;
}

static void test_pipeline_init(void)
{
    TEST(pipeline_init);
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_output_t out;
    eai_bci_pipeline_t pipe;

    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_output_init(&out, &eai_bci_output_log_ops, NULL, 0);

    eai_status_t st = eai_bci_pipeline_init(&pipe, &dev, &dec, &out, 1.0f, 50.0f, 50.0f);
    if (st != EAI_OK) { FAIL("pipeline init failed"); return; }
    if (pipe.running) { FAIL("should not be running"); return; }

    PASS();
}

static void test_pipeline_start_stop(void)
{
    TEST(pipeline_start_stop);
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_output_t out;
    eai_bci_pipeline_t pipe;

    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_output_init(&out, &eai_bci_output_log_ops, NULL, 0);
    eai_bci_pipeline_init(&pipe, &dev, &dec, &out, 1.0f, 50.0f, 50.0f);

    eai_status_t st = eai_bci_pipeline_start(&pipe);
    if (st != EAI_OK) { FAIL("start failed"); return; }
    if (!pipe.running) { FAIL("should be running"); return; }

    st = eai_bci_pipeline_stop(&pipe);
    if (st != EAI_OK) { FAIL("stop failed"); return; }
    if (pipe.running) { FAIL("should not be running"); return; }

    PASS();
}

static void test_pipeline_step(void)
{
    TEST(pipeline_step);
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_output_t out;
    eai_bci_pipeline_t pipe;

    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_output_init(&out, &eai_bci_output_log_ops, NULL, 0);
    eai_bci_pipeline_init(&pipe, &dev, &dec, &out, 1.0f, 50.0f, 50.0f);
    eai_bci_pipeline_start(&pipe);

    /* Run several pipeline steps */
    for (int i = 0; i < 10; i++) {
        eai_status_t st = eai_bci_pipeline_step(&pipe);
        if (st != EAI_OK) { FAIL("step failed"); return; }
    }

    if (pipe.samples_processed == 0) { FAIL("no samples processed"); return; }
    if (pipe.intents_decoded == 0) { FAIL("no intents decoded"); return; }

    const eai_bci_intent_t *intent = eai_bci_pipeline_last_intent(&pipe);
    if (!intent) { FAIL("no last intent"); return; }
    if (intent->label[0] == '\0') { FAIL("empty intent label"); return; }

    eai_bci_pipeline_stop(&pipe);
    PASS();
}

static void test_pipeline_observer(void)
{
    TEST(pipeline_observer);
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_output_t out;
    eai_bci_pipeline_t pipe;

    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_output_init(&out, &eai_bci_output_log_ops, NULL, 0);
    eai_bci_pipeline_init(&pipe, &dev, &dec, &out, 1.0f, 50.0f, 50.0f);

    observer_call_count = 0;
    eai_bci_pipeline_set_observer(&pipe, test_observer, NULL);

    eai_bci_pipeline_start(&pipe);
    for (int i = 0; i < 5; i++) {
        eai_bci_pipeline_step(&pipe);
    }

    if (observer_call_count != 5) { FAIL("observer not called 5 times"); return; }
    if (last_observed_intent.label[0] == '\0') { FAIL("observer got empty intent"); return; }

    eai_bci_pipeline_stop(&pipe);
    PASS();
}

static void test_pipeline_no_output(void)
{
    TEST(pipeline_no_output);
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_pipeline_t pipe;

    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_pipeline_init(&pipe, &dev, &dec, NULL, 1.0f, 50.0f, 50.0f);

    eai_bci_pipeline_start(&pipe);
    eai_status_t st = eai_bci_pipeline_step(&pipe);
    if (st != EAI_OK) { FAIL("step without output should succeed"); return; }

    eai_bci_pipeline_stop(&pipe);
    PASS();
}

static void test_pipeline_end_to_end(void)
{
    TEST(pipeline_end_to_end);
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_output_t out;
    eai_bci_pipeline_t pipe;

    /* Motor imagery paradigm */
    eai_kv_t params[] = {{"paradigm", "1"}, {"channels", "4"}};
    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, params, 2);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_output_init(&out, &eai_bci_output_log_ops, NULL, 0);
    eai_bci_pipeline_init(&pipe, &dev, &dec, &out, 1.0f, 50.0f, 50.0f);

    eai_bci_pipeline_start(&pipe);

    /* Run enough steps for stable classification */
    for (int i = 0; i < 20; i++) {
        eai_bci_pipeline_step(&pipe);
    }

    const eai_bci_intent_t *intent = eai_bci_pipeline_last_intent(&pipe);
    if (!intent) { FAIL("no intent after 20 steps"); return; }
    if (intent->confidence < 0.0f || intent->confidence > 1.0f) {
        FAIL("confidence out of range");
        return;
    }
    if (intent->class_id >= EAI_BCI_MAX_CLASSES) {
        FAIL("class_id out of range");
        return;
    }

    eai_bci_pipeline_shutdown(&pipe);
    PASS();
}

static void test_threshold_decoder(void)
{
    TEST(threshold_decoder);
    eai_bci_decoder_t dec;
    eai_status_t st = eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    if (st != EAI_OK) { FAIL("decoder init failed"); return; }
    if (!dec.initialized) { FAIL("not initialized"); return; }

    /* Create signal with known power */
    eai_bci_signal_t sig;
    eai_bci_signal_init(&sig, 2, 250);
    eai_bci_sample_t sample = {0};
    for (int i = 0; i < 64; i++) {
        sample.channels[0] = 2.0f;
        sample.channels[1] = 2.0f;
        eai_bci_signal_push(&sig, &sample);
    }

    eai_bci_intent_t intent;
    st = eai_bci_decoder_decode(&dec, &sig, &intent);
    if (st != EAI_OK) { FAIL("decode failed"); return; }
    if (intent.label[0] == '\0') { FAIL("empty label"); return; }

    eai_bci_decoder_shutdown(&dec);
    PASS();
}

int main(void)
{
    printf("=== EAI BCI Pipeline Tests ===\n");

    test_pipeline_init();
    test_pipeline_start_stop();
    test_pipeline_step();
    test_pipeline_observer();
    test_pipeline_no_output();
    test_pipeline_end_to_end();
    test_threshold_decoder();

    printf("\nResults: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
