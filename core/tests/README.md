# 测试框架文档

## 概述

这是一个自包含的 C 测试框架，专为 ai-web-os 项目设计，支持多语言测试集成。

## 设计原则

1. **自包含**: 不依赖外部测试库
2. **可移植**: 可在 C、Rust、JavaScript、Python 等语言中运行相同测试
3. **标准化输出**: 支持 TAP 和 JSON 格式，便于其他语言解析
4. **ABI 稳定**: 测试注册和运行通过稳定的 C ABI

## 输出格式

### TAP (Test Anything Protocol)

默认输出格式，便于其他语言解析：

```tap
TAP version 13
1..42
ok - scheduler_init_null
ok - scheduler_init_basic
not ok - schedule_next_empty_scheduler: expected non-NULL
...
# 40 passed, 2 failed, 42 total
```

### JSON 输出

编译时定义 `TEST_OUTPUT_FORMAT=TEST_OUTPUT_JSON` 启用：

```json
{
  "version": 1,
  "framework": "aiwos-test",
  "total": 42,
  "tests": [
    {"name": "scheduler_init_null", "status": "passed"},
    {"name": "scheduler_init_basic", "status": "passed"},
    {"name": "schedule_next_empty_scheduler", "status": "failed", "message": "..."}
  ],
  "summary": {
    "total": 42,
    "passed": 40,
    "failed": 2,
    "skipped": 0
  }
}
```

## 构建

```bash
cd build
cmake ..
cmake --build .
```

## 运行测试

### 运行所有测试

```bash
# 使用 CTest
ctest

# 或直接运行
./test_scheduler
```

### 运行单个测试

```bash
./test_scheduler
```

## 多语言集成

### Rust 集成示例

```rust
// 通过 FFI 调用 C 测试
extern "C" {
    fn test_get_case_count() -> u32;
    fn test_get_case_name(index: u32) -> *const i8;
    fn test_run_case(index: u32) -> i32;
}

fn run_c_tests_from_rust() {
    let count = unsafe { test_get_case_count() };
    for i in 0..count {
        let name = unsafe { std::ffi::CStr::from_ptr(test_get_case_name(i)) };
        println!("Running: {:?}", name);
        let result = unsafe { test_run_case(i) };
        assert_eq!(result, 0);
    }
}
```

### Python 集成示例

```python
import ctypes
import json

# 加载测试库
lib = ctypes.CDLL('./build/test_scheduler.exe')

# 获取测试数量
lib.test_get_case_count.restype = ctypes.c_uint32
count = lib.test_get_case_count()

# 运行所有测试
results = []
for i in range(count):
    lib.test_run_case.argtypes = [ctypes.c_uint32]
    lib.test_run_case.restype = ctypes.c_int32
    result = lib.test_run_case(i)
    results.append(result)
```

### JavaScript (Node.js) 集成

```javascript
const ffi = require('ffi-napi');

const lib = ffi.Library('./test_scheduler', {
    'test_get_case_count': ['uint32', []],
    'test_get_case_name': ['string', ['uint32']],
    'test_run_case': ['int32', ['uint32']]
});

const count = lib.test_get_case_count();
for (let i = 0; i < count; i++) {
    const name = lib.test_get_case_name(i);
    console.log(`Running: ${name}`);
    lib.test_run_case(i);
}
```

## 添加新测试

```c
#include "test_framework.h"

TEST_CASE(my_feature_basic) {
    aiwos_scheduler_t sched;
    aiwos_scheduler_init(&sched);

    TEST_ASSERT_EQ(sched.state, AIWOS_SCHED_IDLE, "should be idle");

    // 更多测试...
}

TEST_CASE(my_feature_edge_case) {
    // 边界情况测试
}
```

## 测试覆盖率

当前 `test_scheduler.c` 覆盖以下函数：

### 调度器核心

- `aiwos_scheduler_init`
- `aiwos_scheduler_shutdown`
- `aiwos_schedule_next`
- `aiwos_scheduler_tick`

### 任务管理

- `aiwos_task_create`
- `aiwos_task_destroy`
- `aiwos_task_get`

### 状态控制

- `aiwos_task_yield`
- `aiwos_task_block`
- `aiwos_task_wakeup`
- `aiwos_task_wakeup_by_event`
- `aiwos_task_complete`

### 查询函数

- `aiwos_scheduler_current`
- `aiwos_scheduler_ready_count`
- `aiwos_scheduler_blocked_count`
- `aiwos_scheduler_has_ready`

### 权重计算

- `aiwos_nice_to_weight`
- `aiwos_calc_delta_vruntime`
