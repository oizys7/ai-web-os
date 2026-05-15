#include "test_framework.h"
#include <stdlib.h>

test_stats_t g_test_stats = {0, 0, 0, 0};
static const char *g_current_test_name = NULL;

/* ============ 测试运行 ============ */

int test_run_array(const test_case_t *tests, uint32_t count) {
#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
    test_output_tap_begin(count);
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
    test_output_json_begin();
#endif

    for (uint32_t i = 0; i < count; i++) {
        const test_case_t *tc = &tests[i];
        uint32_t failed_before = g_test_stats.failed;

        g_test_stats.total++;
        g_current_test_name = tc->name;  /* 设置当前测试名称 */

        /* 运行测试 */
        tc->func();

        /* 如果测试过程中失败计数增加了，说明测试失败 */
        if (g_test_stats.failed > failed_before) {
            /* 测试失败，已经由 test_fail_xxx 输出了 */
        } else {
            /* 测试通过 */
#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
            test_output_tap_case(tc->name, 1, NULL);
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
            test_output_json_case(tc->name, 1, NULL);
#endif
        }
    }

#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
    test_output_tap_end();
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
    test_output_json_end();
#endif

    return (g_test_stats.failed == 0) ? 0 : 1;
}

test_stats_t test_get_stats(void) {
    return g_test_stats;
}

void test_reset_stats(void) {
    memset(&g_test_stats, 0, sizeof(g_test_stats));
}

/* ============ 失败处理 ============ */

void test_fail(const char *file, int line, const char *condition, const char *message) {
    char fail_msg[512];
    snprintf(fail_msg, sizeof(fail_msg), "%s:%d: %s (condition: %s)",
             file, line, message ? message : "assertion failed", condition);

    g_test_stats.failed++;

    /* 找到当前运行的测试名称 */
    const char *test_name = g_current_test_name ? g_current_test_name : "unknown";
    /* 无法知道测试名称，因为测试是通过数组传入的 */

#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
    test_output_tap_case(test_name, 0, fail_msg);
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
    test_output_json_case(test_name, 0, fail_msg);
#endif
}

void test_fail_eq(const char *file, int line, const char *a, const char *b,
                  uint64_t actual, uint64_t expected, const char *message) {
    char fail_msg[512];
    snprintf(fail_msg, sizeof(fail_msg),
             "%s:%d: %s (expected %s == %s, got %llu != %llu)",
             file, line, message ? message : "equality check failed",
             a, b, (unsigned long long)actual, (unsigned long long)expected);

    g_test_stats.failed++;

    const char *test_name = g_current_test_name ? g_current_test_name : "unknown";

#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
    test_output_tap_case(test_name, 0, fail_msg);
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
    test_output_json_case(test_name, 0, fail_msg);
#endif
}

void test_fail_ne(const char *file, int line, const char *a, const char *b,
                  uint64_t actual, uint64_t expected, const char *message) {
    char fail_msg[512];
    snprintf(fail_msg, sizeof(fail_msg),
             "%s:%d: %s (expected %s != %s, got %llu == %llu)",
             file, line, message ? message : "inequality check failed",
             a, b, (unsigned long long)actual, (unsigned long long)expected);

    g_test_stats.failed++;

    const char *test_name = g_current_test_name ? g_current_test_name : "unknown";

#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
    test_output_tap_case(test_name, 0, fail_msg);
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
    test_output_json_case(test_name, 0, fail_msg);
#endif
}

void test_fail_str_eq(const char *file, int line, const char *a, const char *b,
                      const char *actual, const char *expected, const char *message) {
    char fail_msg[512];
    snprintf(fail_msg, sizeof(fail_msg),
             "%s:%d: %s (expected %s == %s, got \"%s\" != \"%s\")",
             file, line, message ? message : "string equality check failed",
             a, b, actual, expected);

    g_test_stats.failed++;

    const char *test_name = g_current_test_name ? g_current_test_name : "unknown";

#if TEST_OUTPUT_FORMAT == TEST_OUTPUT_TAP
    test_output_tap_case(test_name, 0, fail_msg);
#elif TEST_OUTPUT_FORMAT == TEST_OUTPUT_JSON
    test_output_json_case(test_name, 0, fail_msg);
#endif
}

/* ============ TAP 输出格式 ============ */

void test_output_tap_begin(uint32_t count) {
    printf("TAP version 13\n");
    printf("1..%u\n", count);
}

void test_output_tap_end(void) {
    printf("# %u passed, %u failed, %u total\n",
           g_test_stats.total - g_test_stats.failed,
           g_test_stats.failed, g_test_stats.total);
}

void test_output_tap_case(const char *name, int passed, const char *message) {
    if (passed) {
        g_test_stats.passed++;
        printf("ok - %s\n", name);
    } else {
        printf("not ok - %s", name);
        if (message) {
            printf(": %s", message);
        }
        printf("\n");
    }
}

/* ============ JSON 输出格式 ============ */

static int json_first_case = 1;

void test_output_json_begin(void) {
    json_first_case = 1;
    printf("{\n");
    printf("  \"version\": 1,\n");
    printf("  \"framework\": \"aiwos-test\",\n");
    printf("  \"tests\": [\n");
}

void test_output_json_end(void) {
    printf("\n  ],\n");
    printf("  \"summary\": {\n");
    printf("    \"total\": %u,\n", g_test_stats.total);
    printf("    \"passed\": %u,\n", g_test_stats.passed);
    printf("    \"failed\": %u,\n", g_test_stats.failed);
    printf("    \"skipped\": %u\n", g_test_stats.skipped);
    printf("  }\n");
    printf("}\n");
}

void test_output_json_case(const char *name, int passed, const char *message) {
    if (!json_first_case) {
        printf(",\n");
    }
    json_first_case = 0;

    printf("    {\"name\": \"%s\", \"status\": \"%s\"",
           name, passed ? "passed" : "failed");

    if (message && !passed) {
        printf(", \"message\": \"");
        for (const char *p = message; *p; p++) {
            switch (*p) {
                case '\\': printf("\\\\"); break;
                case '"':  printf("\\\""); break;
                case '\n': printf("\\n"); break;
                case '\r': printf("\\r"); break;
                case '\t': printf("\\t"); break;
                default:   putchar(*p); break;
            }
        }
        printf("\"");
    }
    printf("}");
}
