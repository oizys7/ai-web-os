import { useState } from "react";

interface Props {
  currentTask: number;
  tasks: number[];
  onCreateTask: () => void;
  onDestroyTask: (id: number) => void;
  onYield: () => void;
  onBlock: (reason: number) => void;
  onComplete: () => void;
  onScheduleNext: () => void;
  onWakeup: (id: number) => void;
  onWakeupByEvent: (eventType: number) => void;
}

const BLOCK_REASONS = [
  { value: 1, label: "Event" },
  { value: 2, label: "Timeout" },
  { value: 3, label: "Resource" },
  { value: 4, label: "AI Inference" },
];

export function TaskPanel({
  currentTask,
  tasks,
  onCreateTask,
  onDestroyTask,
  onYield,
  onBlock,
  onComplete,
  onScheduleNext,
  onWakeup,
  onWakeupByEvent,
}: Props) {
  const [destroyId, setDestroyId] = useState("");
  const [wakeupId, setWakeupId] = useState("");
  const [wakeupEvent, setWakeupEvent] = useState("1");
  const [blockReason, setBlockReason] = useState(1);

  return (
    <section className="panel">
      <h2>Task Operations</h2>

      <div className="kv" style={{ marginBottom: 8 }}>
        <span className="muted">Current Task</span>
        <span>{currentTask !== 0 ? `#${currentTask}` : "(none)"}</span>
        <span className="muted">Total Tasks</span>
        <span>{tasks.length}</span>
      </div>

      <div style={{ marginBottom: 8 }}>
        <div className="muted" style={{ marginBottom: 4 }}>Task List</div>
        {tasks.length === 0 ? (
          <span className="muted" style={{ fontSize: 13 }}>(empty)</span>
        ) : (
          <div style={{ display: "flex", flexWrap: "wrap", gap: 4 }}>
            {tasks.map((id) => (
              <span
                key={id}
                style={{
                  fontSize: 12,
                  padding: "1px 6px",
                  borderRadius: 4,
                  background: id === currentTask ? "#15803d" : "#1e293b",
                }}
              >
                #{id}
              </span>
            ))}
          </div>
        )}
      </div>

      <div className="controls">
        <button className="primary" onClick={onCreateTask}>
          Create Task
        </button>
        <button onClick={onScheduleNext}>Schedule Next</button>
        <button onClick={onYield}>Yield</button>
        <button onClick={onComplete}>Complete</button>
      </div>

      <div className="controls" style={{ marginTop: 8 }}>
        <select
          value={blockReason}
          onChange={(e) => setBlockReason(Number(e.target.value))}
          style={{
            background: "#0b1222",
            color: "#e5e7eb",
            border: "1px solid #334155",
            borderRadius: 4,
            padding: "4px 8px",
            fontSize: 13,
          }}
        >
          {BLOCK_REASONS.map((r) => (
            <option key={r.value} value={r.value}>{r.label}</option>
          ))}
        </select>
        <button onClick={() => onBlock(blockReason)}>Block</button>
        <input
          type="number"
          min="1"
          placeholder="Task ID"
          value={destroyId}
          onChange={(e) => setDestroyId(e.target.value)}
          style={{
            width: 70,
            background: "#0b1222",
            color: "#e5e7eb",
            border: "1px solid #334155",
            borderRadius: 4,
            padding: "4px 8px",
            fontSize: 13,
          }}
        />
        <button onClick={() => { onDestroyTask(Number(destroyId)); setDestroyId(""); }}>Destroy</button>
      </div>

      <div className="controls" style={{ marginTop: 8 }}>
        <input
          type="number"
          min="1"
          placeholder="Task ID"
          value={wakeupId}
          onChange={(e) => setWakeupId(e.target.value)}
          style={{
            width: 70,
            background: "#0b1222",
            color: "#e5e7eb",
            border: "1px solid #334155",
            borderRadius: 4,
            padding: "4px 8px",
            fontSize: 13,
          }}
        />
        <button onClick={() => { onWakeup(Number(wakeupId)); setWakeupId(""); }}>Wakeup</button>
        <input
          type="number"
          min="1"
          placeholder="Event"
          value={wakeupEvent}
          onChange={(e) => setWakeupEvent(e.target.value)}
          style={{
            width: 60,
            background: "#0b1222",
            color: "#e5e7eb",
            border: "1px solid #334155",
            borderRadius: 4,
            padding: "4px 8px",
            fontSize: 13,
          }}
        />
        <button onClick={() => onWakeupByEvent(Number(wakeupEvent))}>Wakeup by Event</button>
      </div>
    </section>
  );
}
