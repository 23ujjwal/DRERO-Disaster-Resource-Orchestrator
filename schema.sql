-- DRERO: Disaster Resource Orchestrator
-- schema.sql — DDL for all 17 main tables + views (human-readable names)
-- MySQL 8.4 compatible
-- Safe to re-run: drops and recreates everything.

SET FOREIGN_KEY_CHECKS = 0;-- schema in 

DROP VIEW IF EXISTS pending_rescue_requests;
DROP VIEW IF EXISTS available_resources_summary;
DROP VIEW IF EXISTS allocation_details;
DROP VIEW IF EXISTS scheduling_performance_summary;
DROP VIEW IF EXISTS audit_activity_report;

-- legacy view names (drop if old schema was applied)
DROP VIEW IF EXISTS v_pending_requests;
DROP VIEW IF EXISTS v_available_resources;
DROP VIEW IF EXISTS v_allocation_details;
DROP VIEW IF EXISTS v_scheduling_summary;
DROP VIEW IF EXISTS v_audit_report;

DROP TABLE IF EXISTS deadlock_events;
DROP TABLE IF EXISTS audit_log;
DROP TABLE IF EXISTS scheduling_log;
DROP TABLE IF EXISTS scheduling_runs;
DROP TABLE IF EXISTS resource_status_history;
DROP TABLE IF EXISTS allocations;
DROP TABLE IF EXISTS request_status_history;
DROP TABLE IF EXISTS rescue_requests;
DROP TABLE IF EXISTS assistance_types;
DROP TABLE IF EXISTS resources;
DROP TABLE IF EXISTS resource_types;
DROP TABLE IF EXISTS disaster_zone_status;
DROP TABLE IF EXISTS disasters;
DROP TABLE IF EXISTS users;
DROP TABLE IF EXISTS localities;
DROP TABLE IF EXISTS zones;
DROP TABLE IF EXISTS cities;

SET FOREIGN_KEY_CHECKS = 1;

-- ─────────────────────────────────────────────
-- Table 1: cities
-- Stores supported cities where disasters can occur.
-- ─────────────────────────────────────────────
CREATE TABLE cities (
    city_id      INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    city_name    VARCHAR(100) NOT NULL,
    state_name   VARCHAR(100) NOT NULL,
    country      VARCHAR(60)  NOT NULL DEFAULT 'India',
    is_active    TINYINT(1)   NOT NULL DEFAULT 1,
    created_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY unique_city_per_state (city_name, state_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Supported cities for disaster operations';

-- ─────────────────────────────────────────────
-- Table 2: zones
-- Divides each city into administrative response zones.
-- ─────────────────────────────────────────────
CREATE TABLE zones (
    zone_id      INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    city_id      INT UNSIGNED NOT NULL,
    zone_name    VARCHAR(100) NOT NULL,
    is_active    TINYINT(1)   NOT NULL DEFAULT 1,
    created_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY fk_zone_city (city_id) REFERENCES cities(city_id) ON DELETE RESTRICT,
    UNIQUE KEY unique_zone_in_city (zone_name, city_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Response zones within a city';

-- ─────────────────────────────────────────────
-- Table 3: localities
-- Neighbourhoods, sectors, colonies mapped to zones.
-- ─────────────────────────────────────────────
CREATE TABLE localities (
    locality_id   INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    zone_id       INT UNSIGNED NOT NULL,
    locality_name VARCHAR(150) NOT NULL,
    pincode       VARCHAR(10)  DEFAULT NULL,
    latitude      DECIMAL(9,6) DEFAULT NULL,
    longitude     DECIMAL(9,6) DEFAULT NULL,
    is_active     TINYINT(1)   NOT NULL DEFAULT 1,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY fk_locality_zone (zone_id) REFERENCES zones(zone_id) ON DELETE RESTRICT,
    UNIQUE KEY unique_locality_in_zone (locality_name, zone_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Localities (colonies/sectors) mapped to zones';

-- ─────────────────────────────────────────────
-- Table 4: disasters
-- Records disaster events (floods, cyclones, earthquakes).
-- ─────────────────────────────────────────────
CREATE TABLE disasters (
    disaster_id   INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    disaster_name VARCHAR(150) NOT NULL,
    disaster_type VARCHAR(60)  NOT NULL,
    description   TEXT         DEFAULT NULL,
    start_time    DATETIME     NOT NULL,
    end_time      DATETIME     DEFAULT NULL,
    status        ENUM('Active','Monitoring','Closed') NOT NULL DEFAULT 'Active',
    created_by    INT UNSIGNED DEFAULT NULL,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_disaster_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Disaster events; each can affect multiple zones';

-- ─────────────────────────────────────────────
-- Table 5: disaster_zone_status
-- Severity level for a zone during a specific disaster.
-- Composite PK ensures one severity per (disaster, zone).
-- ─────────────────────────────────────────────
CREATE TABLE disaster_zone_status (
    disaster_id      INT UNSIGNED NOT NULL,
    zone_id          INT UNSIGNED NOT NULL,
    severity_level   TINYINT      NOT NULL CHECK (severity_level BETWEEN 1 AND 5),
    risk_description VARCHAR(255) DEFAULT NULL,
    updated_by       INT UNSIGNED DEFAULT NULL,
    updated_at       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (disaster_id, zone_id),
    FOREIGN KEY fk_dzs_disaster (disaster_id) REFERENCES disasters(disaster_id) ON DELETE CASCADE,
    FOREIGN KEY fk_dzs_zone     (zone_id)     REFERENCES zones(zone_id)         ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Severity of each zone for each disaster event';

-- ─────────────────────────────────────────────
-- Table 6: users  (staff accounts only)
-- ─────────────────────────────────────────────
CREATE TABLE users (
    user_id       INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    username      VARCHAR(60)  NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    full_name     VARCHAR(150) NOT NULL,
    role          ENUM('DISPATCHER','ZONE_ADMIN','AUDITOR') NOT NULL,
    zone_id       INT UNSIGNED DEFAULT NULL,
    is_active     TINYINT(1)   NOT NULL DEFAULT 1,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY fk_user_zone (zone_id) REFERENCES zones(zone_id) ON DELETE SET NULL,
    INDEX idx_user_role (role)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Staff accounts: DISPATCHER, ZONE_ADMIN, AUDITOR';

-- Add FK back-references now that users table exists
ALTER TABLE disasters
    ADD CONSTRAINT fk_disaster_created_by
    FOREIGN KEY (created_by) REFERENCES users(user_id) ON DELETE SET NULL;

ALTER TABLE disaster_zone_status
    ADD CONSTRAINT fk_dzs_updated_by
    FOREIGN KEY (updated_by) REFERENCES users(user_id) ON DELETE SET NULL;

-- ─────────────────────────────────────────────
-- Table 7: assistance_types
-- Categories of help that can be requested (boat, medical, etc.).
-- ─────────────────────────────────────────────
CREATE TABLE assistance_types (
    assistance_type_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    type_name          VARCHAR(100) NOT NULL UNIQUE,
    description        VARCHAR(255) DEFAULT NULL,
    is_active          TINYINT(1)   NOT NULL DEFAULT 1,
    created_at         DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Types of rescue/relief assistance available';

-- ─────────────────────────────────────────────
-- Table 8: rescue_requests
-- Core table: each row is one public or staff-created rescue request.
-- Also acts as a simulated OS Process Control Block.
-- ─────────────────────────────────────────────
CREATE TABLE rescue_requests (
    request_id         INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    public_ref         VARCHAR(20)  NOT NULL UNIQUE,
    disaster_id        INT UNSIGNED NOT NULL,
    locality_id        INT UNSIGNED NOT NULL,
    zone_id            INT UNSIGNED NOT NULL,
    victim_name        VARCHAR(150) NOT NULL,
    contact_number     VARCHAR(20)  NOT NULL,
    reported_location  VARCHAR(255) NOT NULL,
    assistance_type_id INT UNSIGNED NOT NULL,
    people_affected    SMALLINT UNSIGNED NOT NULL CHECK (people_affected >= 1),
    details            TEXT         DEFAULT NULL,
    severity_snapshot  TINYINT      NOT NULL CHECK (severity_snapshot BETWEEN 1 AND 5),
    arrival_time       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    status             ENUM(
        'Submitted','New','Ready','Scheduled',
        'Under Review','Allocated','Deferred','Completed','Cancelled'
    ) NOT NULL DEFAULT 'Submitted',
    created_by         INT UNSIGNED DEFAULT NULL,
    created_at         DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at         DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY fk_req_disaster    (disaster_id)        REFERENCES disasters(disaster_id)              ON DELETE RESTRICT,
    FOREIGN KEY fk_req_locality    (locality_id)        REFERENCES localities(locality_id)             ON DELETE RESTRICT,
    FOREIGN KEY fk_req_zone        (zone_id)            REFERENCES zones(zone_id)                      ON DELETE RESTRICT,
    FOREIGN KEY fk_req_asst_type   (assistance_type_id) REFERENCES assistance_types(assistance_type_id) ON DELETE RESTRICT,
    FOREIGN KEY fk_req_created_by  (created_by)         REFERENCES users(user_id)                      ON DELETE SET NULL,
    INDEX idx_request_status   (status),
    INDEX idx_request_disaster (disaster_id),
    INDEX idx_request_zone     (zone_id),
    INDEX idx_request_arrival  (arrival_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Rescue requests; each request is a simulated OS process';

-- ─────────────────────────────────────────────
-- Table 9: request_status_history
-- Full audit trail of every status change on a request.
-- ─────────────────────────────────────────────
CREATE TABLE request_status_history (
    history_id  INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    request_id  INT UNSIGNED NOT NULL,
    old_status  VARCHAR(30)  NOT NULL,
    new_status  VARCHAR(30)  NOT NULL,
    changed_by  INT UNSIGNED DEFAULT NULL,
    changed_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    remarks     VARCHAR(255) DEFAULT NULL,
    FOREIGN KEY fk_rsh_request    (request_id) REFERENCES rescue_requests(request_id) ON DELETE CASCADE,
    FOREIGN KEY fk_rsh_changed_by (changed_by) REFERENCES users(user_id)              ON DELETE SET NULL,
    INDEX idx_rsh_request (request_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Every status change ever made on a rescue request';

-- ─────────────────────────────────────────────
-- Table 10: resource_types
-- Categories of physical resources (boat, truck, etc.).
-- ─────────────────────────────────────────────
CREATE TABLE resource_types (
    resource_type_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    type_name        VARCHAR(100) NOT NULL UNIQUE,
    description      VARCHAR(255) DEFAULT NULL,
    is_active        TINYINT(1)   NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Categories of rescue resources';

-- ─────────────────────────────────────────────
-- Table 11: resources
-- Individual resources and their current availability.
-- ─────────────────────────────────────────────
CREATE TABLE resources (
    resource_id      INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    resource_name    VARCHAR(100) NOT NULL,
    resource_type_id INT UNSIGNED NOT NULL,
    zone_id          INT UNSIGNED NOT NULL,
    capacity         SMALLINT UNSIGNED NOT NULL DEFAULT 1,
    capacity_unit    VARCHAR(40)  NOT NULL DEFAULT 'units',
    status           ENUM('Available','Allocated','Maintenance','Inactive') NOT NULL DEFAULT 'Available',
    notes            VARCHAR(255) DEFAULT NULL,
    created_by       INT UNSIGNED DEFAULT NULL,
    created_at       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY fk_res_type       (resource_type_id) REFERENCES resource_types(resource_type_id) ON DELETE RESTRICT,
    FOREIGN KEY fk_res_zone       (zone_id)          REFERENCES zones(zone_id)                   ON DELETE RESTRICT,
    FOREIGN KEY fk_res_created_by (created_by)       REFERENCES users(user_id)                   ON DELETE SET NULL,
    INDEX idx_resource_status (status),
    INDEX idx_resource_zone   (zone_id),
    INDEX idx_resource_type   (resource_type_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Individual rescue resources (boats, trucks, medical teams, etc.)';

-- ─────────────────────────────────────────────
-- Table 12: allocations
-- Tracks which resource was assigned to which request.
-- ─────────────────────────────────────────────
CREATE TABLE allocations (
    allocation_id INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    request_id    INT UNSIGNED NOT NULL,
    resource_id   INT UNSIGNED NOT NULL,
    allocated_by  INT UNSIGNED DEFAULT NULL,
    allocated_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    released_at   DATETIME     DEFAULT NULL,
    status        ENUM('Active','Released','Cancelled') NOT NULL DEFAULT 'Active',
    remarks       VARCHAR(255) DEFAULT NULL,
    FOREIGN KEY fk_alloc_request      (request_id)  REFERENCES rescue_requests(request_id) ON DELETE RESTRICT,
    FOREIGN KEY fk_alloc_resource     (resource_id) REFERENCES resources(resource_id)      ON DELETE RESTRICT,
    FOREIGN KEY fk_alloc_allocated_by (allocated_by) REFERENCES users(user_id)             ON DELETE SET NULL,
    INDEX idx_alloc_request  (request_id),
    INDEX idx_alloc_resource (resource_id),
    INDEX idx_alloc_status   (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Resource-to-request assignments; active allocations prevent double booking';

-- ─────────────────────────────────────────────
-- Table 13: resource_status_history
-- Records every availability change for a resource.
-- ─────────────────────────────────────────────
CREATE TABLE resource_status_history (
    history_id  INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    resource_id INT UNSIGNED NOT NULL,
    old_status  VARCHAR(30)  NOT NULL,
    new_status  VARCHAR(30)  NOT NULL,
    changed_by  INT UNSIGNED DEFAULT NULL,
    changed_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    remarks     VARCHAR(255) DEFAULT NULL,
    FOREIGN KEY fk_resh_resource   (resource_id) REFERENCES resources(resource_id) ON DELETE CASCADE,
    FOREIGN KEY fk_resh_changed_by (changed_by)  REFERENCES users(user_id)         ON DELETE SET NULL,
    INDEX idx_resh_resource (resource_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Full history of resource status changes';

-- ─────────────────────────────────────────────
-- Table 14: scheduling_runs
-- Records each execution of a scheduling algorithm.
-- ─────────────────────────────────────────────
CREATE TABLE scheduling_runs (
    run_id       INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    algorithm    ENUM('FCFS','SJF','RR','PRIORITY') NOT NULL,
    time_quantum SMALLINT UNSIGNED DEFAULT NULL,
    started_by   INT UNSIGNED DEFAULT NULL,
    started_at   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    finished_at  DATETIME     DEFAULT NULL,
    status       ENUM('Running','Completed','Failed') NOT NULL DEFAULT 'Running',
    FOREIGN KEY fk_run_started_by (started_by) REFERENCES users(user_id) ON DELETE SET NULL,
    INDEX idx_run_algorithm (algorithm)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Each run of a scheduling algorithm; stores which algorithm, when, by whom';

-- ─────────────────────────────────────────────
-- Table 15: scheduling_log
-- Per-request results for each scheduling run.
-- ─────────────────────────────────────────────
CREATE TABLE scheduling_log (
    log_id          INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    run_id          INT UNSIGNED NOT NULL,
    request_id      INT UNSIGNED NOT NULL,
    queue_position  SMALLINT UNSIGNED NOT NULL,
    priority_score  DECIMAL(8,3)  DEFAULT NULL,
    start_time      DECIMAL(12,3) NOT NULL COMMENT 'simulated time units (1 unit = 10 min)',
    completion_time DECIMAL(12,3) NOT NULL,
    waiting_time    DECIMAL(12,3) NOT NULL,
    turnaround_time DECIMAL(12,3) NOT NULL,
    response_time   DECIMAL(12,3) NOT NULL,
    FOREIGN KEY fk_log_run     (run_id)     REFERENCES scheduling_runs(run_id)     ON DELETE CASCADE,
    FOREIGN KEY fk_log_request (request_id) REFERENCES rescue_requests(request_id) ON DELETE CASCADE,
    INDEX idx_log_run (run_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Per-request scheduling results: waiting time, turnaround, response time';

-- ─────────────────────────────────────────────
-- Table 16: audit_log
-- Immutable record of all important system actions.
-- ─────────────────────────────────────────────
CREATE TABLE audit_log (
    audit_id    INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    user_id     INT UNSIGNED DEFAULT NULL,
    action      VARCHAR(100) NOT NULL,
    entity_type VARCHAR(60)  NOT NULL,
    entity_id   INT UNSIGNED DEFAULT NULL,
    action_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    details     TEXT         DEFAULT NULL,
    FOREIGN KEY fk_audit_user (user_id) REFERENCES users(user_id) ON DELETE SET NULL,
    INDEX idx_audit_entity (entity_type, entity_id),
    INDEX idx_audit_time   (action_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='System-wide audit trail; records who did what and when';

-- ─────────────────────────────────────────────
-- Table 17: deadlock_events
-- Records simulated RAG cycle detections and resolutions.
-- ─────────────────────────────────────────────
CREATE TABLE deadlock_events (
    event_id          INT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
    detected_at       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    cycle_details     TEXT         NOT NULL,
    resolution_action VARCHAR(255) DEFAULT NULL,
    resolved_at       DATETIME     DEFAULT NULL,
    status            ENUM('Detected','Resolved','Simulated') NOT NULL DEFAULT 'Detected'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Simulated deadlock events detected by RAG DFS; distinct from MySQL deadlocks';

-- ═══════════════════════════════════════════════════════════════
-- VIEWS — human-readable names describing what each view shows
-- ═══════════════════════════════════════════════════════════════

-- Shows all rescue requests that are still waiting to be processed
CREATE OR REPLACE VIEW pending_rescue_requests AS
SELECT
    rr.request_id,
    rr.public_ref,
    rr.victim_name,
    rr.people_affected,
    rr.severity_snapshot,
    rr.status,
    rr.arrival_time,
    d.disaster_name,
    c.city_name,
    l.locality_name,
    z.zone_name,
    at2.type_name AS assistance_type
FROM rescue_requests rr
JOIN disasters        d   ON d.disaster_id         = rr.disaster_id
JOIN localities       l   ON l.locality_id          = rr.locality_id
JOIN zones            z   ON z.zone_id              = rr.zone_id
JOIN cities           c   ON c.city_id              = z.city_id
JOIN assistance_types at2 ON at2.assistance_type_id = rr.assistance_type_id
WHERE rr.status IN ('Submitted','New','Ready','Scheduled','Deferred');

-- Shows all resources currently available for deployment
CREATE OR REPLACE VIEW available_resources_summary AS
SELECT
    r.resource_id,
    r.resource_name,
    rt.type_name  AS resource_type,
    z.zone_name,
    c.city_name,
    r.capacity,
    r.capacity_unit,
    r.status
FROM resources    r
JOIN resource_types rt ON rt.resource_type_id = r.resource_type_id
JOIN zones          z  ON z.zone_id           = r.zone_id
JOIN cities         c  ON c.city_id           = z.city_id
WHERE r.status = 'Available';

-- Shows all allocations with request and resource details
CREATE OR REPLACE VIEW allocation_details AS
SELECT
    a.allocation_id,
    rr.public_ref,
    rr.victim_name,
    r.resource_name,
    rt.type_name  AS resource_type,
    u.username    AS allocated_by,
    a.allocated_at,
    a.released_at,
    a.status      AS allocation_status
FROM allocations     a
JOIN rescue_requests rr ON rr.request_id       = a.request_id
JOIN resources        r ON r.resource_id        = a.resource_id
JOIN resource_types  rt ON rt.resource_type_id  = r.resource_type_id
LEFT JOIN users       u ON u.user_id            = a.allocated_by;

-- Aggregates scheduling performance metrics per run
CREATE OR REPLACE VIEW scheduling_performance_summary AS
SELECT
    sr.run_id,
    sr.algorithm,
    sr.time_quantum,
    COUNT(sl.log_id)        AS requests_scheduled,
    AVG(sl.waiting_time)    AS avg_waiting_time,
    AVG(sl.turnaround_time) AS avg_turnaround_time,
    AVG(sl.response_time)   AS avg_response_time,
    sr.started_at,
    sr.finished_at
FROM scheduling_runs sr
LEFT JOIN scheduling_log sl ON sl.run_id = sr.run_id
GROUP BY sr.run_id;

-- Shows all audit entries with actor username resolved
CREATE OR REPLACE VIEW audit_activity_report AS
SELECT
    al.audit_id,
    COALESCE(u.username, 'system') AS actor,
    al.action,
    al.entity_type,
    al.entity_id,
    al.action_time,
    al.details
FROM audit_log al
LEFT JOIN users u ON u.user_id = al.user_id
ORDER BY al.action_time DESC;
