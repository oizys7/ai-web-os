#include "test_framework.h"
#include "../../abi/aiwos_abi.h"

/* ============ ABI 版本测试 ============ */

TEST_CASE(abi_version_constant) {
    TEST_ASSERT_EQ(AIWOS_ABI_VERSION, 3u, "ABI version should be 3");
}

TEST_CASE(abi_version_function) {
    uint32_t v = aiwos_abi_version();
    TEST_ASSERT_EQ(v, AIWOS_ABI_VERSION, "abi_version() should match ABI_VERSION macro");
}

/* ============ 错误码测试 ============ */

TEST_CASE(error_ok_is_zero) {
    TEST_ASSERT_EQ((int32_t)AIWOS_OK, 0, "AIWOS_OK should be 0");
}

TEST_CASE(error_codes_negative) {
    TEST_ASSERT((int32_t)AIWOS_ERR_INVALID_ARGUMENT < 0, "error codes should be negative");
    TEST_ASSERT((int32_t)AIWOS_ERR_INCOMPATIBLE_ABI < 0, "error codes should be negative");
    TEST_ASSERT((int32_t)AIWOS_ERR_BUFFER_TOO_SMALL < 0, "error codes should be negative");
    TEST_ASSERT((int32_t)AIWOS_ERR_NOT_INITIALIZED < 0, "error codes should be negative");
    TEST_ASSERT((int32_t)AIWOS_ERR_NOT_FOUND < 0, "error codes should be negative");
    TEST_ASSERT((int32_t)AIWOS_ERR_LIMIT_REACHED < 0, "error codes should be negative");
    TEST_ASSERT((int32_t)AIWOS_ERR_NOT_SUPPORTED < 0, "error codes should be negative");
}

TEST_CASE(error_codes_unique) {
    /* 验证所有错误码互不相同 */
    int32_t codes[] = {
        AIWOS_OK,
        AIWOS_ERR_INVALID_ARGUMENT,
        AIWOS_ERR_INCOMPATIBLE_ABI,
        AIWOS_ERR_BUFFER_TOO_SMALL,
        AIWOS_ERR_NOT_INITIALIZED,
        AIWOS_ERR_NOT_FOUND,
        AIWOS_ERR_LIMIT_REACHED,
        AIWOS_ERR_NOT_SUPPORTED,
    };
    uint32_t n = sizeof(codes) / sizeof(codes[0]);
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = i + 1; j < n; j++) {
            TEST_ASSERT_NE(codes[i], codes[j], "error codes must be unique");
        }
    }
}

/* ============ 事件类型测试 ============ */

TEST_CASE(event_types_values) {
    TEST_ASSERT_EQ((uint32_t)AIWOS_EVENT_NONE, 0u, "AIWOS_EVENT_NONE should be 0");
    TEST_ASSERT_EQ((uint32_t)AIWOS_EVENT_INTERRUPT, 1u, "AIWOS_EVENT_INTERRUPT should be 1");
    TEST_ASSERT_EQ((uint32_t)AIWOS_EVENT_TIMER, 2u, "AIWOS_EVENT_TIMER should be 2");
}

/* ============ 查询 ID 测试 ============ */

TEST_CASE(query_ids_values) {
    TEST_ASSERT_EQ((uint32_t)AIWOS_QUERY_STATE_SNAPSHOT, 0u, "STATE_SNAPSHOT should be 0");
    TEST_ASSERT_EQ((uint32_t)AIWOS_QUERY_SCHEDULER_STATS, 1u, "SCHEDULER_STATS should be 1");
}

/* ============ 结构体大小测试 ============ */
/* 这些测试值依赖于编译器和架构，但可以捕获意外的 ABI 破坏 */

TEST_CASE(event_struct_size) {
    /* type(4) + data(8) = 16 */
    TEST_ASSERT_EQ(sizeof(aiwos_event_t), 16u, "aiwos_event_t should be 16 bytes");
}

TEST_CASE(event_struct_offsets) {
    /* 验证字段偏移 */
    aiwos_event_t ev;
    ev.type = 0xDEAD;
    ev.data = 0xBEEF;
    TEST_ASSERT_EQ(ev.type, 0xDEADu, "event type field should work");
    TEST_ASSERT_EQ(ev.data, 0xBEEFu, "event data field should work");
}

TEST_CASE(snapshot_struct_size) {
    TEST_ASSERT_EQ(sizeof(aiwos_state_snapshot_t), 56u,
                   "aiwos_state_snapshot_t should be 56 bytes");
}

TEST_CASE(scheduler_stats_struct_size) {
    TEST_ASSERT_EQ(sizeof(aiwos_scheduler_stats_t), 56u,
                   "aiwos_scheduler_stats_t should be 56 bytes");
}

TEST_CASE(host_api_struct_size) {
    /* host_abi_version(4) + pad(4) + log(8) + now_ns(8) + alloc(8) + free(8) + realloc(8) = 48 */
    TEST_ASSERT_EQ(sizeof(aiwos_host_api_t), 48u,
                   "aiwos_host_api_t should be 48 bytes with v3 memory functions");
}

/* ============ 结构体赋值/清零测试 ============ */
/* 验证 _reserved 字段被正确清零，不是垃圾数据 */

TEST_CASE(snapshot_reserved_are_zero) {
    aiwos_state_snapshot_t snap;
    memset(&snap, 0, sizeof(snap));

    /* 写非保留字段，保留字段应保持为零 */
    snap.tick_count = 42;
    snap.last_now_ns = 1000;
    snap.scheduler_state = 1;
    snap.task_count = 5;

    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQ(snap._reserved[i], 0u, "reserved fields should remain zero");
    }
}

TEST_CASE(scheduler_stats_reserved_are_zero) {
    aiwos_scheduler_stats_t stats;
    memset(&stats, 0, sizeof(stats));

    stats.total_ticks = 100;
    stats.task_count = 3;

    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_EQ(stats._reserved[i], 0u, "reserved fields should remain zero");
    }
}

/* ============ 测试列表 ============ */

static const test_case_t g_all_tests[] = {
    TEST_DECL(abi_version_constant),
    TEST_DECL(abi_version_function),
    TEST_DECL(error_ok_is_zero),
    TEST_DECL(error_codes_negative),
    TEST_DECL(error_codes_unique),
    TEST_DECL(event_types_values),
    TEST_DECL(query_ids_values),
    TEST_DECL(event_struct_size),
    TEST_DECL(event_struct_offsets),
    TEST_DECL(snapshot_struct_size),
    TEST_DECL(scheduler_stats_struct_size),
    TEST_DECL(host_api_struct_size),
    TEST_DECL(snapshot_reserved_are_zero),
    TEST_DECL(scheduler_stats_reserved_are_zero),
};

static const uint32_t g_test_count = sizeof(g_all_tests) / sizeof(g_all_tests[0]);

int main(void) {
    return test_run_array(g_all_tests, g_test_count);
}
