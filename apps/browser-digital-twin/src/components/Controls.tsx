interface Props {
  onTick: () => void;
  onTimerIrq: () => void;
  onInterrupt: () => void;
  onReset: () => void;
}

export function Controls({ onTick, onTimerIrq, onInterrupt, onReset }: Props) {
  return (
    <section className="panel">
      <h2>Controls</h2>
      <div className="controls">
        <button className="primary" onClick={onTick}>
          Tick
        </button>
        <button onClick={onTimerIrq}>Trigger Timer IRQ</button>
        <button onClick={onInterrupt}>Trigger Interrupt</button>
        <button onClick={onReset}>Reset</button>
      </div>
    </section>
  );
}
