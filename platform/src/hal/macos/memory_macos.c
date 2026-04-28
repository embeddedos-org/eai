// SPDX-License-Identifier: MIT
// HAL Memory implementation for macOS

#include "eai/platform.h"
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE

#include <mach/mach.h>
#include <sys/sysctl.h>

static eai_status_t macos_hal_get_memory_info(eai_platform_t *plat,
                                               uint64_t *total, uint64_t *available)
{
    if (total) {
        int64_t phys_mem = 0;
        size_t len = sizeof(phys_mem);
        if (sysctlbyname("hw.memsize", &phys_mem, &len, NULL, 0) == 0)
            *total = (uint64_t)phys_mem;
        else
            *total = 0;
    }

    if (available) {
        vm_statistics64_data_t vm_stat;
        memset(&vm_stat, 0, sizeof(vm_stat));
        mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
        if (host_statistics64(mach_host_self(), HOST_VM_INFO64,
                              (host_info64_t)&vm_stat, &count) == KERN_SUCCESS) {
            vm_size_t page_size;
            host_page_size(mach_host_self(), &page_size);
            *available = (uint64_t)(vm_stat.free_count + vm_stat.inactive_count) * page_size;
        } else {
            *available = 0;
        }
    }

    return EAI_OK;
}

static void *macos_hal_alloc_aligned(size_t size, size_t alignment)
{
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
}

static void macos_hal_free_aligned(void *ptr)
{
    free(ptr);
}

static eai_status_t macos_hal_get_heap_stats(eai_platform_t *plat, eai_heap_stats_t *stats)
{
    if (!stats) return EAI_ERR_INVALID;
    memset(stats, 0, sizeof(*stats));
    return EAI_ERR_NOT_IMPLEMENTED;
}

const eai_hal_memory_ops_t eai_hal_macos_memory_ops = {
    .get_memory_info = macos_hal_get_memory_info,
    .alloc_aligned   = macos_hal_alloc_aligned,
    .free_aligned    = macos_hal_free_aligned,
    .get_heap_stats  = macos_hal_get_heap_stats,
};

#endif /* !TARGET_OS_IPHONE */
#endif /* __APPLE__ */
