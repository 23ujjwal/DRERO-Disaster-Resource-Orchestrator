# Backend module map

`backend/src/server.cpp` is the API entry point. It connects to MySQL, sets up
HTTP handling and CORS, registers route groups, and starts the listener.

| File in `backend/src/` | Responsibility |
| --- | --- |
| `api_auth.cpp` | Persistent session helpers, role checks, login, current user, and logout. |
| `api_helpers.cpp` | JSON serialization, resource selection, and scheduled allocation helpers. |
| `routes_public.cpp` | Public reference data, rescue request submission, and request tracking. |
| `routes_dispatcher.cpp` | Dashboard, request lists and status, risk/allocation previews, allocation, release, and safety status. |
| `routes_scheduling.cpp` | Individual scheduling runs and comparison of all four algorithms. |
| `routes_administration.cpp` | Zone administration, resource management, auditor reports, and scheduling history. |
| `routes_operations.cpp` | Multi-resource needs, reservations, dispatch stages, duplicate review, alerts, utilization, and opt-in simulations. |

`backend/include/api.h` declares the shared session type, helper functions, and
route registration functions. CMake compiles all modules into `drero_server`.

The routes continue to use the existing `Database`, scheduler, resource manager,
RAG, and `Operations` implementations. Endpoints, JSON payloads, permissions,
database operations, and frontend calls remain the same. No migration is needed.

The main function owns the database and operations objects for the full server
lifetime. Registered handlers capture references to these long-lived objects.
HTTP handlers remain sequential because the shared MySQL connection is not
thread-safe.

For a presentation: “The startup file assembles the backend. Each route module
handles a feature area, while shared helpers serialize data and enforce access.
The flow remains frontend → API route → core/database layer → MySQL.”

This is a source-file refactor, not a set of separate services. There is still
one backend process and one API port.
