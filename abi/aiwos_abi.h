#ifndef AIWOS_ABI_H
#define AIWOS_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AIWOS_ABI_VERSION 1u

typedef enum aiwos_status {
    AIWOS_OK = 0,
    AIWOS_ERR_INVALID_ARGUMENT = -1,
    AIWOS_ERR_INCOMPATIBLE_ABI = -2,
    AIWOS_ERR_BUFFER_TOO_SMALL = -3,
    AIWOS_ERR_NOT_INITIALIZED = -4
} aiwos_status_t;

typedef enum aiwos_event_type {
    AIWOS_EVENT_NONE = 0,
    AIWOS_EVENT_INTERRUPT = 1,
    AIWOS_EVENT_TIMER = 2
} aiwos_event_type_t;

typedef struct aiwos_event {
    uint32_t type;
    uint64_t data;
} aiwos_event_t;

typedef struct aiwos_state_snapshot {
    uint64_t tick_count;
    uint64_t last_now_ns;
    uint32_t last_event_type;
    uint32_t reserved0;
    uint64_t last_event_data;
} aiwos_state_snapshot_t;

typedef struct aiwos_host_api {
    uint32_t host_abi_version;
    void (*log)(const char *message, size_t len);
    uint64_t (*now_ns)(void);
} aiwos_host_api_t;

uint32_t aiwos_abi_version(void);
int32_t aiwos_init(const aiwos_host_api_t *host);
int32_t aiwos_tick(uint64_t now_ns);
int32_t aiwos_handle_event(const aiwos_event_t *event);
int32_t aiwos_query_state(uint32_t query_id, void *out_buf, uintptr_t out_len);
void aiwos_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif

