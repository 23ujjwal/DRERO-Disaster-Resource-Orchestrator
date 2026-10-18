import { useCallback, useEffect, useMemo, useState } from 'react';
import { apiRequest } from '../services/api';

const transitions = {
  Submitted: ['New', 'Cancelled'],
  New: ['Ready', 'Cancelled'],
  Ready: ['Scheduled', 'Deferred', 'Cancelled'],
  Scheduled: ['Under Review', 'Deferred', 'Cancelled'],
  'Under Review': ['Allocated', 'Deferred', 'Cancelled'],
  Deferred: ['Ready', 'Cancelled'],
};

const schedulerOptions = [
  { key: 'FCFS', label: 'FCFS' },
  { key: 'SJF', label: 'SJF' },
  { key: 'ROUND_ROBIN', label: 'Round Robin' },
  { key: 'PRIORITY', label: 'Priority with Aging' },
];

export default function DispatcherDashboard({ auth }) {
  const [dashboard, setDashboard] = useState(null);
  const [requests, setRequests] = useState([]);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState('');
  const [error, setError] = useState('');
  const [message, setMessage] = useState('');
  const [query, setQuery] = useState('');
  const [statusFilter, setStatusFilter] = useState('All');
  const [severityFilter, setSeverityFilter] = useState('All');
  const [zoneFilter, setZoneFilter] = useState('All');
  const [statusChoices, setStatusChoices] = useState({});
  const [requestId, setRequestId] = useState('');
  const [resourceId, setResourceId] = useState('0');
  const [quantum, setQuantum] = useState('3');
  const [scheduleResult, setScheduleResult] = useState(null);
  const [tab, setTab] = useState('requests');

  const refresh = useCallback(async () => {
    setError('');
    try {
      const [overview, allRequests] = await Promise.all([
        apiRequest('/api/staff/dispatcher/dashboard', { token: auth.token }),
        apiRequest('/api/staff/dispatcher/requests?limit=500', { token: auth.token }),
      ]);
      setDashboard(overview);
      setRequests(allRequests);
      const pending = overview.pending_requests || [];
      setRequestId(current => pending.some(item => String(item.request_id) === current)
        ? current
        : (pending.length ? String(pending[0].request_id) : ''));
      setResourceId(current => overview.available_resources?.some(item => String(item.resource_id) === current)
        ? current
        : '0');
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setLoading(false);
    }
  }, [auth.token]);

  useEffect(() => { refresh(); }, [refresh]);

  const filteredRequests = useMemo(() => requests.filter(request => {
    const matchesSearch = `${request.public_ref} ${request.victim_name} ${request.reported_location}`
      .toLowerCase().includes(query.trim().toLowerCase());
    return matchesSearch
      && (statusFilter === 'All' || request.status === statusFilter)
      && (severityFilter === 'All' || String(request.severity_snapshot) === severityFilter)
      && (zoneFilter === 'All' || String(request.zone_id) === zoneFilter);
  }), [requests, query, statusFilter, severityFilter, zoneFilter]);

  const updateRequestStatus = async request => {
    const status = statusChoices[request.request_id];
    if (!status) return;
    setBusy(`status-${request.request_id}`);
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/dispatcher/request/status', {
        token: auth.token,
        method: 'PATCH',
        body: { request_id: request.request_id, status, remarks: 'Updated by dispatcher' },
      });
      setMessage(result.message);
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  const allocate = async event => {
    event.preventDefault();
    setBusy('allocate');
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/dispatcher/allocate', {
        token: auth.token,
        method: 'POST',
        body: { request_id: Number(requestId), resource_id: Number(resourceId) },
      });
      setMessage(`${result.message} Allocation ID: ${result.allocation_id}.`);
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  const release = async allocation => {
    setBusy(`release-${allocation.allocation_id}`);
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/dispatcher/release', {
        token: auth.token,
        method: 'POST',
        body: { allocation_id: allocation.allocation_id },
      });
      setMessage(result.message);
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  const runScheduler = async algorithm => {
    setBusy(`schedule-${algorithm || 'ALL'}`);
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/dispatcher/schedule', {
        token: auth.token,
        method: 'POST',
        body: algorithm
          ? { algorithm, quantum: Number(quantum) }
          : { quantum: Number(quantum) },
      });
      setScheduleResult(result);
      setMessage(algorithm
        ? `${result.algorithm} completed and saved as run ${result.run_id}.`
        : 'All four algorithms completed and saved.');
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  const allocateScheduledRun = async () => {
    if (!scheduleResult?.run_id) return;
    setBusy('allocate-scheduled');
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/dispatcher/allocate-scheduled', {
        token: auth.token,
        method: 'POST',
        body: { run_id: scheduleResult.run_id },
      });
      setScheduleResult(current => current?.run_id === result.run_id
        ? { ...current, allocation: result.allocation }
        : current);
      setMessage(`${result.algorithm} run ${result.run_id}: allocated ${result.allocation.allocated} of ${result.allocation.attempted} requests in the saved scheduler order.`);
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  if (loading) return <div className="card text-muted">Loading dispatcher data…</div>;

  const activeRequests = dashboard?.pending_requests || [];
  const availableResources = dashboard?.available_resources || [];
  const selectedRequest = activeRequests.find(item => String(item.request_id) === requestId);

  return (
    <section>
      <div className="page-heading">
        <div>
          <h1>Dispatcher dashboard</h1>
          <p className="text-muted">Emergency request queue, backend scheduling, and resource allocation.</p>
        </div>
        <button className="btn btn-outline" type="button" onClick={refresh} disabled={Boolean(busy)}>Refresh</button>
      </div>
      {error && <div className="alert alert-error" role="alert">{error}</div>}
      {message && <div className="alert alert-success" role="status">{message}</div>}

      <div className="stats-grid">
        <div className="stat-box"><div className="stat-value">{activeRequests.length}</div><div className="stat-label">Open requests</div></div>
        <div className="stat-box"><div className="stat-value">{availableResources.length}</div><div className="stat-label">Available resources</div></div>
        <div className="stat-box"><div className="stat-value">{dashboard?.active_allocations?.length || 0}</div><div className="stat-label">Active allocations</div></div>
        <div className="stat-box"><div className="stat-value">{dashboard?.deadlock?.has_cycle ? 'CYCLE' : 'CLEAR'}</div><div className="stat-label">Backend RAG deadlock check</div></div>
      </div>

      <div className="tabs-header" role="tablist" aria-label="Dispatcher tools">
        <button className={`tab-btn ${tab === 'requests' ? 'active' : ''}`} type="button" onClick={() => setTab('requests')}>Requests</button>
        <button className={`tab-btn ${tab === 'scheduling' ? 'active' : ''}`} type="button" onClick={() => setTab('scheduling')}>Scheduling</button>
        <button className={`tab-btn ${tab === 'resources' ? 'active' : ''}`} type="button" onClick={() => setTab('resources')}>Allocation &amp; resources</button>
        <button className={`tab-btn ${tab === 'safety' ? 'active' : ''}`} type="button" onClick={() => setTab('safety')}>Safety status</button>
      </div>

      {tab === 'requests' && (
        <div className="card">
          <h2 className="card-title">Emergency requests ({filteredRequests.length})</h2>
          <div className="filter-row">
            <input className="form-control" aria-label="Search requests" placeholder="Search reference, name, or location" value={query} onChange={event => setQuery(event.target.value)} />
            <select className="form-control" aria-label="Filter by status" value={statusFilter} onChange={event => setStatusFilter(event.target.value)}>
              <option>All</option>{[...new Set(requests.map(item => item.status))].sort().map(status => <option key={status}>{status}</option>)}
            </select>
            <select className="form-control" aria-label="Filter by severity" value={severityFilter} onChange={event => setSeverityFilter(event.target.value)}>
              <option>All</option>{[1, 2, 3, 4, 5].map(level => <option key={level} value={level}>Severity {level}</option>)}
            </select>
            <select className="form-control" aria-label="Filter by zone" value={zoneFilter} onChange={event => setZoneFilter(event.target.value)}>
              <option>All</option>{[...new Map(requests.map(item => [String(item.zone_id), item.zone_name])).entries()].map(([id, name]) => <option key={id} value={id}>{name || `Zone ${id}`}</option>)}
            </select>
          </div>
          <div className="table-responsive">
            <table className="table">
              <thead><tr><th>Reference</th><th>Victim / Location</th><th>Zone</th><th>Severity</th><th>Status</th><th>Actions</th></tr></thead>
              <tbody>
                {filteredRequests.map(request => {
                  const allowed = transitions[request.status] || [];
                  return (
                    <tr key={request.request_id}>
                      <td><strong>{request.public_ref}</strong><div className="text-muted">#{request.request_id}</div></td>
                      <td>
                        <details>
                          <summary>{request.victim_name} — {request.reported_location}</summary>
                          <div className="request-detail">
                            <p>{request.people_affected} people; {request.assistance_type} requested.</p>
                            <p>Disaster severity: {request.severity_snapshot}; current zonal risk: {request.risk_level ?? '—'}.</p>
                            <p>Contact: {request.contact_number || 'Not supplied'}</p>
                            <p>{request.details || 'No additional details.'}</p>
                            <p>{request.disaster_name} / {request.locality_name}, {request.city_name}</p>
                          </div>
                        </details>
                      </td>
                      <td>{request.zone_name || `Zone ${request.zone_id}`}</td>
                      <td><span className={`sev-badge sev-${request.severity_snapshot}`}>{request.severity_snapshot}</span></td>
                      <td><span className="badge badge-scheduled">{request.status}</span></td>
                      <td>
                        {allowed.length > 0 && (
                          <div className="inline-action">
                            <select className="form-control" aria-label={`New status for ${request.public_ref}`} value={statusChoices[request.request_id] || ''} onChange={event => setStatusChoices(current => ({ ...current, [request.request_id]: event.target.value }))}>
                              <option value="">Change status…</option>{allowed.map(next => <option key={next}>{next}</option>)}
                            </select>
                            <button className="btn btn-outline" type="button" disabled={Boolean(busy) || !statusChoices[request.request_id]} onClick={() => updateRequestStatus(request)}>
                              {busy === `status-${request.request_id}` ? 'Saving…' : 'Update'}
                            </button>
                          </div>
                        )}
                      </td>
                    </tr>
                  );
                })}
                {filteredRequests.length === 0 && <tr><td className="text-muted" colSpan="6">No requests match these filters.</td></tr>}
              </tbody>
            </table>
          </div>
        </div>
      )}

      {tab === 'scheduling' && (
        <div className="card">
          <h2 className="card-title">Run an OS scheduling algorithm</h2>
          <p className="text-muted mb-4">Each selection calls the C++ scheduler and saves its run and request metrics through the backend.</p>
          <div className="form-group quantum-control">
            <label className="form-label" htmlFor="quantum">Round Robin quantum</label>
            <input id="quantum" className="form-control" type="number" min="1" max="1000" value={quantum} onChange={event => setQuantum(event.target.value)} />
          </div>
          <div className="button-row">
            {schedulerOptions.map(option => (
              <button key={option.key} className="btn btn-primary" type="button" disabled={Boolean(busy) || Number(quantum) < 1} onClick={() => runScheduler(option.key)}>
                {busy === `schedule-${option.key}` ? 'Running…' : `Run ${option.label}`}
              </button>
            ))}
            <button className="btn btn-secondary" type="button" disabled={Boolean(busy) || Number(quantum) < 1} onClick={() => runScheduler('')}>
              {busy === 'schedule-ALL' ? 'Running…' : 'Compare all four'}
            </button>
          </div>
          {scheduleResult && (
            <div className="result-card">
              {'algorithms' in scheduleResult ? (
                <>
                  <h3>Backend comparison result — best: {scheduleResult.best_algorithm?.name}</h3>
                  <MetricsTable metrics={scheduleResult.comparison || []} />
                  {Object.entries(scheduleResult.algorithms || {}).map(([key, result]) => (
                    <ScheduleTable key={key} title={result.algorithm} requests={result.scheduled} />
                  ))}
                </>
              ) : (
                <>
                  <h3>{scheduleResult.algorithm} result (run {scheduleResult.run_id})</h3>
                  <MetricsTable metrics={[scheduleResult.metrics]} />
                  <ScheduleTable title={`${scheduleResult.algorithm} execution order`} requests={scheduleResult.scheduled} />
                  {!scheduleResult.allocation && (
                    <button className="btn btn-primary" type="button" disabled={Boolean(busy)} onClick={allocateScheduledRun}>
                      {busy === 'allocate-scheduled' ? 'Allocating in saved order…' : 'Allocate requests in this scheduler order'}
                    </button>
                  )}
                  {scheduleResult.allocation && (
                    <>
                      <h3>Allocation results in scheduler order</h3>
                      <p className="text-muted">
                        Allocated {scheduleResult.allocation.allocated}; unallocated {scheduleResult.allocation.unallocated}.
                        Each request was processed in the order returned by the selected backend scheduler.
                      </p>
                      <div className="table-responsive">
                        <table className="table">
                          <thead><tr><th>Position</th><th>Reference</th><th>Result</th><th>Resource</th><th>Reason if unallocated</th></tr></thead>
                          <tbody>
                            {scheduleResult.allocation.results.map(item => (
                              <tr key={item.request_id}>
                                <td>{item.queue_position}</td>
                                <td>{item.public_ref}</td>
                                <td><span className={`badge ${item.status === 'Allocated' ? 'badge-allocated' : 'badge-deferred'}`}>{item.status}</span></td>
                                <td>{item.resource_name || '—'}</td>
                                <td>{item.reason || '—'}</td>
                              </tr>
                            ))}
                            {scheduleResult.allocation.results.length === 0 && <tr><td className="text-muted" colSpan="5">No requests were returned by the scheduler.</td></tr>}
                          </tbody>
                        </table>
                      </div>
                    </>
                  )}
                </>
              )}
            </div>
          )}
        </div>
      )}

      {tab === 'resources' && (
        <>
          <div className="card">
            <h2 className="card-title">Allocate resource</h2>
            <p className="text-muted mb-4">The backend performs allocation with its MySQL transaction and row lock. Auto-pick uses the backend matcher.</p>
            <form onSubmit={allocate}>
              <div className="form-grid-2">
                <div className="form-group">
                  <label className="form-label" htmlFor="allocation-request">Open request *</label>
                  <select id="allocation-request" className="form-control" value={requestId} onChange={event => setRequestId(event.target.value)} required>
                    {activeRequests.map(item => <option key={item.request_id} value={item.request_id}>{item.public_ref} — {item.victim_name}, {item.zone_name}, severity {item.severity_snapshot}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="allocation-resource">Resource</label>
                  <select id="allocation-resource" className="form-control" value={resourceId} onChange={event => setResourceId(event.target.value)}>
                    <option value="0">Auto-pick in backend</option>
                    {availableResources.map(item => <option key={item.resource_id} value={item.resource_id}>#{item.resource_id} {item.resource_name} ({item.resource_type}, {item.zone_name})</option>)}
                  </select>
                </div>
              </div>
              {selectedRequest && <p className="text-muted mb-4">Selected: {selectedRequest.assistance_type}; {selectedRequest.people_affected} affected at {selectedRequest.reported_location}.</p>}
              <button className="btn btn-primary" type="submit" disabled={Boolean(busy) || !requestId}>{busy === 'allocate' ? 'Allocating…' : 'Allocate resource'}</button>
            </form>
          </div>
          <div className="card">
            <h2 className="card-title">Active allocations</h2>
            <SimpleTable headers={['Reference', 'Resource', 'Allocated at', 'Action']} rows={(dashboard?.active_allocations || []).map(item => [
              item.public_ref,
              item.resource_name,
              item.allocated_at,
              <button key="release" className="btn btn-outline" type="button" disabled={Boolean(busy)} onClick={() => release(item)}>{busy === `release-${item.allocation_id}` ? 'Releasing…' : 'Release'}</button>,
            ])} empty="No active allocations." />
          </div>
          <div className="card">
            <h2 className="card-title">Available resource inventory</h2>
            <SimpleTable headers={['Resource', 'Type', 'Zone', 'Capacity']} rows={availableResources.map(item => [
              item.resource_name, item.resource_type, item.zone_name, `${item.capacity} ${item.capacity_unit}`,
            ])} empty="No resources are currently available." />
          </div>
        </>
      )}

      {tab === 'safety' && (
        <div className="card">
          <h2 className="card-title">Backend resource safety</h2>
          <div className={`alert ${dashboard?.deadlock?.has_cycle ? 'alert-error' : 'alert-success'}`}>
            <div><strong>{dashboard?.deadlock?.has_cycle ? 'Cycle detected' : 'No cycle reported'}</strong><p>{dashboard?.deadlock?.description || 'No deadlock result returned.'}</p></div>
          </div>
          <h3>Banker’s algorithm demonstration result</h3>
          <p className="text-muted">{dashboard?.bankers?.reason || 'No safety result returned.'}</p>
          <p className="text-muted">The current backend checks a built-in Banker’s example state; it is separate from live MySQL inventory.</p>
        </div>
      )}
    </section>
  );
}

function MetricsTable({ metrics }) {
  return (
    <div className="table-responsive">
      <table className="table">
        <thead><tr><th>Algorithm</th><th>Requests</th><th>Avg waiting</th><th>Avg turnaround</th><th>Avg response</th><th>Throughput</th><th>Starvation</th></tr></thead>
        <tbody>{metrics.map((item, index) => <tr key={`${item.algorithm}-${index}`}><td>{item.algorithm}</td><td>{item.requests_scheduled}</td><td>{Number(item.avg_waiting_time).toFixed(2)}</td><td>{Number(item.avg_turnaround_time).toFixed(2)}</td><td>{Number(item.avg_response_time).toFixed(2)}</td><td>{Number(item.throughput).toFixed(4)}</td><td>{item.starvation_count}</td></tr>)}</tbody>
      </table>
    </div>
  );
}

function ScheduleTable({ title, requests }) {
  return (
    <>
      <h3>{title}</h3>
      <div className="table-responsive">
        <table className="table">
          <thead><tr><th>Position</th><th>Reference</th><th>Severity</th><th>Zonal risk</th><th>Priority score</th><th>Waiting</th><th>Turnaround</th><th>Response</th></tr></thead>
          <tbody>{requests?.map(item => <tr key={item.request_id}><td>{item.queue_position}</td><td>{item.public_ref}</td><td>{item.severity_snapshot}</td><td>{item.risk_level ?? '—'}</td><td>{Number(item.priority_score || 0).toFixed(2)}</td><td>{Number(item.waiting_time).toFixed(2)}</td><td>{Number(item.turnaround_time).toFixed(2)}</td><td>{Number(item.response_time).toFixed(2)}</td></tr>)}</tbody>
        </table>
      </div>
    </>
  );
}

function SimpleTable({ headers, rows, empty }) {
  return (
    <div className="table-responsive">
      <table className="table">
        <thead><tr>{headers.map(header => <th key={header}>{header}</th>)}</tr></thead>
        <tbody>{rows.length ? rows.map((row, index) => <tr key={index}>{row.map((value, cell) => <td key={cell}>{value}</td>)}</tr>) : <tr><td className="text-muted" colSpan={headers.length}>{empty}</td></tr>}</tbody>
      </table>
    </div>
  );
}
