//! ai-web-os 核心
//!
//! 核心调度器的 Rust 实现，编译为：
//! - **Wasm**（为 wasm32 编译为 cdylib）—— 由浏览器数字孪生加载
//! - **原生**（为 x64 编译为 staticlib）—— 由 C 宿主程序链接
//!
//! 两种目标上的公开 API 都是 `abi/aiwos_abi.h` 中定义的同一组 `extern "C"` 函数。
//! 唯一的区别是宿主回调（日志、时间）的解析方式：
//!
//! - **Wasm** —— 通过实例化时提供的 `extern "C"` 导入
//! - **原生** —— 从 `aiwos_host_api_t` 结构体存储的函数指针
//!
//! ## 构建
//! ```bash
//! # Wasm
//! cargo build --target wasm32-unknown-unknown --release
//!
//! # 原生 staticlib
//! cargo build --release
//! ```

// 测试时需要 std（test runner 不兼容 no_std + 自定义 panic_handler）
#![cfg_attr(not(test), no_std)]
#![allow(non_camel_case_types)]
#![allow(static_mut_refs)]

use core::ptr;

// ============================================================================
// 宿主回调层
// ============================================================================

// Wasm 目标：宿主通过 `WebAssembly.instantiate` 提供这些导入。
#[cfg(target_arch = "wasm32")]
extern "C" {
    fn host_log(ptr: *const u8, len: usize);
    fn host_now_ns() -> u64;
}

// 原生目标：在 `aiwos_init` 时存储，通过函数指针调用。
#[cfg(not(target_arch = "wasm32"))]
static mut HOST_LOG: Option<extern "C" fn(*const u8, usize)> = None;
#[cfg(not(target_arch = "wasm32"))]
static mut HOST_NOW_NS: Option<extern "C" fn() -> u64> = None;
#[cfg(not(target_arch = "wasm32"))]
static mut HOST_ALLOC: Option<extern "C" fn(usize) -> *mut u8> = None;
#[cfg(not(target_arch = "wasm32"))]
static mut HOST_FREE: Option<extern "C" fn(*mut u8)> = None;
#[cfg(not(target_arch = "wasm32"))]
static mut HOST_REALLOC: Option<extern "C" fn(*mut u8, usize) -> *mut u8> = None;

fn log_literal(text: &str) {
    let (p, l) = (text.as_ptr(), text.len());
    unsafe {
        #[cfg(target_arch = "wasm32")]
        host_log(p, l);
        #[cfg(not(target_arch = "wasm32"))]
        if let Some(log) = HOST_LOG {
            log(p, l);
        }
    }
}

fn now_ns() -> u64 {
    unsafe {
        #[cfg(target_arch = "wasm32")]
        return host_now_ns();
        #[cfg(not(target_arch = "wasm32"))]
        return match HOST_NOW_NS {
            Some(f) => f(),
            None => 0,
        };
    }
}

// ============================================================================
// ABI 常量
// ============================================================================

pub const AIWOS_ABI_VERSION: u32 = 3;

pub type aiwos_status_t = i32;

pub const AIWOS_OK: aiwos_status_t = 0;
pub const AIWOS_ERR_INVALID_ARGUMENT: aiwos_status_t = -1;
pub const AIWOS_ERR_INCOMPATIBLE_ABI: aiwos_status_t = -2;
pub const AIWOS_ERR_BUFFER_TOO_SMALL: aiwos_status_t = -3;
pub const AIWOS_ERR_NOT_INITIALIZED: aiwos_status_t = -4;
pub const AIWOS_ERR_NOT_FOUND: aiwos_status_t = -5;
pub const AIWOS_ERR_LIMIT_REACHED: aiwos_status_t = -6;
pub const AIWOS_ERR_NOT_SUPPORTED: aiwos_status_t = -7;

pub const AIWOS_EVENT_NONE: u32 = 0;
pub const AIWOS_EVENT_INTERRUPT: u32 = 1;
pub const AIWOS_EVENT_TIMER: u32 = 2;

pub const AIWOS_QUERY_STATE_SNAPSHOT: u32 = 0;
pub const AIWOS_QUERY_SCHEDULER_STATS: u32 = 1;

// ============================================================================
// ABI 结构体类型（repr(C)
// ============================================================================

#[repr(C)]
pub struct aiwos_event_t {
    pub type_: u32,
    pub data: u64,
}

#[repr(C)]
pub struct aiwos_state_snapshot_t {
    pub tick_count: u64,
    pub last_now_ns: u64,
    pub last_event_type: u32,
    pub scheduler_state: u32,
    pub last_event_data: u64,
    pub task_count: u32,
    pub ready_count: u32,
    pub blocked_count: u32,
    pub _reserved: [u32; 3],
}

#[repr(C)]
pub struct aiwos_scheduler_stats_t {
    pub total_ticks: u64,
    pub last_tick_ns: u64,
    pub min_vruntime: u64,
    pub task_count: u32,
    pub ready_count: u32,
    pub blocked_count: u32,
    pub _reserved: [u32; 5],
}

#[repr(C)]
pub struct aiwos_host_api_t {
    pub host_abi_version: u32,
    pub log: Option<extern "C" fn(*const u8, usize)>,
    pub now_ns: Option<extern "C" fn() -> u64>,
    pub alloc: Option<extern "C" fn(usize) -> *mut u8>,
    pub free: Option<extern "C" fn(*mut u8)>,
    pub realloc: Option<extern "C" fn(*mut u8, usize) -> *mut u8>,
}

// ============================================================================
// 内部调度器状态
// ============================================================================

const AIWOS_MAX_TASKS: usize = 64;
const AIWOS_SCHED_IDLE: u32 = 0;
const AIWOS_SCHED_SHUTDOWN: u32 = 2;
const AIWOS_TASK_READY: u32 = 0;
const AIWOS_TASK_RUNNING: u32 = 1;
const AIWOS_TASK_BLOCKED: u32 = 2;
const AIWOS_TASK_DONE: u32 = 4;
const AIWOS_BLOCK_NONE: u32 = 0;
const AIWOS_BLOCK_EVENT: u32 = 1;
const AIWOS_BLOCK_TIMEOUT: u32 = 2;
const AIWOS_BLOCK_RESOURCE: u32 = 3;
const AIWOS_BLOCK_AI_INFERENCE: u32 = 4;
const INVALID_IDX: u32 = AIWOS_MAX_TASKS as u32;

#[repr(C)]
#[derive(Clone, Copy)]
struct Task {
    id: u32,
    state: u32,
    vruntime: u64,
    runtime_accum: u64,
    block_reason: u32,
    block_timeout: u64,
    wait_event: u32,
    nice: i32,
    weight: u32,
    context: *mut u8,
}

static mut INITIALIZED: bool = false;
static mut TICK_COUNT: u64 = 0;
static mut LAST_NOW_NS: u64 = 0;
static mut LAST_EVENT_TYPE: u32 = 0;
static mut LAST_EVENT_DATA: u64 = 0;
static mut SCHED_STATE: u32 = AIWOS_SCHED_IDLE;
static mut CURRENT_IDX: u32 = INVALID_IDX;
static mut TASK_COUNT: u32 = 0;
static mut TASKS: [Task; AIWOS_MAX_TASKS] = [Task {
    id: 0,
    state: 0,
    vruntime: 0,
    runtime_accum: 0,
    block_reason: 0,
    block_timeout: 0,
    wait_event: 0,
    nice: 0,
    weight: 1024,
    context: core::ptr::null_mut(),
}; AIWOS_MAX_TASKS];
static mut MIN_VRUNTIME: u64 = 0;
static mut TOTAL_TICKS: u64 = 0;
static mut LAST_TICK_NS: u64 = 0;

// ============================================================================
// Nice → Weight 转换（Linux CFS 权重表）
// ============================================================================

const NICE_TO_WEIGHT: [u32; 40] = [
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */  9548,  7620,  6100,  4904,  3906,
    /*  -5 */  3121,  2501,  1991,  1586,  1277,
    /*   0 */  1024,   820,   655,   526,   423,
    /*   5 */   335,   272,   215,   172,   137,
    /*  10 */   110,    87,    70,    56,    45,
    /*  15 */    36,    29,    23,    18,    15,
];

fn nice_to_weight(nice: i32) -> u32 {
    let idx = nice.clamp(-20, 19) + 20;
    NICE_TO_WEIGHT[idx as usize]
}

#[allow(dead_code)]
fn calc_delta_vruntime(delta_ns: u64, weight: u32) -> u64 {
    let w = if weight == 0 { 1024 } else { weight };
    delta_ns * 1024 / w as u64
}

// ============================================================================
// 导出 ABI 函数
// ============================================================================

#[no_mangle]
pub extern "C" fn aiwos_abi_version() -> u32 {
    AIWOS_ABI_VERSION
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_init(host: *const aiwos_host_api_t) -> aiwos_status_t {
    if host.is_null() {
        return AIWOS_ERR_INCOMPATIBLE_ABI;
    }
    let host_ref = &*host;
    if host_ref.host_abi_version != AIWOS_ABI_VERSION {
        return AIWOS_ERR_INCOMPATIBLE_ABI;
    }

    #[cfg(not(target_arch = "wasm32"))]
    if host_ref.log.is_none() || host_ref.now_ns.is_none() {
        return AIWOS_ERR_INCOMPATIBLE_ABI;
    }

    // 原生目标：存储宿主回调（wasm 通过导入）
    #[cfg(not(target_arch = "wasm32"))]
    {
        HOST_LOG = host_ref.log;
        HOST_NOW_NS = host_ref.now_ns;
        HOST_ALLOC = host_ref.alloc;
        HOST_FREE = host_ref.free;
        HOST_REALLOC = host_ref.realloc;
    }

    // 重置调度器状态
    TICK_COUNT = 0;
    LAST_NOW_NS = 0;
    LAST_EVENT_TYPE = 0;
    LAST_EVENT_DATA = 0;
    SCHED_STATE = AIWOS_SCHED_IDLE;
    CURRENT_IDX = INVALID_IDX;
    TASK_COUNT = 0;
    MIN_VRUNTIME = 0;
    TOTAL_TICKS = 0;
    LAST_TICK_NS = 0;
    for i in 0..AIWOS_MAX_TASKS {
        TASKS[i] = Task {
            id: 0,
            state: 0,
            vruntime: 0,
            runtime_accum: 0,
            block_reason: 0,
            block_timeout: 0,
            wait_event: 0,
            nice: 0,
            weight: 1024,
            context: ptr::null_mut(),
        };
    }

    INITIALIZED = true;
    LAST_NOW_NS = now_ns();

    log_literal("aiwos_init: initialized");
    AIWOS_OK
}

#[no_mangle]
pub extern "C" fn aiwos_tick(tick_now_ns: u64) -> aiwos_status_t {
    unsafe {
        if !INITIALIZED {
            return AIWOS_ERR_NOT_INITIALIZED;
        }

        let delta_ns = if tick_now_ns > LAST_TICK_NS {
            tick_now_ns - LAST_TICK_NS
        } else {
            0
        };

        TOTAL_TICKS = TOTAL_TICKS.wrapping_add(1);
        TICK_COUNT = TICK_COUNT.wrapping_add(1);
        LAST_TICK_NS = tick_now_ns;
        LAST_NOW_NS = tick_now_ns;

        // 更新当前任务的 vruntime
        if CURRENT_IDX < AIWOS_MAX_TASKS as u32 {
            let task = &mut TASKS[CURRENT_IDX as usize];
            if task.state == AIWOS_TASK_RUNNING {
                let weight = if task.weight == 0 { 1024 } else { task.weight };
                let delta_vruntime = delta_ns * 1024 / weight as u64;
                task.vruntime = task.vruntime.wrapping_add(delta_vruntime);
                task.runtime_accum = task.runtime_accum.wrapping_add(delta_ns);
                if task.vruntime < MIN_VRUNTIME {
                    MIN_VRUNTIME = task.vruntime;
                }
            }
        }

        // 检查阻塞任务是否超时
        for i in 0..TASK_COUNT as usize {
            let task = &mut TASKS[i];
            if task.state == AIWOS_TASK_BLOCKED
                && task.block_timeout > 0
                && tick_now_ns >= task.block_timeout
            {
                task.state = AIWOS_TASK_READY;
                task.block_reason = AIWOS_BLOCK_NONE;
                task.block_timeout = 0;
            }
        }

        log_literal("aiwos_tick: tick advanced");
    }
    AIWOS_OK
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_handle_event(event: *const aiwos_event_t) -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if event.is_null() {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }

    let ev = &*event;
    LAST_EVENT_TYPE = ev.type_;
    LAST_EVENT_DATA = ev.data;

    log_literal("aiwos_handle_event: event applied");
    AIWOS_OK
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_query_state(
    query_id: u32,
    out_buf: *mut u8,
    out_len: usize,
) -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if out_buf.is_null() {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }

    match query_id {
        AIWOS_QUERY_STATE_SNAPSHOT => {
            if out_len < core::mem::size_of::<aiwos_state_snapshot_t>() {
                return AIWOS_ERR_BUFFER_TOO_SMALL;
            }
            let (mut ready, mut blocked) = (0u32, 0u32);
            for i in 0..TASK_COUNT as usize {
                match TASKS[i].state {
                    AIWOS_TASK_READY => ready += 1,
                    AIWOS_TASK_BLOCKED | AIWOS_TASK_DONE => blocked += 1,
                    _ => {}
                }
            }
            let snap = aiwos_state_snapshot_t {
                tick_count: TICK_COUNT,
                last_now_ns: LAST_NOW_NS,
                last_event_type: LAST_EVENT_TYPE,
                scheduler_state: SCHED_STATE,
                last_event_data: LAST_EVENT_DATA,
                task_count: TASK_COUNT,
                ready_count: ready,
                blocked_count: blocked,
                _reserved: [0; 3],
            };
            ptr::write(out_buf as *mut aiwos_state_snapshot_t, snap);
            AIWOS_OK
        }
        AIWOS_QUERY_SCHEDULER_STATS => {
            if out_len < core::mem::size_of::<aiwos_scheduler_stats_t>() {
                return AIWOS_ERR_BUFFER_TOO_SMALL;
            }
            let (mut ready, mut blocked) = (0u32, 0u32);
            for i in 0..TASK_COUNT as usize {
                match TASKS[i].state {
                    AIWOS_TASK_READY => ready += 1,
                    AIWOS_TASK_BLOCKED | AIWOS_TASK_DONE => blocked += 1,
                    _ => {}
                }
            }
            let stats = aiwos_scheduler_stats_t {
                total_ticks: TOTAL_TICKS,
                last_tick_ns: LAST_TICK_NS,
                min_vruntime: MIN_VRUNTIME,
                task_count: TASK_COUNT,
                ready_count: ready,
                blocked_count: blocked,
                _reserved: [0; 5],
            };
            ptr::write(out_buf as *mut aiwos_scheduler_stats_t, stats);
            AIWOS_OK
        }
        _ => AIWOS_ERR_NOT_SUPPORTED,
    }
}

#[no_mangle]
pub extern "C" fn aiwos_shutdown() {
    unsafe {
        if !INITIALIZED {
            return;
        }
        SCHED_STATE = AIWOS_SCHED_SHUTDOWN;
        CURRENT_IDX = INVALID_IDX;
        INITIALIZED = false;
        log_literal("aiwos_shutdown: shutdown complete");
    }
}

// ============================================================================
// 任务管理 ABI 函数
// ============================================================================

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_create(context: *mut u8) -> u32 {
    if !INITIALIZED || TASK_COUNT >= AIWOS_MAX_TASKS as u32 {
        return 0;
    }
    let id = TASK_COUNT.wrapping_add(1);
    let idx = TASK_COUNT as usize;
    TASKS[idx] = Task {
        id,
        state: AIWOS_TASK_READY,
        vruntime: MIN_VRUNTIME,
        runtime_accum: 0,
        block_reason: AIWOS_BLOCK_NONE,
        block_timeout: 0,
        wait_event: 0,
        nice: 0,
        weight: 1024,
        context,
    };
    TASK_COUNT += 1;
    id
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_destroy(task_id: u32) -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    let found = aiwos_task_find_index(task_id);
    if found < 0 {
        return AIWOS_ERR_NOT_FOUND;
    }
    let idx = found as usize;
    let last = (TASK_COUNT - 1) as usize;

    // 在压缩前更新 CURRENT_IDX
    if CURRENT_IDX == idx as u32 {
        CURRENT_IDX = INVALID_IDX;
    } else if CURRENT_IDX == last as u32 {
        CURRENT_IDX = idx as u32; // 被交换到 idx
    }

    if idx != last {
        TASKS[idx] = TASKS[last];
    }
    TASKS[last] = Task {
        id: 0, state: 0, vruntime: 0, runtime_accum: 0,
        block_reason: 0, block_timeout: 0, wait_event: 0,
        nice: 0, weight: 1024, context: ptr::null_mut(),
    };
    TASK_COUNT -= 1;
    AIWOS_OK
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_find_index(task_id: u32) -> i32 {
    if !INITIALIZED || task_id == 0 {
        return -1;
    }
    for i in 0..TASK_COUNT as usize {
        if TASKS[i].id == task_id {
            return i as i32;
        }
    }
    -1
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_get(task_id: u32) -> *mut u8 {
    if !INITIALIZED {
        return ptr::null_mut();
    }
    let idx = aiwos_task_find_index(task_id);
    if idx < 0 {
        return ptr::null_mut();
    }
    TASKS[idx as usize].context
}

// ============================================================================
// 任务状态控制 ABI 函数
// ============================================================================

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_yield() -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if CURRENT_IDX >= AIWOS_MAX_TASKS as u32 {
        return AIWOS_ERR_NOT_FOUND;
    }
    let curr = &mut TASKS[CURRENT_IDX as usize];
    if curr.state != AIWOS_TASK_RUNNING {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }
    curr.state = AIWOS_TASK_READY;
    CURRENT_IDX = INVALID_IDX;
    AIWOS_OK
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_block(
    reason: u32,
    wait_event: u32,
    timeout_ns: u64,
) -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if CURRENT_IDX >= AIWOS_MAX_TASKS as u32 {
        return AIWOS_ERR_NOT_FOUND;
    }
    let curr = &mut TASKS[CURRENT_IDX as usize];
    if curr.state != AIWOS_TASK_RUNNING {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }
    curr.state = AIWOS_TASK_BLOCKED;
    curr.block_reason = reason;
    curr.wait_event = wait_event;
    curr.block_timeout = timeout_ns;
    CURRENT_IDX = INVALID_IDX;
    AIWOS_OK
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_wakeup(task_id: u32) -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    let idx = aiwos_task_find_index(task_id);
    if idx < 0 {
        return AIWOS_ERR_NOT_FOUND;
    }
    TASKS[idx as usize].state = AIWOS_TASK_READY;
    AIWOS_OK
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_wakeup_by_event(event_type: u32) -> i32 {
    if !INITIALIZED {
        return -1;
    }
    let mut count = 0i32;
    for i in 0..TASK_COUNT as usize {
        if TASKS[i].wait_event == event_type && TASKS[i].state == AIWOS_TASK_BLOCKED {
            TASKS[i].state = AIWOS_TASK_READY;
            count += 1;
        }
    }
    count
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_task_complete() -> aiwos_status_t {
    if !INITIALIZED {
        return AIWOS_ERR_NOT_INITIALIZED;
    }
    if CURRENT_IDX >= AIWOS_MAX_TASKS as u32 {
        return AIWOS_ERR_NOT_FOUND;
    }
    let curr = &mut TASKS[CURRENT_IDX as usize];
    if curr.state != AIWOS_TASK_RUNNING {
        return AIWOS_ERR_INVALID_ARGUMENT;
    }
    curr.state = AIWOS_TASK_DONE;
    CURRENT_IDX = INVALID_IDX;
    AIWOS_OK
}

// ============================================================================
// 调度与查询 ABI 函数
// ============================================================================

#[no_mangle]
pub unsafe extern "C" fn aiwos_schedule_next() -> u32 {
    if !INITIALIZED {
        return 0;
    }
    // 如果当前任务还在 RUNNING，先设为 READY
    if CURRENT_IDX < AIWOS_MAX_TASKS as u32 {
        let curr = &mut TASKS[CURRENT_IDX as usize];
        if curr.state == AIWOS_TASK_RUNNING {
            curr.state = AIWOS_TASK_READY;
        }
    }
    // 挑选 vruntime 最小的 READY 任务
    let mut best_idx = INVALID_IDX;
    let mut best_vruntime = u64::MAX;
    for i in 0..TASK_COUNT as usize {
        if TASKS[i].state == AIWOS_TASK_READY && TASKS[i].vruntime < best_vruntime {
            best_vruntime = TASKS[i].vruntime;
            best_idx = i as u32;
        }
    }
    if best_idx == INVALID_IDX {
        CURRENT_IDX = INVALID_IDX;
        return 0;
    }
    CURRENT_IDX = best_idx;
    TASKS[best_idx as usize].state = AIWOS_TASK_RUNNING;
    TASKS[best_idx as usize].id
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_current() -> u32 {
    if !INITIALIZED {
        return 0;
    }
    if CURRENT_IDX >= AIWOS_MAX_TASKS as u32 {
        return 0;
    }
    TASKS[CURRENT_IDX as usize].id
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_current_index() -> u32 {
    if !INITIALIZED {
        return INVALID_IDX;
    }
    CURRENT_IDX
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_has_ready() -> i32 {
    if !INITIALIZED {
        return 0;
    }
    for i in 0..TASK_COUNT as usize {
        if TASKS[i].state == AIWOS_TASK_READY {
            return 1;
        }
    }
    0
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_nice_to_weight(nice: i32) -> u32 {
    nice_to_weight(nice)
}

// ============================================================================
// 碰撞分配器（仅 wasm — JS 在线性内存中构造结构体）
// ============================================================================

#[cfg(target_arch = "wasm32")]
const HEAP_SIZE: usize = 4096;
#[cfg(target_arch = "wasm32")]
static mut HEAP: [u8; 4096] = [0; 4096];
#[cfg(target_arch = "wasm32")]
static mut HEAP_POS: usize = 0;

#[no_mangle]
pub unsafe extern "C" fn aiwos_alloc(size: u32) -> *mut u8 {
    #[cfg(target_arch = "wasm32")]
    {
        let pos = HEAP_POS;
        let end = pos + size as usize;
        if end > HEAP_SIZE {
            return ptr::null_mut();
        }
        ptr::write_bytes(HEAP.as_mut_ptr().add(pos), 0, size as usize);
        HEAP_POS = end;
        HEAP.as_mut_ptr().add(pos)
    }
    #[cfg(not(target_arch = "wasm32"))]
    match HOST_ALLOC {
        Some(f) => f(size as usize),
        None => ptr::null_mut(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn aiwos_alloc_reset() {
    #[cfg(target_arch = "wasm32")]
    {
        HEAP_POS = 0;
    }
}

// ============================================================================
// Panic 处理函数
// ============================================================================

// 测试时由 std 提供 panic_handler，避免冲突
#[cfg(not(test))]
#[panic_handler]
fn core_panic(_info: &core::panic::PanicInfo) -> ! {
    loop {}
}

// ============================================================================
// Tests
// ============================================================================
//
// 全局共享的 static mut 状态使测试无法安全并行 —— 使用 Mutex 序列化。
// 每个会修改全局状态的测试函数都从 `lock()` 或 `lock_and_init()` 获取守卫，
// 守卫存活期间其他测试被阻塞。
//
// 使用方式（在测试函数第一行）：
//   let _guard = lock();         // 重置 + init
//   let _guard = lock_raw();     // 仅重置，不 init


#[cfg(test)]
mod tests;
