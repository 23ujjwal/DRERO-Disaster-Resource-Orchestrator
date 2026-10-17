-- DRERO: demo_queries.sql
-- Demonstrates all required DBMS concepts for the course viva.
-- Safe to run multiple times (SELECT only, no modifications).

USE drero_db;

-- ─────────────────────────────────────────────────────────────
-- 1. INNER JOIN — requests with their disaster and zone
-- ─────────────────────────────────────────────────────────────
SELECT '=== INNER JOIN: requests with disaster and zone ===' AS demo; -- request what demo is doing i want to do it beacuse ia m not a p
SELECT
    rr.public_ref,
    rr.victim_name,
    rr.people_affected,
    rr.severity_snapshot,
    d.disaster_name,
    z.zone_name,
    rr.status
FROM rescue_requests rr
INNER JOIN disasters d ON d.disaster_id = rr.disaster_id
INNER JOIN zones     z ON z.zone_id     = rr.zone_id
ORDER BY rr.severity_snapshot DESC
LIMIT 10;

-- ─────────────────────────────────────────────────────────────
-- 2. LEFT JOIN — all localities, showing zone severity even
--    if no disaster zone status exists yet
-- ─────────────────────────────────────────────────────────────
SELECT '=== LEFT JOIN: localities with optional severity ===' AS demo;
SELECT
    l.locality_name,
    z.zone_name,
    c.city_name,
    COALESCE(dzs.severity_level, 'Not configured') AS severity
FROM localities l
LEFT JOIN zones              z   ON z.zone_id     = l.zone_id
LEFT JOIN cities             c   ON c.city_id     = z.city_id
LEFT JOIN disaster_zone_status dzs ON dzs.zone_id = l.zone_id
ORDER BY c.city_name, l.locality_name
LIMIT 15;

-- ─────────────────────────────────────────────────────────────
-- 3. RIGHT JOIN — all users, showing their zone even if no
--    requests exist in that zone
-- ─────────────────────────────────────────────────────────────
SELECT '=== RIGHT JOIN: all users with optional zone ===' AS demo;
SELECT
    COALESCE(u.username, '(no user)') AS username,
    u.role,
    COALESCE(z.zone_name, 'No zone assigned') AS zone
FROM zones z
RIGHT JOIN users u ON u.zone_id = z.zone_id
ORDER BY u.role, u.username;

-- ─────────────────────────────────────────────────────────────
-- 4. SELF JOIN — find requests in the same zone and disaster
--    (models process interdependency for RAG analysis)
-- ─────────────────────────────────────────────────────────────
SELECT '=== SELF JOIN: pairs of requests in same zone+disaster ===' AS demo;
SELECT
    r1.public_ref  AS request_1,
    r2.public_ref  AS request_2,
    r1.zone_id,
    r1.disaster_id
FROM rescue_requests r1
JOIN rescue_requests r2
    ON  r1.zone_id     = r2.zone_id
    AND r1.disaster_id = r2.disaster_id
    AND r1.request_id  < r2.request_id   -- avoid (A,B) and (B,A) duplicates
ORDER BY r1.zone_id
LIMIT 10;

-- ─────────────────────────────────────────────────────────────
-- 5. Aggregate + GROUP BY + HAVING
--    Zones that have more than 1 pending request
-- ─────────────────────────────────────────────────────────────
SELECT '=== GROUP BY + HAVING: zones with >1 pending request ===' AS demo;
SELECT
    z.zone_name,
    COUNT(rr.request_id)    AS pending_count,
    AVG(rr.severity_snapshot) AS avg_severity,
    SUM(rr.people_affected)   AS total_people
FROM rescue_requests rr
JOIN zones z ON z.zone_id = rr.zone_id
WHERE rr.status NOT IN ('Completed','Cancelled')
GROUP BY z.zone_id, z.zone_name
HAVING COUNT(rr.request_id) > 0
ORDER BY pending_count DESC;

-- ─────────────────────────────────────────────────────────────
-- 6. Correlated Subquery — requests where severity is above
--    the average severity in their own zone
-- ─────────────────────────────────────────────────────────────
SELECT '=== CORRELATED SUBQUERY: high-severity requests in zone ===' AS demo;
SELECT
    rr.public_ref,
    rr.victim_name,
    rr.severity_snapshot,
    rr.zone_id
FROM rescue_requests rr
WHERE rr.severity_snapshot > (
    SELECT AVG(r2.severity_snapshot)
    FROM rescue_requests r2
    WHERE r2.zone_id = rr.zone_id    -- correlated: uses outer rr.zone_id
)
ORDER BY rr.severity_snapshot DESC;

-- ─────────────────────────────────────────────────────────────
-- 7. EXISTS — cities that have at least one active disaster
-- ─────────────────────────────────────────────────────────────
SELECT '=== EXISTS: cities with at least one active disaster ===' AS demo;
SELECT c.city_name, c.state_name
FROM cities c
WHERE EXISTS (
    SELECT 1
    FROM zones z
    JOIN disaster_zone_status dzs ON dzs.zone_id = z.zone_id
    JOIN disasters d ON d.disaster_id = dzs.disaster_id
    WHERE z.city_id = c.city_id
      AND d.status = 'Active'
)
ORDER BY c.city_name;

-- ─────────────────────────────────────────────────────────────
-- 8. NOT EXISTS — assistance types not used in any active request
-- ─────────────────────────────────────────────────────────────
SELECT '=== NOT EXISTS: unused assistance types ===' AS demo;
SELECT at2.type_name, at2.description
FROM assistance_types at2
WHERE NOT EXISTS (
    SELECT 1
    FROM rescue_requests rr
    WHERE rr.assistance_type_id = at2.assistance_type_id
      AND rr.status NOT IN ('Completed','Cancelled')
)
AND at2.is_active = 1;

-- ─────────────────────────────────────────────────────────────
-- 9. IN — requests in zones that have severity >= 4
-- ─────────────────────────────────────────────────────────────
SELECT '=== IN: requests in high-severity zones ===' AS demo;
SELECT rr.public_ref, rr.victim_name, rr.severity_snapshot, rr.status
FROM rescue_requests rr
WHERE rr.zone_id IN (
    SELECT zone_id
    FROM disaster_zone_status
    WHERE severity_level >= 4
)
ORDER BY rr.severity_snapshot DESC;

-- ─────────────────────────────────────────────────────────────
-- 10. NOT IN — resources in zones with no active disasters
-- ─────────────────────────────────────────────────────────────
SELECT '=== NOT IN: resources in zones with no active disasters ===' AS demo;
SELECT r.resource_name, r.status, z.zone_name
FROM resources r
JOIN zones z ON z.zone_id = r.zone_id
WHERE r.zone_id NOT IN (
    SELECT DISTINCT dzs.zone_id
    FROM disaster_zone_status dzs
    JOIN disasters d ON d.disaster_id = dzs.disaster_id
    WHERE d.status = 'Active'
);

-- ─────────────────────────────────────────────────────────────
-- 11. ANY — requests where people_affected is greater than
--     ANY single request in zone 1 (at least one match)
-- ─────────────────────────────────────────────────────────────
SELECT '=== ANY: requests affecting more people than any zone-1 request ===' AS demo;
SELECT rr.public_ref, rr.people_affected, rr.zone_id
FROM rescue_requests rr
WHERE rr.people_affected > ANY (
    SELECT people_affected
    FROM rescue_requests
    WHERE zone_id = 1
)
AND rr.zone_id != 1
ORDER BY rr.people_affected DESC;

-- ─────────────────────────────────────────────────────────────
-- 12. ALL — requests affecting more people than ALL requests
--     in zone 5 (greater than the maximum in zone 5)
-- ─────────────────────────────────────────────────────────────
SELECT '=== ALL: requests affecting more people than every zone-5 request ===' AS demo;
SELECT rr.public_ref, rr.people_affected, rr.zone_id
FROM rescue_requests rr
WHERE rr.people_affected > ALL (
    SELECT people_affected
    FROM rescue_requests
    WHERE zone_id = 5
)
ORDER BY rr.people_affected DESC;

-- ─────────────────────────────────────────────────────────────
-- 13. CASE expression — categorize requests by severity level
-- ─────────────────────────────────────────────────────────────
SELECT '=== CASE: severity category per request ===' AS demo;
SELECT
    rr.public_ref,
    rr.severity_snapshot,
    CASE rr.severity_snapshot
        WHEN 5 THEN 'CRITICAL'
        WHEN 4 THEN 'HIGH'
        WHEN 3 THEN 'MEDIUM'
        WHEN 2 THEN 'LOW'
        ELSE        'MINIMAL'
    END AS severity_label,
    rr.status
FROM rescue_requests rr
ORDER BY rr.severity_snapshot DESC;

-- ─────────────────────────────────────────────────────────────
-- 14. UNION — combine pending requests and available resources
--     into a single operational readiness report
-- ─────────────────────────────────────────────────────────────
SELECT '=== UNION: operational readiness snapshot ===' AS demo;
SELECT 'PENDING REQUEST' AS item_type, public_ref AS identifier,
       CAST(severity_snapshot AS CHAR) AS detail, status
FROM rescue_requests
WHERE status NOT IN ('Completed','Cancelled')
UNION ALL
SELECT 'AVAILABLE RESOURCE', resource_name,
       capacity_unit, status
FROM resources
WHERE status = 'Available'
ORDER BY item_type, identifier;

-- ─────────────────────────────────────────────────────────────
-- 15. String, date, and numeric functions
-- ─────────────────────────────────────────────────────────────
SELECT '=== BUILT-IN FUNCTIONS: string, date, numeric ===' AS demo;
SELECT
    UPPER(rr.victim_name)                                  AS name_upper,
    CHAR_LENGTH(rr.reported_location)                      AS location_len,
    DATE_FORMAT(rr.arrival_time, '%d %b %Y %H:%i')        AS arrival_formatted,
    DATEDIFF(NOW(), rr.arrival_time)                       AS days_since_arrival,
    ROUND((rr.people_affected * 0.5) + (rr.severity_snapshot * 2.0), 2) AS service_time_units,
    CONCAT(rr.public_ref, ' / ', rr.status)                AS ref_status
FROM rescue_requests rr
LIMIT 5;

-- ─────────────────────────────────────────────────────────────
-- 16. NULL handling — requests without a staff creator (public)
-- ─────────────────────────────────────────────────────────────
SELECT '=== NULL HANDLING: public vs staff requests ===' AS demo;
SELECT
    rr.public_ref,
    COALESCE(u.username, 'Public Submission')   AS submitted_by,
    IFNULL(rr.details, 'No additional details') AS details_display
FROM rescue_requests rr
LEFT JOIN users u ON u.user_id = rr.created_by;

-- ─────────────────────────────────────────────────────────────
-- 17. Stored function call — get_severity_for_zone
-- ─────────────────────────────────────────────────────────────
SELECT '=== STORED FUNCTION: get_severity_for_zone ===' AS demo;
SELECT
    d.disaster_name,
    z.zone_name,
    get_severity_for_zone(d.disaster_id, z.zone_id) AS severity
FROM disasters d
CROSS JOIN zones z
WHERE d.status = 'Active'
  AND z.zone_id <= 5
ORDER BY d.disaster_id, z.zone_id;

-- ─────────────────────────────────────────────────────────────
-- 18. Stored function call — calculate_request_service_time
-- ─────────────────────────────────────────────────────────────
SELECT '=== STORED FUNCTION: calculate_request_service_time ===' AS demo;
SELECT
    rr.public_ref,
    rr.people_affected,
    rr.severity_snapshot,
    calculate_request_service_time(rr.people_affected, rr.severity_snapshot) AS service_time_units
FROM rescue_requests rr
ORDER BY service_time_units DESC;

-- ─────────────────────────────────────────────────────────────
-- 19. View usage — pending_rescue_requests
-- ─────────────────────────────────────────────────────────────
SELECT '=== VIEW: pending_rescue_requests ===' AS demo;
SELECT * FROM pending_rescue_requests ORDER BY severity_snapshot DESC;

-- ─────────────────────────────────────────────────────────────
-- 20. View usage — available_resources_summary
-- ─────────────────────────────────────────────────────────────
SELECT '=== VIEW: available_resources_summary ===' AS demo;
SELECT * FROM available_resources_summary ORDER BY resource_type, zone_name;

-- ─────────────────────────────────────────────────────────────
-- 21. EXPLAIN — show query execution plan for request lookup
-- ─────────────────────────────────────────────────────────────
SELECT '=== EXPLAIN: request lookup by status and severity ===' AS demo;
EXPLAIN SELECT rr.public_ref, rr.status, rr.severity_snapshot
FROM rescue_requests rr
WHERE rr.status = 'Ready'
  AND rr.severity_snapshot >= 4;

-- ─────────────────────────────────────────────────────────────
-- 22. TCL: SAVEPOINT demo (inside a transaction that we roll back)
-- ─────────────────────────────────────────────────────────────
SELECT '=== TCL: SAVEPOINT + ROLLBACK TO SAVEPOINT demo ===' AS demo;
START TRANSACTION;
    -- Simulate partial work: update a request status
    UPDATE rescue_requests SET status = 'New' WHERE public_ref = 'DRERO-005';
    SAVEPOINT before_audit_insert;
    -- Simulate an audit record
    INSERT INTO audit_log (action, entity_type, entity_id, details)
    VALUES ('TEST_SAVEPOINT', 'rescue_request', 5, 'Savepoint demonstration');
    -- Roll back only the audit insert, keep the status update
    ROLLBACK TO SAVEPOINT before_audit_insert;
    -- Roll back everything — leaving data unchanged
ROLLBACK;
SELECT 'SAVEPOINT demo complete - all changes rolled back' AS result;

-- ─────────────────────────────────────────────────────────────
-- 23. Scheduling performance via view
-- ─────────────────────────────────────────────────────────────
SELECT '=== VIEW: scheduling_performance_summary ===' AS demo;
SELECT
    algorithm,
    time_quantum,
    requests_scheduled,
    ROUND(avg_waiting_time, 2)    AS avg_wait,
    ROUND(avg_turnaround_time, 2) AS avg_tat,
    ROUND(avg_response_time, 2)   AS avg_resp
FROM scheduling_performance_summary
WHERE requests_scheduled > 0
ORDER BY avg_waiting_time;

-- ─────────────────────────────────────────────────────────────
-- 24. Audit report via view
-- ─────────────────────────────────────────────────────────────
SELECT '=== VIEW: audit_activity_report (last 10) ===' AS demo;
SELECT audit_id, actor, action, entity_type, action_time, details
FROM audit_activity_report
LIMIT 10;
