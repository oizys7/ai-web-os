import { SchedulerStats as SchedulerStatsType } from "../types";

interface Props {
  stats: SchedulerStatsType | null;
}

export function SchedulerStats({ stats }: Props) {
  return (
    <section className="panel">
      <h2>Scheduler Stats</h2>
      <div className="kv">
        <span className="muted">total_ticks</span>
        <span>{stats?.total_ticks?.toString() ?? "-"}</span>
        <span className="muted">min_vruntime</span>
        <span>{stats?.min_vruntime?.toString() ?? "-"}</span>
      </div>
    </section>
  );
}
