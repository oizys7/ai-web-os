import { HardwareState, createHardwareState } from "./types";

/**
 * 模拟硬件状态管理。
 * 与原有 hw 对象行为一致，提供 tick 和 reset 操作。
 */
export class SimulatedHardware {
  state: HardwareState;

  constructor() {
    this.state = createHardwareState();
  }

  /** 推进一个 CPU tick（纯可视化，不涉及 Wasm 核心） */
  tick(): void {
    const hw = this.state;
    hw.cpu.tick += 1;
    hw.cpu.pc += 4;
    hw.memory[hw.cpu.tick % hw.memory.length] = hw.cpu.tick & 0xff;
  }

  /** 重置硬件状态 */
  reset(): void {
    const mem = this.state.memory;
    mem.fill(0);
    this.state = {
      memory: mem,
      cpu: { pc: 0, tick: 0 },
      irqPending: false,
    };
  }
}
