/* ============ ABI 常量 ============ */

export const AIWOS_QUERY_STATE_SNAPSHOT = 0;
export const AIWOS_QUERY_SCHEDULER_STATS = 1;
export const AIWOS_EVENT_TIMER = 2;
export const AIWOS_EVENT_INTERRUPT = 1;

/* ============ Wasm 导出接口 ============ */

export interface WasmExports {
  memory: WebAssembly.Memory;
  aiwos_abi_version: () => number;
  aiwos_init: (hostPtr: number) => number;
  aiwos_tick: (nowNs: bigint) => number;
  aiwos_handle_event: (eventPtr: number) => number;
  aiwos_query_state: (queryId: number, buf: number, len: number) => number;
  aiwos_shutdown: () => void;
  aiwos_alloc: (size: number) => number;
  aiwos_alloc_reset: () => void;
  /* 任务管理 */
  aiwos_task_create: (context: number) => number;
  aiwos_task_destroy: (taskId: number) => number;
  aiwos_task_yield: () => number;
  aiwos_task_block: (reason: number, waitEvent: number, timeoutNs: bigint) => number;
  aiwos_task_wakeup: (taskId: number) => number;
  aiwos_task_wakeup_by_event: (eventType: number) => number;
  aiwos_task_complete: () => number;
  aiwos_schedule_next: () => number;
  aiwos_current: () => number;
  aiwos_has_ready: () => number;
}

/* ============ 任务状态常量 ============ */

export const TASK_STATE_READY = 0;
export const TASK_STATE_RUNNING = 1;
export const TASK_STATE_BLOCKED = 2;
export const TASK_STATE_DONE = 4;

export function taskStateName(s: number): string {
  return (
    s === TASK_STATE_READY ? "READY" :
    s === TASK_STATE_RUNNING ? "RUNNING" :
    s === TASK_STATE_BLOCKED ? "BLOCKED" :
    s === TASK_STATE_DONE ? "DONE" : "?"
  );
}

/* ============ Wasm 查询结构体 ============ */

export interface CoreSnapshot {
  tick_count: bigint;
  last_now_ns: bigint;
  last_event_type: number;
  scheduler_state: number;
  last_event_data: bigint;
  task_count: number;
  ready_count: number;
  blocked_count: number;
}

export interface SchedulerStats {
  total_ticks: bigint;
  last_tick_ns: bigint;
  min_vruntime: bigint;
  task_count: number;
  ready_count: number;
  blocked_count: number;
}

/* ============ 模拟硬件状态 ============ */

export interface HardwareState {
  memory: Uint8Array;
  cpu: { pc: number; tick: number };
  irqPending: boolean;
}

export function createHardwareState(): HardwareState {
  return {
    memory: new Uint8Array(64 * 1024),
    cpu: { pc: 0, tick: 0 },
    irqPending: false,
  };
}

export function schedulerStateName(s: number): string {
  return s === 0 ? "IDLE" : s === 1 ? "RUNNING" : s === 2 ? "SHUTDOWN" : "?";
}
