export default function HomeView({ setView }) {
  return (
    <>
      <section className="hero-section">
        <p className="eyebrow">DISASTER RESOURCE &amp; EMERGENCY RESPONSE ORCHESTRATOR</p>
        <h1 className="hero-title">Coordinating help when every minute matters.</h1>
        <p className="hero-description">
          Submit a request for emergency assistance or sign in to the operations system.
          Scheduling, allocation, and request tracking are handled by the DRERO C++ backend.
        </p>
      </section>
      <div className="role-cards-grid">
        <button className="role-card victim-card" type="button" onClick={() => setView('victim')}>
          <span className="role-card-icon" aria-hidden="true">!</span>
          <span className="role-card-title">Victim / Citizen Portal</span>
          <span className="role-card-desc">Submit an emergency request and follow its status using the reference returned by the system.</span>
          <span className="role-card-btn">Open citizen portal →</span>
        </button>
        <button className="role-card" type="button" onClick={() => setView('login')}>
          <span className="role-card-icon" aria-hidden="true">+</span>
          <span className="role-card-title">Admin / Staff Login</span>
          <span className="role-card-desc">Authenticated dispatcher, zone administrator, and auditor workspaces.</span>
          <span className="role-card-btn">Staff sign in →</span>
        </button>
      </div>
      <p className="text-muted home-note">
        Public request submission and tracking do not grant access to staff tools.
      </p>
    </>
  );
}
