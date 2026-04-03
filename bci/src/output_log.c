// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/output.h"
#include "eai/log.h"
#include <stdio.h>
#include <string.h>

/* ========== Log Output Backend ========== */

static eai_status_t log_init(eai_bci_output_t *out, const eai_kv_t *params, int param_count)
{
    (void)params;
    (void)param_count;
    out->initialized = true;
    return EAI_OK;
}

static eai_status_t log_execute(eai_bci_output_t *out, const eai_bci_intent_t *intent)
{
    (void)out;
    if (!intent) return EAI_ERR_INVALID;

    EAI_LOG_INFO("BCI Intent: class=%u label='%s' confidence=%.3f timestamp=%llu",
                 intent->class_id, intent->label, intent->confidence,
                 (unsigned long long)intent->timestamp_us);
    return EAI_OK;
}

static void log_shutdown(eai_bci_output_t *out)
{
    out->initialized = false;
}

const eai_bci_output_ops_t eai_bci_output_log_ops = {
    .name     = "log",
    .type     = EAI_BCI_OUT_LOG,
    .init     = log_init,
    .execute  = log_execute,
    .shutdown = log_shutdown,
};
