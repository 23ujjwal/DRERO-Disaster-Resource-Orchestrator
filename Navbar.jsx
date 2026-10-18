export default function Navbar({ currentView, setView, auth, onLogout }) {
  return (
    <header className="navbar">
      <button className="nav-brand" type="button" onClick={() => !auth && setView('home')}>
        <span className="brand-badge">EMERGENCY</span>
        <span>
          <span className="brand-title">DRERO</span>
          <span className="brand-subtitle">Disaster Resource Orchestrator</span>
        </span>
      </button>
      <div className="nav-actions">
        {auth ? (
          <>
            <span className="nav-identity">
              <strong>{auth.user.username}</strong> <span className="badge badge-ready">{auth.user.role}</span>
              {auth.user.zone_id > 0 && <span className="text-muted">Zone {auth.user.zone_id}</span>}
            </span>
            <button className="btn btn-outline" type="button" onClick={onLogout}>Logout</button>
          </>
        ) : (
          <>
            {currentView !== 'home' && <button className="btn btn-outline" type="button" onClick={() => setView('home')}>Home</button>}
            {currentView !== 'victim' && <button className="btn btn-danger" type="button" onClick={() => setView('victim')}>Victim Portal</button>}
            {currentView !== 'login' && <button className="btn btn-primary" type="button" onClick={() => setView('login')}>Staff Login</button>}
          </>
        )}
      </div>
    </header>
  );
}
