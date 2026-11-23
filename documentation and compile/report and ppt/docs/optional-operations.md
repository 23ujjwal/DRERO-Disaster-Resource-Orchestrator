# Optional operations and simulation tools

These eight additions extend the existing workflow. Resource-type management was
explicitly excluded and has not been added. All requirements use existing assistance
and inventory types. Public submission, normal scheduling, single-resource allocation,
and the existing release action remain available.

## Where to find the additions

| Feature | Location | Behavior |
| --- | --- | --- |
| Multiple resources | Dispatcher → Operations tools → Multiple resources | Choose a pending request, add existing assistance requirements, save, and choose one distinct resource per required unit. Primary assistance remains mandatory. All assignments commit together or all roll back. |
| Dispatch lifecycle | Dispatcher → Operations tools → Dispatch progress | Optional Allocated → Dispatched → Arrived → Working stages with timestamps and actors. Existing Release remains valid from any active stage. “Show recent released allocations” includes completed allocation histories. |
| Temporary reservations | Dispatcher → Operations tools → Reservations | Hold a compatible resource for one request and dispatcher for 30–900 seconds. The owner can cancel it. MySQL checks expiry at allocation time; no browser timer or background job is needed for enforcement. |
| Controlled deadlock simulator | Simulation lab | **Nothing runs until Run deadlock simulation is clicked.** Choose 2–5 simulated processes; inspect the cycle/Coffman conditions, then explicitly simulate recovery by preemption or reset. No live RAG or database data changes. |
| Scheduling experiment lab | Simulation lab | Explicitly generate a seeded synthetic workload with request count, quantum, severity/headcount bounds, arrival gap and staggered/burst/random arrivals. Compare four algorithms and actual slice timelines. Results are not saved as operational runs. |
| Possible duplicates | Dispatcher → Operations tools → Duplicate review | Conservatively flag matching phone or name/location pairs within the same disaster, zone, assistance and 24-hour arrival window. Staff can record Related/Distinct. No request is merged, blocked, cancelled or deleted automatically. |
| Request age/alerts | Dispatcher → Operations tools → Overdue alerts | Warning/critical thresholds are stored per dispatcher. Age is real MySQL wall-clock time, separate from OS simulated waiting time. Alerts never change queue order or request status. |
| Utilization and demand | Dispatcher/Auditor → Utilization; Zone Admin → Zone utilization report | Windowed resource allocation frequency, busy/unallocated hours and busy percentage; request demand by assistance and zone. Zone admins see only their assigned zone. |

Simulation tools are available to authenticated dispatcher, zone-admin and auditor
accounts. They operate on synthetic data only. Operational mutations are dispatcher-only.
Opening the lab or changing its controls sends no deadlock request; Run/Recovery are
the only actions that invoke the simulation API. Reset is local UI state only.

## Preserve the existing flow

- A request with no additional requirements still uses the original allocation API.
- Additional requirements are opt-in and can be configured only before allocation.
  Once configured, the single-resource action explains that a bundle is required.
- Each resource with people/patient units must independently cover the full affected
  group. This feature does not invent conversions for kits or divide a group among
  undersized vehicles. At most 12 resources can be required for a request.
- A bundle requires exactly the configured units/types and distinct resources.
- Multi-resource release keeps the request Allocated until its final active
  allocation is released. Then it becomes Completed, with one completion history event.
- Dispatch stages are separate events; no request/resource status enum was changed.
  There is no new required dispatch step before the existing Release action.
- Reservations do not allocate resources or change request status. Dispatcher's
  available-inventory query excludes active holds. Underlying inventory status stays
  Available until allocation; the reservation is separate data.
- Cancelling/completing a request cancels its outstanding reservations automatically.
- Duplicate reviews are annotations, not automatic merges. The newest 100 candidate
  pairs are shown. Age reports include up to 500 oldest pending requests.
- Busy time is allocation overlap within the reporting window since resource creation.
  Unallocated time includes maintenance/inactive periods and reservations; it does not
  mean the resource was deployable. Historical rows do not invent dispatch timestamps.

## Migration

The additive migration creates six tables:

- request_resource_needs
- allocation_assistance
- resource_reservations
- dispatch_events
- request_duplicate_reviews
- request_alert_settings

It grants table-level rights to the existing drero_application_role, installs
reservation/dispatch triggers, backfills known allocation/release events, and replaces
release_resource_allocation with the same signature and multi-resource-safe completion.
It does not recreate existing application tables or change staff passwords.

In this Windows/WSL setup, run from the VS Code terminal and enter the administrator
password only at the prompt (adjust the UNC distro/path if your checkout differs):

```bash
mysql.exe --host=127.0.0.1 --port=3306 --user=root --password --database=drero_db --execute="SOURCE //wsl.localhost/Ubuntu/home/ujjwal/projects/DRERO/database/migrations/20261115_optional_operations.sql;"
```

A successful run ends with optional_operations_tables_installed = 6. The migration
is repeatable. Install it before starting the updated backend. Fresh installations
also require it after the base schema/routines and persistent-session migration.
Do not run schema.sql to upgrade an existing database; it is a destructive fresh reset.

MySQL may hide routine metadata from an account whose EXECUTE rights come through a
role. Runtime CALL checks, not metadata visibility alone, validate procedure access.

## Backend paths

All paths below require existing bearer-token authentication.

| Endpoint | Method | Access |
| --- | --- | --- |
| /api/staff/operations/overview | GET | Dispatcher |
| /api/staff/operations/needs?request_id=… | GET | Dispatcher |
| /api/staff/operations/needs | PUT | Dispatcher |
| /api/staff/operations/allocate-bundle | POST | Dispatcher |
| /api/staff/operations/reserve | POST | Dispatcher |
| /api/staff/operations/cancel-reservation | POST | Reserving dispatcher |
| /api/staff/operations/dispatch-stage | POST | Dispatcher |
| /api/staff/operations/duplicate-review | POST | Dispatcher |
| /api/staff/operations/alert-settings | PUT | Dispatcher, own settings |
| /api/staff/operations/utilization?days=7 | GET | Dispatcher/Auditor; assigned-zone scope for Zone Admin |
| /api/staff/labs/deadlock | POST | All staff, synthetic/in-memory only |
| /api/staff/labs/scheduling | POST | All staff, synthetic/in-memory only |

Existing /api/staff/dispatcher/allocate and /release stay compatible. Reservation
triggers also protect allocations made through the stored procedure, not just the
new UI. Resource/request mutations and audit/history writes share a transaction.

The normal scheduler still normalizes arrivals exactly as before. Only the synthetic
lab honors numeric arrivals and idle gaps. Round Robin was extended to resume after
an idle gap for lab workloads without changing its normal request scheduling behavior.

## Concurrency and rollback

Allocation/reservation writers acquire the request lock before the resource lock.
Bundle resource IDs are locked in sorted order. Reservation checks use current
SELECT … FOR UPDATE reads after obtaining the resource lock, so a transaction with
an older snapshot cannot miss a newly committed hold. Allocation consumes a rightful
reservation in the same transaction; rollback restores it along with allocation,
request/resource statuses, history, dispatch events and audit rows. The final remaining-resource count during release is also a current locking read, preventing simultaneous releases from leaving a completed request marked Allocated.

Expired reservations may remain as expired rows until overwritten. Expiry is enforced
by comparing expires_at with MySQL NOW(), so an expired row never keeps a resource held.

## Checks

```bash
cmake --build build
./build/test_optional_core
# With the normal DRERO_DB_* environment set:
python3 tests/test_optional_api.py
python3 tests/test_concurrent_release.py
NODE_PATH=/path/to/playwright/node_modules node tests/test_optional_ui.cjs
npm --prefix frontend run lint
npm --prefix frontend run build
```

The core tests cover explicit simulated cycles/recovery, isolation from live RAG state,
synthetic idle gaps/bursts, and unchanged operational arrival normalization. API/browser
tests exercise the real C++ server and MySQL with labelled fixtures; they verify bundle
rollback, reservation conflicts/expiry/ownership, dispatch stages and final completion,
duplicate review, threshold persistence, utilization, role/zone restrictions, and explicit
button-only simulation execution. Simulation calls are checked against database snapshots.
The concurrency regressions also use a second backend/database connection and external
transactions to test both the older-snapshot reservation race and simultaneous final releases.

Fixture requests/resources are cancelled, completed, or inactivated after checks;
associated audit/history/review records are retained. Existing requests remain untouched.

## Verified result

- C++: all 117 assertions passed (existing scheduling/allocation/concurrency/RAG and
  incremental tests plus 16 new lab/arrival-pattern checks).
- Frontend lint and production build passed.
- Real API tests passed for all eight features, including two-backend reservation
  conflicts, older-snapshot holds, atomic bundle rollback, and simultaneous releases.
- The simultaneous-release regression returned HTTP 200 for both resources and
  persisted request status Completed with zero active allocations.
- Chromium verified zero deadlock API calls on opening the lab or editing controls;
  Run and Recovery each send one explicit call, and Reset sends none.
- Chromium also exercised requirements editing, reservation countdown, multi-resource
  allocation, dispatch stages/releases, duplicate review, alert settings, utilization,
  CSV export and mobile layout against real API responses.
- Existing comparison, session, risk-preview, allocation and public-tracking checks passed.

The migration includes the final current-read release fix. For an installation of an
earlier revision only, `database/migrations/20261115_release_completion_fix.sql`
replaces just that procedure with the corrected current locking count.

Environment tested: MySQL 8.4.10, drero_db at 172.24.128.1:3306, application account
drero_app@172.24.143.176 with drero_application_role. No administrator credentials
are used by the website backend. Test servers are stopped after verification.
