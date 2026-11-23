import { useState } from 'react';
import { apiRequest } from '../../services/api';
import ResourceGraph from './ResourceGraph';
import ResultTable from './ResultTable';
import GanttChart from './GanttChart';
import SchedulingComparisonChart from '../SchedulingComparisonChart';
import AlgorithmRecommendation from './AlgorithmRecommendation';

export default function SimulationLab({ auth }) {
  const [processes, setProcesses] = useState('3');
  const [deadlock, setDeadlock] = useState(null);
  const [deadlockBusy, setDeadlockBusy] = useState(false);
  const [deadlockError, setDeadlockError] = useState('');
  const [inputs, setInputs] = useState({ count: '10', quantum: '3', arrival_gap: '2', max_severity: '5', max_people: '20', seed: '42', pattern: 'staggered' });
  const [workload, setWorkload] = useState(null);
  const [workloadBusy, setWorkloadBusy] = useState(false);
  const [workloadError, setWorkloadError] = useState('');
  const runDeadlock = async recover => {
    setDeadlockBusy(true); setDeadlockError('');
    try { setDeadlock(await apiRequest('/api/staff/labs/deadlock', { token: auth.token, method: 'POST', body: { processes: Number(processes), recover } })); }
    catch (error) { setDeadlockError(error.message); } finally { setDeadlockBusy(false); }
  };
  const runWorkload = async event => {
    event.preventDefault(); setWorkloadBusy(true); setWorkloadError(''); setWorkload(null);
    try { setWorkload(await apiRequest('/api/staff/labs/scheduling', { token: auth.token, method: 'POST', body: Object.fromEntries(Object.entries(inputs).map(([key, value]) => [key, key === 'pattern' ? value : Number(value)])) })); }
    catch (error) { setWorkloadError(error.message); } finally { setWorkloadBusy(false); }
  };
  return <section className="simulation-lab"><h2>Simulation lab</h2><p className="text-muted">Optional teaching tools. Results are generated in memory and never change real requests, inventory, scheduling runs, or audit records.</p>
    <div className="tool-panel deadlock-lab"><h3>Controlled deadlock simulator</h3>
      <p>The simulation starts only when you click the button below.</p>
      <label className="form-label">Simulated processes<select className="form-control" aria-label="Simulated processes" value={processes} disabled={deadlockBusy} onChange={event => { setProcesses(event.target.value); setDeadlock(null); }}>{[2, 3, 4, 5].map(n => <option key={n}>{n}</option>)}</select></label>
      <div className="button-row"><button type="button" className="btn btn-primary" disabled={deadlockBusy} onClick={() => runDeadlock(false)}>{deadlockBusy ? 'Simulating…' : 'Run deadlock simulation'}</button>{deadlock && <button type="button" className="btn btn-outline" disabled={deadlockBusy || !deadlock.has_cycle} onClick={() => runDeadlock(true)}>Simulate recovery by preemption</button>}<button className="btn btn-outline" type="button" disabled={deadlockBusy} onClick={() => { setDeadlock(null); setDeadlockError(''); }}>Reset simulation</button></div>
      {deadlockError && <p role="alert" className="text-danger">{deadlockError}</p>}
      {!deadlock && !deadlockBusy && <p role="status" className="text-muted">Not started. Live operations are unchanged.</p>}
      {deadlock && <div className="deadlock-result"><p role="status" className={deadlock.has_cycle ? 'text-danger' : 'text-success'}>{deadlock.has_cycle ? 'Simulated circular wait detected' : 'Simulated cycle removed'}</p><p>{deadlock.recovery}</p><dl className="details-grid">{Object.entries(deadlock.conditions).map(([key, value]) => <div key={key}><dt>{key.replaceAll('_', ' ')}</dt><dd>{value ? 'Present' : 'Broken by recovery'}</dd></div>)}</dl>{deadlock.progress_sequence.length > 0 && <p>Remaining progress sequence: {deadlock.progress_sequence.join(' → ')}</p>}<ResourceGraph graph={deadlock.graph} /></div>}
    </div>
    <div className="tool-panel scheduling-lab"><h3>Scheduling experiment lab</h3><p className="text-muted">Generate a reproducible synthetic workload. Unlike the operational scheduler’s normalized arrivals, this lab honors the simulated arrival pattern and idle gaps you choose.</p>
      <form onSubmit={runWorkload}><div className="form-grid-2">{[
        ['count', 'Synthetic request count', 1, 30], ['quantum', 'Lab Round Robin quantum', 1, 100], ['arrival_gap', 'Arrival gap', 0, 20], ['max_severity', 'Maximum severity', 1, 5], ['max_people', 'Maximum affected people', 1, 50], ['seed', 'Random seed', 0, 1000000],
      ].map(([key, label, min, max]) => <label key={key} className="form-label">{label}<input aria-label={label} className="form-control" type="number" min={min} max={max} value={inputs[key]} required onChange={event => { setInputs(current => ({ ...current, [key]: event.target.value })); setWorkload(null); }} /></label>)}<label className="form-label">Arrival pattern<select className="form-control" aria-label="Arrival pattern" value={inputs.pattern} onChange={event => { setInputs(current => ({ ...current, pattern: event.target.value })); setWorkload(null); }}><option value="staggered">Staggered</option><option value="burst">Bursts of five</option><option value="random">Random arrivals</option></select></label></div><button className="btn btn-primary" type="submit" disabled={workloadBusy}>{workloadBusy ? 'Running experiment…' : 'Generate and compare synthetic workload'}</button></form>
      {workloadError && <p role="alert" className="text-danger">{workloadError}</p>}
      {workload && <div className="lab-results"><p role="status">Simulation only · seed {workload.seed} · {workload.workload.length} synthetic requests. No operational data saved.</p><AlgorithmRecommendation comparison={workload.comparison} /><SchedulingComparisonChart metrics={workload.comparison} hasResult loading={false} error="" /><ResultTable rows={workload.workload} filename="synthetic-workload.csv" columns={[{ key: 'public_ref', label: 'Synthetic request' }, { key: 'arrival_time', label: 'Simulated arrival' }, { key: 'severity_snapshot', label: 'Severity' }, { key: 'people_affected', label: 'People' }]} />{Object.entries(workload.algorithms).map(([key, result]) => <GanttChart key={key} timeline={result.timeline} algorithm={result.algorithm} />)}</div>}
    </div>
  </section>;
}
