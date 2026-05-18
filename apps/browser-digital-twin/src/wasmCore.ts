import {
  WasmExports,
  CoreSnapshot,
  SchedulerStats,
  AIWOS_QUERY_STATE_SNAPSHOT,
  AIWOS_QUERY_SCHEDULER_STATS,
} from "./types";

/**
 * Wasm 核心 ABI 封装。
 *
 * 职责：
 *  - 加载 `aiwos_core.wasm`
 *  - 提供类型安全的 init / tick / event / query / shutdown 操作
 *  - 任务管理：创建、销毁、调度、状态控制
 *  - 管理 bump allocator 生命周期（alloc → write → read → reset）
 *
 * 用法：
 *   const core = new WasmCore();
 *   await core.load();
 *   core.init();
 *   core.tick(nowNs);
 *   const snap = core.readSnapshot();
 */
export class WasmCore {
  private exports: WasmExports | null = null;
  initialized = false;

  // ---------- 宿主回调（注入 Wasm "env" 模块） ----------

  private host_log = (ptr: number, len: number): void => {
    if (!this.exports) return;
    const bytes = new Uint8Array(this.exports.memory.buffer, ptr, len);
    const msg = new TextDecoder().decode(bytes);
    this.onLog?.(msg);
  };

  private host_now_ns = (): bigint => {
    return BigInt(Date.now()) * 1_000_000n;
  };

  /** 外部 log 回调，由 App 绑定用于更新 UI */
  onLog: ((msg: string) => void) | null = null;

  // ---------- 加载 ----------

  async load(): Promise<void> {
    const resp = await fetch("/core/aiwos_core.wasm");
    if (!resp.ok) throw new Error(`fetch failed (${resp.status})`);

    const result = await WebAssembly.instantiate(await resp.arrayBuffer(), {
      env: {
        host_log: this.host_log,
        host_now_ns: this.host_now_ns,
      },
    });
    this.exports = result.instance.exports as unknown as WasmExports;
  }

  // ---------- 初始化 / 关闭 ----------

  init(): void {
    const exp = this.assertExports();

    // wasm32 上 aiwos_host_api_t = 24 字节
    //   偏移 0: host_abi_version (u32) — 只需要写这个，回调用 import
    exp.aiwos_alloc_reset();
    const ptr = exp.aiwos_alloc(24);
    if (!ptr) throw new Error("aiwos_alloc failed");

    const dv = new DataView(exp.memory.buffer);
    dv.setUint32(ptr, 3, true); // AIWOS_ABI_VERSION = 3

    const status = exp.aiwos_init(ptr);
    exp.aiwos_alloc_reset();
    if (status !== 0) throw new Error(`aiwos_init returned ${status}`);

    this.initialized = true;
  }

  shutdown(): void {
    if (!this.initialized || !this.exports) return;
    this.exports.aiwos_shutdown();
    this.initialized = false;
  }

  // ---------- 核心操作 ----------

  tick(nowNs: bigint): boolean {
    if (!this.initialized) return false;
    this.assertExports().aiwos_tick(nowNs);
    return true;
  }

  handleEvent(eventPtr: number): boolean {
    if (!this.initialized) return false;
    const st = this.assertExports().aiwos_handle_event(eventPtr);
    return st === 0;
  }

  // ---------- 任务管理 ----------

  /** 创建任务，返回 task_id（0 = 失败） */
  taskCreate(): number {
    if (!this.initialized) return 0;
    return this.assertExports().aiwos_task_create(0);
  }

  /** 销毁任务 */
  taskDestroy(taskId: number): boolean {
    if (!this.initialized) return false;
    return this.assertExports().aiwos_task_destroy(taskId) === 0;
  }

  /** 让出当前任务 */
  taskYield(): boolean {
    if (!this.initialized) return false;
    return this.assertExports().aiwos_task_yield() === 0;
  }

  /** 阻塞当前任务 */
  taskBlock(reason: number, waitEvent: number, timeoutNs: bigint): boolean {
    if (!this.initialized) return false;
    return this.assertExports().aiwos_task_block(reason, waitEvent, timeoutNs) === 0;
  }

  /** 唤醒任务 */
  taskWakeup(taskId: number): boolean {
    if (!this.initialized) return false;
    return this.assertExports().aiwos_task_wakeup(taskId) === 0;
  }

  /** 按事件类型唤醒所有匹配的阻塞任务 */
  taskWakeupByEvent(eventType: number): number {
    if (!this.initialized) return 0;
    return this.assertExports().aiwos_task_wakeup_by_event(eventType);
  }

  /** 完成当前任务 */
  taskComplete(): boolean {
    if (!this.initialized) return false;
    return this.assertExports().aiwos_task_complete() === 0;
  }

  /** 调度下一个 READY 任务，返回选中的 task_id（0 = 无） */
  scheduleNext(): number {
    if (!this.initialized) return 0;
    return this.assertExports().aiwos_schedule_next();
  }

  /** 当前正在运行的任务 ID（0 = 无） */
  current(): number {
    if (!this.initialized) return 0;
    return this.assertExports().aiwos_current();
  }

  /** 是否有 READY 任务 */
  hasReady(): boolean {
    if (!this.initialized) return false;
    return this.assertExports().aiwos_has_ready() !== 0;
  }

  // ---------- 状态查询 ----------

  readSnapshot(): CoreSnapshot | null {
    const exp = this.assertExports();
    exp.aiwos_alloc_reset();
    const buf = exp.aiwos_alloc(56);
    if (!buf) return null;

    const st = exp.aiwos_query_state(AIWOS_QUERY_STATE_SNAPSHOT, buf, 56);
    if (st !== 0) return null;

    const dv = new DataView(exp.memory.buffer, buf, 56);
    const snap: CoreSnapshot = {
      tick_count: dv.getBigUint64(0, true),
      last_now_ns: dv.getBigUint64(8, true),
      last_event_type: dv.getUint32(16, true),
      scheduler_state: dv.getUint32(20, true),
      last_event_data: dv.getBigUint64(24, true),
      task_count: dv.getUint32(32, true),
      ready_count: dv.getUint32(36, true),
      blocked_count: dv.getUint32(40, true),
    };
    exp.aiwos_alloc_reset();
    return snap;
  }

  readStats(): SchedulerStats | null {
    const exp = this.assertExports();
    exp.aiwos_alloc_reset();
    const buf = exp.aiwos_alloc(56);
    if (!buf) return null;

    const st = exp.aiwos_query_state(AIWOS_QUERY_SCHEDULER_STATS, buf, 56);
    if (st !== 0) return null;

    const dv = new DataView(exp.memory.buffer, buf, 56);
    const stats: SchedulerStats = {
      total_ticks: dv.getBigUint64(0, true),
      last_tick_ns: dv.getBigUint64(8, true),
      min_vruntime: dv.getBigUint64(16, true),
      task_count: dv.getUint32(24, true),
      ready_count: dv.getUint32(28, true),
      blocked_count: dv.getUint32(32, true),
    };
    exp.aiwos_alloc_reset();
    return stats;
  }

  // ---------- 事件构造 ----------

  /**
   * 在 Wasm 线性内存中构造一个 aiwos_event_t。
   * 调用方必须在用完结果后调用 freeEvent()。
   *
   * aiwos_event_t 布局（wasm32, repr(C)）：
   *   偏移 0: type_ (u32)
   *   偏移 4: padding (4 bytes)
   *   偏移 8: data (u64)
   */
  allocEvent(type: number, data: number | bigint): number {
    const exp = this.assertExports();
    exp.aiwos_alloc_reset();
    const ptr = exp.aiwos_alloc(16);
    if (!ptr) return 0;

    const dv = new DataView(exp.memory.buffer);
    dv.setUint32(ptr, type, true);
    dv.setUint32(ptr + 4, 0, true); // padding
    dv.setBigUint64(ptr + 8, BigInt(data), true);
    return ptr;
  }

  freeEvent(): void {
    if (this.exports) {
      this.exports.aiwos_alloc_reset();
    }
  }

  // ---------- 内部 ----------

  private assertExports(): WasmExports {
    if (!this.exports) throw new Error("Wasm not loaded");
    return this.exports;
  }
}
