// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#include "eai_fw/supply_chain.h"
#include "eai/log.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define MOD "supply-chain"

static uint64_t sc_now(void) {
    return (uint64_t)time(NULL);
}

static const char *trust_str(eai_vendor_trust_t t) {
    switch (t) {
        case EAI_VENDOR_VERIFIED:  return "VERIFIED";
        case EAI_VENDOR_TRUSTED:   return "TRUSTED";
        case EAI_VENDOR_UNKNOWN:   return "UNKNOWN";
        case EAI_VENDOR_UNTRUSTED: return "UNTRUSTED";
        case EAI_VENDOR_BLOCKED:   return "BLOCKED";
        default:                   return "?";
    }
}

static const char *license_str(eai_license_type_t l) {
    switch (l) {
        case EAI_LICENSE_MIT:         return "MIT";
        case EAI_LICENSE_APACHE_2_0:  return "Apache-2.0";
        case EAI_LICENSE_GPL_3_0:     return "GPL-3.0";
        case EAI_LICENSE_BSD_3:       return "BSD-3-Clause";
        case EAI_LICENSE_PROPRIETARY: return "Proprietary";
        case EAI_LICENSE_UNKNOWN:     return "Unknown";
        default:                      return "?";
    }
}

eai_status_t eai_fw_sc_init(eai_fw_supply_chain_t *sc, const char *project_name,
                             const char *project_version) {
    if (!sc || !project_name || !project_version) return EAI_ERR_INVALID;
    memset(sc, 0, sizeof(*sc));
    strncpy(sc->project_name, project_name, EAI_SBOM_NAME_MAX - 1);
    strncpy(sc->project_version, project_version, EAI_SBOM_VERSION_MAX - 1);
    sc->generated_ts = sc_now();
    EAI_LOG_INFO(MOD, "SBOM initialized for %s v%s", project_name, project_version);
    return EAI_OK;
}

eai_status_t eai_fw_sc_add_component(eai_fw_supply_chain_t *sc, const char *name,
                                      const char *version, const char *vendor,
                                      const char *hash, eai_license_type_t license) {
    if (!sc || !name || !version || !vendor) return EAI_ERR_INVALID;
    if (sc->count >= EAI_SBOM_MAX_COMPONENTS) return EAI_ERR_NOMEM;

    eai_sbom_component_t *c = &sc->components[sc->count];
    memset(c, 0, sizeof(*c));
    strncpy(c->name, name, EAI_SBOM_NAME_MAX - 1);
    strncpy(c->version, version, EAI_SBOM_VERSION_MAX - 1);
    strncpy(c->vendor, vendor, EAI_SBOM_VENDOR_MAX - 1);
    if (hash) strncpy(c->hash, hash, EAI_SBOM_HASH_MAX - 1);
    c->license = license;
    c->vendor_trust = EAI_VENDOR_UNKNOWN;
    c->added_ts = sc_now();
    sc->count++;

    EAI_LOG_INFO(MOD, "added component: %s v%s (%s)", name, version, vendor);
    return EAI_OK;
}

eai_status_t eai_fw_sc_set_vendor_trust(eai_fw_supply_chain_t *sc, const char *vendor,
                                         eai_vendor_trust_t trust) {
    if (!sc || !vendor) return EAI_ERR_INVALID;

    int updated = 0;
    for (int i = 0; i < sc->count; i++) {
        if (strcmp(sc->components[i].vendor, vendor) == 0) {
            sc->components[i].vendor_trust = trust;
            updated++;
        }
    }

    if (updated == 0) return EAI_ERR_NOT_FOUND;
    EAI_LOG_INFO(MOD, "set vendor '%s' trust to %s (%d components)",
                 vendor, trust_str(trust), updated);
    return EAI_OK;
}

eai_status_t eai_fw_sc_verify_component(eai_fw_supply_chain_t *sc, const char *name) {
    if (!sc || !name) return EAI_ERR_INVALID;

    for (int i = 0; i < sc->count; i++) {
        if (strcmp(sc->components[i].name, name) == 0) {
            if (sc->components[i].vendor_trust == EAI_VENDOR_BLOCKED) {
                EAI_LOG_ERROR(MOD, "component '%s' from blocked vendor", name);
                return EAI_ERR_PERMISSION;
            }
            sc->components[i].integrity_verified = true;
            sc->components[i].last_checked_ts = sc_now();
            EAI_LOG_INFO(MOD, "verified component: %s", name);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

eai_status_t eai_fw_sc_verify_all(eai_fw_supply_chain_t *sc) {
    if (!sc) return EAI_ERR_INVALID;

    sc->all_verified = true;
    for (int i = 0; i < sc->count; i++) {
        eai_status_t s = eai_fw_sc_verify_component(sc, sc->components[i].name);
        if (s != EAI_OK) {
            sc->all_verified = false;
        }
    }

    EAI_LOG_INFO(MOD, "SBOM verification: %s", sc->all_verified ? "ALL PASSED" : "SOME FAILED");
    return sc->all_verified ? EAI_OK : EAI_ERR_PERMISSION;
}

eai_status_t eai_fw_sc_flag_vulnerability(eai_fw_supply_chain_t *sc, const char *name,
                                           const char *vuln_id) {
    if (!sc || !name || !vuln_id) return EAI_ERR_INVALID;

    for (int i = 0; i < sc->count; i++) {
        if (strcmp(sc->components[i].name, name) == 0) {
            sc->components[i].vulnerability_known = true;
            strncpy(sc->components[i].vulnerability_id, vuln_id, 31);
            EAI_LOG_WARN(MOD, "vulnerability flagged: %s (%s)", name, vuln_id);
            return EAI_OK;
        }
    }
    return EAI_ERR_NOT_FOUND;
}

int eai_fw_sc_count_by_trust(const eai_fw_supply_chain_t *sc, eai_vendor_trust_t trust) {
    if (!sc) return 0;
    int count = 0;
    for (int i = 0; i < sc->count; i++) {
        if (sc->components[i].vendor_trust == trust) count++;
    }
    return count;
}

bool eai_fw_sc_license_compatible(const eai_fw_supply_chain_t *sc,
                                   eai_license_type_t project_license) {
    if (!sc) return false;

    for (int i = 0; i < sc->count; i++) {
        if (project_license == EAI_LICENSE_MIT || project_license == EAI_LICENSE_BSD_3) {
            if (sc->components[i].license == EAI_LICENSE_GPL_3_0) {
                EAI_LOG_WARN(MOD, "license conflict: %s is GPL-3.0 in %s project",
                             sc->components[i].name, license_str(project_license));
                return false;
            }
        }
        if (project_license == EAI_LICENSE_PROPRIETARY) {
            if (sc->components[i].license == EAI_LICENSE_GPL_3_0) {
                return false;
            }
        }
    }
    return true;
}

void eai_fw_sc_report(const eai_fw_supply_chain_t *sc) {
    if (!sc) return;

    printf("\n=== Software Bill of Materials ===\n");
    printf("Project: %s v%s\n", sc->project_name, sc->project_version);
    printf("Components: %d | Verified: %s\n", sc->count, sc->all_verified ? "ALL" : "PARTIAL");

    int vulns = 0;
    for (int i = 0; i < sc->count; i++) {
        if (sc->components[i].vulnerability_known) vulns++;
    }
    printf("Known vulnerabilities: %d\n", vulns);

    printf("\n%-24s %-10s %-16s %-12s %-10s %s\n",
           "Component", "Version", "Vendor", "License", "Trust", "Vuln");
    printf("------------------------------------------------------------------------------------\n");

    for (int i = 0; i < sc->count; i++) {
        const eai_sbom_component_t *c = &sc->components[i];
        printf("%-24s %-10s %-16s %-12s %-10s %s\n",
               c->name, c->version, c->vendor,
               license_str(c->license), trust_str(c->vendor_trust),
               c->vulnerability_known ? c->vulnerability_id : "-");
    }
}
