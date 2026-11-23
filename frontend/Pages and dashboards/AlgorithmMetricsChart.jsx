import { useId } from 'react';
import { formatSchedulingValue } from './schedulingMetrics';

const timeMetrics = [
  { key: 'avg_waiting_time', label: 'Average Waiting' },
  { key: 'avg_turnaround_time', label: 'Average Turnaround' },
  { key: 'avg_response_time', label: 'Average Response' },
];

export default function AlgorithmMetricsChart({ algorithm, metrics }) {
  const headingId = useId();
  const maxValue = Math.max(0, ...timeMetrics.map(({ key }) =>
    typeof metrics?.[key] === 'number' && Number.isFinite(metrics[key]) ? metrics[key] : 0));

  return (
    <figure className="scheduling-chart algorithm-metrics-chart" aria-labelledby={headingId} data-algorithm={algorithm}>
      <div className="scheduling-chart-heading">
        <figcaption id={headingId}>{algorithm} metrics</figcaption>
        <span className="algorithm-request-count">Requests: <output data-metric="requests_scheduled">{formatSchedulingValue(metrics?.requests_scheduled)}</output></span>
      </div>
      {!metrics ? <p className="text-muted" role="status">No metrics were returned for this algorithm.</p> : (
        <>
          <p className="text-muted scheduling-chart-unit">Average times in simulated time units. Exact values from this algorithm’s backend result.</p>
          <div className="scheduling-chart-scroll">
            <div className="scheduling-chart-plot">
              <div className="scheduling-chart-axis" aria-hidden="true">
                {[1, 0.75, 0.5, 0.25, 0].map(fraction => (
                  <div key={fraction} className="scheduling-chart-tick" style={{ top: `${(1 - fraction) * 100}%` }}>
                    <span>{Number(((maxValue || 1) * fraction).toPrecision(3))}</span>
                  </div>
                ))}
              </div>
              <ul className="scheduling-chart-bars" aria-label={`${algorithm} average times`}>
                {timeMetrics.map(({ key, label }, index) => {
                  const value = metrics[key];
                  const valid = typeof value === 'number' && Number.isFinite(value) && value >= 0;
                  const height = valid && maxValue > 0 ? value / maxValue * 100 : 0;
                  return (
                    <li key={key} className="scheduling-chart-row" data-metric={key}>
                      <div className="scheduling-chart-track">
                        <output className="scheduling-chart-value" style={{ bottom: `calc(${height}% + 8px)` }}>{formatSchedulingValue(value)}</output>
                        <div aria-hidden="true" className={`scheduling-chart-bar algorithm-${index}`} style={{ height: `${height}%` }} />
                      </div>
                      <span className="scheduling-chart-label">{label}</span>
                    </li>
                  );
                })}
              </ul>
            </div>
          </div>
          <p className="text-muted scheduling-chart-note">Equal times have equal heights; zero times have no filled bar. Request count is shown separately because it is a count, not a duration.</p>
        </>
      )}
    </figure>
  );
}
