// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/adaptive.h"
#include "eai/log.h"
#include <string.h>
#include <stdlib.h>

#define LOG_MOD "fw-adaptive"

eai_status_t eai_fw_adaptive_init(eai_fw_adaptive_t *adaptive,
                                   eai_runtime_t *runtime,
                                   eai_fw_policy_t *policy)
{
    if (!adaptive) return EAI_ERR_INVALID;
    memset(adaptive, 0, sizeof(*adaptive));
    adaptive->runtime = runtime;
    adaptive->policy = policy;

    adaptive->prefs = calloc(1, sizeof(eai_preference_store_t));
    adaptive->feedback = calloc(1, sizeof(eai_feedback_buffer_t));
    adaptive->training = calloc(1, sizeof(eai_training_buffer_t));

    if (!adaptive->prefs || !adaptive->feedback || !adaptive->training) {
        eai_fw_adaptive_shutdown(adaptive);
        return EAI_ERR_NOMEM;
    }

    eai_pref_init(adaptive->prefs, NULL);
    eai_feedback_init(adaptive->feedback);
    eai_training_buf_init(adaptive->training);
    eai_adaptive_config_defaults(&adaptive->config);

    EAI_LOG_INFO(LOG_MOD, "adaptive engine initialized");
    return EAI_OK;
}

void eai_fw_adaptive_shutdown(eai_fw_adaptive_t *adaptive)
{
    if (!adaptive) return;
    free(adaptive->prefs);
    free(adaptive->feedback);
    free(adaptive->training);
    adaptive->prefs = NULL;
    adaptive->feedback = NULL;
    adaptive->training = NULL;
    EAI_LOG_INFO(LOG_MOD, "adaptive engine shutdown");
}

bool eai_fw_adaptive_should_train(const eai_fw_adaptive_t *adaptive,
                                   uint32_t available_ram_mb,
                                   float cpu_temp_c)
{
    if (!adaptive) return false;
    if (!adaptive->config.enable_learning) return false;

    /* check policy */
    if (adaptive->policy && !adaptive->policy->allow_on_device_learning)
        return false;

    /* resource gating */
    if (available_ram_mb < adaptive->config.max_training_memory_mb) {
        EAI_LOG_DEBUG(LOG_MOD, "training skipped: RAM %u < %u MB",
                      available_ram_mb, adaptive->config.max_training_memory_mb);
        return false;
    }
    if (cpu_temp_c > 80.0f) {
        EAI_LOG_DEBUG(LOG_MOD, "training skipped: CPU temp %.1f > 80°C", cpu_temp_c);
        return false;
    }

    /* need training samples */
    if (!adaptive->training || adaptive->training->count == 0)
        return false;

    return true;
}

eai_status_t eai_fw_adaptive_run_training_cycle(eai_fw_adaptive_t *adaptive)
{
    if (!adaptive) return EAI_ERR_INVALID;
    if (!adaptive->runtime) return EAI_ERR_INVALID;
    if (!adaptive->training || adaptive->training->count == 0) {
        EAI_LOG_DEBUG(LOG_MOD, "no training samples, skipping cycle");
        return EAI_OK;
    }

    /* check runtime supports training */
    if (!adaptive->runtime->ops || !adaptive->runtime->ops->train_step) {
        EAI_LOG_DEBUG(LOG_MOD, "runtime does not support training");
        return EAI_ERR_UNSUPPORTED;
    }

    uint32_t max_steps = adaptive->config.max_training_steps_per_cycle;
    if (adaptive->policy)
        max_steps = adaptive->policy->max_training_steps_per_cycle;

    int batch_size = (int)max_steps;
    if (batch_size > adaptive->training->count)
        batch_size = adaptive->training->count;
    if (batch_size > 32) batch_size = 32;

    eai_training_sample_t *samples = calloc((size_t)batch_size, sizeof(eai_training_sample_t));
    if (!samples) return EAI_ERR_NOMEM;

    int n = eai_training_buf_get_batch(adaptive->training, samples, batch_size);

    EAI_LOG_INFO(LOG_MOD, "training cycle: %d samples, max %u steps", n, max_steps);

    uint32_t steps = 0;
    for (int i = 0; i < n && steps < max_steps; i++, steps++) {
        eai_training_input_t in = {
            .input_text = samples[i].input_text,
            .target_text = samples[i].target_text,
            .learning_rate = adaptive->config.learning_rate,
            .lora_rank = adaptive->config.lora_rank,
        };
        eai_training_output_t out = {0};

        eai_status_t s = adaptive->runtime->ops->train_step(adaptive->runtime, &in, &out);
        if (s != EAI_OK) {
            EAI_LOG_WARN(LOG_MOD, "train_step failed at step %u: %s", steps, eai_status_str(s));
            break;
        }
        adaptive->last_loss = out.loss;
    }

    adaptive->cycles_run++;
    free(samples);

    EAI_LOG_INFO(LOG_MOD, "training cycle %u complete: %u steps, loss=%.4f",
                 adaptive->cycles_run, steps, adaptive->last_loss);
    return EAI_OK;
}

eai_status_t eai_fw_adaptive_update_preferences(eai_fw_adaptive_t *adaptive, float elapsed_days)
{
    if (!adaptive || !adaptive->prefs) return EAI_ERR_INVALID;
    return eai_pref_decay(adaptive->prefs, elapsed_days);
}

eai_status_t eai_fw_adaptive_record_feedback(eai_fw_adaptive_t *adaptive,
                                              float score, const char *context,
                                              const char *input, const char *output)
{
    if (!adaptive) return EAI_ERR_INVALID;

    /* record in feedback buffer */
    if (adaptive->feedback) {
        eai_status_t s = eai_feedback_record(adaptive->feedback, score, context, 0, 0);
        if (s != EAI_OK) return s;
    }

    /* generate training sample from positive feedback */
    if (score > 0.5f && input && output && adaptive->training) {
        eai_training_buf_add(adaptive->training, input, output, score);
        EAI_LOG_DEBUG(LOG_MOD, "generated training sample from positive feedback (score=%.2f)", score);
    }

    return EAI_OK;
}
