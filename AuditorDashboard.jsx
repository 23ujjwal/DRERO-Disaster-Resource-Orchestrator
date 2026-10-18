import { useCallback, useEffect, useState } from 'react';
import { apiRequest } from '../services/api';

export default function AuditorDashboard({ auth }) {
  const [data, setData] = useState({ audit: [], scheduling: [], allocations: [], safety: null });
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState('');
  const [tab, setTab] = useState('audit');

  const refresh = useCallback(async () => {
    setError('');
    try {
      const [audit, scheduling, allocations, safety] = await Promise.all([
        apiRequest('/api/staff/auditor/audit-log?limit=100', { token: auth.token }),
        apiRequest('/api/staff/auditor/scheduling-history', { token: auth.token }),
        apiRequest('/api/staff/auditor/allocations?limit=200', { token: auth.token }),
        apiRequest('/api/staff/dispatcher/deadlock', { token: auth.token }),
      ]);
      setData({ audit, scheduling, allocations, safety });
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setLoading(false);
    }
  }, [auth.token]);

  useEffect(() => { refresh(); }, [refresh]);

  if (loading) return <div className="card text-muted">Loading audit records…</div>;

  return (
    <section>
      <div className="page-heading">
        <div>
          <h1>Auditor view</h1>
          <p className="text-muted">Read-only system records from the DRERO backend.</p>
        </div>
        <button className="btn btn-outline" type="button" onClick={refresh}>Refresh records</button>
      </div>
      {error && <div className="alert alert-error" role="alert">{error}</div>}
      <div className="stats-grid">
        <div className="stat-box"><div className="stat-value">{data.audit.length}</div><div className="stat-label">Audit entries loaded</div></div>
        <div className="stat-box"><div className="stat-value">{data.scheduling.length}</div><div className="stat-label">Scheduling runs</div></div>
        <div className="stat-box"><div className="stat-value">{data.allocations.length}</div><div className="stat-label">Allocation records</div></div>
        <div className="stat-box"><div className="stat-value">{data.safety?.deadlock?.has_cycle ? 'CYCLE' : 'CLEAR'}</div><div className="stat-label">Backend RAG result</div></div>
      </div>
      <div className="tabs-header" role="tablist" aria-label="Auditor records">
        <button className={`tab-btn ${tab === 'audit' ? 'active' : ''}`} type="button" onClick={() => setTab('audit')}>Audit log</button>
        <button className={`tab-btn ${tab === 'scheduling' ? 'active' : ''}`} type="button" onClick={() => setTab('scheduling')}>Scheduling history</button>
        <button className={`tab-btn ${tab === 'allocations' ? 'active' : ''}`} type="button" onClick={() => setTab('allocations')}>Allocation history</button>
        <button className={`tab-btn ${tab === 'safety' ? 'active' : ''}`} type="button" onClick={() => setTab('safety')}>Safety status</button>
      </div>

      {tab === 'audit' && (
        <div className="card">
          <h2 className="card-title">Audit log</h2>
          <ReadOnlyTable
            headers={['ID', 'Actor', 'Action', 'Entity', 'Entity ID', 'Time', 'Details']}
            rows={data.audit.map(item => [item.audit_id, item.actor, item.action, item.entity_type, item.entity_id, item.action_time, item.details])}
            empty="No audit records returned."
          />
        </div>
      )}
      {tab === 'scheduling' && (
        <div className="card">
          <h2 className="card-title">Scheduling history and metrics</h2>
          <ReadOnlyTable
            headers={['Algorithm', 'Requests', 'Avg waiting', 'Avg turnaround', 'Avg response', 'Throughput', 'Starvation']}
            rows={data.scheduling.map(item => [
              item.algorithm, item.requests_scheduled, Number(item.avg_waiting_time).toFixed(2),
              Number(item.avg_turnaround_time).toFixed(2), Number(item.avg_response_time).toFixed(2),
              Number(item.throughput).toFixed(4), item.starvation_count,
            ])}
            empty="No completed scheduler runs returned."
          />
        </div>
      )}
      {tab === 'allocations' && (
        <div className="card">
          <h2 className="card-title">Allocation history</h2>
          <ReadOnlyTable
            headers={['ID', 'Request reference', 'Resource', 'Status', 'Allocated', 'Released', 'Remarks']}
            rows={data.allocations.map(item => [
              item.allocation_id, item.public_ref, item.resource_name, item.status,
              item.allocated_at, item.released_at || '—', item.remarks || '—',
            ])}
            empty="No allocation records returned."
          />
        </div>
      )}
      {tab === 'safety' && (
        <div className="card">
          <h2 className="card-title">Backend safety status</h2>
          <p className={data.safety?.deadlock?.has_cycle ? 'text-danger' : 'text-success'}>{data.safety?.deadlock?.description || 'No deadlock result returned.'}</p>
          <p>{data.safety?.bankers?.reason || 'No Banker’s algorithm result returned.'}</p>
          <p className="text-muted">Banker’s Algorithm uses the backend’s built-in demonstration state. This page has no modification actions.</p>
        </div>
      )}
    </section>
  );
}

function ReadOnlyTable({ headers, rows, empty }) {
  return (
    <div className="table-responsive">
      <table className="table">
        <thead><tr>{headers.map(header => <th key={header}>{header}</th>)}</tr></thead>
        <tbody>
          {rows.length
            ? rows.map((row, index) => <tr key={index}>{row.map((value, column) => <td key={column}>{value ?? '—'}</td>)}</tr>)
            : <tr><td className="text-muted" colSpan={headers.length}>{empty}</td></tr>}
        </tbody>
      </table>
    </div>
  );
}
