#include "..\..\abi\aiwos_abi.h"
#include "core_api.h"
#include "scheduler.h"

#include <string.h>

static aiwos_host_api_t g_host;
static aiwos_state_snapshot_t g_state;
static aiwos_scheduler_t g_sched;
static int g_initialized = 0;

static void aiwos_log_literal(const char *text) {
    size_t len;
    if (!g_initialized || g_host.log == 0 || text == 0) {
        return;
    }

    len = strlen(text);
    g_host.log(text, len);
}

/* 将调度器状态同步到快照 */
static void aiwos_sync_snapshot(void) {
    g_state.scheduler_state = (uint32_t)g_sched.state;
    g_state.task_count = g_sched.task_count;
    g_state.ready_count = aiwos_scheduler_ready_count(&g_sched);
    g_state.blocked_count = aiwos_scheduler_blocked_count(&g_sched);
    g_state.last_now_ns = g_sched.last_tick_ns;
}

uint32_t aiwos_abi_version(void) {
    return AIWOS_ABI_VERSION;
}

int32_t aiwos_init(const aiwos_host_api_t *host) {
    if (host == 0 || host->host_abi_version != AIWOS_ABI_VERSION || host->log == 0 || host->now_ns == 0) {
        return AIWOS_ERR_INCOMPATIBLE_ABI;
    }

    memset(&g_host, 0, sizeof(g_host));
    memset(&g_state, 0, sizeof(g_state));

    g_host = *host;
    g_initialized = 1;

    aiwos_scheduler_init(&g_sched);

    g_sched.last_tick_ns = g_host.now_ns();
    aiwos_sync_snapshot();

    aiwos_log_literal("aiwos_init: initialized");
    return AIWOS_OK;
}

int32_t aiwos_tick(uint64_t now_ns) {
    if (!g_initialized) {
        return AIWOS_ERR_NOT_INITIALIZED;
    }

    g_state.tick_count = aiwos_core_next_tick(g_state.tick_count);

    /* 推进调度器 */
    aiwos_scheduler_tick(&g_sched, now_ns);

    /* 更新核心状态统计 */
    g_state.last_now_ns = now_ns;
    aiwos_sync_snapshot();

    aiwos_log_literal("aiwos_tick: tick advanced");
    return AIWOS_OK;
}

int32_t aiwos_handle_event(const aiwos_event_t *event) {
    if (!g_initialized) {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if (event == 0) {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }

    g_state.last_event_type = event->type;
    g_state.last_event_data = event->data;

    /* 事件传递给核心层处理 */
    aiwos_core_apply_event(&g_state, event);

    aiwos_log_literal("aiwos_handle_event: event applied");
    return AIWOS_OK;
}

int32_t aiwos_query_state(uint32_t query_id, void *out_buf, uintptr_t out_len) {
    if (!g_initialized) {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if (out_buf == 0) {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }

    switch (query_id) {
    case AIWOS_QUERY_STATE_SNAPSHOT: {
        aiwos_state_snapshot_t *snapshot;

        if (out_len < (uintptr_t)sizeof(aiwos_state_snapshot_t)) {
            return AIWOS_ERR_BUFFER_TOO_SMALL;
        }

        aiwos_sync_snapshot();
        snapshot = (aiwos_state_snapshot_t *)out_buf;
        *snapshot = g_state;
        return AIWOS_OK;
    }
    case AIWOS_QUERY_SCHEDULER_STATS: {
        aiwos_scheduler_stats_t *stats;

        if (out_len < (uintptr_t)sizeof(aiwos_scheduler_stats_t)) {
            return AIWOS_ERR_BUFFER_TOO_SMALL;
        }

        stats = (aiwos_scheduler_stats_t *)out_buf;
        memset(stats, 0, sizeof(*stats));
        stats->total_ticks = g_sched.total_ticks;
        stats->last_tick_ns = g_sched.last_tick_ns;
        stats->min_vruntime = g_sched.min_vruntime;
        stats->task_count = g_sched.task_count;
        stats->ready_count = aiwos_scheduler_ready_count(&g_sched);
        stats->blocked_count = aiwos_scheduler_blocked_count(&g_sched);
        return AIWOS_OK;
    }
    default:
        return AIWOS_ERR_NOT_SUPPORTED;
    }
}

/* ============ 宿主内存抽象 ============ */

void *aiwos_alloc(size_t size) {
    return g_host.alloc ? g_host.alloc(size) : 0;
}

void aiwos_free(void *ptr) {
    if (g_host.free && ptr) {
        g_host.free(ptr);
    }
}

void *aiwos_realloc(void *ptr, size_t size) {
    return g_host.realloc ? g_host.realloc(ptr, size) : 0;
}

void aiwos_shutdown(void) {
    if (!g_initialized) {
        return;
    }

    aiwos_scheduler_shutdown(&g_sched);
    aiwos_log_literal("aiwos_shutdown: shutdown complete");

    memset(&g_host, 0, sizeof(g_host));
    memset(&g_state, 0, sizeof(g_state));
    g_initialized = 0;
}
