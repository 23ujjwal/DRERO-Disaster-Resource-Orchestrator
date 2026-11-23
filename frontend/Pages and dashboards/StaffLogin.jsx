import { useState } from 'react';
import { apiRequest } from '../services/api';

export default function StaffLogin({ onLoginSuccess }) {
  const [username, setUsername] = useState('');
  const [password, setPassword] = useState('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const handleSubmit = async event => {
    event.preventDefault();
    setLoading(true);
    setError('');
    try {
      const session = await apiRequest('/api/auth/login', {
        method: 'POST',
        body: { username: username.trim(), password },
      });
      onLoginSuccess(session);
    } catch (requestError) {
      setError(requestError.message);
    } finally {
      setLoading(false);
    }
  };

  return (
    <section className="card login-card">
      <h1 className="card-title">Staff Operations Login</h1>
      <p className="text-muted mb-4">Sign in with the staff account created in your DRERO database. Permissions come from the authenticated backend role.</p>
      {error && <div className="alert alert-error" role="alert">{error}</div>}
      <form onSubmit={handleSubmit}>
        <div className="form-group">
          <label className="form-label" htmlFor="staff-username">Username</label>
          <input id="staff-username" className="form-control" autoComplete="username" value={username} onChange={event => setUsername(event.target.value)} required />
        </div>
        <div className="form-group">
          <label className="form-label" htmlFor="staff-password">Password</label>
          <input id="staff-password" className="form-control" type="password" autoComplete="current-password" value={password} onChange={event => setPassword(event.target.value)} required />
        </div>
        <button className="btn btn-primary" type="submit" disabled={loading}>
          {loading ? 'Signing in…' : 'Sign in'}
        </button>
      </form>
    </section>
  );
}
