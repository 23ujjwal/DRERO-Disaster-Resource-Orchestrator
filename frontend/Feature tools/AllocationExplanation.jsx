import { useEffect, useState } from 'react';
import { apiRequest } from '../../services/api';
export default function AllocationExplanation({ token, requestId, resourceId }) {
  const [result, setResult] = useState(null);
  const [error, setError] = useState('');
  useEffect(() => {
    setResult(null); setError('');
    if (!requestId) return undefined;
    const controller = new AbortController();
    apiRequest('/api/staff/dispatcher/allocation-preview', { token, method: 'POST', body: { request_id: Number(requestId), resource_id: Number(resourceId) }, signal: controller.signal })
      .then(setResult).catch(failure => { if (failure.name !== 'AbortError') setError(failure.message); });
    return () => controller.abort();
  }, [token, requestId, resourceId]);
  if (!requestId) return null;
  return <div className="tool-panel allocation-explanation"><h3>Why this resource?</h3>
    {error ? <p role="alert" className="text-danger">{error}</p> : !result ? <p role="status">Checking compatible inventory…</p> : <>
      {result.resource && <p><strong>{result.resource.resource_name}</strong> · {result.resource.zone_name}</p>}
      <p>{result.explanation}</p>
      {result.checks && <ul className="allocation-checks">{Object.entries(result.checks).map(([key, value]) => <li key={key}>{key}: {value ? 'Passed' : 'Failed'}</li>)}</ul>}
      <p className={result.eligible ? 'text-success' : 'text-danger'}>{result.eligible ? 'Inventory checks passed. Final allocation rechecks request eligibility and locks the resource.' : 'This inventory choice is not currently eligible.'}</p>
    </>}
  </div>;
}
