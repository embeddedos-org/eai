// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/config.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

/* Simple INI-style config parser.
 * Format: key = value (one per line, # for comments) */
static int parse_config_line(const char *line, char *key, size_t key_size,
                              char *value, size_t val_size)
{
    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Skip comments and empty lines */
    if (*line == '#' || *line == ';' || *line == '\0' || *line == '\n')
        return -1;

    /* Find '=' separator */
    const char *eq = strchr(line, '=');
    if (!eq) return -1;

    /* Extract key (trim trailing whitespace) */
    size_t klen = (size_t)(eq - line);
    while (klen > 0 && (line[klen - 1] == ' ' || line[klen - 1] == '\t'))
        klen--;
    if (klen >= key_size) klen = key_size - 1;
    memcpy(key, line, klen);
    key[klen] = '\0';

    /* Extract value (trim leading/trailing whitespace and newline) */
    const char *vp = eq + 1;
    while (*vp == ' ' || *vp == '\t') vp++;
    size_t vlen = strlen(vp);
    while (vlen > 0 && (vp[vlen - 1] == '\n' || vp[vlen - 1] == '\r' ||
                        vp[vlen - 1] == ' '  || vp[vlen - 1] == '\t'))
        vlen--;
    if (vlen >= val_size) vlen = val_size - 1;
    memcpy(value, vp, vlen);
    value[vlen] = '\0';

    return 0;
}

eai_status_t eai_bci_config_load(eai_bci_config_t *cfg, const char *path)
{
    if (!cfg || !path) return EAI_ERR_INVALID;

    /* Start with defaults */
    eai_bci_config_defaults(cfg);

    FILE *f = fopen(path, "r");
    if (!f) {
        EAI_LOG_WARN("bci", "BCI config: cannot open '%s', using defaults", path);
        return EAI_OK;
    }

    char line[512];
    char key[128], value[256];
    int line_num = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        if (parse_config_line(line, key, sizeof(key), value, sizeof(value)) != 0)
            continue;

        if (strcmp(key, "device") == 0 || strcmp(key, "device_name") == 0) {
            strncpy(cfg->device_name, value, sizeof(cfg->device_name) - 1);
        } else if (strcmp(key, "decoder") == 0 || strcmp(key, "decoder_name") == 0) {
            strncpy(cfg->decoder_name, value, sizeof(cfg->decoder_name) - 1);
        } else if (strcmp(key, "output") == 0 || strcmp(key, "output_name") == 0) {
            strncpy(cfg->output_name, value, sizeof(cfg->output_name) - 1);
        } else if (strcmp(key, "channels") == 0 || strcmp(key, "num_channels") == 0) {
            cfg->num_channels = (uint32_t)atoi(value);
        } else if (strcmp(key, "sample_rate") == 0 || strcmp(key, "sample_rate_hz") == 0) {
            cfg->sample_rate_hz = (uint32_t)atoi(value);
        } else if (strcmp(key, "filter_low") == 0 || strcmp(key, "filter_low_hz") == 0) {
            cfg->filter_low_hz = (float)atof(value);
        } else if (strcmp(key, "filter_high") == 0 || strcmp(key, "filter_high_hz") == 0) {
            cfg->filter_high_hz = (float)atof(value);
        } else if (strcmp(key, "notch") == 0 || strcmp(key, "notch_hz") == 0) {
            cfg->notch_hz = (float)atof(value);
        } else if (strcmp(key, "window_size") == 0 || strcmp(key, "decoder_window_size") == 0) {
            cfg->decoder_window_size = (uint32_t)atoi(value);
        } else if (strcmp(key, "simulator") == 0 || strcmp(key, "simulator_mode") == 0) {
            cfg->simulator_mode = (strcmp(value, "true") == 0 ||
                                   strcmp(value, "1") == 0 ||
                                   strcmp(value, "yes") == 0);
        } else if (strcmp(key, "paradigm") == 0) {
            cfg->paradigm = atoi(value);
        } else {
            EAI_LOG_WARN("bci", "BCI config: unknown key '%s' at line %d", key, line_num);
        }
    }

    fclose(f);
    EAI_LOG_INFO("bci", "BCI config: loaded from '%s' (%d lines parsed)", path, line_num);
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
    EAI_LOG_INFO("bci", "BCI Config:");
    EAI_LOG_INFO("bci", "  device:     %s", cfg->device_name);
    EAI_LOG_INFO("bci", "  decoder:    %s", cfg->decoder_name);
    EAI_LOG_INFO("bci", "  output:     %s", cfg->output_name);
    EAI_LOG_INFO("bci", "  channels:   %u", cfg->num_channels);
    EAI_LOG_INFO("bci", "  rate:       %u Hz", cfg->sample_rate_hz);
    EAI_LOG_INFO("bci", "  filter:     %.1f-%.1f Hz, notch %.1f Hz",
                 cfg->filter_low_hz, cfg->filter_high_hz, cfg->notch_hz);
    EAI_LOG_INFO("bci", "  simulator:  %s", cfg->simulator_mode ? "yes" : "no");
}
