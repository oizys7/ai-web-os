#ifndef AIWOS_TEST_FRAMEWORK_H
#define AIWOS_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============ 测试输出格式配置 ============ */
/* 支持 TAP (Test Anything Protocol) 和 JSON 两种格式 */

#define TEST_OUTPUT_TAP   1
#define TEST_OUTPUT_JSON  2

/* 默认使用 TAP 格式，便于跨语言解析 */
#ifndef TEST_OUTPUT_FORMAT
#define TEST_OUTPUT_FORMAT TEST_OUTPUT_TAP
#endif

/* ============ 测试统计结构 ============ */
typedef struct test_stats {
    uint32_t total;
    uint32_t passed;
    uint32_t failed;
    uint32_t skipped;
} test_stats_t;

/* 全局测试统计 */
extern test_stats_t g_test_stats;

/* ============ 测试断言宏 ============ */

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            test_fail(__FILE__, __LINE__, #condition, message); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(a, b, message) \
    do { \
        if ((a) != (b)) { \
            test_fail_eq(__FILE__, __LINE__, #a, #b, (uint64_t)(a), (uint64_t)(b), message); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_NE(a, b, message) \
    do { \
        if ((a) == (b)) { \
            test_fail_ne(__FILE__, __LINE__, #a, #b, (uint64_t)(a), (uint64_t)(b), message); \
            return; \
        } \
    } while (0)

#define TEST_ASSERT_NULL(ptr, message) \
    TEST_ASSERT((ptr) == 0, message)

#define TEST_ASSERT_NOT_NULL(ptr, message) \
    TEST_ASSERT((ptr) != 0, message)

#define TEST_ASSERT_STR_EQ(a, b, message) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            test_fail_str_eq(__FILE__, __LINE__, #a, #b, (a), (b), message); \
            return; \
        } \
    } while (0)

/* ============ 测试用例定义宏 ============ */

#define TEST_CASE(name) \
    static void test_##name(void); \
    static test_case_t test_case_##name = { #name, test_##name }; \
    __attribute__((used)) \
    static test_case_t* test_reg_##name = test_register(&test_case_##name); \
    static void test_##name(void)

/* ============ 内部结构体（其他语言可解析） ============ */
typedef struct test_case {
    const char *name;
    void (*func)(void);
} test_case_t;

/* 测试套件结构（用于跨语言测试编排） */
typedef struct test_suite {
    const char *name;
    test_case_t *cases;
    uint32_t case_count;
} test_suite_t;

/* ============ 测试框架 API ============ */

/* 注册测试用例 */
test_case_t* test_register(test_case_t *test_case);

/* 运行所有测试 */
int test_run_all(void);

/* 运行指定测试套件 */
int test_run_suite(const char *suite_name);

/* 获取测试统计 */
test_stats_t test_get_stats(void);

/* 重置测试统计 */
void test_reset_stats(void);

/* ============ 失败处理函数 ============ */
void test_fail(const char *file, int line, const char *condition, const char *message);
void test_fail_eq(const char *file, int line, const char *a, const char *b,
                  uint64_t actual, uint64_t expected, const char *message);
void test_fail_ne(const char *file, int line, const char *a, const char *b,
                  uint64_t actual, uint64_t expected, const char *message);
void test_fail_str_eq(const char *file, int line, const char *a, const char *b,
                      const char *actual, const char *expected, const char *message);

/* ============ 输出格式函数 ============ */
void test_output_tap_begin(void);
void test_output_tap_end(void);
void test_output_tap_case(const char *name, int passed, const char *message);

void test_output_json_begin(void);
void test_output_json_end(void);
void test_output_json_case(const char *name, int passed, const char *message);

/* ============ 跨语言测试 ABI ============ */
/* 其他语言可以通过这些函数获取测试信息 */

/* 获取测试用例数量 */
uint32_t test_get_case_count(void);

/* 获取第 N 个测试用例的名称 */
const char* test_get_case_name(uint32_t index);

/* 运行指定索引的测试用例 */
int test_run_case(uint32_t index);

#ifdef __cplusplus
}
#endif

#endif
