import { useId, useState } from 'react';

export default function GanttChart({ timeline = [], algorithm }) {
  const titleId = useId();
  const [selected, setSelected] = useState(null);
  if (!timeline.length) return <p className="text-muted">No execution timeline returned.</p>;
  const requests = [...new Map(timeline.map(slice => [slice.request_id, slice.public_ref])).entries()];
  const end = timeline.reduce((maximum, slice) => Math.max(maximum, slice.end_time), 1);
  const chosen = timeline[selected];
  return <section className="tool-panel gantt-panel" aria-labelledby={titleId}>
    <h3 id={titleId}>{algorithm} execution timeline</h3>
    <p className="text-muted">Simulated time. Each block is one actual execution slice. Select a block for exact start and finish times.</p>
    <div className="gantt-scroll"><svg className="gantt-svg" viewBox={`0 0 1100 ${requests.length * 48 + 64}`} role="group" aria-label={`${algorithm} Gantt chart`}>
      {Array.from({ length: 6 }, (_, i) => <g key={i}><text x={160 + i * 180} y="20" fill="#94a3b8" textAnchor="middle">{Number((end * i / 5).toFixed(2))}</text><line x1={160 + i * 180} x2={160 + i * 180} y1="30" y2={requests.length * 48 + 32} stroke="#334155" /></g>)}
      {requests.map(([id, ref], i) => <text key={id} x="148" y={60 + i * 48} fill="#e2e8f0" textAnchor="end" fontSize="12">{ref}</text>)}
      {timeline.map((slice, index) => {
        const row = requests.findIndex(([id]) => id === slice.request_id);
        const x = 160 + slice.start_time / end * 900, width = (slice.end_time - slice.start_time) / end * 900;
        const label = `${slice.public_ref}: ${slice.start_time} to ${slice.end_time}`;
        return <g key={index} role="button" tabIndex="0" aria-label={label} onClick={() => setSelected(index)} onKeyDown={event => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); setSelected(index); } }}>
          <title>{label}</title><rect x={x} y={40 + row * 48} width={width} height="30" rx="3" fill={['#60a5fa', '#34d399', '#fbbf24', '#c084fc'][row % 4]} stroke={selected === index ? '#fff' : '#0b1120'} strokeWidth="2" />
          {width > 32 && <text x={x + width / 2} y={60 + row * 48} textAnchor="middle" fill="#0b1120" fontSize="11">{slice.request_id}</text>}
        </g>;
      })}
    </svg></div>
    {chosen && <p className="slice-details" role="status">{chosen.public_ref}: start {chosen.start_time}, finish {chosen.end_time}, duration {chosen.end_time - chosen.start_time}.</p>}
  </section>;
}
