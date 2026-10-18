import { useEffect, useState } from 'react';
import { apiRequest } from './services/api';
import Navbar from './components/Navbar';
import HomeView from './components/HomeView';
import VictimPortal from './components/VictimPortal';
import StaffLogin from './components/StaffLogin';
import DispatcherDashboard from './components/DispatcherDashboard';
import ZoneAdminDashboard from './components/ZoneAdminDashboard';
import AuditorDashboard from './components/AuditorDashboard';

const dashboardByRole = {
  DISPATCHER: 'dispatcher',
  ZONE_ADMIN: 'zone_admin',
  AUDITOR: 'auditor',
};

export default function App() {
  const [auth, setAuth] = useState(null);
  const [view, setView] = useState('home');
  const [checkingSession, setCheckingSession] = useState(true);
  const [sessionError, setSessionError] = useState('');

  useEffect(() => {
    const token = localStorage.getItem('drero_token');
    if (!token) {
      setCheckingSession(false);
      return undefined;
    }

    const controller = new AbortController();
    apiRequest('/api/auth/me', { token, signal: controller.signal })
      .then(user => {
        if (!dashboardByRole[user.role]) throw new Error('The backend returned an unsupported staff role.');
        const session = { token, user };
        setAuth(session);
        setView(dashboardByRole[user.role]);
      })
      .catch(error => {
        if (controller.signal.aborted) return;
        if (error.status === 401 || error.status === 403) {
          localStorage.removeItem('drero_token');
        } else {
          setSessionError(error.message);
        }
      })
      .finally(() => {
        if (!controller.signal.aborted) setCheckingSession(false);
      });

    return () => controller.abort();
  }, []);

  const handleLoginSuccess = ({ token, user }) => {
    if (!token || !dashboardByRole[user?.role]) {
      throw new Error('The backend returned an invalid staff session.');
    }
    localStorage.setItem('drero_token', token);
    setAuth({ token, user });
    setView(dashboardByRole[user.role]);
  };

  const handleLogout = async () => {
    try {
      await apiRequest('/api/auth/logout', { token: auth?.token, method: 'POST' });
      setSessionError('');
    } catch (error) {
      setSessionError(`Signed out locally, but the backend logout failed: ${error.message}`);
    } finally {
      localStorage.removeItem('drero_token');
      setAuth(null);
      setView('home');
    }
  };

  return (
    <div className="app-container">
      <Navbar currentView={view} setView={setView} auth={auth} onLogout={handleLogout} />
      <main className="main-content">
        {sessionError && <div className="alert alert-error" role="alert">{sessionError}</div>}
        {checkingSession && !auth && <p className="text-muted">Checking saved staff session…</p>}
        {view === 'home' && !auth && <HomeView setView={setView} />}
        {view === 'victim' && !auth && <VictimPortal />}
        {view === 'login' && !auth && <StaffLogin onLoginSuccess={handleLoginSuccess} />}
        {view === 'dispatcher' && auth?.user?.role === 'DISPATCHER' && <DispatcherDashboard auth={auth} />}
        {view === 'zone_admin' && auth?.user?.role === 'ZONE_ADMIN' && <ZoneAdminDashboard auth={auth} />}
        {view === 'auditor' && auth?.user?.role === 'AUDITOR' && <AuditorDashboard auth={auth} />}
      </main>
    </div>
  );
}
