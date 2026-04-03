// SPDX-License-Identifier: MIT
// Copyright (c) 2026 EoS Project
// ISO/IEC 25000 | ISO/IEC/IEEE 15288:2023

#ifndef EAI_FW_SUPPLY_CHAIN_H
#define EAI_FW_SUPPLY_CHAIN_H

#include "eai/types.h"

#define EAI_SBOM_MAX_COMPONENTS 64
#define EAI_SBOM_NAME_MAX      64
#define EAI_SBOM_VERSION_MAX   32
#define EAI_SBOM_HASH_MAX      72
#define EAI_SBOM_LICENSE_MAX   32
#define EAI_SBOM_VENDOR_MAX    64

typedef enum {
    EAI_VENDOR_VERIFIED,
    EAI_VENDOR_TRUSTED,
    EAI_VENDOR_UNKNOWN,
    EAI_VENDOR_UNTRUSTED,
    EAI_VENDOR_BLOCKED,
} eai_vendor_trust_t;

typedef enum {
    EAI_LICENSE_MIT,
    EAI_LICENSE_APACHE_2_0,
    EAI_LICENSE_GPL_3_0,
    EAI_LICENSE_BSD_3,
    EAI_LICENSE_PROPRIETARY,
    EAI_LICENSE_UNKNOWN,
} eai_license_type_t;

typedef struct {
    char              name[EAI_SBOM_NAME_MAX];
    char              version[EAI_SBOM_VERSION_MAX];
    char              vendor[EAI_SBOM_VENDOR_MAX];
    char              hash[EAI_SBOM_HASH_MAX];
    eai_license_type_t license;
    eai_vendor_trust_t vendor_trust;
    bool              integrity_verified;
    bool              vulnerability_known;
    char              vulnerability_id[32];
    uint64_t          added_ts;
    uint64_t          last_checked_ts;
} eai_sbom_component_t;

typedef struct {
    eai_sbom_component_t components[EAI_SBOM_MAX_COMPONENTS];
    int                  count;
    char                 project_name[EAI_SBOM_NAME_MAX];
    char                 project_version[EAI_SBOM_VERSION_MAX];
    uint64_t             generated_ts;
    bool                 all_verified;
} eai_fw_supply_chain_t;

eai_status_t eai_fw_sc_init(eai_fw_supply_chain_t *sc, const char *project_name,
                             const char *project_version);
eai_status_t eai_fw_sc_add_component(eai_fw_supply_chain_t *sc, const char *name,
                                      const char *version, const char *vendor,
                                      const char *hash, eai_license_type_t license);
eai_status_t eai_fw_sc_set_vendor_trust(eai_fw_supply_chain_t *sc, const char *vendor,
                                         eai_vendor_trust_t trust);
eai_status_t eai_fw_sc_verify_component(eai_fw_supply_chain_t *sc, const char *name);
eai_status_t eai_fw_sc_verify_all(eai_fw_supply_chain_t *sc);
eai_status_t eai_fw_sc_flag_vulnerability(eai_fw_supply_chain_t *sc, const char *name,
                                           const char *vuln_id);
int          eai_fw_sc_count_by_trust(const eai_fw_supply_chain_t *sc, eai_vendor_trust_t trust);
bool         eai_fw_sc_license_compatible(const eai_fw_supply_chain_t *sc,
                                           eai_license_type_t project_license);
void         eai_fw_sc_report(const eai_fw_supply_chain_t *sc);

#endif /* EAI_FW_SUPPLY_CHAIN_H */
