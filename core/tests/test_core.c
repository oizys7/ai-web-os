#include "test_framework.h"
#include "..\include\core_api.h"
#include <stdlib.h>
#include <string.h>

/* ============ 测试辅助：宿主 API ============ */

static uint64_t test_now_ns_val;
static int test_log_called;

static uint64_t test_now_ns(void) {
    return test_now_ns_val;
}

static void test_log_noop(const char *msg, size_t len) {
    (void)msg;
    (void)len;
    test_log_called = 1;
}

static void fill_default_host(aiwos_host_api_t *host) {
    memset(host, 0, sizeof(*host));
    host->host_abi_version = AIWOS_ABI_VERSION;
    host->log = test_log_noop;
    host->now_ns = test_now_ns;
}

static void reset_test_globals(void) {
    test_now_ns_val = 1000000;
    test_log_called = 0;
}

/* ============ aiwos_init 测试 ============ */

TEST_CASE(init_null_host) {
    int32_t rc = aiwos_init(NULL);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INCOMPATIBLE_ABI, "init with NULL should fail");
}

TEST_CASE(init_wrong_abi_version) {
    aiwos_host_api_t host;
    fill_default_host(&host);
    host.host_abi_version = 0; /* 错误版本 */

    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INCOMPATIBLE_ABI, "init with wrong ABI version should fail");
}

TEST_CASE(init_outdated_abi) {
    aiwos_host_api_t host;
    fill_default_host(&host);
    host.host_abi_version = AIWOS_ABI_VERSION - 1;

    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INCOMPATIBLE_ABI, "init with outdated ABI should fail");
}

TEST_CASE(init_null_log) {
    aiwos_host_api_t host;
    fill_default_host(&host);
    host.log = 0;

    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INCOMPATIBLE_ABI, "init with NULL log should fail");
}

TEST_CASE(init_null_now_ns) {
    aiwos_host_api_t host;
    fill_default_host(&host);
    host.now_ns = 0;

    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INCOMPATIBLE_ABI, "init with NULL now_ns should fail");
}

TEST_CASE(init_success) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);

    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "init should succeed");
    aiwos_shutdown();
}

TEST_CASE(init_log_called) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);

    test_log_called = 0;
    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "init should succeed");
    TEST_ASSERT(test_log_called != 0, "init should invoke host log");
    aiwos_shutdown();
}

/* ============ aiwos_tick 测试 ============ */

TEST_CASE(tick_before_init) {
    reset_test_globals();

    int32_t rc = aiwos_tick(2000000);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_NOT_INITIALIZED, "tick before init should fail");
}

TEST_CASE(tick_after_init) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    int32_t rc = aiwos_tick(test_now_ns_val + 1000);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "tick after init should succeed");

    aiwos_shutdown();
}

TEST_CASE(tick_increments_count) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    aiwos_state_snapshot_t snap;
    aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
    uint64_t before = snap.tick_count;

    aiwos_tick(test_now_ns_val + 2000);

    aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
    TEST_ASSERT(snap.tick_count > before, "tick count should increase after tick");

    aiwos_shutdown();
}

TEST_CASE(tick_updates_last_now) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    uint64_t t1 = test_now_ns_val + 5000;
    aiwos_tick(t1);

    aiwos_state_snapshot_t snap;
    aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
    TEST_ASSERT_EQ(snap.last_now_ns, t1, "tick should update last_now_ns");

    aiwos_shutdown();
}

/* ============ aiwos_handle_event 测试 ============ */

TEST_CASE(handle_event_before_init) {
    reset_test_globals();

    aiwos_event_t ev;
    ev.type = AIWOS_EVENT_TIMER;
    ev.data = 42;

    int32_t rc = aiwos_handle_event(&ev);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_NOT_INITIALIZED, "handle_event before init should fail");
}

TEST_CASE(handle_event_null) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    int32_t rc = aiwos_handle_event(NULL);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INVALID_ARGUMENT, "handle_event with NULL event should fail");

    aiwos_shutdown();
}

TEST_CASE(handle_event_timer) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    aiwos_event_t ev;
    ev.type = AIWOS_EVENT_TIMER;
    ev.data = 12345;

    int32_t rc = aiwos_handle_event(&ev);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "handle_event should succeed");

    aiwos_state_snapshot_t snap;
    aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
    TEST_ASSERT_EQ(snap.last_event_type, AIWOS_EVENT_TIMER, "event type should be recorded");
    TEST_ASSERT_EQ(snap.last_event_data, 12345u, "event data should be recorded");

    aiwos_shutdown();
}

TEST_CASE(handle_event_interrupt) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    aiwos_event_t ev;
    ev.type = AIWOS_EVENT_INTERRUPT;
    ev.data = 0;

    int32_t rc = aiwos_handle_event(&ev);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "handle_event should handle INTERRUPT");

    aiwos_shutdown();
}

/* ============ aiwos_query_state 测试 ============ */

TEST_CASE(query_before_init) {
    reset_test_globals();

    aiwos_state_snapshot_t snap;
    int32_t rc = aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
    TEST_ASSERT_EQ(rc, AIWOS_ERR_NOT_INITIALIZED, "query before init should fail");
}

TEST_CASE(query_null_buffer) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    int32_t rc = aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, NULL, sizeof(aiwos_state_snapshot_t));
    TEST_ASSERT_EQ(rc, AIWOS_ERR_INVALID_ARGUMENT, "query with NULL buffer should fail");

    aiwos_shutdown();
}

TEST_CASE(query_buffer_too_small) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    char small_buf[4];
    int32_t rc = aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, small_buf, sizeof(small_buf));
    TEST_ASSERT_EQ(rc, AIWOS_ERR_BUFFER_TOO_SMALL, "query with small buffer should fail");

    aiwos_shutdown();
}

TEST_CASE(query_state_snapshot) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    aiwos_tick(test_now_ns_val + 1000);

    aiwos_state_snapshot_t snap;
    memset(&snap, 0xAA, sizeof(snap));
    int32_t rc = aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap));
    TEST_ASSERT_EQ(rc, AIWOS_OK, "snapshot query should succeed");
    TEST_ASSERT(snap.tick_count > 0, "snapshot should report tick_count > 0");

    aiwos_shutdown();
}

TEST_CASE(query_scheduler_stats) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    aiwos_scheduler_stats_t stats;
    memset(&stats, 0xAA, sizeof(stats));
    int32_t rc = aiwos_query_state(AIWOS_QUERY_SCHEDULER_STATS, &stats, sizeof(stats));
    TEST_ASSERT_EQ(rc, AIWOS_OK, "scheduler stats query should succeed");
    TEST_ASSERT_EQ(stats.task_count, 0u, "stats should show 0 tasks initially");

    aiwos_shutdown();
}

TEST_CASE(query_unknown_id) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);

    uint32_t buf[4];
    int32_t rc = aiwos_query_state(9999, buf, sizeof(buf));
    TEST_ASSERT_EQ(rc, AIWOS_ERR_NOT_SUPPORTED, "unknown query ID should return NOT_SUPPORTED");

    aiwos_shutdown();
}

/* ============ aiwos_shutdown 测试 ============ */

TEST_CASE(shutdown_before_init) {
    /* 未初始化时调用 shutdown 不应崩溃 */
    aiwos_shutdown();
    TEST_ASSERT(1, "shutdown before init should not crash");
}

TEST_CASE(shutdown_then_reinit) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);
    aiwos_shutdown();

    /* 再次初始化应成功 */
    reset_test_globals();
    int32_t rc = aiwos_init(&host);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "re-init after shutdown should succeed");

    /* 再次使用应正常 */
    rc = aiwos_tick(test_now_ns_val + 1000);
    TEST_ASSERT_EQ(rc, AIWOS_OK, "tick after re-init should succeed");

    aiwos_shutdown();
}

TEST_CASE(shutdown_clears_state) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    aiwos_init(&host);
    aiwos_shutdown();

    /* shutdown 后调用 tick 应返回 NOT_INITIALIZED */
    int32_t rc = aiwos_tick(test_now_ns_val + 2000);
    TEST_ASSERT_EQ(rc, AIWOS_ERR_NOT_INITIALIZED, "tick after shutdown should fail");
}

/* ============ 完整的生命周期测试 ============ */

TEST_CASE(full_lifecycle) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);

    /* init */
    TEST_ASSERT_EQ(aiwos_init(&host), AIWOS_OK, "init");

    /* tick */
    TEST_ASSERT_EQ(aiwos_tick(test_now_ns_val + 1000), AIWOS_OK, "first tick");

    /* event */
    aiwos_event_t ev;
    ev.type = AIWOS_EVENT_TIMER;
    ev.data = 42;
    TEST_ASSERT_EQ(aiwos_handle_event(&ev), AIWOS_OK, "handle event");

    /* tick again */
    TEST_ASSERT_EQ(aiwos_tick(test_now_ns_val + 2000), AIWOS_OK, "second tick");

    /* query snapshot */
    aiwos_state_snapshot_t snap;
    TEST_ASSERT_EQ(aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, &snap, sizeof(snap)),
                   AIWOS_OK, "query snapshot");
    TEST_ASSERT(snap.tick_count >= 2, "should have at least 2 ticks");
    TEST_ASSERT_EQ(snap.last_event_type, AIWOS_EVENT_TIMER, "event type recorded");
    TEST_ASSERT_EQ(snap.last_event_data, 42u, "event data recorded");

    /* query stats */
    aiwos_scheduler_stats_t stats;
    TEST_ASSERT_EQ(aiwos_query_state(AIWOS_QUERY_SCHEDULER_STATS, &stats, sizeof(stats)),
                   AIWOS_OK, "query scheduler stats");

    /* shutdown */
    aiwos_shutdown();

    /* post shutdown ops fail */
    TEST_ASSERT_EQ(aiwos_tick(test_now_ns_val + 3000), AIWOS_ERR_NOT_INITIALIZED, "tick after shutdown");
}

/* ============ 宿主内存抽象测试 ============ */

TEST_CASE(memory_alloc_basic) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    host.alloc = malloc;
    host.free = free;
    host.realloc = realloc;
    aiwos_init(&host);

    void *p = aiwos_alloc(64);
    TEST_ASSERT_NOT_NULL(p, "aiwos_alloc should return non-NULL");
    if (p) {
        memset(p, 0xAB, 64);
        aiwos_free(p);
    }

    aiwos_shutdown();
}

TEST_CASE(memory_realloc_grow) {
    reset_test_globals();
    aiwos_host_api_t host;
    fill_default_host(&host);
    host.alloc = malloc;
    host.free = free;
    host.realloc = realloc;
    aiwos_init(&host);

    void *p = aiwos_alloc(16);
    TEST_ASSERT_NOT_NULL(p, "initial alloc should succeed");
    if (p) {
        memset(p, 0xCD, 16);
        void *q = aiwos_realloc(p, 64);
        TEST_ASSERT_NOT_NULL(q, "realloc should succeed");
        if (q) {
            aiwos_free(q);
        } else {
            aiwos_free(p);
        }
    }

    aiwos_shutdown();
}

TEST_CASE(memory_free_null) {
    /* free(NULL) 不应崩溃 */
    aiwos_free(NULL);
    TEST_ASSERT(1, "free(NULL) should not crash");
}

/* ============ 测试列表 ============ */

static const test_case_t g_all_tests[] = {
    /* aiwos_init 测试 */
    TEST_DECL(init_null_host),
    TEST_DECL(init_wrong_abi_version),
    TEST_DECL(init_outdated_abi),
    TEST_DECL(init_null_log),
    TEST_DECL(init_null_now_ns),
    TEST_DECL(init_success),
    TEST_DECL(init_log_called),

    /* aiwos_tick 测试 */
    TEST_DECL(tick_before_init),
    TEST_DECL(tick_after_init),
    TEST_DECL(tick_increments_count),
    TEST_DECL(tick_updates_last_now),

    /* aiwos_handle_event 测试 */
    TEST_DECL(handle_event_before_init),
    TEST_DECL(handle_event_null),
    TEST_DECL(handle_event_timer),
    TEST_DECL(handle_event_interrupt),

    /* aiwos_query_state 测试 */
    TEST_DECL(query_before_init),
    TEST_DECL(query_null_buffer),
    TEST_DECL(query_buffer_too_small),
    TEST_DECL(query_state_snapshot),
    TEST_DECL(query_scheduler_stats),
    TEST_DECL(query_unknown_id),

    /* aiwos_shutdown 测试 */
    TEST_DECL(shutdown_before_init),
    TEST_DECL(shutdown_then_reinit),
    TEST_DECL(shutdown_clears_state),

    /* 完整生命周期 */
    TEST_DECL(full_lifecycle),

    /* 内存抽象 */
    TEST_DECL(memory_alloc_basic),
    TEST_DECL(memory_realloc_grow),
    TEST_DECL(memory_free_null),
};

static const uint32_t g_test_count = sizeof(g_all_tests) / sizeof(g_all_tests[0]);

int main(void) {
    return test_run_array(g_all_tests, g_test_count);
}
