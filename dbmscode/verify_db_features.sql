-- Run as a database administrator after applying the feature migration.
USE drero_db;

SELECT VERSION() AS server_version, @@version_comment AS server_edition;

SHOW TRIGGERS WHERE `Trigger` IN
    ('audit_new_resource_creation', 'audit_new_disaster_creation');
SHOW CREATE TRIGGER audit_new_resource_creation;
SHOW CREATE TRIGGER audit_new_disaster_creation;

-- This matches the former C++ lookup: return the row's severity, or 1 if absent.
SELECT COUNT(*) AS severity_function_mismatches
FROM disaster_zone_status dz
WHERE get_severity_for_zone(dz.disaster_id, dz.zone_id)
      <> COALESCE(dz.severity_level, 1);
SELECT get_severity_for_zone(4294967295, 4294967295) AS missing_zone_default,
       get_severity_for_zone(4294967295, 4294967295) = 1 AS matches_cpp_default;

-- Compare the stored scheduling function to the C++ formula on a representative input.
SELECT calculate_request_service_time(3, 4) AS stored_result,
       (3 * 0.5) + (4 * 2.0) AS cpp_formula,
       calculate_request_service_time(3, 4) =
           ((3 * 0.5) + (4 * 2.0)) AS matches_cpp_formula;

-- Check the backfilled summary against its source view and scheduling log.
SELECT COUNT(*) AS missing_summary_rows
FROM scheduling_runs sr
LEFT JOIN scheduling_performance_summary_cache cache
  ON cache.run_id = sr.run_id
WHERE cache.run_id IS NULL;

SELECT COUNT(*) AS summary_mismatches
FROM scheduling_performance_summary_cache cache
JOIN scheduling_performance_summary live ON live.run_id = cache.run_id
LEFT JOIN (
    SELECT run_id,
           CASE WHEN COALESCE(MAX(completion_time), 0) > 0
                THEN CAST(COUNT(*) AS DECIMAL(20,10)) / MAX(completion_time) ELSE 0 END AS throughput,
           COALESCE(SUM(CASE WHEN waiting_time > 20 THEN 1 ELSE 0 END), 0)
               AS starvation_count
    FROM scheduling_log
    GROUP BY run_id
) metrics ON metrics.run_id = cache.run_id
WHERE cache.requests_scheduled <> live.requests_scheduled
   OR NOT (cache.avg_waiting_time <=> ROUND(live.avg_waiting_time, 3))
   OR NOT (cache.avg_turnaround_time <=> ROUND(live.avg_turnaround_time, 3))
   OR NOT (cache.avg_response_time <=> ROUND(live.avg_response_time, 3))
   OR cache.throughput <> ROUND(COALESCE(metrics.throughput, 0), 6)
   OR cache.starvation_count <> COALESCE(metrics.starvation_count, 0);

-- A deliberately invalid transition must leave request, history, and audit unchanged.
-- Use a database with at least one request; request_id 0 safely exercises "not found"
-- if the database is empty, but the unchanged flags then report NULL.
SET @drero_rollback_request_id = (SELECT MIN(request_id) FROM rescue_requests);
SET @drero_rollback_status = NULL;
SELECT status INTO @drero_rollback_status
FROM rescue_requests WHERE request_id = @drero_rollback_request_id;
SET @drero_rollback_history_count =
    (SELECT COUNT(*) FROM request_status_history
     WHERE request_id = @drero_rollback_request_id);
SET @drero_rollback_audit_count =
    (SELECT COUNT(*) FROM audit_log
     WHERE action = 'UPDATE_STATUS'
       AND entity_type = 'rescue_requests'
       AND entity_id = @drero_rollback_request_id);
SET @drero_rollback_result = NULL;
CALL change_request_status(
    COALESCE(@drero_rollback_request_id, 0),
    '__invalid__', NULL, 'rollback verification', @drero_rollback_result);
SELECT @drero_rollback_result AS expected_failure,
       IF(@drero_rollback_request_id IS NULL, NULL,
          (SELECT status <=> @drero_rollback_status
           FROM rescue_requests
           WHERE request_id = @drero_rollback_request_id)) AS status_unchanged,
       IF(@drero_rollback_request_id IS NULL, NULL,
          (SELECT COUNT(*) = @drero_rollback_history_count
           FROM request_status_history
           WHERE request_id = @drero_rollback_request_id)) AS history_unchanged,
       IF(@drero_rollback_request_id IS NULL, NULL,
          (SELECT COUNT(*) = @drero_rollback_audit_count
           FROM audit_log
           WHERE action = 'UPDATE_STATUS'
             AND entity_type = 'rescue_requests'
             AND entity_id = @drero_rollback_request_id)) AS audit_unchanged;

-- A transaction-scoped probe proves the disaster trigger writes exactly one row;
-- rolling back removes both the test disaster and its audit row.
START TRANSACTION;
SET @drero_trigger_probe = CONCAT('DRERO trigger probe ', UUID());
INSERT INTO disasters
    (disaster_name, disaster_type, description, start_time, status, created_by)
VALUES
    (@drero_trigger_probe, 'TriggerAuditTest', NULL, NOW(), 'Monitoring', NULL);
SET @drero_trigger_probe_id = LAST_INSERT_ID();
SELECT COUNT(*) AS disaster_trigger_audit_rows
FROM audit_log
WHERE action = 'DISASTER_CREATED'
  AND entity_type = 'disaster'
  AND entity_id = @drero_trigger_probe_id;
ROLLBACK;
