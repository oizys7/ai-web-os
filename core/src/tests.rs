//! ai-web-os Core 测试
//!
//! 作为 `mod tests` 与 lib.rs 同属一个编译单元，`cfg(test)` 生效，
//! 因此可访问 crate 的私有类型（Task、TASKS、CURRENT_IDX 等）。
//!
//! 全局 `static mut` 状态使测试无法安全并行 —— 使用 Mutex 序列化。

use std::mem::MaybeUninit;
use std::sync::Mutex;

use super::*;

static GLOBAL: Mutex<()> = Mutex::new(());

// ---------------------------------------------------------------------------
// 辅助函数
// ---------------------------------------------------------------------------

extern "C" fn dummy_log(_ptr: *const u8, _len: usize) {}
extern "C" fn dummy_now_ns() -> u64 { 0 }
extern "C" fn dummy_alloc(_size: usize) -> *mut u8 { core::ptr::null_mut() }
extern "C" fn dummy_free(_ptr: *mut u8) {}
extern "C" fn dummy_realloc(_ptr: *mut u8, _size: usize) -> *mut u8 { core::ptr::null_mut() }

fn dummy_host() -> aiwos_host_api_t {
    aiwos_host_api_t {
        host_abi_version: AIWOS_ABI_VERSION,
        log: Some(dummy_log),
        now_ns: Some(dummy_now_ns),
        alloc: Some(dummy_alloc),
        free: Some(dummy_free),
        realloc: Some(dummy_realloc),
    }
}

/// 重置所有全局状态（含 INITIALIZED = false）。
unsafe fn reset_all() {
    INITIALIZED = false;
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
    for t in TASKS.iter_mut() {
        *t = Task {
            id: 0, state: 0, vruntime: 0, runtime_accum: 0,
            block_reason: 0, block_timeout: 0, wait_event: 0,
            nice: 0, weight: 1024, context: core::ptr::null_mut(),
        };
    }
}

/// 获取锁 + 重置（不 init）。
fn lock_raw() -> std::sync::MutexGuard<'static, ()> {
    let guard = GLOBAL.lock().unwrap_or_else(|e| e.into_inner());
    unsafe { reset_all() };
    guard
}

/// 获取锁 + 重置 + aiwos_init。
fn lock_and_init() -> std::sync::MutexGuard<'static, ()> {
    let guard = lock_raw();
    let host = dummy_host();
    let ret = unsafe { aiwos_init(&host as *const _) };
    assert_eq!(ret, AIWOS_OK, "lock_and_init 失败");
    guard
}

fn query_snapshot() -> aiwos_state_snapshot_t {
    let mut snap = MaybeUninit::<aiwos_state_snapshot_t>::uninit();
    assert_eq!(unsafe {
        aiwos_query_state(
            AIWOS_QUERY_STATE_SNAPSHOT,
            snap.as_mut_ptr() as *mut u8,
            size_of::<aiwos_state_snapshot_t>(),
        )
    }, AIWOS_OK);
    unsafe { snap.assume_init() }
}

fn query_stats() -> aiwos_scheduler_stats_t {
    let mut s = MaybeUninit::<aiwos_scheduler_stats_t>::uninit();
    assert_eq!(unsafe {
        aiwos_query_state(
            AIWOS_QUERY_SCHEDULER_STATS,
            s.as_mut_ptr() as *mut u8,
            size_of::<aiwos_scheduler_stats_t>(),
        )
    }, AIWOS_OK);
    unsafe { s.assume_init() }
}

fn base_task() -> Task {
    Task {
        id: 0, state: 0, vruntime: 0, runtime_accum: 0,
        block_reason: 0, block_timeout: 0, wait_event: 0,
        nice: 0, weight: 1024, context: core::ptr::null_mut(),
    }
}

// ===========================================================================
// 内部状态测试 —— 需要直接访问 TASKS[]、CURRENT_IDX 等 static mut
// ===========================================================================

#[test]
fn vruntime_accumulates_for_running_task() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0;
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, ..base_task() };
    }
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[0].vruntime, 1000);         // 1000*1024/1024
        assert_eq!(TASKS[0].runtime_accum, 1000);
        assert_eq!(MIN_VRUNTIME, 0);                 // vruntime 只增不减
    }
}

#[test]
fn weight_scales_vruntime() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0;
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, weight: 2048, ..base_task() };
    }
    assert_eq!(aiwos_tick(2000), AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[0].vruntime, 1000);         // 2000*1024/2048
        assert_eq!(TASKS[0].runtime_accum, 2000);    // 原始时间不加权
    }
}

#[test]
fn zero_weight_falls_back_to_1024() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0;
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, weight: 0, ..base_task() };
    }
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    unsafe { assert_eq!(TASKS[0].vruntime, 1000); }
}

#[test]
fn vruntime_wrapping() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0;
        TASK_COUNT = 1;
        TASKS[0] = Task {
            id: 1, state: AIWOS_TASK_RUNNING,
            vruntime: u64::MAX, weight: 1024, ..base_task()
        };
    }
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    unsafe { assert_eq!(TASKS[0].vruntime, u64::MAX.wrapping_add(1000)); }
}

#[test]
fn tick_counter_wrapping() {
    let _g = lock_and_init();
    unsafe {
        TICK_COUNT = u64::MAX;
        TOTAL_TICKS = u64::MAX;
    }
    assert_eq!(aiwos_tick(100), AIWOS_OK);
    let snap = query_snapshot();
    assert_eq!(snap.tick_count, 0);
    let stats = query_stats();
    assert_eq!(stats.total_ticks, 0);
}

#[test]
fn tick_unblocks_timedout_tasks() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, block_timeout: 200, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_BLOCKED, block_timeout: 500, ..base_task() };
    }
    assert_eq!(aiwos_tick(300), AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[0].state, AIWOS_TASK_READY);
        assert_eq!(TASKS[0].block_timeout, 0);
        assert_eq!(TASKS[1].state, AIWOS_TASK_BLOCKED);
    }
    assert_eq!(aiwos_tick(600), AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[1].state, AIWOS_TASK_READY);
        assert_eq!(TASKS[1].block_timeout, 0);
    }
}

#[test]
fn zero_timeout_never_unblocks() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, block_timeout: 0, ..base_task() };
    }
    assert_eq!(aiwos_tick(u64::MAX), AIWOS_OK);
    unsafe { assert_eq!(TASKS[0].state, AIWOS_TASK_BLOCKED); }
}

#[test]
fn snapshot_internal_state() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    assert_eq!(aiwos_tick(2000), AIWOS_OK);
    let ev = aiwos_event_t { type_: AIWOS_EVENT_INTERRUPT, data: 7 };
    unsafe { aiwos_handle_event(&ev as *const _) };
    unsafe {
        TASK_COUNT = 3;
        TASKS[0].state = AIWOS_TASK_READY;
        TASKS[1].state = AIWOS_TASK_BLOCKED;
        TASKS[2].state = AIWOS_TASK_DONE;
    }
    let snap = query_snapshot();
    assert_eq!(snap.tick_count, 2);
    assert_eq!(snap.last_now_ns, 2000);
    assert_eq!(snap.last_event_type, AIWOS_EVENT_INTERRUPT);
    assert_eq!(snap.last_event_data, 7);
    assert_eq!(snap.task_count, 3);
    assert_eq!(snap.ready_count, 1);
    assert_eq!(snap.blocked_count, 2);
}

#[test]
fn stats_internal_state() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(500), AIWOS_OK);
    assert_eq!(aiwos_tick(1500), AIWOS_OK);
    unsafe {
        TASK_COUNT = 2;
        TASKS[0].state = AIWOS_TASK_READY;
        TASKS[1].state = AIWOS_TASK_BLOCKED;
        MIN_VRUNTIME = 42;
    }
    let stats = query_stats();
    assert_eq!(stats.total_ticks, 2);
    assert_eq!(stats.last_tick_ns, 1500);
    assert_eq!(stats.min_vruntime, 42);
    assert_eq!(stats.task_count, 2);
    assert_eq!(stats.ready_count, 1);
    assert_eq!(stats.blocked_count, 1);
}

// ===========================================================================
// 公开 API 测试 —— 仅通过 ABI 函数验证行为
// ===========================================================================

#[test]
fn abi_version_is_consistent() {
    assert_eq!(aiwos_abi_version(), AIWOS_ABI_VERSION);
    assert_eq!(aiwos_abi_version(), 3);
}

// ========== Init ==========

#[test]
fn init_rejects_null_host() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_init(core::ptr::null()) }, AIWOS_ERR_INCOMPATIBLE_ABI);
}

#[test]
fn init_rejects_incompatible_abi() {
    let _g = lock_raw();
    let host = aiwos_host_api_t {
        host_abi_version: 9999, log: None, now_ns: None,
        alloc: None, free: None, realloc: None,
    };
    assert_eq!(unsafe { aiwos_init(&host as *const _) }, AIWOS_ERR_INCOMPATIBLE_ABI);
}

#[test]
fn init_successful_with_valid_host() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_init(&dummy_host() as *const _) }, AIWOS_OK);
}

#[test]
fn init_resets_previous_state() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    assert_eq!(unsafe { aiwos_init(&dummy_host() as *const _) }, AIWOS_OK);
    let snap = query_snapshot();
    assert_eq!(snap.tick_count, 0);
    assert_eq!(snap.scheduler_state, 0); // AIWOS_SCHED_IDLE
    assert_eq!(snap.task_count, 0);
}

// ========== Shutdown ==========

#[test]
fn shutdown_causes_tick_to_fail() {
    let _g = lock_and_init();
    aiwos_shutdown();
    assert_eq!(aiwos_tick(100), AIWOS_ERR_NOT_INITIALIZED);
}

#[test]
fn shutdown_is_idempotent() {
    let _g = lock_and_init();
    aiwos_shutdown();
    aiwos_shutdown();
}

#[test]
fn reinit_after_shutdown_works() {
    let _g = lock_and_init();
    aiwos_shutdown();
    assert_eq!(unsafe { aiwos_init(&dummy_host() as *const _) }, AIWOS_OK);
    assert_eq!(aiwos_tick(100), AIWOS_OK);
    assert_eq!(query_snapshot().tick_count, 1);
}

// ========== Tick ==========

#[test]
fn tick_requires_initialized() {
    let _g = lock_raw();
    assert_eq!(aiwos_tick(100), AIWOS_ERR_NOT_INITIALIZED);
}

#[test]
fn tick_increases_counter() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    assert_eq!(query_snapshot().tick_count, 1);
    assert_eq!(query_stats().total_ticks, 1);
}

#[test]
fn tick_accumulates_multiple() {
    let _g = lock_and_init();
    for i in 1..=10u64 { assert_eq!(aiwos_tick(i * 100), AIWOS_OK); }
    assert_eq!(query_stats().total_ticks, 10);
}

#[test]
fn tick_handles_zero_delta() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(500), AIWOS_OK);
    assert_eq!(aiwos_tick(500), AIWOS_OK);
    assert_eq!(query_snapshot().tick_count, 2);
}

#[test]
fn tick_handles_regressive_time() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    assert_eq!(aiwos_tick(500), AIWOS_OK);
    let snap = query_snapshot();
    assert_eq!(snap.last_now_ns, 500);
    assert_eq!(snap.tick_count, 2);
}

#[test]
fn tick_updates_last_now_ns() {
    let _g = lock_and_init();
    assert_eq!(aiwos_tick(42), AIWOS_OK);
    assert_eq!(query_snapshot().last_now_ns, 42);
}

// ========== Handle Event ==========

#[test]
fn handle_event_requires_initialized() {
    let _g = lock_raw();
    let ev = aiwos_event_t { type_: AIWOS_EVENT_INTERRUPT, data: 42 };
    assert_eq!(unsafe { aiwos_handle_event(&ev as *const _) }, AIWOS_ERR_NOT_INITIALIZED);
}

#[test]
fn handle_event_rejects_null() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_handle_event(core::ptr::null()) }, AIWOS_ERR_INVALID_ARGUMENT);
}

#[test]
fn handle_event_stores_type_and_data() {
    let _g = lock_and_init();
    let ev = aiwos_event_t { type_: AIWOS_EVENT_TIMER, data: 0xDEAD };
    assert_eq!(unsafe { aiwos_handle_event(&ev as *const _) }, AIWOS_OK);
    let snap = query_snapshot();
    assert_eq!(snap.last_event_type, AIWOS_EVENT_TIMER);
    assert_eq!(snap.last_event_data, 0xDEAD);
}

// ========== Query State ==========

#[test]
fn query_state_requires_initialized() {
    let _g = lock_raw();
    let mut buf = [0u8; 56];
    assert_eq!(
        unsafe { aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, buf.as_mut_ptr(), buf.len()) },
        AIWOS_ERR_NOT_INITIALIZED,
    );
}

#[test]
fn query_state_rejects_null_buffer() {
    let _g = lock_and_init();
    assert_eq!(
        unsafe { aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, core::ptr::null_mut(), 56) },
        AIWOS_ERR_INVALID_ARGUMENT,
    );
}

#[test]
fn query_state_rejects_small_buffer() {
    let _g = lock_and_init();
    let mut buf = [0u8; 55];
    assert_eq!(
        unsafe { aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, buf.as_mut_ptr(), buf.len()) },
        AIWOS_ERR_BUFFER_TOO_SMALL,
    );
    let mut buf2 = [0u8; 55];
    assert_eq!(
        unsafe { aiwos_query_state(AIWOS_QUERY_SCHEDULER_STATS, buf2.as_mut_ptr(), buf2.len()) },
        AIWOS_ERR_BUFFER_TOO_SMALL,
    );
}

#[test]
fn query_state_unknown_id() {
    let _g = lock_and_init();
    let mut buf = [0u8; 56];
    assert_eq!(
        unsafe { aiwos_query_state(9999, buf.as_mut_ptr(), buf.len()) },
        AIWOS_ERR_NOT_SUPPORTED,
    );
}

// ========== 综合场景 ==========

#[test]
fn full_lifecycle() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_init(&dummy_host() as *const _) }, AIWOS_OK);
    for ns in [100, 200, 300, 400, 500] { assert_eq!(aiwos_tick(ns), AIWOS_OK); }
    let ev = aiwos_event_t { type_: AIWOS_EVENT_INTERRUPT, data: 99 };
    assert_eq!(unsafe { aiwos_handle_event(&ev as *const _) }, AIWOS_OK);
    let snap = query_snapshot();
    assert_eq!(snap.tick_count, 5);
    assert_eq!(snap.last_now_ns, 500);
    assert_eq!(snap.last_event_type, AIWOS_EVENT_INTERRUPT);
    assert_eq!(snap.last_event_data, 99);
    let stats = query_stats();
    assert_eq!(stats.total_ticks, 5);
    assert_eq!(stats.last_tick_ns, 500);
    aiwos_shutdown();
    assert_eq!(aiwos_tick(600), AIWOS_ERR_NOT_INITIALIZED);
}

#[test]
fn high_frequency_ticks() {
    let _g = lock_and_init();
    for i in 0..1000u64 { assert_eq!(aiwos_tick(i), AIWOS_OK); }
    assert_eq!(query_stats().total_ticks, 1000);
}

// ===========================================================================
// 阻塞原因常量
// ===========================================================================

#[test]
fn block_reason_constants() {
    assert_eq!(AIWOS_BLOCK_NONE, 0);
    assert_eq!(AIWOS_BLOCK_EVENT, 1);
    assert_eq!(AIWOS_BLOCK_TIMEOUT, 2);
    assert_eq!(AIWOS_BLOCK_RESOURCE, 3);
    assert_eq!(AIWOS_BLOCK_AI_INFERENCE, 4);
}

// ===========================================================================
// Init 校验
// ===========================================================================

#[test]
fn init_rejects_null_callbacks() {
    let _g = lock_raw();
    let host = aiwos_host_api_t {
        host_abi_version: AIWOS_ABI_VERSION,
        log: None, now_ns: None,
        alloc: None, free: None, realloc: None,
    };
    assert_eq!(unsafe { aiwos_init(&host as *const _) }, AIWOS_ERR_INCOMPATIBLE_ABI);
}

// ===========================================================================
// Nice → Weight 转换
// ===========================================================================

#[test]
fn nice_to_weight_zero() {
    assert_eq!(nice_to_weight(0), 1024);
}

#[test]
fn nice_to_weight_boundaries() {
    assert_eq!(nice_to_weight(-20), 88761);
    assert_eq!(nice_to_weight(19), 15);
}

#[test]
fn nice_to_weight_clamp() {
    assert_eq!(nice_to_weight(-30), 88761);
    assert_eq!(nice_to_weight(30), 15);
}

#[test]
fn aiwos_nice_to_weight_extern() {
    assert_eq!(unsafe { aiwos_nice_to_weight(0) }, 1024);
    assert_eq!(unsafe { aiwos_nice_to_weight(-20) }, 88761);
}

// ===========================================================================
// 任务创建 / 销毁 / 查找
// ===========================================================================

#[test]
fn task_create_basic() {
    let _g = lock_and_init();
    let id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    assert_ne!(id, 0);
    unsafe { assert_eq!(TASK_COUNT, 1); }
}

#[test]
fn task_create_starts_ready() {
    let _g = lock_and_init();
    let id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    unsafe {
        assert_eq!(TASKS[0].id, id);
        assert_eq!(TASKS[0].state, AIWOS_TASK_READY);
    }
}

#[test]
fn task_create_uses_min_vruntime() {
    let _g = lock_and_init();
    unsafe { MIN_VRUNTIME = 5000; }
    let _id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    unsafe { assert_eq!(TASKS[0].vruntime, 5000); }
}

#[test]
fn task_create_unique_ids() {
    let _g = lock_and_init();
    let id1 = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    let id2 = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    let id3 = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    assert_ne!(id1, id2);
    assert_ne!(id2, id3);
    assert_ne!(id1, id3);
    unsafe { assert_eq!(TASK_COUNT, 3); }
}

#[test]
fn task_create_max_tasks() {
    let _g = lock_and_init();
    for _ in 0..AIWOS_MAX_TASKS {
        assert_ne!(unsafe { aiwos_task_create(core::ptr::null_mut()) }, 0);
    }
    assert_eq!(unsafe { aiwos_task_create(core::ptr::null_mut()) }, 0);
}

#[test]
fn task_create_uninitialized() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_task_create(core::ptr::null_mut()) }, 0);
}

#[test]
fn task_create_sets_context() {
    let _g = lock_and_init();
    let ctx: *mut u8 = 0xDEAD as *mut u8;
    let id = unsafe { aiwos_task_create(ctx) };
    assert_eq!(unsafe { aiwos_task_get(id) }, ctx);
}

#[test]
fn task_destroy_basic() {
    let _g = lock_and_init();
    let id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    assert_eq!(unsafe { aiwos_task_destroy(id) }, AIWOS_OK);
    unsafe { assert_eq!(TASK_COUNT, 0); }
}

#[test]
fn task_destroy_nonexistent() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_destroy(999) }, AIWOS_ERR_NOT_FOUND);
}

#[test]
fn task_destroy_removes_from_find() {
    let _g = lock_and_init();
    let id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    assert_eq!(unsafe { aiwos_task_destroy(id) }, AIWOS_OK);
    assert_eq!(unsafe { aiwos_task_find_index(id) }, -1);
}

#[test]
fn task_destroy_current() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, ..base_task() };
        CURRENT_IDX = 0;
    }
    assert_eq!(unsafe { aiwos_task_destroy(1) }, AIWOS_OK);
    unsafe { assert_eq!(CURRENT_IDX, INVALID_IDX); }
}

#[test]
fn task_destroy_uninitialized() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_task_destroy(1) }, AIWOS_ERR_NOT_INITIALIZED);
}

#[test]
fn task_find_index_found() {
    let _g = lock_and_init();
    let id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    assert_eq!(unsafe { aiwos_task_find_index(id) }, 0);
}

#[test]
fn task_find_index_not_found() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_find_index(999) }, -1);
}

#[test]
fn task_find_index_zero_id() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_find_index(0) }, -1);
}

// ===========================================================================
// 任务状态控制
// ===========================================================================

#[test]
fn task_yield_basic() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[0].state, AIWOS_TASK_READY);
        assert_eq!(CURRENT_IDX, INVALID_IDX);
    }
}

#[test]
fn task_yield_no_current() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_ERR_NOT_FOUND);
}

#[test]
fn task_yield_not_running() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_ERR_INVALID_ARGUMENT);
}

#[test]
fn task_yield_uninitialized() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_ERR_NOT_INITIALIZED);
}

#[test]
fn task_block_basic() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_block(AIWOS_BLOCK_EVENT, 5, 10000) }, AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[0].state, AIWOS_TASK_BLOCKED);
        assert_eq!(TASKS[0].block_reason, AIWOS_BLOCK_EVENT);
        assert_eq!(TASKS[0].wait_event, 5);
        assert_eq!(TASKS[0].block_timeout, 10000);
        assert_eq!(CURRENT_IDX, INVALID_IDX);
    }
}

#[test]
fn task_block_ai_inference() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_block(AIWOS_BLOCK_AI_INFERENCE, 0, 5000) }, AIWOS_OK);
    unsafe { assert_eq!(TASKS[0].block_reason, AIWOS_BLOCK_AI_INFERENCE); }
}

#[test]
fn task_block_no_current() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_block(AIWOS_BLOCK_NONE, 0, 0) }, AIWOS_ERR_NOT_FOUND);
}

#[test]
fn task_block_not_running() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_block(AIWOS_BLOCK_NONE, 0, 0) }, AIWOS_ERR_INVALID_ARGUMENT);
}

#[test]
fn task_wakeup_basic() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_wakeup(1) }, AIWOS_OK);
    unsafe { assert_eq!(TASKS[0].state, AIWOS_TASK_READY); }
}

#[test]
fn task_wakeup_nonexistent() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_wakeup(999) }, AIWOS_ERR_NOT_FOUND);
}

#[test]
fn task_wakeup_already_ready() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_wakeup(1) }, AIWOS_OK);
    unsafe { assert_eq!(TASKS[0].state, AIWOS_TASK_READY); }
}

#[test]
fn task_wakeup_by_event_basic() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 3;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, wait_event: 5, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_BLOCKED, wait_event: 5, ..base_task() };
        TASKS[2] = Task { id: 3, state: AIWOS_TASK_BLOCKED, wait_event: 7, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_wakeup_by_event(5) }, 2);
    unsafe {
        assert_eq!(TASKS[0].state, AIWOS_TASK_READY);
        assert_eq!(TASKS[1].state, AIWOS_TASK_READY);
        assert_eq!(TASKS[2].state, AIWOS_TASK_BLOCKED);
    }
}

#[test]
fn task_wakeup_by_event_no_match() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, wait_event: 5, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_wakeup_by_event(99) }, 0);
}

#[test]
fn task_wakeup_by_event_skips_ready() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, wait_event: 5, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_BLOCKED, wait_event: 5, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_wakeup_by_event(5) }, 1);
}

#[test]
fn task_complete_basic() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_complete() }, AIWOS_OK);
    unsafe {
        assert_eq!(TASKS[0].state, AIWOS_TASK_DONE);
        assert_eq!(CURRENT_IDX, INVALID_IDX);
    }
}

#[test]
fn task_complete_no_current() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_task_complete() }, AIWOS_ERR_NOT_FOUND);
}

#[test]
fn task_complete_not_running() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_task_complete() }, AIWOS_ERR_INVALID_ARGUMENT);
}

// ===========================================================================
// 调度
// ===========================================================================

#[test]
fn schedule_next_empty() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_schedule_next() }, 0);
}

#[test]
fn schedule_next_uninitialized() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_schedule_next() }, 0);
}

#[test]
fn schedule_next_picks_min_vruntime() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 3;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, vruntime: 100, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_READY, vruntime: 50, ..base_task() };
        TASKS[2] = Task { id: 3, state: AIWOS_TASK_READY, vruntime: 200, ..base_task() };
    }
    let picked = unsafe { aiwos_schedule_next() };
    assert_eq!(picked, 2);
    unsafe {
        assert_eq!(CURRENT_IDX, 1);
        assert_eq!(TASKS[1].state, AIWOS_TASK_RUNNING);
    }
}

#[test]
fn schedule_next_sets_current() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 42, state: AIWOS_TASK_READY, ..base_task() };
    }
    unsafe { aiwos_schedule_next(); }
    assert_eq!(unsafe { aiwos_current() }, 42);
}

#[test]
fn schedule_next_skips_blocked() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, vruntime: 10, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_READY, vruntime: 100, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_schedule_next() }, 2);
}

#[test]
fn schedule_next_skips_done() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_DONE, vruntime: 10, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_READY, vruntime: 100, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_schedule_next() }, 2);
}

#[test]
fn schedule_next_preempts_running() {
    let _g = lock_and_init();
    unsafe {
        CURRENT_IDX = 0; TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_RUNNING, vruntime: 200, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_READY, vruntime: 50, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_schedule_next() }, 2);
    unsafe { assert_eq!(TASKS[0].state, AIWOS_TASK_READY); }
}

#[test]
fn schedule_next_all_blocked() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_BLOCKED, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_schedule_next() }, 0);
    unsafe { assert_eq!(CURRENT_IDX, INVALID_IDX); }
}

#[test]
fn schedule_next_all_done() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 2;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_DONE, ..base_task() };
        TASKS[1] = Task { id: 2, state: AIWOS_TASK_DONE, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_schedule_next() }, 0);
}

// ===========================================================================
// 当前任务 / 是否有就绪任务
// ===========================================================================

#[test]
fn current_no_task() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_current() }, 0);
}

#[test]
fn current_uninitialized() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_current() }, 0);
}

#[test]
fn current_index_after_schedule() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 7, state: AIWOS_TASK_READY, ..base_task() };
    }
    unsafe { aiwos_schedule_next(); }
    assert_eq!(unsafe { aiwos_current_index() }, 0);
    assert_eq!(unsafe { aiwos_current() }, 7);
}

#[test]
fn current_index_uninitialized() {
    let _g = lock_raw();
    assert_eq!(unsafe { aiwos_current_index() }, INVALID_IDX);
}

#[test]
fn has_ready_true() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, ..base_task() };
    }
    assert_ne!(unsafe { aiwos_has_ready() }, 0);
}

#[test]
fn has_ready_false() {
    let _g = lock_and_init();
    assert_eq!(unsafe { aiwos_has_ready() }, 0);
}

#[test]
fn has_ready_after_schedule_all() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, ..base_task() };
    }
    unsafe { aiwos_schedule_next(); }
    assert_eq!(unsafe { aiwos_has_ready() }, 0);  // 唯一任务现在是 RUNNING
}

#[test]
fn has_ready_skips_blocked() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_BLOCKED, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_has_ready() }, 0);
}

#[test]
fn has_ready_skips_done() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_DONE, ..base_task() };
    }
    assert_eq!(unsafe { aiwos_has_ready() }, 0);
}

// ===========================================================================
// 完整集成场景
// ===========================================================================

#[test]
fn create_schedule_yield_cycle() {
    let _g = lock_and_init();
    let id1 = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    let id2 = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    assert_ne!(id1, 0);
    assert_ne!(id2, 0);

    // 调度选中 id1（都排在数组前面，两者 vruntime=0）
    assert_eq!(unsafe { aiwos_schedule_next() }, id1);
    assert_eq!(unsafe { aiwos_current() }, id1);
    assert_ne!(unsafe { aiwos_has_ready() }, 0);  // id2 还是 READY

    // 让出 —— 两者现在都是 READY
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_OK);
    assert_ne!(unsafe { aiwos_has_ready() }, 0);

    // vruntime 相同，调度选中排在最前面的（id1 在 index 0）
    assert_eq!(unsafe { aiwos_schedule_next() }, id1);
    assert_eq!(unsafe { aiwos_current() }, id1);
}

#[test]
fn tick_updates_current_task_vruntime() {
    let _g = lock_and_init();
    unsafe {
        TASK_COUNT = 1;
        TASKS[0] = Task { id: 1, state: AIWOS_TASK_READY, ..base_task() };
    }
    unsafe { aiwos_schedule_next(); }
    assert_eq!(aiwos_tick(1000), AIWOS_OK);
    unsafe { assert!(TASKS[0].vruntime > 0); }
}

#[test]
fn block_wakeup_schedule_cycle() {
    let _g = lock_and_init();
    let id1 = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    let id2 = unsafe { aiwos_task_create(core::ptr::null_mut()) };

    // 运行 id1
    assert_eq!(unsafe { aiwos_schedule_next() }, id1);

    // 阻塞 id1，等待事件 5
    assert_eq!(unsafe { aiwos_task_block(AIWOS_BLOCK_EVENT, 5, 0) }, AIWOS_OK);
    assert_eq!(unsafe { aiwos_current() }, 0);

    // 调度选中 id2
    assert_eq!(unsafe { aiwos_schedule_next() }, id2);

    // 事件 5 唤醒 id1
    assert_eq!(unsafe { aiwos_task_wakeup_by_event(5) }, 1);

    // id1 就绪，id2 还在运行 —— 让出后调度选中 id1
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_OK);
    assert_eq!(unsafe { aiwos_schedule_next() }, id1);  // vruntime 相同，id1 在 idx 0
}

#[test]
fn complete_removes_from_ready() {
    let _g = lock_and_init();
    let _id = unsafe { aiwos_task_create(core::ptr::null_mut()) };
    unsafe { aiwos_schedule_next(); }
    assert_eq!(unsafe { aiwos_task_complete() }, AIWOS_OK);
    assert_eq!(unsafe { aiwos_has_ready() }, 0);
    assert_eq!(unsafe { aiwos_current() }, 0);
}

#[test]
fn full_lifecycle_with_all_states() {
    let _g = lock_and_init();

    // 创建 3 个任务
    let ids: Vec<u32> = (0..3).map(|_| unsafe { aiwos_task_create(core::ptr::null_mut()) }).collect();
    assert!(ids.iter().all(|&id| id != 0));
    unsafe { assert_eq!(TASK_COUNT, 3); }

    // 手动设置 vruntime，使调度顺序确定
    unsafe {
        TASKS[0].vruntime = 300;
        TASKS[1].vruntime = 100; // 应被优先选中
        TASKS[2].vruntime = 200;
    }

    // 调度选中 id2（idx 1, vruntime=100）
    assert_eq!(unsafe { aiwos_schedule_next() }, ids[1]);
    assert_eq!(unsafe { aiwos_current() }, ids[1]);

    // 让任务 2 运行一会
    assert_eq!(aiwos_tick(500), AIWOS_OK);
    unsafe { let v = TASKS[1].vruntime; assert!(v > 100); }

    // 完成任务 2
    assert_eq!(unsafe { aiwos_task_complete() }, AIWOS_OK);
    unsafe { assert_eq!(TASKS[1].state, AIWOS_TASK_DONE); }

    // 调度选中 id3（idx 2, vruntime=200）
    assert_eq!(unsafe { aiwos_schedule_next() }, ids[2]);
    unsafe { assert_eq!(TASKS[2].state, AIWOS_TASK_RUNNING); }

    // 阻塞任务 3 等待 AI 推理
    assert_eq!(unsafe { aiwos_task_block(AIWOS_BLOCK_AI_INFERENCE, 0, 99999) }, AIWOS_OK);
    unsafe { assert_eq!(TASKS[2].state, AIWOS_TASK_BLOCKED); }

    // 调度选中 id1（最后一个就绪的，vruntime=300）
    assert_eq!(unsafe { aiwos_schedule_next() }, ids[0]);

    // 让出任务 1
    assert_eq!(unsafe { aiwos_task_yield() }, AIWOS_OK);
    assert_eq!(unsafe { aiwos_has_ready() }, 1);

    // 调度再次选中 id1
    assert_eq!(unsafe { aiwos_schedule_next() }, ids[0]);

    // 销毁任务 1
    assert_eq!(unsafe { aiwos_task_destroy(ids[0]) }, AIWOS_OK);
    assert_eq!(unsafe { aiwos_has_ready() }, 0);
    assert_eq!(unsafe { aiwos_schedule_next() }, 0);
}
