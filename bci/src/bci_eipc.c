// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_bci/eipc.h"
#include "eai/log.h"
#include <string.h>

#ifdef EAI_EIPC_ENABLED

eai_status_t eai_bci_eipc_init(eai_bci_eipc_t *eipc, const char *endpoint)
{
    if (!eipc || !endpoint) return EAI_ERR_INVALID;
    memset(eipc, 0, sizeof(*eipc));
    strncpy(eipc->endpoint, endpoint, sizeof(eipc->endpoint) - 1);
    eipc->socket_fd = -1;
    eipc->connected = false;

    EAI_LOG_INFO("bci", "BCI EIPC: init endpoint '%s' (stub)", endpoint);
    return EAI_OK;
}

eai_status_t eai_bci_eipc_send_intent(eai_bci_eipc_t *eipc, const eai_bci_intent_t *intent)
{
    if (!eipc || !intent) return EAI_ERR_INVALID;
    EAI_LOG_DEBUG("bci", "BCI EIPC: send intent '%s' (class %u, conf %.2f) (stub)",
                  intent->label, intent->class_id, intent->confidence);
    return EAI_OK;
}

eai_status_t eai_bci_eipc_stream_signal(eai_bci_eipc_t *eipc, const eai_bci_sample_t *samples,
                                         int count)
{
    if (!eipc || !samples) return EAI_ERR_INVALID;
    EAI_LOG_DEBUG("bci", "BCI EIPC: stream %d samples (stub)", count);
    return EAI_OK;
}

eai_status_t eai_bci_eipc_receive_command(eai_bci_eipc_t *eipc, char *cmd_buf, size_t buf_size)
{
    if (!eipc || !cmd_buf || buf_size == 0) return EAI_ERR_INVALID;
    cmd_buf[0] = '\0';
    return EAI_OK;
}

void eai_bci_eipc_shutdown(eai_bci_eipc_t *eipc)
{
    if (!eipc) return;
    eipc->connected = false;
    eipc->socket_fd = -1;
    EAI_LOG_INFO("bci", "BCI EIPC: shutdown");
}

#endif /* EAI_EIPC_ENABLED */
