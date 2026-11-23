import { useState } from 'react';
import { schedulingMetrics } from '../schedulingMetrics';
const objectives = schedulingMetrics.filter(metric => metric.key !== 'requests_scheduled');
export default function AlgorithmRecommendation({ comparison = [] }) {
  const [objective, setObjective] = useState('avg_waiting_time');
  const eligible = comparison.filter(item => typeof item[objective] === 'number' && Number.isFinite(item[objective]));
  const target = objective === 'throughput' ? Math.max(...eligible.map(item => item[objective])) : Math.min(...eligible.map(item => item[objective]));
  const best = eligible.filter(item => item[objective] === target).map(item => item.algorithm.startsWith('RR') ? 'Round Robin' : item.algorithm === 'PRIORITY' ? 'Priority with Aging' : item.algorithm);
  return <section className="tool-panel"><h3>Recommendation by objective</h3>
    <label className="form-label" htmlFor="scheduling-objective">Optimize for</label><select id="scheduling-objective" className="form-control" value={objective} onChange={event => setObjective(event.target.value)}>{objectives.map(item => <option key={item.key} value={item.key}>{item.label}</option>)}</select>
    <p className="recommendation" role="status">{best.length ? `${best.join(' / ')}${best.length > 1 ? ' (tie)' : ''}: ${target}. ${objective === 'throughput' ? 'Highest' : 'Lowest'} ${objectives.find(item => item.key === objective).label.toLowerCase()} in this run.` : 'No metrics available.'}</p>
    <p className="text-muted">This recommendation reflects the selected metric. Review severity, risk and fairness before dispatching.</p>
  </section>;
}
