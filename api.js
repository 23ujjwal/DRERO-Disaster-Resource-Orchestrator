const apiBaseUrl = (import.meta.env.VITE_API_BASE_URL || '').replace(/\/$/, '');

export class ApiError extends Error {
  constructor(message, status = 0) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
  }
}

export async function apiRequest(path, { token, method = 'GET', body, signal } = {}) {
  const headers = { Accept: 'application/json' };
  if (body !== undefined) headers['Content-Type'] = 'application/json';
  if (token) headers.Authorization = `Bearer ${token}`;

  let response;
  try {
    response = await fetch(`${apiBaseUrl}${path}`, {
      method,
      headers,
      body: body === undefined ? undefined : JSON.stringify(body),
      signal,
    });
  } catch (error) {
    if (error.name === 'AbortError') throw error;
    throw new ApiError('Cannot reach the DRERO backend. Check that the C++ server is running and the API URL is correct.');
  }

  const contentType = response.headers.get('content-type') || '';
  let data = null;
  if (contentType.includes('application/json')) {
    try {
      data = await response.json();
    } catch {
      throw new ApiError('The backend returned invalid JSON.', response.status);
    }
  } else if (response.status !== 204) {
    const text = await response.text();
    if (text) data = { message: text };
  }

  if (!response.ok) {
    const fallback = response.status >= 500
      ? `DRERO backend returned HTTP ${response.status}. It may be offline or unable to reach MySQL; check the C++ server logs.`
      : `Backend request failed (${response.status}).`;
    throw new ApiError(data?.error || data?.message || fallback, response.status);
  }
  return data;
}
