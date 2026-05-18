import { useState, useEffect, useCallback, useRef } from "react";
import { WasmCore } from "./wasmCore";
import { SimulatedHardware } from "./simulatedHardware";
import { CoreSnapshot as CoreSnapshotType, SchedulerStats as SchedulerStatsType, createHardwareState } from "./types";
import { SimulatedHardware as SimulatedHardwarePanel } from "./components/SimulatedHardware";
import { CoreSnapshot as CoreSnapshotPanel } from "./components/CoreSnapshot";
import { SchedulerStats as SchedulerStatsPanel } from "./components/SchedulerStats";
import { Controls } from "./components/Controls";
import { RuntimeLog } from "./components/RuntimeLog";
import { TaskPanel } from "./components/TaskPanel";
import "./App.css";

export default function App() {
  const [hw, setHw] = useState(createHardwareState);
  const [snap, setSnap] = useState<CoreSnapshotType | null>(null);
  const [stats, setStats] = useState<SchedulerStatsType | null>(null);
  const [logs, setLogs] = useState<string[]>([`[${ts()}] Booting ai-web-os digital twin…`]);
  const [currentTask, setCurrentTask] = useState(0);
  const [taskIds, setTaskIds] = useState<number[]>([]);

  const coreRef = useRef<WasmCore | null>(null);
  const hwRef = useRef<SimulatedHardware | null>(null);

  // ---------- 日志辅助 ----------

  const appendLog = useCallback((msg: string) => {
    setLogs((prev) => [...prev, `[${ts()}] ${msg}`]);
  }, []);

  // ---------- 刷新快照 ----------

  const refreshState = useCallback(() => {
    const core = coreRef.current;
    if (!core?.initialized) return;
    setSnap(core.readSnapshot());
    setStats(core.readStats());
    setCurrentTask(core.current());
  }, []);

  // ---------- 加载 Wasm + 初始化 ----------

  useEffect(() => {
    const core = new WasmCore();
    coreRef.current = core;
    const sim = new SimulatedHardware();
    hwRef.current = sim;

    core.onLog = (msg) => appendLog(msg);

    (async () => {
      try {
        await core.load();
        appendLog(`Wasm loaded`);
        core.init();
        appendLog("Core initialized");
        refreshState();
        appendLog("Ready — core is live");
      } catch (err: unknown) {
        const msg = err instanceof Error ? err.message : String(err);
        appendLog(`BOOT FAILED: ${msg}`);
      }
    })();
  }, []); // eslint-disable-line react-hooks/exhaustive-deps

  // ---------- 事件处理 ----------

  const handleTick = useCallback(() => {
    const core = coreRef.current;
    const sim = hwRef.current;
    if (!core?.initialized || !sim) return;

    const ns = BigInt(Date.now()) * 1_000_000n;
    core.tick(ns);
    sim.tick();
    setHw({ ...sim.state });
    refreshState();
  }, [refreshState]);

  const handleEvent = useCallback(
    (type: number, data: number) => {
      const core = coreRef.current;
      if (!core?.initialized) return;

      const ptr = core.allocEvent(type, data);
      if (!ptr) {
        appendLog("allocEvent failed");
        return;
      }
      core.handleEvent(ptr);
      core.freeEvent();
      setHw((prev) => ({ ...prev, irqPending: false }));
      refreshState();
    },
    [refreshState, appendLog]
  );

  const handleReset = useCallback(() => {
    const core = coreRef.current;
    const sim = hwRef.current;
    if (!core || !sim) return;

    core.shutdown();
    sim.reset();
    setHw({ ...sim.state });
    core.init();
    setTaskIds([]);
    setCurrentTask(0);
    refreshState();
    appendLog("State reset");
  }, [refreshState, appendLog]);

  // ---------- 任务操作 ----------

  const handleCreateTask = useCallback(() => {
    const core = coreRef.current;
    if (!core?.initialized) return;
    const id = core.taskCreate();
    if (id === 0) {
      appendLog("taskCreate failed (limit reached?)");
    } else {
      setTaskIds((prev) => [...prev, id]);
      appendLog(`Task #${id} created`);
    }
    refreshState();
  }, [refreshState, appendLog]);

  const handleDestroyTask = useCallback(
    (id: number) => {
      const core = coreRef.current;
      if (!core?.initialized || id < 1) return;
      if (core.taskDestroy(id)) {
        setTaskIds((prev) => prev.filter((tid) => tid !== id));
        appendLog(`Task #${id} destroyed`);
      } else {
        appendLog(`taskDestroy #${id} failed`);
      }
      refreshState();
    },
    [refreshState, appendLog]
  );

  const handleYield = useCallback(() => {
    const core = coreRef.current;
    if (!core?.initialized) return;
    core.taskYield() ? appendLog("Yielded") : appendLog("Yield failed");
    refreshState();
  }, [refreshState, appendLog]);

  const handleBlock = useCallback(
    (reason: number) => {
      const core = coreRef.current;
      if (!core?.initialized) return;
      core.taskBlock(reason, 0, 0n)
        ? appendLog(`Blocked (reason=${reason})`)
        : appendLog("Block failed");
      refreshState();
    },
    [refreshState, appendLog]
  );

  const handleComplete = useCallback(() => {
    const core = coreRef.current;
    if (!core?.initialized) return;
    core.taskComplete() ? appendLog("Completed") : appendLog("Complete failed");
    refreshState();
  }, [refreshState, appendLog]);

  const handleScheduleNext = useCallback(() => {
    const core = coreRef.current;
    if (!core?.initialized) return;
    const id = core.scheduleNext();
    appendLog(id !== 0 ? `Scheduled #${id}` : "No ready task");
    refreshState();
  }, [refreshState, appendLog]);

  const handleWakeup = useCallback(
    (id: number) => {
      const core = coreRef.current;
      if (!core?.initialized || id < 1) return;
      core.taskWakeup(id) ? appendLog(`Task #${id} woken`) : appendLog(`Wakeup #${id} failed`);
      refreshState();
    },
    [refreshState, appendLog]
  );

  const handleWakeupByEvent = useCallback(
    (eventType: number) => {
      const core = coreRef.current;
      if (!core?.initialized) return;
      const n = core.taskWakeupByEvent(eventType);
      appendLog(`Wakeup by event ${eventType}: ${n} task(s)`);
      refreshState();
    },
    [refreshState, appendLog]
  );

  // ---------- 渲染 ----------

  return (
    <div className="wrap">
      <h1>AI Web OS: Digital Twin</h1>
      <div className="tag">Phase 2 · Rust Wasm core + browser host</div>
      <div className="grid">
        <SimulatedHardwarePanel hw={hw} />
        <CoreSnapshotPanel snap={snap} />
        <SchedulerStatsPanel stats={stats} />
        <Controls
          onTick={handleTick}
          onTimerIrq={() => handleEvent(2, 42)}
          onInterrupt={() => handleEvent(1, 0)}
          onReset={handleReset}
        />
        <TaskPanel
          currentTask={currentTask}
          tasks={taskIds}
          onCreateTask={handleCreateTask}
          onDestroyTask={handleDestroyTask}
          onYield={handleYield}
          onBlock={handleBlock}
          onComplete={handleComplete}
          onScheduleNext={handleScheduleNext}
          onWakeup={handleWakeup}
          onWakeupByEvent={handleWakeupByEvent}
        />
        <RuntimeLog lines={logs} />
      </div>
    </div>
  );
}

function ts(): string {
  return new Date().toISOString().slice(11, 19);
}
