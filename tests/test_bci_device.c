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

static void test_simulator_init(void)
{
    TEST(simulator_init);
    eai_bci_device_t dev;
    eai_status_t st = eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    if (st != EAI_OK) { FAIL("init failed"); return; }
    if (dev.num_channels != 4) { FAIL("expected 4 channels"); return; }
    if (dev.sample_rate_hz != 250) { FAIL("expected 250 Hz"); return; }
    if (dev.state != EAI_BCI_STATE_DISCONNECTED) { FAIL("wrong initial state"); return; }
    eai_bci_device_shutdown(&dev);
    PASS();
}

static void test_simulator_with_params(void)
{
    TEST(simulator_with_params);
    eai_kv_t params[] = {
        {"channels", "8"},
        {"sample_rate", "500"},
        {"paradigm", "1"},
    };
    eai_bci_device_t dev;
    eai_status_t st = eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, params, 3);
    if (st != EAI_OK) { FAIL("init with params failed"); return; }
    if (dev.num_channels != 8) { FAIL("expected 8 channels"); return; }
    if (dev.sample_rate_hz != 500) { FAIL("expected 500 Hz"); return; }
    eai_bci_device_shutdown(&dev);
    PASS();
}

static void test_simulator_stream(void)
{
    TEST(simulator_stream);
    eai_bci_device_t dev;
    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);

    eai_status_t st = eai_bci_device_start(&dev);
    if (st != EAI_OK) { FAIL("start failed"); return; }
    if (dev.state != EAI_BCI_STATE_STREAMING) { FAIL("not streaming"); return; }

    eai_bci_sample_t samples[8];
    int read_count = 0;
    st = eai_bci_device_read(&dev, samples, 8, &read_count);
    if (st != EAI_OK) { FAIL("read failed"); return; }
    if (read_count != 8) { FAIL("expected 8 samples"); return; }

    /* Verify samples have non-zero data */
    int has_data = 0;
    for (int i = 0; i < read_count; i++) {
        if (samples[i].channels[0] != 0.0f) has_data++;
    }
    if (has_data == 0) { FAIL("all samples zero"); return; }

    st = eai_bci_device_stop(&dev);
    if (st != EAI_OK) { FAIL("stop failed"); return; }

    eai_bci_device_shutdown(&dev);
    PASS();
}

static void test_simulator_read_without_stream(void)
{
    TEST(simulator_read_without_stream);
    eai_bci_device_t dev;
    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);

    eai_bci_sample_t samples[4];
    int read_count = 0;
    eai_status_t st = eai_bci_device_read(&dev, samples, 4, &read_count);
    if (st != EAI_ERR_BCI_SIGNAL) { FAIL("expected BCI_SIGNAL error"); return; }

    eai_bci_device_shutdown(&dev);
    PASS();
}

static void test_simulator_impedance(void)
{
    TEST(simulator_impedance);
    eai_bci_device_t dev;
    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);

    float impedance[4];
    eai_status_t st = dev.ops->get_impedance(&dev, impedance, 4);
    if (st != EAI_OK) { FAIL("get_impedance failed"); return; }
    if (impedance[0] < 1000.0f) { FAIL("impedance too low"); return; }

    eai_bci_device_shutdown(&dev);
    PASS();
}

static void test_simulator_calibrate(void)
{
    TEST(simulator_calibrate);
    eai_bci_device_t dev;
    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_device_start(&dev);

    eai_status_t st = dev.ops->calibrate(&dev);
    if (st != EAI_OK) { FAIL("calibrate failed"); return; }

    eai_bci_device_shutdown(&dev);
    PASS();
}

int main(void)
{
    printf("=== EAI BCI Device Tests ===\n");

    test_simulator_init();
    test_simulator_with_params();
    test_simulator_stream();
    test_simulator_read_without_stream();
    test_simulator_impedance();
    test_simulator_calibrate();

    printf("\nResults: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
