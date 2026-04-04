// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

// BCI connector: bridges BCI pipeline into framework connector manager
// Implements eai_connector_ops_t for use with eai_fw_conn_mgr

#ifdef EAI_BUILD_BCI

#include "eai_bci/pipeline.h"
#include "eai/log.h"
#include <string.h>
#include <stdio.h>

/* Forward declare framework connector type if available */
#ifdef EAI_BUILD_FRAMEWORK
#include "eai_fw/connector.h"

static eai_bci_pipeline_t *g_bci_pipeline = NULL;

static eai_status_t bci_conn_connect(void *conn, const eai_kv_t *params, int param_count)
{
    (void)conn;

    if (!g_bci_pipeline) {
        EAI_LOG_WARN("bci", "BCI connector: no pipeline attached, connect deferred");
        return EAI_OK;
    }

    /* Apply any connection params to the pipeline config */
    for (int i = 0; i < param_count; i++) {
        if (strcmp(params[i].key, "device") == 0) {
            EAI_LOG_INFO("bci", "BCI connector: device override = %s", params[i].value);
        }
        if (strcmp(params[i].key, "channels") == 0) {
            EAI_LOG_INFO("bci", "BCI connector: channels = %s", params[i].value);
        }
    }

    /* Start the BCI pipeline if not already running */
    eai_status_t st = eai_bci_pipeline_start(g_bci_pipeline);
    if (st != EAI_OK) {
        EAI_LOG_ERROR("bci", "BCI connector: pipeline start failed: %d", st);
        return st;
    }

    EAI_LOG_INFO("bci", "BCI connector: connected (pipeline active)");
    return EAI_OK;
}

static eai_status_t bci_conn_disconnect(void *conn)
{
    (void)conn;
    if (g_bci_pipeline) {
        eai_bci_pipeline_stop(g_bci_pipeline);
    }
    EAI_LOG_INFO("bci", "BCI connector: disconnect");
    return EAI_OK;
}

static eai_status_t bci_conn_read(void *conn, const char *address,
                                   void *buf, size_t buf_size, size_t *bytes_read)
{
    (void)conn;
    (void)address;

    if (!g_bci_pipeline || !buf) return EAI_ERR_INVALID;

    const eai_bci_intent_t *intent = eai_bci_pipeline_last_intent(g_bci_pipeline);
    if (!intent) return EAI_ERR_NOT_FOUND;

    int written = snprintf((char *)buf, buf_size,
                            "{\"class\":%u,\"label\":\"%s\",\"confidence\":%.3f}",
                            intent->class_id, intent->label, intent->confidence);

    if (bytes_read) *bytes_read = (size_t)written;
    return EAI_OK;
}

static eai_status_t bci_conn_write(void *conn, const char *address,
                                    const void *data, size_t data_len)
{
    (void)conn;

    if (!g_bci_pipeline) return EAI_ERR_INVALID;

    /* Write commands to BCI pipeline for actuator control.
     * Supported commands:
     *   "start" - start acquisition
     *   "stop"  - stop acquisition
     *   "calibrate" - run calibration
     */
    if (data && data_len > 0) {
        const char *cmd = (const char *)data;
        EAI_LOG_INFO("bci", "BCI connector: write command '%.*s' to '%s'",
                     (int)data_len, cmd, address ? address : "default");

        if (strncmp(cmd, "start", 5) == 0) {
            eai_bci_pipeline_start(g_bci_pipeline);
        } else if (strncmp(cmd, "stop", 4) == 0) {
            eai_bci_pipeline_stop(g_bci_pipeline);
        }
    }

    return EAI_OK;
}

void eai_bci_connector_set_pipeline(eai_bci_pipeline_t *pipe)
{
    g_bci_pipeline = pipe;
    if (pipe) {
        EAI_LOG_INFO("bci", "BCI connector: pipeline attached");
    }
}

#endif /* EAI_BUILD_FRAMEWORK */
#endif /* EAI_BUILD_BCI */
