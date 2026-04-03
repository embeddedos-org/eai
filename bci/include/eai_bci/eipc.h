// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_BCI_EIPC_H
#define EAI_BCI_EIPC_H

#include "eai_bci/types.h"

#ifdef EAI_EIPC_ENABLED

typedef struct {
    int  socket_fd;
    bool connected;
    char endpoint[256];
} eai_bci_eipc_t;

eai_status_t eai_bci_eipc_init(eai_bci_eipc_t *eipc, const char *endpoint);
eai_status_t eai_bci_eipc_send_intent(eai_bci_eipc_t *eipc, const eai_bci_intent_t *intent);
eai_status_t eai_bci_eipc_stream_signal(eai_bci_eipc_t *eipc, const eai_bci_sample_t *samples,
                                         int count);
eai_status_t eai_bci_eipc_receive_command(eai_bci_eipc_t *eipc, char *cmd_buf, size_t buf_size);
void         eai_bci_eipc_shutdown(eai_bci_eipc_t *eipc);

#endif /* EAI_EIPC_ENABLED */

#endif /* EAI_BCI_EIPC_H */
