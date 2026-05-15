#ifndef AIWOS_ABI_H
#define AIWOS_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 版本兼容 ============ */

#define AIWOS_ABI_VERSION 3u

/* ============ 状态码 ============ */

typedef enum aiwos_status {
    AIWOS_OK                = 0,
    AIWOS_ERR_INVALID_ARGUMENT   = -1,
    AIWOS_ERR_INCOMPATIBLE_ABI  = -2,
    AIWOS_ERR_BUFFER_TOO_SMALL  = -3,
    AIWOS_ERR_NOT_INITIALIZED   = -4,
    AIWOS_ERR_NOT_FOUND         = -5,
    AIWOS_ERR_LIMIT_REACHED     = -6,
    AIWOS_ERR_NOT_SUPPORTED     = -7,
} aiwos_status_t;

/* ============ 事件类型 ============ */

typedef enum aiwos_event_type {
    AIWOS_EVENT_NONE      = 0,
    AIWOS_EVENT_INTERRUPT = 1,
    AIWOS_EVENT_TIMER     = 2,
} aiwos_event_type_t;

/* ============ 查询 ID ============ */

typedef enum aiwos_query_id {
    /* 系统状态快照，输出 aiwos_state_snapshot_t */
    AIWOS_QUERY_STATE_SNAPSHOT   = 0,
    /* 调度器统计，输出 aiwos_scheduler_stats_t */
    AIWOS_QUERY_SCHEDULER_STATS  = 1,
} aiwos_query_id_t;

/* ============ 事件结构 ============ */

typedef struct aiwos_event {
    uint32_t type;
    uint64_t data;
} aiwos_event_t;

/* ============ 状态快照 ============ */

typedef struct aiwos_state_snapshot {
    uint64_t tick_count;
    uint64_t last_now_ns;
    uint32_t last_event_type;
    uint32_t scheduler_state;      /* aiwos_scheduler_state_t */
    uint64_t last_event_data;
    uint32_t task_count;
    uint32_t ready_count;
    uint32_t blocked_count;
    uint32_t _reserved[3];         /* 保留字段，为后续扩展预留 */
} aiwos_state_snapshot_t;

/* ============ 调度器统计 ============ */

typedef struct aiwos_scheduler_stats {
    uint64_t total_ticks;
    uint64_t last_tick_ns;
    uint64_t min_vruntime;
    uint32_t task_count;
    uint32_t ready_count;
    uint32_t blocked_count;
    uint32_t _reserved[5];
} aiwos_scheduler_stats_t;

/* ============ 宿主能力接口 ============ */

typedef struct aiwos_host_api {
    uint32_t host_abi_version;
    void (*log)(const char *message, size_t len);
    uint64_t (*now_ns)(void);
    void *(*alloc)(size_t size);
    void (*free)(void *ptr);
    void *(*realloc)(void *ptr, size_t size);
} aiwos_host_api_t;

/* ============ 导出函数 ============ */

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
