import { useCallback, useEffect, useMemo, useState } from 'react';
import { apiRequest } from '../services/api';

export default function ZoneAdminDashboard({ auth }) {
  const [data, setData] = useState(null);
  const [safety, setSafety] = useState(null);
  const [loading, setLoading] = useState(true);
  const [busy, setBusy] = useState('');
  const [error, setError] = useState('');
  const [message, setMessage] = useState('');
  const [zoneFilter, setZoneFilter] = useState('All');
  const [typeFilter, setTypeFilter] = useState('All');
  const [availabilityFilter, setAvailabilityFilter] = useState('All');
  const [resourceName, setResourceName] = useState('');
  const [typeId, setTypeId] = useState('');
  const [capacity, setCapacity] = useState('10');
  const [capacityUnit, setCapacityUnit] = useState('people');
  const [notes, setNotes] = useState('');
  const [selectedResourceId, setSelectedResourceId] = useState('');
  const [newStatus, setNewStatus] = useState('Available');
  const [remarks, setRemarks] = useState('');
  const [riskLevels, setRiskLevels] = useState({});
  const [riskDescriptions, setRiskDescriptions] = useState({});

  const refresh = useCallback(async () => {
    setError('');
    try {
      const [zoneData, deadlock] = await Promise.all([
        apiRequest('/api/staff/zone/data', { token: auth.token }),
        apiRequest('/api/staff/dispatcher/deadlock', { token: auth.token }),
      ]);
      setData(zoneData);
      setSafety(deadlock);
      setRiskLevels(Object.fromEntries((zoneData.zone_risks || []).map(item => [item.disaster_id, String(item.risk_level)])));
      setRiskDescriptions(Object.fromEntries((zoneData.zone_risks || []).map(item => [item.disaster_id, item.risk_description || ''])));
      setSelectedResourceId(current => zoneData.resources.some(item => String(item.resource_id) === current)
        ? current
        : (zoneData.resources.length ? String(zoneData.resources[0].resource_id) : ''));
      setTypeId(current => current || (zoneData.resource_types.length ? String(zoneData.resource_types[0].id) : ''));
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setLoading(false);
    }
  }, [auth.token]);

  useEffect(() => { refresh(); }, [refresh]);

  const resources = data?.resources || [];
  const filteredResources = useMemo(() => (data?.resources || []).filter(item =>
    (zoneFilter === 'All' || String(item.zone_id) === zoneFilter)
    && (typeFilter === 'All' || String(item.resource_type_id) === typeFilter)
    && (availabilityFilter === 'All' || item.status === availabilityFilter)
  ), [data?.resources, zoneFilter, typeFilter, availabilityFilter]);

  const handleRegister = async event => {
    event.preventDefault();
    setBusy('register');
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/zone/resource', {
        token: auth.token,
        method: 'POST',
        body: {
          resource_name: resourceName.trim(),
          resource_type_id: Number(typeId),
          capacity: Number(capacity),
          capacity_unit: capacityUnit,
          notes: notes.trim(),
        },
      });
      setMessage(`${result.message} Resource ID: ${result.resource_id}.`);
      setResourceName('');
      setNotes('');
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  const handleStatusUpdate = async event => {
    event.preventDefault();
    setBusy('status');
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/zone/resource/status', {
        token: auth.token,
        method: 'PATCH',
        body: {
          resource_id: Number(selectedResourceId),
          status: newStatus,
          remarks: remarks.trim() || 'Updated by Zone Administrator',
        },
      });
      setMessage(result.message);
      setRemarks('');
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  const handleRiskUpdate = async (event, risk) => {
    event.preventDefault();
    setBusy(`risk-${risk.disaster_id}`);
    setError('');
    setMessage('');
    try {
      const result = await apiRequest('/api/staff/zone/risk', {
        token: auth.token,
        method: 'PATCH',
        body: {
          disaster_id: risk.disaster_id,
          risk_level: Number(riskLevels[risk.disaster_id] || risk.risk_level),
          risk_description: (riskDescriptions[risk.disaster_id] || '').trim(),
        },
      });
      setMessage(result.message);
      await refresh();
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setBusy('');
    }
  };

  if (loading) return <div className="card text-muted">Loading zone inventory…</div>;

  return (
    <section>
      <div className="page-heading">
        <div>
          <h1>Zone administrator</h1>
          <p className="text-muted">Manage the resource inventory assigned to your zone.</p>
        </div>
        <button className="btn btn-outline" type="button" onClick={refresh} disabled={Boolean(busy)}>Refresh</button>
      </div>
      {error && <div className="alert alert-error" role="alert">{error}</div>}
      {message && <div className="alert alert-success" role="status">{message}</div>}

      <div className="stats-grid">
        <div className="stat-box"><div className="stat-value">{data?.zone_id ? `Zone ${data.zone_id}` : 'Unassigned'}</div><div className="stat-label">Assigned zone</div></div>
        <div className="stat-box"><div className="stat-value">{resources.length}</div><div className="stat-label">Inventory items</div></div>
        <div className="stat-box"><div className="stat-value">{resources.filter(item => item.status === 'Available').length}</div><div className="stat-label">Available</div></div>
        <div className="stat-box"><div className="stat-value">{data?.requests?.length || 0}</div><div className="stat-label">Recent zone requests</div></div>
      </div>

      <div className="card">
        <h2 className="card-title">Zone risk management</h2>
        <p className="text-muted">Risk is a separate 1–5 scheduling input from disaster severity. Changes apply to pending requests the next time scheduling runs; completed scheduling history is not rewritten.</p>
        <div className="table-responsive">
          <table className="table">
            <thead><tr><th>Disaster</th><th>Zone</th><th>Severity</th><th>Current risk</th><th>Risk description</th><th>Last updated</th><th>Action</th></tr></thead>
            <tbody>
              {(data?.zone_risks || []).map(item => (
                <tr key={`${item.disaster_id}-${item.zone_id}`}>
                  <td>{item.disaster_name}</td>
                  <td>{item.zone_name}</td>
                  <td>{item.severity_level}</td>
                  <td>
                    <select
                      className="form-control"
                      aria-label={`Risk level for ${item.disaster_name}`}
                      form={`risk-form-${item.disaster_id}`}
                      value={riskLevels[item.disaster_id] ?? String(item.risk_level)}
                      onChange={event => setRiskLevels(current => ({ ...current, [item.disaster_id]: event.target.value }))}
                    >
                      {[1, 2, 3, 4, 5].map(level => <option key={level} value={level}>{level}</option>)}
                    </select>
                  </td>
                  <td>
                    <input
                      className="form-control"
                      aria-label={`Risk description for ${item.disaster_name}`}
                      form={`risk-form-${item.disaster_id}`}
                      maxLength="255"
                      value={riskDescriptions[item.disaster_id] ?? item.risk_description ?? ''}
                      onChange={event => setRiskDescriptions(current => ({ ...current, [item.disaster_id]: event.target.value }))}
                    />
                  </td>
                  <td>{item.updated_at || '—'}</td>
                  <td>
                    <form id={`risk-form-${item.disaster_id}`} onSubmit={event => handleRiskUpdate(event, item)}>
                      <button className="btn btn-primary" type="submit" disabled={Boolean(busy)}>{busy === `risk-${item.disaster_id}` ? 'Saving…' : 'Save risk'}</button>
                    </form>
                  </td>
                </tr>
              ))}
              {!data?.zone_risks?.length && <tr><td colSpan="7" className="text-muted">No risk records are available for your assigned zone.</td></tr>}
            </tbody>
          </table>
        </div>
      </div>

      <div className="form-grid-2">
        <div className="card">
          <h2 className="card-title">Register resource</h2>
          <form onSubmit={handleRegister}>
            <div className="form-group">
              <label className="form-label" htmlFor="resource-name">Resource name *</label>
              <input id="resource-name" className="form-control" value={resourceName} onChange={event => setResourceName(event.target.value)} required />
            </div>
            <div className="form-grid-2">
              <div className="form-group">
                <label className="form-label" htmlFor="resource-type">Type *</label>
                <select id="resource-type" className="form-control" value={typeId} onChange={event => setTypeId(event.target.value)} required>
                  {(data?.resource_types || []).map(item => <option key={item.id} value={item.id}>{item.name}</option>)}
                </select>
              </div>
              <div className="form-group">
                <label className="form-label" htmlFor="resource-capacity">Capacity *</label>
                <input id="resource-capacity" className="form-control" type="number" min="1" max="65535" value={capacity} onChange={event => setCapacity(event.target.value)} required />
              </div>
            </div>
            <div className="form-grid-2">
              <div className="form-group">
                <label className="form-label" htmlFor="capacity-unit">Capacity unit</label>
                <input id="capacity-unit" className="form-control" value={capacityUnit} onChange={event => setCapacityUnit(event.target.value)} maxLength="30" />
              </div>
              <div className="form-group">
                <label className="form-label" htmlFor="resource-notes">Notes</label>
                <input id="resource-notes" className="form-control" value={notes} onChange={event => setNotes(event.target.value)} />
              </div>
            </div>
            <p className="text-muted mb-4">New resource is created in your assigned zone by the backend.</p>
            <button className="btn btn-primary" type="submit" disabled={Boolean(busy) || !typeId}>{busy === 'register' ? 'Registering…' : 'Register resource'}</button>
          </form>
        </div>

        <div className="card">
          <h2 className="card-title">Update resource status</h2>
          <form onSubmit={handleStatusUpdate}>
            <div className="form-group">
              <label className="form-label" htmlFor="status-resource">Resource *</label>
              <select id="status-resource" className="form-control" value={selectedResourceId} onChange={event => setSelectedResourceId(event.target.value)} required>
                {resources.map(item => <option key={item.resource_id} value={item.resource_id}>#{item.resource_id} {item.resource_name} ({item.status})</option>)}
              </select>
            </div>
            <div className="form-group">
              <label className="form-label" htmlFor="resource-status">New status *</label>
              <select id="resource-status" className="form-control" value={newStatus} onChange={event => setNewStatus(event.target.value)}>
                <option>Available</option><option>Maintenance</option><option>Inactive</option>
              </select>
            </div>
            <div className="form-group">
              <label className="form-label" htmlFor="status-remarks">Remarks</label>
              <input id="status-remarks" className="form-control" value={remarks} onChange={event => setRemarks(event.target.value)} />
            </div>
            <button className="btn btn-primary" type="submit" disabled={Boolean(busy) || !selectedResourceId}>{busy === 'status' ? 'Updating…' : 'Update status'}</button>
          </form>
        </div>
      </div>

      <div className="card">
        <h2 className="card-title">Resource inventory ({filteredResources.length})</h2>
        <div className="filter-row">
          <select className="form-control" aria-label="Filter by zone" value={zoneFilter} onChange={event => setZoneFilter(event.target.value)}>
            <option value="All">All zones</option>
            {[...new Map(resources.map(item => [String(item.zone_id), item.zone_name])).entries()].map(([id, name]) => <option key={id} value={id}>{name || `Zone ${id}`}</option>)}
          </select>
          <select className="form-control" aria-label="Filter by resource type" value={typeFilter} onChange={event => setTypeFilter(event.target.value)}>
            <option value="All">All types</option>{(data?.resource_types || []).map(item => <option key={item.id} value={item.id}>{item.name}</option>)}
          </select>
          <select className="form-control" aria-label="Filter by availability" value={availabilityFilter} onChange={event => setAvailabilityFilter(event.target.value)}>
            <option value="All">All statuses</option><option>Available</option><option>Allocated</option><option>Maintenance</option><option>Inactive</option>
          </select>
        </div>
        <div className="table-responsive">
          <table className="table">
            <thead><tr><th>Name</th><th>Type</th><th>Zone</th><th>Capacity</th><th>Status</th><th>Notes</th></tr></thead>
            <tbody>
              {filteredResources.map(item => <tr key={item.resource_id}><td>{item.resource_name}</td><td>{item.resource_type}</td><td>{item.zone_name || item.zone_id}</td><td>{item.capacity} {item.capacity_unit}</td><td><span className="badge badge-scheduled">{item.status}</span></td><td>{item.notes || '—'}</td></tr>)}
              {!filteredResources.length && <tr><td colSpan="6" className="text-muted">No resources match these filters.</td></tr>}
            </tbody>
          </table>
        </div>
      </div>

      <div className="card">
        <h2 className="card-title">Zone emergency requests</h2>
        <div className="table-responsive">
          <table className="table">
            <thead><tr><th>Reference</th><th>Locality</th><th>Severity</th><th>Assistance</th><th>People</th><th>Status</th></tr></thead>
            <tbody>
              {(data?.requests || []).map(item => <tr key={item.request_id}><td>{item.public_ref}</td><td>{item.locality_name}</td><td>{item.severity_snapshot}</td><td>{item.assistance_type}</td><td>{item.people_affected}</td><td>{item.status}</td></tr>)}
              {!data?.requests?.length && <tr><td colSpan="6" className="text-muted">No recent requests in this zone.</td></tr>}
            </tbody>
          </table>
        </div>
      </div>

      <div className="card">
        <h2 className="card-title">System resource-safety result</h2>
        <p className={safety?.deadlock?.has_cycle ? 'text-danger' : 'text-success'}>{safety?.deadlock?.description || 'No deadlock result returned.'}</p>
        <p className="text-muted">{safety?.bankers?.reason || 'No Banker’s algorithm result returned.'}</p>
        <p className="text-muted">The backend’s Banker’s result uses its built-in demonstration state; the RAG check uses current backend request and allocation data.</p>
      </div>
    </section>
  );
}
