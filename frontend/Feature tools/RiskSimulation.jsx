import { useState } from 'react';
import { apiRequest } from '../../services/api';
import ResultTable from './ResultTable';

export default function RiskSimulation({ auth, targets = [] }) {
  const options = [...new Map(targets.map(item => [`${item.disaster_id}:${item.zone_id}`, item])).entries()];
  const [choice, setChoice] = useState('');
  const [risk, setRisk] = useState('3');
  const [result, setResult] = useState(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const selected = options.find(([key]) => key === choice)?.[1] || options[0]?.[1];
  const preview = async event => {
    event.preventDefault(); if (!selected) return;
    setBusy(true); setError(''); setResult(null);
    try {
      setResult(await apiRequest('/api/staff/dispatcher/risk-preview', { token: auth.token, method: 'POST', body: { disaster_id: selected.disaster_id, zone_id: selected.zone_id, risk_level: Number(risk) } }));
    } catch (failure) { setError(failure.message); } finally { setBusy(false); }
  };
  const rows = (result?.preview?.scheduled || []).map(item => {
    const baseline = result.baseline.scheduled.find(original => original.request_id === item.request_id);
    return { request_id: item.request_id, public_ref: item.public_ref, baseline_position: baseline?.queue_position, preview_position: item.queue_position, original_score: baseline?.priority_score, preview_score: item.priority_score };
  });
  return <section className="tool-panel risk-simulation"><h3>Risk-change simulation</h3><p className="text-muted">Preview Priority with Aging using current pending requests. This does not save risk values, scheduling runs, or allocations.</p>
    <form onSubmit={preview}><div className="form-grid-2"><label className="form-label">Disaster / zone<select aria-label="Simulation disaster and zone" className="form-control" value={selected ? `${selected.disaster_id}:${selected.zone_id}` : ''} onChange={event => { setChoice(event.target.value); setResult(null); }}>{options.map(([key, item]) => <option key={key} value={key}>{item.disaster_name || `Disaster ${item.disaster_id}`} / {item.zone_name || `Zone ${item.zone_id}`}</option>)}</select></label><label className="form-label">Preview risk<select aria-label="Preview risk" className="form-control" value={risk} onChange={event => { setRisk(event.target.value); setResult(null); }}>{[1, 2, 3, 4, 5].map(level => <option key={level}>{level}</option>)}</select></label></div><button className="btn btn-outline" type="submit" disabled={!selected || busy}>{busy ? 'Calculating preview…' : 'Preview risk change'}</button></form>
    {!selected && <p className="text-muted">No disaster/zone records are available to preview.</p>}
    {error && <p role="alert" className="text-danger">{error}</p>}
    {result && <div><p role="status">Preview only: {result.affected_requests} pending requests affected. Saved data is unchanged.</p><ResultTable columns={[{ key: 'public_ref', label: 'Request' }, { key: 'baseline_position', label: 'Original position' }, { key: 'preview_position', label: 'Preview position' }, { key: 'original_score', label: 'Original priority' }, { key: 'preview_score', label: 'Preview priority' }]} rows={rows} filename="risk-preview.csv" empty="No pending requests in this scope." /></div>}
  </section>;
}
