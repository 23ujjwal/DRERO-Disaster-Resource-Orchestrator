# Incremental features

## Implemented

| Feature | Where to use it | Data / behavior |
| --- | --- | --- |
| Gantt timelines | Dispatcher → Scheduling → run any algorithm or compare all | Each block comes from the scheduler's execution trace. Round Robin emits every quantum slice. Click or focus/Enter a block for exact times. |
| Priority explanations | Request details and Priority result rows → Why this priority? | Severity, independent risk, affected people, aging, simulated wait, and total. Pending requests show an initial score; scheduled rows show contributions at selection time. |
| Interactive resource graph | Safety tab in dispatcher, auditor, and zone views | Search, zoom, select nodes, highlight related edges, or show assignments only. Nodes include holders of active allocations. Compatibility uses type names and capacity, not unrelated IDs. |
| Objective-based recommendations | Dispatcher → Scheduling → Compare all four | Choose waiting, turnaround, response, throughput, or starvation. Ties are reported. Throughput is maximized; the other objectives are minimized. |
| Allocation explanation | Dispatcher → Allocation & resources → Why this resource? | Read-only preview of selected/auto-selected inventory with compatibility, capacity, zone, and availability. Allocation revalidates eligibility and locks rows; successful allocation responses also explain the choice. |
| Citizen request timeline | Citizen portal → Track request | Visual progression built exclusively from persisted request_status_history, including cancellation/deferral and actual timestamps. |
| Risk-change simulation | Dispatcher scheduling; Zone Admin risk management | Computes Priority with Aging twice using current pending data. Shows original/preview queue positions and scores. Does not save risk, runs, logs, allocations, or audit records. Zone admins can preview only their own zone. |
| Persistent staff sessions | Normal staff login/logout | 32-byte random bearer tokens; only SHA-256 digests are saved in MySQL. Sessions survive backend restarts, expire after 8 hours by default, and are revoked on logout. Active account/role/zone is re-read on each request. |
| Hashed passwords | Automatic one-time upgrade on backend/CLI startup | Legacy demo values become independently salted PBKDF2-HMAC-SHA256 hashes with 600,000 iterations. Usernames/passwords do not change. No raw credentials are logged. |
| CSV export and sorting | Scheduling metrics/request tables and auditor reports | Export current sorted rows with quoted fields and spreadsheet-formula neutralization. Select column headings to toggle sort order. Numeric metrics remain numeric for correct decimal sorting. |
| Refresh timestamps | Dispatcher, Zone Admin, Auditor | Last successful data load/refresh time, formatted by the browser. |

The resource graph distinguishes **recorded assignments** from **compatible candidates**.
Candidate links are not confirmed resource waits and are excluded from cycle detection.
The application does not record multi-resource hold-and-wait dependencies, so the
live graph must not be interpreted as proof that MySQL or a broader system cannot
deadlock. The existing low-level RAG cycle detector still detects explicit test
cycles. Banker's remains the separately labelled demonstration state.

Scheduling uses normalized simulated arrivals and durations, not elapsed rescue times.
A Gantt chart visualizes that simulation. Recommendations evaluate a selected metric,
not an emergency-response policy.

## Installation

OpenSSL development headers/libraries are now required (`libssl-dev` on Ubuntu).
The frontend adds no package dependencies.

After the existing schema/routines installation, run this **once as administrator**
from the project root, entering the password only at the client prompt:

```bash
mysql.exe --host=127.0.0.1 --port=3306 --user=root --password --execute="SOURCE database/migrations/20261115_persistent_staff_sessions.sql;"
```

The additive migration creates staff_sessions and grants the existing application
role SELECT/INSERT/UPDATE on that table and UPDATE on users.password_hash only.
It preserves all rescue data. The existing application account host/default role
configuration remains unchanged. Reapplying this migration is safe.

The new backend refuses to start if this security migration is absent. Its first
startup hashes legacy staff passwords in place; later startups do not rehash them.
Do not use schema.sql to upgrade an existing database because it recreates tables.
Fresh installs must also run the session migration to install its restricted grants.

```bash
cmake -S . -B build
cmake --build build
./run.sh
```

`DRERO_SESSION_TTL_SECONDS` optionally overrides session lifetime (1–604800 seconds;
default 28800). Database time controls expiry. Revoked/expired tokens cannot restore
a session. Password verification uses OpenSSL PBKDF2 and constant-time digest
comparison. Parameters follow the [OWASP password-storage guidance](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html).

## API additions (existing response fields remain compatible)

- POST /api/staff/dispatcher/schedule: adds timeline to individual and per-algorithm
  comparison results. Each slice contains request_id, public_ref, start_time, end_time.
- Request JSON adds priority_breakdown (severity, risk, people, aging, waited, total).
- POST /api/staff/dispatcher/risk-preview: dispatcher/zone-admin only; accepts
  disaster_id, zone_id, risk_level. Returns preview_only, affected_requests, baseline,
  and preview. It performs no writes. Zone-admin scope is enforced by the backend.
- POST /api/staff/dispatcher/allocation-preview: dispatcher only; accepts request_id
  and optional resource_id (0 for auto selection). Returns eligible, resource,
  explanation, and checks. This is a snapshot; allocation performs final checks.
- Dashboard/safety JSON adds graph.nodes, graph.edges, and graph.scope. Edge kind
  is assignment, candidate, or an explicitly recorded low-level request edge.
- Login adds expires_in. Tokens remain bearer tokens and the frontend's existing
  restoration/logout paths are preserved. Sessions now reside in MySQL.

## Verification performed

- C++ build: server, CLI, and all test binaries succeeded.
- Existing tests: scheduling 36/36, allocation 15/15, graph/Banker's 14/14,
  concurrency 15/15.
- New core test: 21/21, including execution-trace completeness, RR quantum bounds
  and >400 slices, >200 scheduling requests, correct graph compatibility, explicit
  cycle detection, >400 graph nodes, password hashes, and random token digests.
- Real database session tests: new-connection persistence, revocation, expiry,
  and zero remaining legacy plaintext staff-password rows.
- Real API tests: fresh backend process recognizes an existing login; all four
  scheduler traces/priority breakdowns; snapshot checks proving previews make no
  writes; zone-admin/auditor permission checks; allocation explanations; recorded
  assignment edges; citizen timeline after release; logout invalidation.
- Chromium: actual C++ responses drive all four Gantt charts and priority details;
  objective selection; column sorting; downloaded CSV; risk preview; graph search,
  selection, zoom/filter; allocation preview; citizen timeline; mobile scroll.
- Comparison regression: single-request ties/zeros, multi-request update, all graph
  values, mobile, loading/error/empty states and retry. Session regression: stale
  startup token, real login/restore, mid-session revocation, startup API failure.

Development environment: MySQL 8.4.10, drero_db, 172.24.128.1:3306,
drero_app@172.24.143.176 with drero_application_role active.
Test requests/resources are labelled and cancelled/completed/inactivated after tests;
saved scheduler runs and their audit/history records are retained.

## Repeat checks

```bash
./build/test_incremental_features
# With normal DRERO_DB_* settings already exported:
./build/test_persistent_sessions
python3 tests/test_incremental_api.py
# With Playwright installed outside or inside the repo:
NODE_PATH=/path/to/node_modules node tests/test_incremental_ui.cjs
NODE_PATH=/path/to/node_modules node tests/test_scheduling_comparison.cjs
NODE_PATH=/path/to/node_modules node tests/test_staff_session.cjs
npm --prefix frontend run lint
npm --prefix frontend run build
```

The comparison browser test expects exactly one pending request before creating its
two temporary requests. Run these database tests sequentially on development data.
