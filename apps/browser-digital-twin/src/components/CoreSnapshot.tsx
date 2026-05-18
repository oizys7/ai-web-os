import { CoreSnapshot as CoreSnapshotType, schedulerStateName } from "../types";

interface Props {
  snap: CoreSnapshotType | null;
}

export function CoreSnapshot({ snap }: Props) {
  return (
    <section className="panel">
      <h2>Wasm Core Snapshot</h2>
      <div className="kv">
        <span className="muted">tick_count</span>
        <span>{snap?.tick_count?.toString() ?? "-"}</span>
        <span className="muted">last_event_type</span>
        <span>{snap?.last_event_type?.toString() ?? "-"}</span>
        <span className="muted">last_event_data</span>
        <span>{snap?.last_event_data?.toString() ?? "-"}</span>
        <span className="muted">scheduler_state</span>
        <span>
          {snap != null
            ? `${snap.scheduler_state} (${schedulerStateName(snap.scheduler_state)})`
            : "-"}
        </span>
        <span className="muted">task_count / ready / blocked</span>
        <span>
          {snap != null
            ? `${snap.task_count} / ${snap.ready_count} / ${snap.blocked_count}`
            : "-"}
        </span>
      </div>
    </section>
  );
}
