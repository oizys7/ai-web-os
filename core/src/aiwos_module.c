#include "..\..\abi\aiwos_abi.h"
#include "core_api.h"

#include <string.h>

static aiwos_host_api_t g_host;
static aiwos_state_snapshot_t g_state;
static int g_initialized = 0;

static void aiwos_log_literal(const char *text) {
    size_t len;
    if (!g_initialized || g_host.log == 0 || text == 0) {
        return;
    }

    len = strlen(text);
    g_host.log(text, len);
}

uint32_t aiwos_abi_version(void) {
    return AIWOS_ABI_VERSION;
}

int32_t aiwos_init(const aiwos_host_api_t *host) {
    /* 初始化阶段先校验 ABI，再复制宿主能力，避免后续直接依赖外部状态。 */
    if (host == 0 || host->host_abi_version != AIWOS_ABI_VERSION || host->log == 0 || host->now_ns == 0) {
        return AIWOS_ERR_INCOMPATIBLE_ABI;
    }

    memset(&g_host, 0, sizeof(g_host));
    memset(&g_state, 0, sizeof(g_state));

    g_host = *host;
    g_state.last_now_ns = g_host.now_ns();
    g_initialized = 1;

    aiwos_log_literal("aiwos_init: initialized");
    return AIWOS_OK;
}

int32_t aiwos_tick(uint64_t now_ns) {
    if (!g_initialized) {
        return AIWOS_ERR_NOT_INITIALIZED;
    }

    /* 核心状态推进只依赖内部状态和显式时间输入。 */
    g_state.tick_count = aiwos_core_next_tick(g_state.tick_count);
    g_state.last_now_ns = now_ns;
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

    /* 事件先进入核心层，再更新快照，便于后续做回放和可视化。 */
    aiwos_core_apply_event(&g_state, event);
    aiwos_log_literal("aiwos_handle_event: event applied");
    return AIWOS_OK;
}

int32_t aiwos_query_state(uint32_t query_id, void *out_buf, uintptr_t out_len) {
    aiwos_state_snapshot_t *snapshot;

    if (!g_initialized) {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if (query_id != 0u || out_buf == 0) {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }
    if (out_len < (uintptr_t)sizeof(aiwos_state_snapshot_t)) {
        return AIWOS_ERR_BUFFER_TOO_SMALL;
    }

    snapshot = (aiwos_state_snapshot_t *)out_buf;
    *snapshot = g_state;
    return AIWOS_OK;
}

void aiwos_shutdown(void) {
    if (!g_initialized) {
        return;
    }

    /* 退出时清空宿主引用和内部快照，避免下次初始化误用旧状态。 */
    aiwos_log_literal("aiwos_shutdown: shutdown complete");
    memset(&g_host, 0, sizeof(g_host));
    memset(&g_state, 0, sizeof(g_state));
    g_initialized = 0;
}
