import { useEffect, useState } from 'react';
import { apiRequest } from '../../services/api';
import ResultTable from './ResultTable';
import LastUpdated from './LastUpdated';
export default function UtilizationDashboard({ auth }) {
  const [days, setDays] = useState('7');
  const [result, setResult] = useState(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');
  const [updated, setUpdated] = useState(null);
  const [version, setVersion] = useState(0);
  useEffect(() => {
    const controller = new AbortController(); setLoading(true); setError(''); setResult(null);
    apiRequest(`/api/staff/operations/utilization?days=${days}`, { token: auth.token, signal: controller.signal }).then(data => { setResult(data); setUpdated(new Date()); }).catch(failure => { if (failure.name !== 'AbortError') setError(failure.message); }).finally(() => { if (!controller.signal.aborted) setLoading(false); });
    return () => controller.abort();
  }, [auth.token, days, version]);
  const hours = seconds => (Number(seconds) / 3600).toFixed(3);
  return <section className="utilization-dashboard"><h2>Resource utilization</h2><div className="filter-row"><label className="form-label">Reporting window<select aria-label="Utilization reporting window" className="form-control" value={days} onChange={event => setDays(event.target.value)}>{[1, 7, 30, 90].map(n => <option key={n} value={n}>Last {n} days</option>)}</select></label><button className="btn btn-outline" type="button" disabled={loading} onClick={() => setVersion(value => value + 1)}>Refresh utilization</button></div><LastUpdated value={updated} />
    {error && <p role="alert" className="text-danger">{error}</p>}{loading && <p role="status">Loading utilization…</p>}
    {result && !loading && <><p className="text-muted">{result.note}</p><ResultTable filename="resource-utilization.csv" rows={result.resources} columns={[{ key: 'resource_name', label: 'Resource' }, { key: 'resource_type', label: 'Type' }, { key: 'zone_name', label: 'Zone' }, { key: 'status', label: 'Current status' }, { key: 'busy_seconds', label: 'Busy hours', render: hours, exportValue: hours }, { key: 'unallocated_seconds', label: 'Unallocated hours', render: hours, exportValue: hours }, { key: 'allocation_count', label: 'Allocation frequency' }, { key: 'utilization_percent', label: 'Busy %', render: value => `${Number(value).toFixed(2)}%` }]} /><h3>Demand by assistance type and zone</h3><ResultTable filename="zone-demand.csv" rows={result.demand} columns={[{ key: 'zone_name', label: 'Zone' }, { key: 'assistance_type', label: 'Assistance' }, { key: 'request_count', label: 'Requests' }, { key: 'people_affected', label: 'Affected people' }]} empty="No arrivals in this reporting window." /></>}
  </section>;
}
