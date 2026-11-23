import { useState } from 'react';
import { formatSchedulingValue, schedulingMetrics } from './schedulingMetrics';

const algorithms = [
  { label: 'FCFS', matches: name => name === 'FCFS' },
  { label: 'SJF', matches: name => name === 'SJF' },
  { label: 'Round Robin', matches: name => /^RR(?:\(|$)/.test(name) },
  { label: 'Priority with Aging', matches: name => name === 'PRIORITY' },
];

export default function SchedulingComparisonChart({ metrics, loading, error, hasResult }) {
  const [metricKey, setMetricKey] = useState('avg_turnaround_time');
  const metric = schedulingMetrics.find(item => item.key === metricKey);
  const rows = algorithms.map(algorithm => ({
    label: algorithm.label,
    value: metrics.find(item => algorithm.matches(item.algorithm))?.[metricKey],
  }));
  const maxValue = Math.max(0, ...rows.map(row => Number.isFinite(row.value) ? row.value : 0));

  return (
    <figure className="scheduling-chart scheduling-comparison-chart" aria-labelledby="comparison-chart-title" aria-busy={loading}>
      <div className="scheduling-chart-heading">
        <figcaption id="comparison-chart-title">Scheduling comparison</figcaption>
        <div className="scheduling-chart-selector">
          <label className="form-label" htmlFor="comparison-metric">Comparison metric</label>
          <select id="comparison-metric" className="form-control" value={metricKey} onChange={event => setMetricKey(event.target.value)}>
            {schedulingMetrics.map(item => <option key={item.key} value={item.key}>{item.label}</option>)}
          </select>
        </div>
      </div>
      {loading ? <p role="status">Running all four algorithms… Waiting for backend results.</p>
        : error ? <p className="text-danger" role="alert">Comparison failed: {error} Run “Compare all four” to retry.</p>
          : metrics.length === 0 ? <p className="text-muted" role="status">{hasResult ? 'No comparison results were returned by the backend.' : 'Run “Compare all four” to load scheduling results.'}</p>
            : (
              <>
                <p className="text-muted scheduling-chart-unit">{metric.label} · {metric.unit}. Values are shown as returned by the backend.</p>
                <div className="scheduling-chart-scroll">
                  <div className="scheduling-chart-plot">
                    <div className="scheduling-chart-axis" aria-hidden="true">
                      {[1, 0.75, 0.5, 0.25, 0].map(fraction => (
                        <div key={fraction} className="scheduling-chart-tick" style={{ top: `${(1 - fraction) * 100}%` }}>
                          <span>{Number(((maxValue || 1) * fraction).toPrecision(3))}</span>
                        </div>
                      ))}
                    </div>
                    <ul className="scheduling-chart-bars" aria-label={metric.label}>
                      {rows.map((row, index) => {
                        const valid = typeof row.value === 'number' && Number.isFinite(row.value) && row.value >= 0;
                        const height = valid && maxValue > 0 ? row.value / maxValue * 100 : 0;
                        return (
                          <li key={row.label} className="scheduling-chart-row" data-algorithm={row.label}>
                            <div className="scheduling-chart-track">
                              <output className="scheduling-chart-value" data-metric={metricKey} style={{ bottom: `calc(${height}% + 8px)` }}>{formatSchedulingValue(row.value)}</output>
                              <div aria-hidden="true" className={`scheduling-chart-bar algorithm-${index}`} style={{ height: `${height}%` }} />
                            </div>
                            <span className="scheduling-chart-label">{row.label}</span>
                          </li>
                        );
                      })}
                    </ul>
                  </div>
                </div>
                <p className="text-muted scheduling-chart-note">Bars share a zero baseline and scale to the largest value. Equal values have equal heights; zero values have no filled bar.</p>
              </>
            )}
    </figure>
  );
}
