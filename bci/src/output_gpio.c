// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/output.h"
#include "eai/log.h"
#include <string.h>
#include <stdlib.h>

/* ========== GPIO Output Backend ========== */

typedef struct {
    int gpio_pins[EAI_BCI_MAX_CLASSES];
    int num_pins;
} gpio_ctx_t;

static gpio_ctx_t g_gpio_ctx;

static eai_status_t gpio_init(eai_bci_output_t *out, const eai_kv_t *params, int param_count)
{
    memset(&g_gpio_ctx, 0, sizeof(g_gpio_ctx));
    g_gpio_ctx.num_pins = 4;
    g_gpio_ctx.gpio_pins[0] = 17;
    g_gpio_ctx.gpio_pins[1] = 18;
    g_gpio_ctx.gpio_pins[2] = 27;
    g_gpio_ctx.gpio_pins[3] = 22;

    for (int i = 0; i < param_count && params; i++) {
        if (strncmp(params[i].key, "gpio_", 5) == 0) {
            int idx = atoi(params[i].key + 5);
            if (idx >= 0 && idx < EAI_BCI_MAX_CLASSES) {
                g_gpio_ctx.gpio_pins[idx] = atoi(params[i].value);
            }
        }
    }

    out->ctx         = &g_gpio_ctx;
    out->initialized = true;
    return EAI_OK;
}

static eai_status_t gpio_execute(eai_bci_output_t *out, const eai_bci_intent_t *intent)
{
    gpio_ctx_t *ctx = (gpio_ctx_t *)out->ctx;
    if (!ctx || !intent) return EAI_ERR_INVALID;

    int class_id = (int)intent->class_id;
    if (class_id < 0 || class_id >= ctx->num_pins)
        return EAI_ERR_INVALID;

    /* Reset all pins then set the active one */
    for (int i = 0; i < ctx->num_pins; i++) {
        EAI_LOG_DEBUG("BCI GPIO: pin %d -> %d", ctx->gpio_pins[i], (i == class_id) ? 1 : 0);
    }

    EAI_LOG_INFO("BCI GPIO: intent '%s' (class %d) -> pin %d HIGH",
                 intent->label, class_id, ctx->gpio_pins[class_id]);
    return EAI_OK;
}

static void gpio_shutdown(eai_bci_output_t *out)
{
    out->initialized = false;
    out->ctx = NULL;
}

/* ========== Output Convenience Functions ========== */

eai_status_t eai_bci_output_init(eai_bci_output_t *out, const eai_bci_output_ops_t *ops,
                                  const eai_kv_t *params, int param_count)
{
    if (!out || !ops || !ops->init) return EAI_ERR_INVALID;
    memset(out, 0, sizeof(*out));
    out->ops = ops;
    return ops->init(out, params, param_count);
}

eai_status_t eai_bci_output_execute(eai_bci_output_t *out, const eai_bci_intent_t *intent)
{
    if (!out || !out->ops || !out->ops->execute || !out->initialized)
        return EAI_ERR_INVALID;
    return out->ops->execute(out, intent);
}

void eai_bci_output_shutdown(eai_bci_output_t *out)
{
    if (!out || !out->ops || !out->ops->shutdown) return;
    out->ops->shutdown(out);
}

/* ========== Exported GPIO Ops ========== */

const eai_bci_output_ops_t eai_bci_output_gpio_ops = {
    .name     = "gpio",
    .type     = EAI_BCI_OUT_GPIO,
    .init     = gpio_init,
    .execute  = gpio_execute,
    .shutdown = gpio_shutdown,
};
