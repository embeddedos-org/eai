// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/config.h"
#include "eai/log.h"
#include <string.h>

eai_status_t eai_bci_config_defaults(eai_bci_config_t *cfg)
{
    if (!cfg) return EAI_ERR_INVALID;
    memset(cfg, 0, sizeof(*cfg));

    strncpy(cfg->device_name, "simulator", sizeof(cfg->device_name) - 1);
    strncpy(cfg->decoder_name, "threshold", sizeof(cfg->decoder_name) - 1);
    strncpy(cfg->output_name, "log", sizeof(cfg->output_name) - 1);

    cfg->num_channels       = 4;
    cfg->sample_rate_hz     = 250;
    cfg->filter_low_hz      = 1.0f;
    cfg->filter_high_hz     = 50.0f;
    cfg->notch_hz           = 50.0f;
    cfg->decoder_window_size = 64;
    cfg->simulator_mode     = true;
    cfg->paradigm           = 0;

    return EAI_OK;
}

eai_status_t eai_bci_config_load(eai_bci_config_t *cfg, const char *path)
{
    if (!cfg || !path) return EAI_ERR_INVALID;

    eai_bci_config_defaults(cfg);

    EAI_LOG_INFO("BCI config: loaded defaults (file parsing placeholder for '%s')", path);
    return EAI_OK;
}

eai_status_t eai_bci_config_validate(const eai_bci_config_t *cfg)
{
    if (!cfg) return EAI_ERR_INVALID;

    if (cfg->num_channels == 0 || cfg->num_channels > EAI_BCI_MAX_CHANNELS)
        return EAI_ERR_CONFIG;

    if (cfg->sample_rate_hz == 0 || cfg->sample_rate_hz > 10000)
        return EAI_ERR_CONFIG;

    if (cfg->filter_low_hz >= cfg->filter_high_hz)
        return EAI_ERR_CONFIG;

    if (cfg->filter_high_hz > (float)cfg->sample_rate_hz / 2.0f)
        return EAI_ERR_CONFIG;

    return EAI_OK;
}

void eai_bci_config_dump(const eai_bci_config_t *cfg)
{
    if (!cfg) return;
    EAI_LOG_INFO("BCI Config:");
    EAI_LOG_INFO("  device:     %s", cfg->device_name);
    EAI_LOG_INFO("  decoder:    %s", cfg->decoder_name);
    EAI_LOG_INFO("  output:     %s", cfg->output_name);
    EAI_LOG_INFO("  channels:   %u", cfg->num_channels);
    EAI_LOG_INFO("  rate:       %u Hz", cfg->sample_rate_hz);
    EAI_LOG_INFO("  filter:     %.1f-%.1f Hz, notch %.1f Hz",
                 cfg->filter_low_hz, cfg->filter_high_hz, cfg->notch_hz);
    EAI_LOG_INFO("  simulator:  %s", cfg->simulator_mode ? "yes" : "no");
}
