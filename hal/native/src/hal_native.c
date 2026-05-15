#include "hal_native.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void aiwos_native_log(const char *message, size_t len) {
    if (message == 0) {
        return;
    }

    printf("[hal-native] %.*s\n", (int)len, message);
}

static void *aiwos_native_alloc(size_t size) {
    return malloc(size);
}

static void aiwos_native_free(void *ptr) {
    free(ptr);
}

static void *aiwos_native_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

static uint64_t aiwos_native_now_ns(void) {
    FILETIME ft;
    ULARGE_INTEGER ticks;
    const uint64_t EPOCH_DIFF_100NS = 116444736000000000ull;

    /* Windows 下用高精度系统时间转换成 Unix epoch 纳秒。 */
    GetSystemTimePreciseAsFileTime(&ft);
    ticks.LowPart = ft.dwLowDateTime;
    ticks.HighPart = ft.dwHighDateTime;

    if (ticks.QuadPart < EPOCH_DIFF_100NS) {
        return 0ull;
    }

    return (ticks.QuadPart - EPOCH_DIFF_100NS) * 100ull;
}

void aiwos_hal_native_fill_host_api(aiwos_host_api_t *host) {
    if (host == 0) {
        return;
    }

    /* 这里把本机宿主能力装配成 ABI 约定好的 host_api。 */
    memset(host, 0, sizeof(*host));
    host->host_abi_version = AIWOS_ABI_VERSION;
    host->log = aiwos_native_log;
    host->now_ns = aiwos_native_now_ns;
    host->alloc = aiwos_native_alloc;
    host->free = aiwos_native_free;
    host->realloc = aiwos_native_realloc;
}
