# Getting Started with eAI BCI

## Quick Start (Simulator)

### Build

```bash
mkdir build && cd build
cmake .. -DEAI_BUILD_BCI=ON -DEAI_BUILD_TESTS=ON
cmake --build . --parallel
```

### Run Tests

```bash
ctest --output-on-failure
```

### C Example

```c
#include "eai_bci/eai_bci.h"

int main(void) {
    eai_bci_device_t dev;
    eai_bci_decoder_t dec;
    eai_bci_output_t out;
    eai_bci_pipeline_t pipe;

    // Initialize with simulator
    eai_bci_device_init(&dev, &eai_bci_device_simulator_ops, NULL, 0);
    eai_bci_decoder_init(&dec, &eai_bci_decoder_threshold_ops, NULL, 0);
    eai_bci_output_init(&out, &eai_bci_output_log_ops, NULL, 0);

    // Create pipeline (1-50 Hz bandpass, 50 Hz notch)
    eai_bci_pipeline_init(&pipe, &dev, &dec, &out, 1.0f, 50.0f, 50.0f);
    eai_bci_pipeline_start(&pipe);

    // Run 100 decode cycles
    for (int i = 0; i < 100; i++) {
        eai_bci_pipeline_step(&pipe);
        const eai_bci_intent_t *intent = eai_bci_pipeline_last_intent(&pipe);
        printf("Intent: %s (%.2f)\n", intent->label, intent->confidence);
    }

    eai_bci_pipeline_shutdown(&pipe);
    return 0;
}
```

### Python Example

```python
from eai_bci import BciPipeline

with BciPipeline(simulator=True) as bci:
    for _ in range(100):
        bci.poll()
        intent = bci.get_intent()
        print(f"{intent['label']} ({intent['confidence']:.2f})")
```

## MCU Deployment

For ARM Cortex-M4 targets:

```bash
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-arm-cortex-m4.cmake \
         -DEAI_BUILD_BCI=ON -DEAI_BUILD_FRAMEWORK=OFF
```

This automatically sets `EAI_BCI_MAX_CHANNELS=4` and `EAI_BCI_RING_SIZE=128`
for a ~5 KB BCI memory footprint.
