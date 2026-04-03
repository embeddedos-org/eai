// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_NETWORK_SECURITY_H
#define EAI_FW_NETWORK_SECURITY_H

#include "eai/types.h"

#define EAI_NETSEC_MAX_CERTS     16
#define EAI_NETSEC_MAX_SESSIONS  32
#define EAI_NETSEC_CERT_CN_MAX   128
#define EAI_NETSEC_HASH_MAX      72

typedef enum {
    EAI_TLS_1_2,
    EAI_TLS_1_3,
    EAI_DTLS_1_2,
} eai_tls_version_t;

typedef enum {
    EAI_CERT_VALID,
    EAI_CERT_EXPIRED,
    EAI_CERT_REVOKED,
    EAI_CERT_NOT_YET_VALID,
    EAI_CERT_UNKNOWN,
} eai_cert_status_t;

typedef struct {
    char              common_name[EAI_NETSEC_CERT_CN_MAX];
    char              fingerprint[EAI_NETSEC_HASH_MAX];
    eai_cert_status_t status;
    uint64_t          not_before;
    uint64_t          not_after;
    bool              pinned;
    bool              is_ca;
} eai_certificate_t;

typedef struct {
    char              endpoint[EAI_NETSEC_CERT_CN_MAX];
    eai_tls_version_t tls_version;
    bool              mutual_tls;
    bool              active;
    uint64_t          established_ts;
    uint64_t          last_activity_ts;
    uint32_t          bytes_sent;
    uint32_t          bytes_received;
} eai_net_session_t;

typedef struct {
    eai_tls_version_t min_tls_version;
    bool              require_mtls;
    bool              enable_cert_pinning;
    uint32_t          key_rotation_interval_hours;
    uint32_t          session_timeout_ms;
} eai_netsec_config_t;

typedef struct {
    eai_netsec_config_t config;
    eai_certificate_t   certs[EAI_NETSEC_MAX_CERTS];
    int                 cert_count;
    eai_net_session_t   sessions[EAI_NETSEC_MAX_SESSIONS];
    int                 session_count;
    uint64_t            last_key_rotation;
    uint64_t            next_key_rotation;
    bool                initialized;
} eai_fw_network_security_t;

eai_status_t eai_fw_netsec_init(eai_fw_network_security_t *ns, const eai_netsec_config_t *cfg);
eai_status_t eai_fw_netsec_add_cert(eai_fw_network_security_t *ns,
                                     const char *cn, const char *fingerprint,
                                     uint64_t not_before, uint64_t not_after,
                                     bool is_ca);
eai_status_t eai_fw_netsec_pin_cert(eai_fw_network_security_t *ns, const char *cn);
eai_status_t eai_fw_netsec_revoke_cert(eai_fw_network_security_t *ns, const char *cn);
eai_status_t eai_fw_netsec_check_cert(eai_fw_network_security_t *ns, const char *cn);
eai_status_t eai_fw_netsec_open_session(eai_fw_network_security_t *ns,
                                         const char *endpoint, eai_tls_version_t version,
                                         bool mtls);
eai_status_t eai_fw_netsec_close_session(eai_fw_network_security_t *ns, const char *endpoint);
eai_status_t eai_fw_netsec_rotate_keys(eai_fw_network_security_t *ns);
bool         eai_fw_netsec_key_rotation_due(const eai_fw_network_security_t *ns);
int          eai_fw_netsec_expiring_certs(const eai_fw_network_security_t *ns, uint32_t days);
void         eai_fw_netsec_report(const eai_fw_network_security_t *ns);

#endif /* EAI_FW_NETWORK_SECURITY_H */
