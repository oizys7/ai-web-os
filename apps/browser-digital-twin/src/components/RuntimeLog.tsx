import { useRef, useEffect } from "react";

interface Props {
  lines: string[];
}

export function RuntimeLog({ lines }: Props) {
  const preRef = useRef<HTMLPreElement>(null);

  useEffect(() => {
    if (preRef.current) {
      preRef.current.scrollTop = preRef.current.scrollHeight;
    }
  }, [lines.length]);

  return (
    <section className="panel" style={{ gridColumn: "1 / -1" }}>
      <h2>Runtime Log</h2>
      <pre ref={preRef}>{lines.join("\n")}</pre>
    </section>
  );
}
