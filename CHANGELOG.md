# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-03-31

### Added
- Initial release of eai
- **EAI-Min:** Lightweight runtime for resource-constrained devices
- **EAI-Framework:** Industrial-grade AI platform with orchestrator and policy engine
- **LLM Runtime:** llama.cpp backend with 12 curated models (TinyLlama 80MB to Llama 3.2 8B)
- **Agent Loop:** Think-act-observe cycle with tool framework (MQTT, sensor, HTTP)
- **Ebot Server:** Interactive AI assistant at 192.168.1.100:8420
- **5 hardware tiers:** Nano (≤256KB), Micro (≤16MB), Edge (≤1GB), Standard (≤8GB), Cloud
- **Adaptive/Personalized AI:** User preference store, feedback buffer, LoRA adapter support
- **Runtime Training:** load_adapter/unload_adapter, train_step, save_checkpoint
- **Preference-Aware Agent:** Injects top-10 preferences and feedback summary into prompts
- **Built-in Tools:** preference.set, preference.get, feedback.record, model.status
- **Framework Adaptive Engine:** Resource-gated training with RAM + CPU temp checks
- **Profiles:** smart-camera, industrial-gateway, robot-controller, mobile-edge, adaptive-edge
- **EIPC Integration:** Optional ENI→EAI communication bridge
- Complete CI/CD pipeline with nightly, weekly, EoSim sanity, and simulation test runs
- Full cross-platform support (Linux, Windows, macOS)
- ISO/IEC standards compliance documentation
- MIT license

[0.1.0]: https://github.com/embeddedos-org/eai/releases/tag/v0.1.0
