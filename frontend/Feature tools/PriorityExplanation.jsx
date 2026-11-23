export default function PriorityExplanation({ request }) {
  const score = request.priority_breakdown;
  if (!score) return null;
  return <details className="priority-explanation"><summary>Why this priority? Total {score.total}</summary>
    <dl className="details-grid"><dt>Severity × 20</dt><dd>{score.severity}</dd><dt>Risk × 15</dt><dd>{score.risk}</dd><dt>Affected people (capped at 50) × 0.6</dt><dd>{score.people}</dd><dt>Aging: 10 per 5 simulated units</dt><dd>{score.aging}</dd><dt>Simulated wait at scoring</dt><dd>{score.waited}</dd></dl>
    <p className="text-muted">{request.queue_position > 0 ? 'Contributions at this algorithm’s simulated start time.' : 'Initial score before simulated waiting. The scheduler recalculates aging when selecting each request.'}</p>
  </details>;
}
