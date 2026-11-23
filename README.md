# DRERO

Disaster Resource & Emergency Response Orchestrator is an academic project with a React/Vite frontend, a C++ REST API, and a MySQL database. The frontend talks only to the HTTP API; the C++ backend and the existing CLI use MySQL.

## Components

- `frontend/` — React 18 and Vite UI for public requests and authenticated staff views.
- `backend/src/server.cpp` — C++ REST API on port 8080 by default.
- `core/` — shared scheduling, database, resource management, and RAG/Banker's implementations.
- `cli/` — existing terminal client; it remains available independently of the frontend.
- `database/` — MySQL schema, routines/triggers/roles, demo data, and verification queries.

See the [database module map](docs/database-modules.md) for the five database implementation files and their responsibilities.
See the [backend module map](docs/backend-modules.md) for the API entry point and feature route modules.

## One-command website startup (WSL/Linux)

After installing the project dependencies and preparing MySQL as described below, run this from the project root:

```bash
./run.sh
```

The launcher builds the C++ API, prompts for the MySQL application-account password if `DRERO_DB_PASSWORD` is not already set, then starts the API and Vite frontend together. Open `http://localhost:3000`; press Ctrl+C in that terminal to stop both services. Set `DRERO_DB_HOST`, `DRERO_DB_PORT`, `DRERO_DB_USER`, and `DRERO_DB_NAME` in the same shell first only if your MySQL settings differ from the project defaults. Run `cd frontend && npm install` once if frontend dependencies have not been installed.

This starts the **website and API**, not the interactive CLI. To run the existing terminal CLI by itself, build it with `cmake --build build --target drero_cli` and run `./build/drero_cli` in a shell with the database settings configured.

## Windows setup

The C++ sources use GCC/POSIX facilities (`bits/stdc++.h`, pthreads, and Linux-style include/library paths). For Windows, run the backend and CLI in WSL 2 (Ubuntu) with the build dependencies installed there. MySQL 8.4 may run in Windows; configure the WSL-to-Windows database host as described below. The frontend can run either in Windows PowerShell or WSL.

### 1. Prepare MySQL 8.4

For a new classroom database only, create `drero_db`, then run `database/schema.sql`: it drops/recreates the project tables and must not be run against data you need to retain. In MySQL Workbench connected as a database administrator, execute:

1. `CREATE DATABASE IF NOT EXISTS drero_db CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci;`
2. Select `drero_db`, then execute `database/schema.sql`.
3. Create the application account with `CREATE USER IF NOT EXISTS 'drero_app'@'%' IDENTIFIED BY '<choose-a-private-password>';`
4. `database/seed.sql`
5. `database/schema_routines.sql` (creates/assigns the least-privilege application role, installs routines/triggers, and backfills the seeded scheduling runs).
6. Optionally, `database/verify.sql` to check seeded tables and views.

For an existing database, apply `database/migrations/20261018_add_zone_risk_level.sql` once if it has not already been applied. It initializes each new risk value from the prior severity value; later edits are independent of severity. Then use the repeatable feature migration described in [Database feature installation and verification](#database-feature-installation-and-verification).

Alternatively, from PowerShell with the MySQL client installed, from the project root:

```powershell
mysql.exe -u root -p -e "CREATE DATABASE IF NOT EXISTS drero_db CHARACTER SET utf8mb4 COLLATE utf8mb4_0900_ai_ci"
mysql.exe -u root -p drero_db < database/schema.sql
mysql.exe -u root -p drero_db < database/seed.sql
mysql.exe -u root -p < database/schema_routines.sql
mysql.exe -u root -p drero_db < database/verify.sql
```

Create the `drero_app` MySQL account in Workbench with a private password before running `schema_routines.sql`; the script grants it `drero_application_role` and sets it as the account's default role. Do not add a direct database-wide grant: the role contains the application's table-level privileges and routine execution rights. If you use an account host other than `%`, change the matching account host in `schema_routines.sql` before applying it.

Restrict the account host to the backend machine's address where possible. Set the database connection in the environment; do not put credentials in frontend `.env` files or source control. The seed data is for a classroom demonstration. After the security migration, first startup upgrades its legacy password values to salted hashes.

### 2. Build and run the C++ backend in WSL

Install CMake, a C++17 compiler, MySQL Connector/C++, cpp-httplib, nlohmann/json, and OpenSSL development headers (`libssl-dev`) in WSL. From the project directory in the WSL shell:

```bash
cmake -S . -B build
cmake --build build --target drero_server drero_cli
```

For the MySQL 8.4 server currently reachable from this workspace, the verified MySQL account is `drero_app` (not MySQL's `root` account); `root` with the supplied password is denied from the WSL client host. The connection has been confirmed against `drero_db` on MySQL 8.4.10. Use the password configured for `drero_app` locally.

For a MySQL server running inside WSL, use `127.0.0.1` as the host. For MySQL running on Windows, set `DRERO_DB_HOST` to the Windows host address reachable from WSL (find it with `ip route show default`). Set all connection values in the same WSL shell before starting either binary; prompt for the password so it is not saved in source or shell history:

```bash
export DRERO_DB_HOST=172.24.128.1
export DRERO_DB_PORT=3306
export DRERO_DB_USER=drero_app
export DRERO_DB_NAME=drero_db
read -rsp "MySQL password for drero_app: " DRERO_DB_PASSWORD
export DRERO_DB_PASSWORD
printf '\n'
./build/drero_server 8080
```

The API serializes HTTP handlers because the current server shares one MySQL Connector/C++ connection, which is not thread-safe. This protects requests such as the frontend's parallel dropdown loads from crashing the backend.

Start the existing CLI in a second WSL shell after setting the same `DRERO_DB_*` values:

```bash
./build/drero_cli
```

The API health endpoint is `http://127.0.0.1:8080/api/health`. The server must be running and connected to MySQL for API-backed forms and dashboards to load.

### 3. Run the frontend

In Windows PowerShell, in `frontend/`:

```powershell
Copy-Item .env.example .env.local
npm install
npm run dev
```

Open `http://localhost:3000`. Set `VITE_API_BASE_URL` in `frontend/.env.local` to the backend URL if it is not reachable at `http://127.0.0.1:8080`. Restart Vite after changing this setting. Without the variable, Vite uses the `/api` proxy configured in `vite.config.js`.

For a production build:

```powershell
npm run build
npm run preview
```

## API-backed flows

- Citizen: load active disaster/city/locality/assistance options, submit to `POST /api/public/requests`, then track the returned `public_ref` at `GET /api/public/track`.
- Staff: authenticate through `POST /api/auth/login`; the backend role determines which dashboard is rendered. The token is validated against `GET /api/auth/me` when a saved session is restored.
- Dispatcher: list/search/filter requests, update only valid request-state transitions, run one backend scheduler or compare all four, and optionally allocate resources in the selected scheduler's returned request order. The batch response reports allocations and per-request reasons when no eligible resource can be assigned.
- Zone administrator: read inventory and recent zone requests, register resources in the assigned zone, update permitted resource statuses, and manage zonal risk for their assigned zone.
- Auditor: read audit entries, saved scheduling metrics, allocation history, and backend safety status. The view has no write actions.

The C++ database layer uses the existing MySQL reporting views and a precomputed summary table for read operations:

- `pending_rescue_requests` supplies the readable request fields used by the dispatcher dashboard and scheduling input; `Under Review` requests are added separately because that status is not included by the view.
- `available_resources_summary` supplies the readable location and type fields for the dispatcher's available-resource list. Resource matching still reads the full inventory separately.
- `allocation_details` supplies request references and resource names for the auditor's allocation history. Allocation IDs, statuses, remarks, and transaction-critical data still come from `allocations`.
- `scheduling_performance_summary_cache` supplies per-run counts, average timing metrics, throughput, and starvation count to the auditor. The live `scheduling_performance_summary` view remains the source for verification; the cache is backfilled on install and refreshed transactionally when a run completes.
- `audit_activity_report` supplies the actor and audit fields for the auditor's audit log.

These views are used for reporting/read paths only. Allocation and release continue to use the existing transaction and row-lock logic; the reporting views do not replace availability checks or writes.

Zone-risk management uses the existing `disaster_zone_status` row for each disaster and zone. `PATCH /api/staff/zone/risk` accepts `disaster_id`, `risk_level` (1–5), and an optional `risk_description`; it does not accept a target zone. The server requires the `ZONE_ADMIN` role and takes the zone from the authenticated session, then updates `updated_by`/`updated_at` and writes the old and new values to `audit_log` in the same transaction. The Zone Admin dashboard only receives risk rows for that account's assigned zone. Risk and disaster severity are separate values.

Priority with Aging uses the current risk value when a scheduling run loads pending requests. Its score is `severity_snapshot × 20 + risk_level × 15 + min(people_affected, 50) × 0.6 + floor(waited / 5) × 10`; `waited` is the scheduler's existing simulated queue time, not wall-clock elapsed time. Thus, changing risk from 1 to 5 adds 60 points to an otherwise identical request, without changing severity. A later scheduling run uses the latest risk; existing saved run metrics and scores are not recalculated. The risk edit itself is recorded in `audit_log`, preserving who changed the value and the old/new values.

Public requests and staff operations are persisted by the existing C++/MySQL path. Staff credentials and roles are validated by the server; choosing a role in the browser is not authentication. After the persistent-session migration, startup upgrades legacy seeded passwords to salted PBKDF2 hashes and staff sessions persist in MySQL. Demo credentials remain for classroom use.

Scheduled allocation is a two-step workflow. First, call `POST /api/staff/dispatcher/schedule` with one selected `algorithm` and `quantum` (for Round Robin); this saves and returns the scheduler run and its order. After reviewing it, call `POST /api/staff/dispatcher/allocate-scheduled` with that `run_id`. The C++ backend reads the persisted queue positions and allocates in that exact order rather than rerunning the scheduler. Each request is matched against compatible inventory, preferring a primary resource type in its zone, then a compatible alternative in-zone, then primary type elsewhere, then any compatible resource. It never falls back to an incompatible type. Capacity is enforced when the resource unit is people or patients; other units cannot safely be compared to people affected without a domain conversion. Unavailable or unsuitable requests remain unallocated with a reason, and processing continues.

Each allocation locks the request and resource rows, validates request eligibility, compatibility, capacity, and active allocations, then inserts the allocation and status-history rows and updates both statuses in one transaction. The resource row lock is the serialization point that prevents competing requests from claiming the same resource. Round Robin retains the project's existing time-slice calculations and metrics; the batch allocator consumes the request vector that its current implementation returns.

To exercise scheduled allocation against a configured test database, log in as a dispatcher to obtain a token, then call:

```bash
curl -X POST http://127.0.0.1:8080/api/staff/dispatcher/schedule \
  -H "Authorization: Bearer <dispatcher-token>" \
  -H "Content-Type: application/json" \
  -d '{"algorithm":"FCFS","quantum":3}'
```

After reviewing its returned `run_id` and `scheduled` order, allocate that same run:

```bash
curl -X POST http://127.0.0.1:8080/api/staff/dispatcher/allocate-scheduled \
  -H "Authorization: Bearer <dispatcher-token>" \
  -H "Content-Type: application/json" \
  -d '{"run_id":123}'
```

The allocation response includes `allocation.results`; each entry carries the saved queue position, request reference, allocation status, and either the selected resource or a failure reason. For example, if the scheduler returns references `DR-101` then `DR-102`, allocation results are processed and reported in positions 1 then 2. To verify persistence, inspect `allocations`, `rescue_requests`, and `resources` for the returned request/resource IDs, and check `request_status_history` and `resource_status_history` for their changes. Run conflict/failure scenarios only against a disposable test database.

## Database feature installation and verification

MySQL does not provide native materialized views. The auditor's scheduling-history report now reads `scheduling_performance_summary_cache`, a precomputed summary table. Applying the repeatable migration backfills existing runs; completing a scheduling run refreshes its cache row in the same transaction that marks the run completed. The report still filters to completed runs.

For an existing database, first inspect the current triggers, apply the risk-level migration if it has not already been applied, then run the feature migration from the project root as a MySQL administrator. It preserves application tables and rows while replacing the project routines/triggers and updating the app role; it requires the `drero_app`@`%` account to exist. Do not run these commands against a production database without first reviewing the DDL and your account/host setup.

```bash
mysql -u root -p drero_db -e "SHOW TRIGGERS"
# Only if the risk-level migration has not already run:
mysql -u root -p drero_db < database/migrations/20261018_add_zone_risk_level.sql
mysql -u root -p < database/migrations/20261115_integrate_routines_reporting_roles_triggers.sql
mysql -u root -p < database/verify_db_features.sql
mysql -u drero_app -p < database/verify_application_role.sql
```

`verify_db_features.sql` prints the server version/edition, both trigger definitions, severity-function comparison results, the service-time comparison, summary-cache mismatches, and a deliberately rejected status transition with rollback checks. Run it on a disposable development database with at least one request; `status_unchanged`, `history_unchanged`, and `audit_unchanged` should be `1`. The disaster-trigger probe inserts and audits a test disaster inside one transaction, checks for exactly one audit row, and rolls both rows back. The role verification file must be run while connected as the same MySQL account used by the API; expect `drero_application_role` in `CURRENT_ROLE()` and in `SHOW GRANTS`. The API also checks the active role and rejects direct global or database-wide grants at connection time.

After building and starting the API, use normal website sessions or these endpoints to verify the real flows:

- Public request submission: `POST /api/public/requests` calls `submit_rescue_request`; its `get_severity_for_zone` lookup is the same stored-value-or-1 behavior previously used by the C++ lookup.
- Dispatcher status change: `PATCH /api/staff/dispatcher/request/status` calls `change_request_status`. Its allowed-state validation remains in the route and is repeated under a database row lock.
- Dispatcher allocation and release: `POST /api/staff/dispatcher/allocate` and `/api/staff/dispatcher/release` call the corresponding stored procedures, which own the transaction, history writes, and audit write.
- Zone administrator/dispatcher resource creation: `POST /api/staff/zone/resource` is audited by `audit_new_resource_creation`; the application no longer writes a second creation audit row.
- Scheduling report: run `POST /api/staff/dispatcher/schedule`, then read `GET /api/staff/auditor/scheduling-history`. A completed run should appear with metrics from the summary table.

For an end-to-end resource audit check, create a resource through the website or API and use its returned `resource_id`:

```sql
SELECT COUNT(*) AS resource_creation_audit_rows
FROM audit_log
WHERE action = 'INSERT_RESOURCE'
  AND entity_type = 'resources'
  AND entity_id = <resource_id>;
```

Expect exactly `1`. There is no disaster-creation website route; the SQL trigger probe is the verification path for `audit_new_disaster_creation`. To test the API routes, authenticate with a dispatcher or zone-administrator account, pass its bearer token, and use valid IDs from the disposable database. Do not use the invalid-transition API route as the rollback test: it rejects that transition before calling SQL, so the SQL verification script calls the procedure directly.

## Verification commands

```powershell
cd frontend
npm run build
npm run lint
```

```bash
cmake --build build
./build/test_scheduling
./build/test_allocation_matching
./build/test_concurrency
./build/test_rag
```

These commands verify compilation/tests; exercising persistence and authenticated API actions also requires the configured MySQL server, schema, seed, and a running `drero_server`.

### Host-specific existing application accounts

Check `CURRENT_USER()` using the backend account first. A specific host account
shadows `drero_app`@`%`. In the administrator MySQL session, set
`SET @drero_app_host = '172.24.143.176';` (replace with the host returned locally)
before sourcing the feature migration. This configures that existing account
without changing its password. The local default is `172.24.143.176`, verified through the backend TCP connection.
The feature migration explicitly resets this target; update it if the backend client host changes.
Run SOURCE commands from the project root so nested SQL paths resolve.

## Incremental visualization and authentication features

Gantt timelines (including real Round Robin slices), priority explanations, interactive
resource graphs, objective-based recommendations, allocation explanations, citizen
timelines, risk previews, persistent expiring sessions, password hashing, CSV export,
sortable results, and refresh timestamps are documented in
[Incremental features](docs/incremental-features.md).

Before starting the updated backend on an existing database, apply the additive
`database/migrations/20261115_persistent_staff_sessions.sql` as administrator.
It preserves rescue data and staff credentials. OpenSSL development headers are
required for the C++ build. The session lifetime defaults to eight hours and can be
configured with `DRERO_SESSION_TTL_SECONDS`.

## Optional operations and simulation lab

Additional request requirements, atomic multi-resource allocation, temporary reservations,
optional dispatch progress, duplicate-review suggestions, wall-clock overdue alerts,
resource utilization, and explicitly started simulation labs are described in
[Optional operations](docs/optional-operations.md). Resource-type management was not added.
The existing workflow stays available; these tools are optional.

Before running this version, apply
`database/migrations/20261115_optional_operations.sql` as administrator after the existing
base and persistent-session migrations. It is additive and preserves existing data.
The deadlock simulator runs **only when its Run button is clicked** and never modifies
live allocations or the database.
