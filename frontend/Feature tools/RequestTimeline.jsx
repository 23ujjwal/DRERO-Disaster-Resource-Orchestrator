export default function RequestTimeline({ history = [] }) {
  return <section className="tool-panel"><h3>Request progress timeline</h3>{history.length ? <ol className="request-timeline">{history.map((event, index) => <li key={`${event.changed_at}-${index}`}><span className="timeline-dot" aria-hidden="true" /><div><strong>{event.new_status}</strong><p><time>{event.changed_at}</time> · {event.old_status} → {event.new_status}</p>{event.remarks && <p className="text-muted">{event.remarks}</p>}</div></li>)}</ol> : <p className="text-muted">No status events have been recorded yet.</p>}</section>;
}
