# eAI Porting Guide

> How to add a new platform adapter to the eAI framework using the HAL sub-vtable system.

---

## Overview

eAI's platform layer uses a **composable HAL (Hardware Abstraction Layer)** with sub-vtables for different hardware domains. Each platform implements the sub-vtables it supports and sets unsupported ones to `NULL`.

## HAL Sub-Vtables

| Sub-vtable | Header | Required? | Purpose |
|---|---|---|---|
| `eai_hal_core_ops_t` | `hal_core.h` | **Yes** | Init, shutdown, device info, CPU info |
| `eai_hal_memory_ops_t` | `hal_memory.h` | **Yes** | Memory info, aligned allocation |
| `eai_hal_gpio_ops_t` | `hal_gpio.h` | No | GPIO read/write/configure |
| `eai_hal_thread_ops_t` | `hal_thread.h` | No | Threading, mutexes, semaphores |
| `eai_hal_fs_ops_t` | `hal_fs.h` | No | File I/O operations |
| `eai_hal_net_ops_t` | `hal_net.h` | No | Network sockets |
| `eai_hal_timer_ops_t` | `hal_timer.h` | No | High-resolution timers |
| `eai_hal_accel_ops_t` | `hal_accel.h` | No | Hardware accelerator enumeration |

## Step-by-Step: Adding a New Platform

### 1. Create the source directory

```
platform/src/hal/myplatform/
├── core_myplatform.c
├── memory_myplatform.c
└── (other sub-modules as needed)
```

### 2. Implement the core ops (required)

```c
// platform/src/hal/myplatform/core_myplatform.c
#include "eai/platform.h"
#include "eai/log.h"

#if defined(EAI_PLATFORM_MYPLATFORM)

static eai_status_t myplat_init(eai_platform_t *plat) {
    // Initialize your platform here
    return EAI_OK;
}

static void myplat_shutdown(eai_platform_t *plat) {
    // Cleanup
}

static eai_status_t myplat_get_device_info(eai_platform_t *plat, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "MyPlatform v1.0");
    return EAI_OK;
}

static eai_status_t myplat_get_os_name(eai_platform_t *plat, char *buf, size_t buf_size) {
    snprintf(buf, buf_size, "MyPlatform");
    return EAI_OK;
}

static eai_status_t myplat_get_cpu_temp(eai_platform_t *plat, float *temp_c) {
    return EAI_ERR_NOT_IMPLEMENTED;
}

static int myplat_get_cpu_count(eai_platform_t *plat) {
    return 1;
}

const eai_hal_core_ops_t eai_hal_myplatform_core_ops = {
    .init            = myplat_init,
    .shutdown        = myplat_shutdown,
    .get_device_info = myplat_get_device_info,
    .get_os_name     = myplat_get_os_name,
    .get_cpu_temp    = myplat_get_cpu_temp,
    .get_cpu_count   = myplat_get_cpu_count,
};

#endif
```

### 3. Implement memory ops (required)

Implement at minimum `get_memory_info`, `alloc_aligned`, and `free_aligned`.

### 4. Implement optional sub-modules

Only implement what your platform supports:
- **MCUs**: Typically `core` + `memory` + `gpio` + `timer`
- **Desktop/Server**: Typically `core` + `memory` + `thread` + `fs` + `net` + `timer`
- **Mobile**: Typically `core` + `memory` + `thread` + `fs` + `timer`

### 5. Register in the HAL

```c
static const eai_platform_hal_t myplatform_hal = {
    .name   = "myplatform",
    .core   = &eai_hal_myplatform_core_ops,
    .memory = &eai_hal_myplatform_memory_ops,
    .gpio   = &eai_hal_myplatform_gpio_ops,  // or NULL
    .thread = NULL,  // Not supported
    .fs     = NULL,  // Not supported
    .net    = NULL,  // Not supported
    .timer  = &eai_hal_myplatform_timer_ops,
    .accel  = NULL,
};
```

### 6. Update CMakeLists.txt

Add your sources to `platform/CMakeLists.txt` with appropriate guards:

```cmake
if(EAI_PLATFORM_MYPLATFORM)
    target_sources(eai_platform PRIVATE
        src/hal/myplatform/core_myplatform.c
        src/hal/myplatform/memory_myplatform.c
    )
    target_compile_definitions(eai_platform PUBLIC EAI_PLATFORM_MYPLATFORM)
endif()
```

### 7. Add platform detection (optional)

If auto-detection is possible, add it to `eai_platform_detect()` in `platform/src/platform.c`.

### 8. Add tests

Create `tests/test_myplatform.c` with platform-specific tests gated by `#ifdef`.

## Existing Platform Adapters

| Platform | Directory | Status |
|---|---|---|
| Linux | `hal/linux/` + `hal/posix/` | Complete |
| Windows | `hal/windows/` | Complete |
| macOS | `hal/macos/` + `hal/posix/` | Complete |
| Android | `hal/android/` + `hal/posix/` | Complete |
| iOS | `hal/ios/` | Core only |
| Zephyr RTOS | `hal/zephyr/` | Core only (stub) |
| FreeRTOS | `hal/freertos/` | Core only (stub) |
| Bare-metal | `hal/baremetal/` | Core only |
| Container | `hal/container/` + `hal/posix/` | Complete |

## Reusing POSIX Code

For any POSIX-compatible platform (Linux, macOS, Android, containers), you can reuse the shared implementations in `hal/posix/`:
- `thread_posix.c` — pthreads
- `fs_posix.c` — POSIX file I/O
- `net_posix.c` — BSD sockets
- `timer_posix.c` — clock_gettime / nanosleep

Simply link these in your CMakeLists.txt instead of writing platform-specific versions.
