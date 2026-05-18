import { HardwareState } from "../types";

interface Props {
  hw: HardwareState;
}

export function SimulatedHardware({ hw }: Props) {
  return (
    <section className="panel">
      <h2>Simulated Hardware</h2>
      <div className="kv">
        <span className="muted">Physical memory bytes</span>
        <span>{hw.memory.length.toLocaleString()}</span>
        <span className="muted">CPU.pc</span>
        <span>{hw.cpu.pc}</span>
        <span className="muted">CPU.tick</span>
        <span>{hw.cpu.tick}</span>
        <span className="muted">IRQ pending</span>
        <span>{hw.irqPending ? "yes" : "no"}</span>
      </div>
    </section>
  );
}
