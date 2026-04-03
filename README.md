# eAI — Embedded AI Layer

> High-performance on-device AI for embedded systems, edge devices, and intelligent machines.

[![Version](https://img.shields.io/badge/version-0.2.0-blue.svg)](CHANGELOG.md)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![C Standard](https://img.shields.io/badge/C-C11-orange.svg)]()
[![ISO](https://img.shields.io/badge/ISO-15288%20|%2020243%20|%2025000-purple.svg)](docs/compliance/)

---

## Overview

**eAI** is a C11 embedded AI framework that brings LLM inference, autonomous agents, and adaptive learning directly to resource-constrained devices — from microcontrollers to edge servers. It runs entirely on-device with **zero cloud dependency**, while supporting optional hybrid connectivity.

eAI is designed for systems where **latency matters**, **connectivity is unreliable**, and **safety is non-negotiable**: robotics, industrial automation, smart cameras, medical devices, autonomous vehicles, and IoT gateways.

### Key Design Principles

- **Offline-first**: Full autonomy without cloud connectivity
- **Resource-aware**: Quantized models, power-aware scheduling, memory-conscious design
- **Security by default**: 8-layer defense architecture from boot to runtime
- **Two-tier architecture**: Choose the right footprint for your hardware

---

## Two-Tier Architecture

eAI ships as two distinct product tiers sharing a common foundation:

### EAI-Min — Lightweight Runtime

For **MCUs, SBCs, and battery-powered edge devices** (Cortex-M7, RPi, nRF5340).

| Module | Description |
|---|---|
| **Runtime** | Single-backend inference engine (llama.cpp, ONNX, TFLite) |
| **Agent** | ReAct-style think→act→observe loop with tool calling |
| **Router** | Local/cloud/auto inference routing |
| **Memory Lite** | 128-entry key-value store with LRU eviction |
| **Security Lite** | Audit logging, prompt injection detection, boot verification |
| **Observability Lite** | Health counters, latency tracking, system monitoring |
| **Sensor** | 32-sensor registry with calibration and moving-average filter |
| **OTA Update** | Secure model updates with hash verification and rollback |
| **Compression** | Quantization recommender and model size estimation |
| **Power Manager** | Battery-aware inference throttling and thermal management |

**Static footprint**: ~50KB RAM | **Targets**: 80MB–2GB devices

### EAI-Framework — Enterprise Platform

For **edge servers, industrial gateways, and high-performance embedded systems** (Jetson, x86, i.MX8M).

Includes everything in EAI-Min, plus:

| Module | Description |
|---|---|
| **Runtime Manager** | Pool of 8 concurrent inference backends with hot-switching |
| **Orchestrator** | DAG-style workflow engine with 9 step types and branching |
| **Connectors** | MQTT, OPC-UA, Modbus TCP, CAN bus protocol abstraction |
| **Memory** | 1024-entry namespaced KV store with TTL, GC, and persistence |
| **Policy Engine** | Subject/resource/operation ACL with wildcards and audit mode |
| **Observability** | Counters, gauges, histograms, distributed trace spans |
| **Adaptive Engine** | On-device LoRA fine-tuning with feedback-to-training pipeline |
| **Federated Learning** | Multi-device FedAvg with differential privacy (ε,δ) |
| **Update Manager** | A/B partition OTA with rollback, maintenance windows, changelog |
| **Secure Boot** | 4-stage boot chain verification, key management, attestation |
| **Supply Chain** | SBOM management, vendor trust levels, license compliance |
| **Sensor Fusion** | Weighted average, Kalman filter, voting — up to 8 fusion groups |
| **Network Security** | TLS/mTLS, certificate management, key rotation, session audit |
| **Guardrails** | AI output safety — injection blocking, rate limiting, kill switch |

**Targets**: 512MB–16GB+ devices with multi-protocol connectivity

---

## Embedded Constraints & Optimization

Embedded systems are resource-limited. eAI addresses this through tailored optimization:

### Quantization Levels

| Level | Size vs F32 | Quality | Speed vs F32 | Use Case |
|---|---|---|---|---|
| F32 | 100% | 100% | 1.0x | Development/testing |
| F16 | 50% | 99.8% | 1.5x | GPU-accelerated edge |
| Q8_0 | 25% | 98.5% | 2.5x | High-quality edge |
| Q5_0 | 18.8% | 95.0% | 3.2x | Balanced |
| Q4_0 | 15% | 92.0% | 4.0x | **Recommended for most devices** |
| Q3_K | 12.5% | 88.0% | 4.5x | Memory-constrained |
| Q2_K | 10% | 83.0% | 5.0x | Extreme compression |
| IQ2 | 8% | 78.0% | 5.5x | MCU-class devices |

### Memory-Aware Model Selection

eAI automatically selects the largest model that fits within available RAM and storage:

```c
// Finds best model for device with 512MB RAM, 1GB storage
const eai_model_info_t *model = eai_model_find_best_fit(512, 1024);
```

### Power-Aware Inference

The power manager automatically adjusts inference parameters based on battery level and thermal state:

| Power State | Max Tokens | Temperature | Inference Budget |
|---|---|---|---|
| FULL_POWER | 256 | 0.7 | Unlimited |
| ECO | 128 | 0.5 | 1,000 |
| LOW_POWER | 64 | 0.3 | 100 |
| CRITICAL | 32 | 0.1 | 10 |

---

## 8-Layer Security Architecture

eAI implements a comprehensive, defense-in-depth security posture:

| Layer | Security Focus | Implementation |
|---|---|---|
| **1. Boot** | Trusted execution | Secure boot chain verification, hardware root of trust abstraction, remote attestation |
| **2. System** | Isolation & permissions | Capability-based access control, per-module permission sets, wildcard matching |
| **3. AI Runtime** | Model & data protection | Model hash verification, encrypted storage (stub), integrity checks at load time |
| **4. Input/Output** | Safe interactions | Prompt injection detection (8+ patterns), output guardrails, PII leak prevention |
| **5. Networking** | Encrypt & authenticate | TLS 1.2/1.3, mTLS, certificate pinning, key rotation scheduling |
| **6. Logging** | Attack traceability | Signed audit trails, ring-buffer event logs, forensic dump capabilities |
| **7. Deployment** | Secure updates | Signed OTA with hash verification, A/B partitions, automatic rollback |
| **8. Supply Chain** | Component integrity | SBOM tracking, vendor trust levels, license compliance, vulnerability flagging |

### Guardrails & Behavioral Safety

For autonomous AI systems, eAI enforces behavioral boundaries:

- **Autonomy Levels**: `FULL_AUTO` → `SUPERVISED` → `MANUAL_ONLY`
- **Rate Limiting**: Configurable max inferences per time window
- **Kill Switch**: Emergency halt of all autonomous operations
- **Category-based rules**: HARMFUL, UNSAFE_ACTION, PII_LEAK, INJECTION, OUT_OF_SCOPE

---

## Model Registry

12 curated LLMs optimized for embedded deployment:

| Model | Params | Quant | RAM | Tier | LoRA Support |
|---|---|---|---|---|---|
| TinyLlama 1.1B | 1,100M | Q2_K | 80MB | Micro | ✅ (240MB) |
| SmolLM 360M | 360M | Q4_0 | 100MB | Tiny | ✅ (300MB) |
| Qwen2-0.5B | 500M | Q4_0 | 150MB | Tiny | ✅ (450MB) |
| Phi-1.5 | 1,300M | Q4_0 | 200MB | Tiny | ✅ (600MB) |
| Gemma 2B | 2,000M | Q4_0 | 500MB | Small | ✅ (1GB) |
| Phi-2 | 2,700M | Q4_0 | 800MB | Small | ✅ (1.6GB) |
| **Phi-3-mini** | 3,800M | Q4_0 | 1,200MB | Small | ✅ (2.4GB) |
| Qwen2-1.5B | 1,500M | Q4_0 | 2,048MB | Small | ✅ (1GB) |
| Llama 3.2 3B | 3,000M | Q4_0 | 2,500MB | Medium | ✅ (5GB) |
| Mistral 7B | 7,000M | Q4_0 | 3,500MB | Medium | ✅ (7GB) |
| Llama 3.2 8B | 8,000M | Q4_0 | 5,500MB | Large | ✅ (12GB) |
| Qwen2.5 7B | 7,000M | Q4_0 | 6,000MB | Large | ✅ (10GB) |

---

## Deployment Profiles

Pre-configured profiles for common use cases:

| Profile | Variant | Mode | Connectors | Key Features |
|---|---|---|---|---|
| `robot-controller` | Framework | local | CAN, MQTT | GPU accel, 50ms max inference, real-time |
| `smart-camera` | Framework | local | — | NPU acceleration, vision pipeline |
| `industrial-gateway` | Framework | local-first | MQTT, OPC-UA, Modbus | Multi-runtime, 2GB RAM budget |
| `mobile-edge` | Min | hybrid | — | Battery-powered, cloud fallback |
| `adaptive-edge` | Framework | hybrid | — | LoRA fine-tuning, idle training |

Load a profile:
```bash
eai profile robot-controller
```

---

## Build

### Requirements

- CMake 3.16+
- C11 compiler (GCC 9+, Clang 10+, MSVC 2019+)
- Optional: llama.cpp for real LLM inference

### Quick Build

```bash
mkdir build && cd build

# Lightweight only
cmake .. -DEAI_BUILD_FRAMEWORK=OFF
make

# Full enterprise platform
cmake .. -DEAI_BUILD_MIN=ON -DEAI_BUILD_FRAMEWORK=ON
make

# With llama.cpp backend
cmake .. -DEAI_LLAMA_CPP=ON -Dllama_DIR=/path/to/llama.cpp/lib/cmake/llama
make

# With tests
cmake .. -DEAI_BUILD_TESTS=ON
make && ctest
```

### Build Options

| Option | Default | Description |
|---|---|---|
| `EAI_BUILD_MIN` | ON | Build lightweight runtime |
| `EAI_BUILD_FRAMEWORK` | ON | Build enterprise platform |
| `EAI_BUILD_CLI` | ON | Build CLI tool |
| `EAI_BUILD_TESTS` | OFF | Build unit tests |
| `EAI_LLAMA_CPP` | OFF | Enable real llama.cpp backend |
| `EAI_EIPC_ENABLED` | OFF | Enable EIPC neural intent bridge |
| `EAI_BUILD_ADAPTIVE` | ON | Enable adaptive learning features |

---

## Quick Start

### EAI-Min (Lightweight)

```c
#include "eai_min/eai_min.h"

int main(void) {
    // Initialize runtime
    eai_min_runtime_t runtime;
    eai_min_runtime_create(&runtime, EAI_RUNTIME_LLAMA_CPP);
    eai_min_runtime_load(&runtime, "phi-3-mini-q4.gguf", NULL);

    // Initialize power-aware inference
    eai_min_power_t power;
    eai_min_power_init(&power, NULL);
    eai_min_power_set_battery(&power, 75.0f, EAI_POWER_BATTERY);

    // Setup security
    eai_min_security_lite_t sec;
    eai_security_ctx_t ctx;
    eai_security_ctx_init(&ctx, "edge-device-001");
    eai_min_sec_init(&sec, &ctx);
    eai_min_sec_verify_boot(&sec);

    // Run agent
    eai_min_agent_t agent;
    eai_min_agent_init(&agent, &runtime, NULL, NULL);

    eai_agent_task_t task = {
        .goal = "Monitor temperature and report anomalies",
        .max_iterations = 5
    };
    eai_min_agent_run(&agent, &task);

    printf("Result: %s\n", eai_min_agent_output(&agent));

    eai_min_runtime_destroy(&runtime);
    return 0;
}
```

### EAI-Framework (Enterprise)

```c
#include "eai_fw/eai_framework.h"

int main(void) {
    // Runtime manager with multiple backends
    eai_fw_runtime_manager_t rtmgr;
    eai_fw_rtmgr_init(&rtmgr);

    // Security: boot chain + guardrails
    eai_fw_secure_boot_t secboot;
    eai_fw_secboot_init(&secboot);
    eai_fw_secboot_add_chain_entry(&secboot, EAI_BOOT_STAGE_BOOTLOADER,
                                    "bootloader", "sha256:abc123...");
    eai_fw_secboot_verify_chain(&secboot);

    eai_fw_guardrails_t guardrails;
    eai_fw_guard_init(&guardrails);

    // Supply chain verification
    eai_fw_supply_chain_t sbom;
    eai_fw_sc_init(&sbom, "eAI", "0.2.0");
    eai_fw_sc_add_component(&sbom, "llama.cpp", "b3600",
                             "ggerganov", "sha256:...", EAI_LICENSE_MIT);
    eai_fw_sc_verify_all(&sbom);

    // Network security for connectors
    eai_fw_network_security_t netsec;
    eai_fw_netsec_init(&netsec, NULL);

    // Sensor fusion
    eai_fw_sensor_fusion_t fusion;
    eai_fw_fusion_init(&fusion);
    eai_fw_fusion_create_group(&fusion, "temperature", EAI_FUSION_KALMAN);

    // Federated learning
    eai_fw_federated_t fed;
    eai_fw_fed_init(&fed, EAI_FED_COORDINATOR, NULL);
    eai_fw_fed_add_participant(&fed, "device-001");
    eai_fw_fed_add_participant(&fed, "device-002");

    // Cleanup
    eai_fw_rtmgr_shutdown(&rtmgr);
    return 0;
}
```

---

## CLI

```bash
eai version                  # Version and build info
eai run                      # Run inference (Min or Framework)
eai serve                    # EIPC server mode
eai status                   # Device info and memory report
eai tools                    # List registered tools
eai profile <name>           # Load deployment profile
eai config <file>            # Load configuration file
```

---

## Project Structure

```
eAI/
├── common/                  # Shared foundation
│   ├── include/eai/         # Types, config, log, security, tools, manifest, adaptive
│   └── src/                 # Common implementations
├── min/                     # EAI-Min (Lightweight)
│   ├── include/eai_min/     # Runtime, agent, router, memory, security, sensor, power, ...
│   └── src/                 # Min implementations (10 modules)
├── framework/               # EAI-Framework (Enterprise)
│   ├── include/eai_fw/      # All enterprise modules
│   └── src/                 # Framework implementations (17 modules)
├── platform/                # OS abstraction (Linux, Windows, EoS)
├── models/                  # Model registry (12 curated LLMs)
├── profiles/                # Deployment profiles (5 YAML configs)
├── cli/                     # CLI entry point
├── tests/                   # Unit tests
└── docs/
    ├── compliance/          # ISO 15288, ISO 20243, ISO 25000
    └── qms/                 # Quality management system
```

---

## Complete Service Inventory

### Shared (Common Layer)
| Service | Files | Description |
|---|---|---|
| Types & Errors | `types.h` | 16 error codes, variant/mode enums, KV pairs |
| Configuration | `config.h/c` | YAML-like config parser, 5 hardcoded profiles |
| Logging | `log.h/c` | 6-level color-coded ANSI logging |
| Security | `security.h/c` | Permission-based access control with wildcards |
| Tool Registry | `tool.h/c` | 64-slot tool registry with typed parameters |
| Built-in Tools | `tools_builtin.h/c` | 7 tools: MQTT, sensor, HTTP, preference, feedback, model |
| Manifest | `manifest.h/c` | Model manifest parser and validator |
| Adaptive | `adaptive.h/c` | Preference store, feedback buffer, training samples |
| Runtime Contract | `runtime_contract.h/c` | Vtable-based inference + training ops |
| EIPC Listener | `eipc_listener.h/c` | Neural intent bridge (optional) |

### Min Tier (10 modules)
| Service | Description |
|---|---|
| Runtime | Single-backend inference wrapper with stub + llama.cpp |
| Agent | Think→act→observe loop with CALL: protocol |
| Router | Local/cloud/auto inference routing |
| Memory Lite | 128-entry KV with LRU and flat-file persistence |
| Security Lite | Audit log, injection detection, boot/model verification |
| Observability Lite | Counters, latency tracker, health check |
| Sensor | 32-sensor registry with calibration and filtering |
| OTA Update | Hash-verified updates with rollback |
| Compression | Quantization recommender, size/quality estimator |
| Power Manager | Battery/thermal-aware inference throttling |

### Framework Tier (17 modules)
| Service | Description |
|---|---|
| Runtime Manager | Pool of 8 runtimes with hot-switching |
| Orchestrator | DAG workflow engine with 9 step types |
| Connectors (4) | MQTT, OPC-UA, Modbus TCP, CAN bus |
| Memory | Namespaced KV with TTL, GC, persistence |
| Policy Engine | ALLOW/DENY/AUDIT rules with wildcards |
| Observability | Counters, gauges, histograms, trace spans |
| Adaptive Engine | LoRA fine-tuning with feedback pipeline |
| Federated Learning | FedAvg, differential privacy, round management |
| Update Manager | A/B partitions, maintenance windows, changelog |
| Secure Boot | Boot chain verification, key management, attestation |
| Supply Chain | SBOM, vendor trust, license compliance |
| Sensor Fusion | Weighted avg, Kalman, voting — 8 groups × 8 sources |
| Network Security | TLS/mTLS, cert store, key rotation |
| Guardrails | Safety rules, rate limiting, autonomy levels, kill switch |

---

## Compliance

eAI is designed and documented following international standards:

- **ISO/IEC/IEEE 15288:2023** — System lifecycle processes
- **ISO/IEC 20243 (O-TTPS)** — Open Trusted Technology Provider Standard
- **ISO/IEC 25000 (SQuaRE)** — Software quality requirements and evaluation

See [`docs/compliance/`](docs/compliance/) for traceability matrix, risk register, and audit logs.

---

## License

MIT License — see [LICENSE](LICENSE) for details.

Copyright (c) 2026 EoS Project.
