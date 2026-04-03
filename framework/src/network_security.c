// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/network_security.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "netsec"

static uint64_t netsec_now(void) {
    return (uint64_t)time(NULL);
}

eai_status_t eai_fw_netsec_init(eai_fw_network_security_t *ns, const eai_netsec_config_t *cfg) {
    if (!ns) return EAI_ERR_INVALID;
    memset(ns, 0, sizeof(*ns));

    if (cfg) {
        ns->config = *cfg;
    } else {
        ns->config.min_tls_version = EAI_TLS_1_2;
        ns->config.require_mtls = false;
        ns->config.enable_cert_pinning = true;
        ns->config.key_rotation_interval_hours = 720;
        ns->config.session_timeout_ms = 30000;
    }

    ns->last_key_rotation = netsec_now();
    ns->next_key_rotation = ns->last_key_rotation +
                            (uint64_t)ns->config.key_rotation_interval_hours * 3600;
    ns->initialized = true;

    EAI_LOG_INFO(MOD, "network security initialized (TLS >= %s, mTLS=%s)",
                 ns->config.min_tls_version == EAI_TLS_1_3 ? "1.3" : "1.2",
                 ns->config.require_mtls ? "required" : "optional");
    return EAI_OK;
}

eai_status_t eai_fw_netsec_add_cert(eai_fw_network_security_t *ns,
                                     const char *cn, const char *fingerprint,
                                     uint64_t not_before, uint64_t not_after,
                                     bool is_ca) {
    if (!ns || !cn || !fingerprint) return EAI_ERR_INVALID;
    if (ns->cert_count >= EAI_NETSEC_MAX_CERTS) return EAI_ERR_NOMEM;

    eai_certificate_t *c = &ns->certs[ns->cert_count];
    memset(c, 0, sizeof(*c));
    strncpy(c->common_name, cn, EAI_NETSEC_CERT_CN_MAX - 1);
    strncpy(c->fingerprint, fingerprint, EAI_NETSEC_HASH_MAX - 1);
    c->not_before = not_before;
    c->not_after = not_after;
    c->is_ca = is_ca;
    c->pinned = false;

    uint64_t now = netsec_now();
    if (now < not_before) {
        c->status = EAI_CERT_NOT_YET_VALID;
    } else if (now > not_after) {
        c->status = EAI_CERT_EXPIRED;
    } else {
        c->status = EAI_CERT_VALID;
    }

    ns->cert_count++;
    EAI_LOG_INFO(MOD, "added certificate: %s (CA=%s)", cn, is_ca ? "yes" : "no");
    return EAI_OK;
}

eai_status_t eai_fw_netsec_pin_cert(eai_fw_network_security_t *ns, const char *cn) {
    if (!ns || !cn) return EAI_ERR_INVALID;

    for (int i = 0; i < ns->cert_count; i++) {
        if (strcmp(ns->certs[i].common_name, cn) == 0) {
            ns->certs[i].pinned = true;
            EAI_LOG_INFO(MOD, "pinned certificate: %s", cn);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_netsec_revoke_cert(eai_fw_network_security_t *ns, const char *cn) {
    if (!ns || !cn) return EAI_ERR_INVALID;

    for (int i = 0; i < ns->cert_count; i++) {
        if (strcmp(ns->certs[i].common_name, cn) == 0) {
            ns->certs[i].status = EAI_CERT_REVOKED;
            EAI_LOG_WARN(MOD, "revoked certificate: %s", cn);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_netsec_check_cert(eai_fw_network_security_t *ns, const char *cn) {
    if (!ns || !cn) return EAI_ERR_INVALID;

    for (int i = 0; i < ns->cert_count; i++) {
        if (strcmp(ns->certs[i].common_name, cn) == 0) {
            uint64_t now = netsec_now();
            eai_certificate_t *c = &ns->certs[i];

            if (c->status == EAI_CERT_REVOKED) return EAI_ERR_PERMISSION;
            if (now < c->not_before) { c->status = EAI_CERT_NOT_YET_VALID; return EAI_ERR_INVALID; }
            if (now > c->not_after)  { c->status = EAI_CERT_EXPIRED; return EAI_ERR_TIMEOUT; }

            c->status = EAI_CERT_VALID;
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_netsec_open_session(eai_fw_network_security_t *ns,
                                         const char *endpoint, eai_tls_version_t version,
                                         bool mtls) {
    if (!ns || !endpoint) return EAI_ERR_INVALID;

    if (version < ns->config.min_tls_version) {
        EAI_LOG_ERROR(MOD, "TLS version too low for endpoint %s", endpoint);
        return EAI_ERR_PERMISSION;
    }

    if (ns->config.require_mtls && !mtls) {
        EAI_LOG_ERROR(MOD, "mTLS required but not provided for %s", endpoint);
        return EAI_ERR_PERMISSION;
    }

    if (ns->session_count >= EAI_NETSEC_MAX_SESSIONS) return EAI_ERR_NOMEM;

    eai_net_session_t *s = &ns->sessions[ns->session_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->endpoint, endpoint, EAI_NETSEC_CERT_CN_MAX - 1);
    s->tls_version = version;
    s->mutual_tls = mtls;
    s->active = true;
    s->established_ts = netsec_now();
    s->last_activity_ts = s->established_ts;
    ns->session_count++;

    EAI_LOG_INFO(MOD, "opened session to %s (TLS %s, mTLS=%s)",
                 endpoint,
                 version == EAI_TLS_1_3 ? "1.3" : "1.2",
                 mtls ? "yes" : "no");
    return EAI_OK;
}

eai_status_t eai_fw_netsec_close_session(eai_fw_network_security_t *ns, const char *endpoint) {
    if (!ns || !endpoint) return EAI_ERR_INVALID;

    for (int i = 0; i < ns->session_count; i++) {
        if (strcmp(ns->sessions[i].endpoint, endpoint) == 0 && ns->sessions[i].active) {
            ns->sessions[i].active = false;
            EAI_LOG_INFO(MOD, "closed session to %s", endpoint);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_netsec_rotate_keys(eai_fw_network_security_t *ns) {
    if (!ns) return EAI_ERR_INVALID;

    ns->last_key_rotation = netsec_now();
    ns->next_key_rotation = ns->last_key_rotation +
                            (uint64_t)ns->config.key_rotation_interval_hours * 3600;

    EAI_LOG_INFO(MOD, "key rotation completed, next rotation in %u hours",
                 ns->config.key_rotation_interval_hours);
    return EAI_OK;
}

bool eai_fw_netsec_key_rotation_due(const eai_fw_network_security_t *ns) {
    if (!ns) return false;
    return netsec_now() >= ns->next_key_rotation;
}

int eai_fw_netsec_expiring_certs(const eai_fw_network_security_t *ns, uint32_t days) {
    if (!ns) return 0;

    uint64_t threshold = netsec_now() + (uint64_t)days * 86400;
    int count = 0;
    for (int i = 0; i < ns->cert_count; i++) {
        if (ns->certs[i].status == EAI_CERT_VALID && ns->certs[i].not_after <= threshold) {
            count++;
        }
    }
    return count;
}

void eai_fw_netsec_report(const eai_fw_network_security_t *ns) {
    if (!ns) return;

    printf("\n=== Network Security Report ===\n");
    printf("Min TLS: %s | mTLS: %s | Cert pinning: %s\n",
           ns->config.min_tls_version == EAI_TLS_1_3 ? "1.3" : "1.2",
           ns->config.require_mtls ? "required" : "optional",
           ns->config.enable_cert_pinning ? "enabled" : "disabled");
    printf("Key rotation due: %s\n",
           eai_fw_netsec_key_rotation_due(ns) ? "YES" : "no");

    printf("\nCertificates (%d):\n", ns->cert_count);
    for (int i = 0; i < ns->cert_count; i++) {
        const eai_certificate_t *c = &ns->certs[i];
        const char *st;
        switch (c->status) {
            case EAI_CERT_VALID:         st = "VALID";         break;
            case EAI_CERT_EXPIRED:       st = "EXPIRED";       break;
            case EAI_CERT_REVOKED:       st = "REVOKED";       break;
            case EAI_CERT_NOT_YET_VALID: st = "NOT_YET_VALID"; break;
            default:                     st = "UNKNOWN";        break;
        }
        printf("  %-32s %-15s CA=%s pinned=%s\n",
               c->common_name, st, c->is_ca ? "yes" : "no", c->pinned ? "yes" : "no");
    }

    int active_sessions = 0;
    for (int i = 0; i < ns->session_count; i++) {
        if (ns->sessions[i].active) active_sessions++;
    }
    printf("\nActive sessions: %d / %d\n", active_sessions, ns->session_count);
}
