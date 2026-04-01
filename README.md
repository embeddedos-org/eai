# 🤖 EAI — Embedded AI Layer

[![CI](https://github.com/embeddedos-org/eai/actions/workflows/ci.yml/badge.svg)](https://github.com/embeddedos-org/eai/actions/workflows/ci.yml)
[![Nightly](https://github.com/embeddedos-org/eai/actions/workflows/nightly.yml/badge.svg)](https://github.com/embeddedos-org/eai/actions/workflows/nightly.yml)
[![Release](https://github.com/embeddedos-org/eai/actions/workflows/release.yml/badge.svg)](https://github.com/embeddedos-org/eai/actions/workflows/release.yml)
[![Version](https://img.shields.io/github/v/tag/embeddedos-org/eai?label=version)](https://github.com/embeddedos-org/eai/releases/latest)

**AI inference and agent framework for the EoS embedded OS platform.**

EAI provides on-device LLM inference, agent orchestration, and tool execution — from MCUs to edge servers. Supports Neuralink intents via ENI/EIPC bridge.

## Install

```bash
# Clone
git clone https://github.com/embeddedos-org/eai.git
cd eai

# Build (minimal + framework + models)
cmake -B build -DEAI_BUILD_TESTS=ON
cmake --build build

# Build minimal only (for embedded)
cmake -B build -DEAI_BUILD_MIN=ON -DEAI_BUILD_FRAMEWORK=OFF

# With EIPC bridge (receive ENI intents)
cmake -B build -DEAI_EIPC_ENABLED=ON

# Cross-compile for RPi4
cmake -B build-arm64 -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_SYSTEM_NAME=Linux
cmake --build build-arm64

# With real llama.cpp backend
cmake -B build -DEAI_LLAMA_CPP=ON

# With adaptive learning (on by default)
cmake -B build -DEAI_BUILD_ADAPTIVE=ON
```

## Configurations

| Config | Option | Description |
|---|---|---|
| **Minimal** | `EAI_BUILD_MIN=ON` | Lightweight runtime for resource-constrained devices. llama.cpp backend, simple agent loop, memory-lite context. For RPi3/4, i.MX8M, MCUs with external RAM. |
| **Framework** | `EAI_BUILD_FRAMEWORK=ON` | Full industrial AI platform with runtime manager, connector bus, orchestrator, observability. For edge servers, Jetson, x86. |

Both configurations share the common library (tools, security, config, EIPC listener, logging).

## AI Interaction Models

| Model | Status | Description |
|---|---|---|
| Command & Control | ✅ | Tool calling, GPIO, MQTT, sensor reads |
| Conversational | ✅ | Multi-turn chat with memory persistence |
| Autonomous Agent | ✅ | Think→Act→Observe loop with tool chains |
| Ambient / Passive | ✅ | Workflow-driven sensor monitoring & alerts |
| **Adaptive / Personalized** | ✅ | User preferences, feedback, LoRA fine-tuning |

## Adaptive / Personalized AI (v0.2.0)

EAI supports on-device personalization through preference tracking, feedback collection, and LoRA adapter fine-tuning.

### Features

- **User Preference Store** — 256-entry typed key-value store with weight-based decay and persistence
- **Feedback Pipeline** — Ring buffer collects inference quality scores, generates training samples from positive feedback
- **LoRA Adapter Support** — Load/unload LoRA adapters via llama.cpp (`llama_model_apply_lora_from_file`)
- **Resource-Gated Training** — Training only runs when RAM and CPU temp are within budget
- **Preference-Aware Prompting** — Top-10 user preferences injected into agent prompts

### Configuration

```yaml
# In profile.yaml or via eai_adaptive_config_t
adaptive:
  enabled: true
  learning_rate: 0.0001
  lora_rank: 8
  preference_decay_days: 30
  max_training_memory_mb: 512
  train_during_idle: true
```

### New Built-in Tools

| Tool | Permission | Description |
|---|---|---|
| `preference.set` | `preference:write` | Store a user preference |
| `preference.get` | `preference:read` | Retrieve a preference |
| `feedback.record` | `feedback:write` | Record inference quality score |
| `model.status` | `model:read` | Report model/adapter/training state |

### Models with LoRA Support

All 12 models in the registry include training metadata (`train_ram_mb`, `lora_supported`, `lora_adapter_mb`). Example: TinyLlama 1.1B needs 240MB for LoRA fine-tuning and produces ~10MB adapters.

## LLM Models for EoS

EAI includes a curated registry of **12 embedded-optimized LLM models** across 5 hardware tiers:

| Tier | RAM | Model | Params | Hardware | Use Case |
|---|---|---|---|---|---|
| **Micro** | < 100MB | TinyLlama 1.1B Q2_K | 1.1B | STM32H7, ESP32-S3 | Command parsing, simple Q&A |
| **Tiny** | 100-500MB | SmolLM 360M Q5 | 360M | RPi3, nRF5340 | Fast command routing |
| | | Qwen2 0.5B Q4 | 500M | RPi3, AM64x | Multilingual, tool calling |
| | | Phi-1.5 Q4 | 1.3B | RPi3, BeagleBone | Reasoning, code generation |
| **Small** | 500MB-2GB | Qwen2 1.5B Q4 | 1.5B | RPi4 (4GB), SiFive | Tool calling, function routing |
| | | Gemma 2B Q4 | 2B | RPi4 (8GB), Jetson Nano | Instruction following |
| | | Phi-2 Q4 | 2.7B | RPi4, i.MX8M | Reasoning, code, math |
| | | **Phi-3-mini Q4** (default) | 3.8B | RPi4 (8GB), i.MX8M+ | **EoS default** — general assistant |
| **Medium** | 2-4GB | Llama 3.2 3B Q4 | 3B | RPi5, Jetson Nano | State-of-art small, tool use |
| | | Mistral 7B Q3_K | 7B | Jetson Nano, x86 | General assistant |
| **Large** | 4GB+ | Llama 3.2 8B Q4 | 8B | x86 edge, Jetson Orin | Full-capability assistant |
| | | Qwen2.5 7B Q4 | 7B | x86 edge, Jetson Orin | Best multilingual + coding |

## Profiles

Pre-configured EAI profiles for common use cases:

| Profile | Model | Tools | Use Case |
|---|---|---|---|
| `robot-controller` | phi-mini-q4 | sensor, motor, mqtt | Robotic control |
| `smart-camera` | smollm-360m-q5 | http, mqtt | Vision inference |
| `industrial-gateway` | qwen2-1.5b-q4 | mqtt, sensor, gpio | Industrial IoT |
| `mobile-edge` | phi-1.5-q4 | http, system | Mobile edge AI |
| `adaptive-edge` | llama.cpp + LoRA | prefs, feedback, mqtt | Personalized edge AI |

## Built-in Tools

| Tool | Description |
|---|---|
| `mqtt.publish` | Publish to MQTT broker |
| `device.read_sensor` | Read sensor value via HAL |
| `http.get` | HTTP GET request |
| `preference.set` | Store user preference |
| `preference.get` | Retrieve user preference |
| `feedback.record` | Record inference quality score |
| `model.status` | Report model/adapter status |

## Ebot Server

HTTP/JSON AI assistant server at `192.168.1.100:8420`:

| Endpoint | Method | Description |
|---|---|---|
| `/v1/chat` | POST | Chat with conversation history |
| `/v1/complete` | POST | Single-shot text completion |
| `/v1/tools` | GET | List available tools |
| `/v1/models` | GET | List available LLM models |
| `/v1/status` | GET | Server stats (requests, tokens) |
| `/v1/reset` | POST | Clear conversation history |

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      EAI v0.2.0                              │
│                                                              │
│  ┌───────────────────────────────────────────────────────┐   │
│  │              Model Registry (12 models)                │   │
│  │  micro: TinyLlama   tiny: SmolLM, Qwen2, Phi-1.5     │   │
│  │  small: Phi-2, Gemma, Phi-mini, Qwen2-1.5B           │   │
│  │  medium: Llama-3.2-3B, Mistral-7B                     │   │
│  │  large: Llama-3.2-8B, Qwen2.5-7B                     │   │
│  └──────────────────────┬────────────────────────────────┘   │
│                         │                                     │
│  ┌──────────────┐  ┌───▼──────────┐  ┌──────────────────┐   │
│  │  EAI-Min     │  │   Common     │  │  EAI-Framework   │   │
│  │  (lightweight)│  │              │  │  (full platform) │   │
│  │  agent loop  │  │  tools       │  │  runtime manager │   │
│  │  llama.cpp   │  │  security    │  │  connector bus   │   │
│  │  memory-lite │  │  config      │  │  orchestrator    │   │
│  │  router      │  │  adaptive    │  │  observability   │   │
│  │  LoRA adapter│  │  feedback    │  │  adaptive engine │   │
│  └──────────────┘  │  logging     │  └──────────────────┘   │
│                    └──────────────┘                           │
└─────────────────────────────────────────────────────────────┘
```

## Standards Compliance

This project is part of the EoS ecosystem and aligns with international standards including ISO/IEC/IEEE 15288:2023, ISO/IEC 12207, ISO/IEC/IEEE 42010, ISO/IEC 25000, ISO/IEC 25010, ISO/IEC 27001, ISO/IEC 15408, IEC 61508, ISO 26262, DO-178C, FIPS 140-3, POSIX (IEEE 1003), WCAG 2.1, and more. See the [EoS Compliance Documentation](https://github.com/embeddedos-org/.github/tree/master/docs/compliance) for full details including NTIA SBOM, SPDX, CycloneDX, and OpenChain compliance.

## License

MIT License — see [LICENSE](LICENSE) for details.
