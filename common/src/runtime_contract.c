// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai/runtime_contract.h"
#include "eai/log.h"
#include <string.h>

#define LOG_MOD "runtime"

const char *eai_status_str(eai_status_t status)
{
    switch (status) {
        case EAI_OK:                return "OK";
        case EAI_ERR_NOMEM:         return "ERR_NOMEM";
        case EAI_ERR_INVALID:       return "ERR_INVALID";
        case EAI_ERR_NOT_FOUND:     return "ERR_NOT_FOUND";
        case EAI_ERR_IO:            return "ERR_IO";
        case EAI_ERR_TIMEOUT:       return "ERR_TIMEOUT";
        case EAI_ERR_PERMISSION:    return "ERR_PERMISSION";
        case EAI_ERR_RUNTIME:       return "ERR_RUNTIME";
        case EAI_ERR_CONNECT:       return "ERR_CONNECT";
        case EAI_ERR_PROTOCOL:      return "ERR_PROTOCOL";
        case EAI_ERR_CONFIG:        return "ERR_CONFIG";
        case EAI_ERR_UNSUPPORTED:   return "ERR_UNSUPPORTED";
        case EAI_ERR_LEARNING:      return "ERR_LEARNING";
        case EAI_ERR_ADAPTER:       return "ERR_ADAPTER";
        case EAI_ERR_STALE_MODEL:   return "ERR_STALE_MODEL";
        case EAI_ERR_RESOURCE_BUDGET: return "ERR_RESOURCE_BUDGET";
        case EAI_ERR_BCI_SIGNAL:     return "ERR_BCI_SIGNAL";
        case EAI_ERR_BCI_CALIBRATION: return "ERR_BCI_CALIBRATION";
        case EAI_ERR_BCI_DECODE:     return "ERR_BCI_DECODE";
        default:                    return "UNKNOWN";
    }
}

eai_status_t eai_runtime_init(eai_runtime_t *rt, const eai_runtime_ops_t *ops)
{
    if (!rt || !ops) return EAI_ERR_INVALID;
    memset(rt, 0, sizeof(*rt));
    rt->ops = ops;
    rt->loaded = false;

    if (ops->init) {
        return ops->init(rt);
    }
    return EAI_OK;
}

eai_status_t eai_runtime_load(eai_runtime_t *rt, const eai_model_manifest_t *m, const char *path)
{
    if (!rt || !rt->ops || !m || !path) return EAI_ERR_INVALID;
    if (!rt->ops->load_model) return EAI_ERR_UNSUPPORTED;

    eai_status_t s = rt->ops->load_model(rt, m, path);
    if (s == EAI_OK) {
        rt->loaded = true;
        memcpy(&rt->manifest, m, sizeof(eai_model_manifest_t));
        EAI_LOG_INFO(LOG_MOD, "model loaded: %s", m->name);
    }
    return s;
}

eai_status_t eai_runtime_infer(eai_runtime_t *rt, const eai_inference_input_t *in,
                                eai_inference_output_t *out)
{
    if (!rt || !rt->ops || !in || !out) return EAI_ERR_INVALID;
    if (!rt->loaded) return EAI_ERR_RUNTIME;
    if (!rt->ops->infer) return EAI_ERR_UNSUPPORTED;

    return rt->ops->infer(rt, in, out);
}

eai_status_t eai_runtime_unload(eai_runtime_t *rt)
{
    if (!rt || !rt->ops) return EAI_ERR_INVALID;
    if (!rt->loaded) return EAI_OK;

    eai_status_t s = EAI_OK;
    if (rt->ops->unload_model) {
        s = rt->ops->unload_model(rt);
    }
    rt->loaded = false;
    return s;
}

void eai_runtime_shutdown(eai_runtime_t *rt)
{
    if (!rt || !rt->ops) return;
    if (rt->ops->shutdown) {
        rt->ops->shutdown(rt);
    }
}

/* ========== Adaptive Runtime Operations ========== */

eai_status_t eai_runtime_load_adapter(eai_runtime_t *rt, const char *adapter_path)
{
    if (!rt || !rt->ops || !adapter_path) return EAI_ERR_INVALID;
    if (!rt->loaded) return EAI_ERR_RUNTIME;
    if (!rt->ops->load_adapter) return EAI_ERR_UNSUPPORTED;

    eai_status_t s = rt->ops->load_adapter(rt, adapter_path);
    if (s == EAI_OK) {
        EAI_LOG_INFO(LOG_MOD, "adapter loaded: %s", adapter_path);
    }
    return s;
}

eai_status_t eai_runtime_unload_adapter(eai_runtime_t *rt)
{
    if (!rt || !rt->ops) return EAI_ERR_INVALID;
    if (!rt->ops->unload_adapter) return EAI_ERR_UNSUPPORTED;

    return rt->ops->unload_adapter(rt);
}

eai_status_t eai_runtime_train_step(eai_runtime_t *rt, const eai_training_input_t *in,
                                     eai_training_output_t *out)
{
    if (!rt || !rt->ops || !in || !out) return EAI_ERR_INVALID;
    if (!rt->loaded) return EAI_ERR_RUNTIME;
    if (!rt->ops->train_step) return EAI_ERR_UNSUPPORTED;

    return rt->ops->train_step(rt, in, out);
}

eai_status_t eai_runtime_save_checkpoint(eai_runtime_t *rt, const char *path)
{
    if (!rt || !rt->ops || !path) return EAI_ERR_INVALID;
    if (!rt->ops->save_checkpoint) return EAI_ERR_UNSUPPORTED;

    eai_status_t s = rt->ops->save_checkpoint(rt, path);
    if (s == EAI_OK) {
        EAI_LOG_INFO(LOG_MOD, "checkpoint saved: %s", path);
    }
    return s;
}

eai_status_t eai_runtime_get_training_state(eai_runtime_t *rt, eai_training_state_t *state)
{
    if (!rt || !rt->ops || !state) return EAI_ERR_INVALID;
    if (!rt->ops->get_training_state) return EAI_ERR_UNSUPPORTED;

    return rt->ops->get_training_state(rt, state);
}
