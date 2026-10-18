import { useEffect, useState } from 'react';
import { apiRequest } from '../services/api';

export default function VictimPortal() {
  const [tab, setTab] = useState('submit');
  const [options, setOptions] = useState({ disasters: [], cities: [], assistanceTypes: [] });
  const [localities, setLocalities] = useState([]);
  const [cityId, setCityId] = useState('');
  const [form, setForm] = useState({
    disaster_id: '',
    locality_id: '',
    assistance_type_id: '',
    victim_name: '',
    contact_number: '',
    reported_location: '',
    people_affected: '1',
    details: '',
  });
  const [optionsLoading, setOptionsLoading] = useState(true);
  const [localitiesLoading, setLocalitiesLoading] = useState(false);
  const [submitLoading, setSubmitLoading] = useState(false);
  const [submitError, setSubmitError] = useState('');
  const [submitResult, setSubmitResult] = useState(null);
  const [trackRef, setTrackRef] = useState('');
  const [trackData, setTrackData] = useState(null);
  const [trackLoading, setTrackLoading] = useState(false);
  const [trackError, setTrackError] = useState('');

  useEffect(() => {
    const controller = new AbortController();
    Promise.all([
      apiRequest('/api/public/disasters', { signal: controller.signal }),
      apiRequest('/api/public/cities', { signal: controller.signal }),
      apiRequest('/api/public/assistance-types', { signal: controller.signal }),
    ]).then(([disasters, cities, assistanceTypes]) => {
      setOptions({ disasters, cities, assistanceTypes });
      setCityId(cities.length ? String(cities[0].city_id) : '');
      setForm(current => ({
        ...current,
        disaster_id: disasters.length ? String(disasters[0].disaster_id) : '',
        assistance_type_id: assistanceTypes.length ? String(assistanceTypes[0].assistance_type_id) : '',
      }));
    }).catch(error => {
      if (error.name !== 'AbortError') setSubmitError(error.message);
    }).finally(() => setOptionsLoading(false));
    return () => controller.abort();
  }, []);

  useEffect(() => {
    if (!cityId) {
      setLocalities([]);
      setForm(current => ({ ...current, locality_id: '' }));
      return undefined;
    }
    const controller = new AbortController();
    setLocalitiesLoading(true);
    apiRequest(`/api/public/localities?city_id=${encodeURIComponent(cityId)}`, { signal: controller.signal })
      .then(list => {
        setLocalities(list);
        setForm(current => ({ ...current, locality_id: list.length ? String(list[0].locality_id) : '' }));
      })
      .catch(error => {
        if (error.name !== 'AbortError') setSubmitError(error.message);
      })
      .finally(() => {
        if (!controller.signal.aborted) setLocalitiesLoading(false);
      });
    return () => controller.abort();
  }, [cityId]);

  const updateField = event => {
    const { name, value } = event.target;
    setForm(current => ({ ...current, [name]: value }));
  };

  const handleSubmit = async event => {
    event.preventDefault();
    setSubmitLoading(true);
    setSubmitError('');
    setSubmitResult(null);
    try {
      const result = await apiRequest('/api/public/requests', {
        method: 'POST',
        body: {
          ...form,
          disaster_id: Number(form.disaster_id),
          locality_id: Number(form.locality_id),
          assistance_type_id: Number(form.assistance_type_id),
          people_affected: Number(form.people_affected),
        },
      });
      setSubmitResult(result);
      setTrackRef(result.public_ref);
      setForm(current => ({
        ...current,
        victim_name: '',
        contact_number: '',
        reported_location: '',
        people_affected: '1',
        details: '',
      }));
    } catch (error) {
      setSubmitError(error.message);
    } finally {
      setSubmitLoading(false);
    }
  };

  const handleTrack = async event => {
    event.preventDefault();
    setTrackLoading(true);
    setTrackError('');
    setTrackData(null);
    try {
      const result = await apiRequest(`/api/public/track?ref=${encodeURIComponent(trackRef.trim())}`);
      setTrackData(result);
    } catch (error) {
      setTrackError(error.message);
    } finally {
      setTrackLoading(false);
    }
  };

  const canSubmit = !optionsLoading && !localitiesLoading
    && options.disasters.length > 0 && options.assistanceTypes.length > 0 && localities.length > 0;

  return (
    <section className="portal">
      <div className="tabs-header" role="tablist" aria-label="Citizen portal">
        <button className={`tab-btn ${tab === 'submit' ? 'active' : ''}`} type="button" onClick={() => setTab('submit')}>Submit request</button>
        <button className={`tab-btn ${tab === 'track' ? 'active' : ''}`} type="button" onClick={() => setTab('track')}>Track request</button>
      </div>

      {tab === 'submit' && (
        <div className="card">
          <h1 className="card-title">Emergency assistance request</h1>
          <p className="text-muted mb-4">Fields marked * are required. Your tracking reference will be returned by the backend after the request is saved.</p>
          {submitError && <div className="alert alert-error" role="alert">{submitError}</div>}
          {submitResult && (
            <div className="alert alert-success" role="status">
              <div>
                Request submitted. Reference: <strong>{submitResult.public_ref}</strong>
                <div>{submitResult.message} Status: {submitResult.status}; severity: {submitResult.severity_snapshot}; zone: {submitResult.zone_name} ({submitResult.city_name}).</div>
              </div>
              <button className="btn btn-secondary" type="button" onClick={() => setTab('track')}>Track this request</button>
            </div>
          )}
          {optionsLoading ? <p className="text-muted">Loading disaster and assistance options…</p> : (
            <form onSubmit={handleSubmit}>
              {!canSubmit && <div className="alert alert-warning">No active disaster, city locality, or assistance options are available. Contact the DRERO administrator.</div>}
              <div className="form-grid-2">
                <div className="form-group">
                  <label className="form-label" htmlFor="disaster">Active disaster *</label>
                  <select id="disaster" name="disaster_id" className="form-control" value={form.disaster_id} onChange={updateField} required>
                    {options.disasters.map(item => <option key={item.disaster_id} value={item.disaster_id}>{item.disaster_name} ({item.disaster_type})</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="city">City *</label>
                  <select id="city" className="form-control" value={cityId} onChange={event => setCityId(event.target.value)} required>
                    {options.cities.map(item => <option key={item.city_id} value={item.city_id}>{item.city_name}, {item.state_name}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="locality">Zone / locality *</label>
                  <select id="locality" name="locality_id" className="form-control" value={form.locality_id} onChange={updateField} disabled={localitiesLoading || !localities.length} required>
                    {localities.map(item => <option key={item.locality_id} value={item.locality_id}>{item.locality_name} (Zone {item.zone_id})</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="assistance">Assistance type *</label>
                  <select id="assistance" name="assistance_type_id" className="form-control" value={form.assistance_type_id} onChange={updateField} required>
                    {options.assistanceTypes.map(item => <option key={item.assistance_type_id} value={item.assistance_type_id}>{item.type_name}</option>)}
                  </select>
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="victim-name">Name *</label>
                  <input id="victim-name" name="victim_name" className="form-control" value={form.victim_name} onChange={updateField} maxLength="150" autoComplete="name" required />
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="contact-number">Contact phone *</label>
                  <input id="contact-number" name="contact_number" type="tel" className="form-control" value={form.contact_number} onChange={updateField} maxLength="20" autoComplete="tel" required />
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="location">Exact location / landmark *</label>
                  <input id="location" name="reported_location" className="form-control" value={form.reported_location} onChange={updateField} maxLength="255" required />
                </div>
                <div className="form-group">
                  <label className="form-label" htmlFor="people">People affected *</label>
                  <input id="people" name="people_affected" type="number" min="1" max="65535" className="form-control" value={form.people_affected} onChange={updateField} required />
                </div>
              </div>
              <div className="form-group">
                <label className="form-label" htmlFor="details">Situation details</label>
                <textarea id="details" name="details" className="form-control" rows="3" value={form.details} onChange={updateField} />
              </div>
              <button className="btn btn-danger" type="submit" disabled={!canSubmit || submitLoading}>
                {submitLoading ? 'Submitting…' : 'Submit emergency request'}
              </button>
            </form>
          )}
        </div>
      )}

      {tab === 'track' && (
        <div className="card">
          <h1 className="card-title">Track emergency request</h1>
          <p className="text-muted mb-4">Enter the tracking reference returned when the request was submitted.</p>
          <form className="track-form" onSubmit={handleTrack}>
            <input className="form-control" aria-label="Request reference" value={trackRef} onChange={event => setTrackRef(event.target.value)} placeholder="e.g. REQ-12345-261017" required />
            <button className="btn btn-primary" type="submit" disabled={trackLoading}>{trackLoading ? 'Searching…' : 'Track'}</button>
          </form>
          {trackError && <div className="alert alert-error" role="alert">{trackError}</div>}
          {trackData && (
            <div className="card result-card">
              <h2>{trackData.public_ref} <span className="badge badge-scheduled">{trackData.status}</span></h2>
              <dl className="details-grid">
                <dt>Name</dt><dd>{trackData.victim_name}</dd>
                <dt>Location</dt><dd>{trackData.reported_location}, {trackData.locality_name}, {trackData.zone_name}</dd>
                <dt>Disaster</dt><dd>{trackData.disaster_name}</dd>
                <dt>Assistance</dt><dd>{trackData.assistance_type}; {trackData.people_affected} affected</dd>
                <dt>Severity</dt><dd>{trackData.severity_snapshot} / 5</dd>
              </dl>
              <h3>Status history</h3>
              {!trackData.status_history?.length ? <p className="text-muted">No status history recorded.</p> : (
                <ul className="history-list">
                  {trackData.status_history.map((item, index) => <li key={`${item.changed_at}-${index}`}>{item.changed_at}: {item.old_status} → <strong>{item.new_status}</strong>{item.remarks ? ` — ${item.remarks}` : ''}</li>)}
                </ul>
              )}
              <h3>Resource assignments</h3>
              {!trackData.allocations?.length ? <p className="text-muted">No resources assigned yet.</p> : (
                <ul className="history-list">
                  {trackData.allocations.map(item => <li key={item.allocation_id}>{item.resource_name} — {item.status} ({item.allocated_at})</li>)}
                </ul>
              )}
            </div>
          )}
        </div>
      )}
    </section>
  );
}
