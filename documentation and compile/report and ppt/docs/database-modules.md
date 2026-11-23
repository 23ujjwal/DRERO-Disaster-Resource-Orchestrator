# Database module map

The database layer exposes one shared `Database` interface in
`core/include/database.h`. Its 55 method implementations are grouped into five
source files by responsibility. CMake compiles them into `drero_core`, which is
linked by the backend, CLI, and tests.

| Implementation file in `core/src/` | Responsibility |
| --- | --- |
| `database_connection.cpp` | MySQL connection, application role validation, password authentication, persistent sessions, and staff identity. |
| `database_requests.cpp` | Request references, submission, retrieval, status changes, and request history. |
| `database_resources.cpp` | Resource inventory, allocation, release, and resource history. |
| `database_reporting.cpp` | Scheduling runs and metrics, audit records, and deadlock event records. |
| `database_catalog.cpp` | Disasters, zone risks, cities, localities, assistance types, and resource types. |

The header holds shared data types and method declarations. Each `.cpp` file
holds the corresponding method bodies and SQL operations. This is a separation
of implementation files; callers still use the same `Database` class.

The request flow remains:

`Frontend → C++ API handler → Database method → MySQL → API response → Frontend`

This split preserves the existing SQL, stored procedure calls, transactions,
authentication, and public method signatures. It needs no database migration.
The scheduling algorithms themselves remain in `scheduling.cpp`; the reporting
module stores their runs and results. Optional operations remain in the existing
backend operations module.

For a presentation: “I divided the database layer by responsibility. The shared
header defines the interface, and five implementation modules handle connection
and security, requests, resources, reporting, and reference data. This makes the
code easier to explain and maintain without changing the application workflow.”

The split improves navigation and keeps implementation details out of callers.
It does not by itself make database queries faster.
