# eAI BCI Architecture

## Overview

The eAI BCI module extends the embedded AI framework with Brain-Computer Interface
capabilities for neural signal acquisition, preprocessing, decoding, and assistive
output. It follows all existing eAI architectural patterns.

## Data Flow

```
┌──────────┐    ┌─────────────┐    ┌─────────┐    ┌────────┐
│  Device   │───▶│ Preprocessor│───▶│ Decoder │───▶│ Output │
│ (EEG hw) │    │ (IIR/notch) │    │(ML/SNN) │    │(GPIO)  │
└──────────┘    └─────────────┘    └─────────┘    └────────┘
     ▲                                   │
     │              Pipeline             │
     └──────────── orchestrates ─────────┘
```

## Core Vtable Interfaces

### eai_bci_device_ops_t
Hardware abstraction for BCI signal sources.
- `init`, `start_stream`, `stop_stream`, `read_samples`, `calibrate`, `get_impedance`, `shutdown`
- Implementations: simulator, OpenBCI, Emotiv, Muse

### eai_bci_decoder_ops_t
Neural signal decoder abstraction.
- `init`, `decode(signal → intent)`, `reset`, `shutdown`
- Implementations: threshold, SNN, TinyML, ML-bridge

### eai_bci_output_ops_t
Assistive output abstraction.
- `init`, `execute(intent)`, `shutdown`
- Implementations: log, GPIO, keyboard, cursor, prosthetic

## Memory Budget (MCU)

| Component | RAM |
|-----------|-----|
| Signal ring buffer (256 × 4ch × 4B) | 4,096 B |
| Filter state | 128 B |
| Decoder state | 256 B |
| Pipeline struct | 512 B |
| **BCI total** | **~5 KB** |

## Plugin System

Plugins bundle optional `device_ops`, `decoder_ops`, and `output_ops` into
registerable units. The plugin registry follows the `eai_tool_registry_t` pattern.

## Build Options

| CMake Option | Default | Description |
|-------------|---------|-------------|
| EAI_BUILD_BCI | ON | Enable BCI module |
| EAI_BCI_SIMULATOR | ON | Built-in EEG simulator |
| EAI_BCI_OPENBCI | OFF | OpenBCI Cyton adapter |
| EAI_BCI_EMOTIV | OFF | Emotiv EPOC+/FLEX adapter |
| EAI_BCI_MUSE | OFF | Muse EEG adapter |
| EAI_BCI_TINYML | OFF | TFLite Micro decoder |
