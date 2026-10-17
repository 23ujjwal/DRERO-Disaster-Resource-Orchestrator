-- DRERO: verify.sql
-- Checks row counts in all 17 tables and all 5 views.
-- Every table must have >= 10 rows after seed.sql is applied.
-- Run as: mysql -h <host> -u drero_app -p drero_db < verify.sql

SELECT '=== TABLE ROW COUNTS ===' AS info;-- system hang being state of art is really nice which is co

SELECT tbl, cnt,
       CASE WHEN cnt >= 10 THEN 'OK (>=10)'
            ELSE CONCAT('WARNING: only ', cnt, ' rows') END AS status
FROM (
    SELECT 'cities'                  AS tbl, COUNT(*) AS cnt FROM cities                  UNION ALL
    SELECT 'zones',                           COUNT(*) FROM zones                         UNION ALL
    SELECT 'localities',                      COUNT(*) FROM localities                    UNION ALL
    SELECT 'disasters',                       COUNT(*) FROM disasters                     UNION ALL
    SELECT 'disaster_zone_status',            COUNT(*) FROM disaster_zone_status          UNION ALL
    SELECT 'users',                           COUNT(*) FROM users                         UNION ALL
    SELECT 'assistance_types',                COUNT(*) FROM assistance_types              UNION ALL
    SELECT 'rescue_requests',                 COUNT(*) FROM rescue_requests               UNION ALL
    SELECT 'request_status_history',          COUNT(*) FROM request_status_history        UNION ALL
    SELECT 'resource_types',                  COUNT(*) FROM resource_types               UNION ALL
    SELECT 'resources',                       COUNT(*) FROM resources                     UNION ALL
    SELECT 'allocations',                     COUNT(*) FROM allocations                   UNION ALL
    SELECT 'resource_status_history',         COUNT(*) FROM resource_status_history       UNION ALL
    SELECT 'scheduling_runs',                 COUNT(*) FROM scheduling_runs               UNION ALL
    SELECT 'scheduling_log',                  COUNT(*) FROM scheduling_log                UNION ALL
    SELECT 'audit_log',                       COUNT(*) FROM audit_log                     UNION ALL
    SELECT 'deadlock_events',                 COUNT(*) FROM deadlock_events
) counts;

SELECT '=== VIEWS ACCESSIBLE ===' AS info;

SELECT 'pending_rescue_requests'      AS view_name, COUNT(*) AS rows FROM pending_rescue_requests      UNION ALL
SELECT 'available_resources_summary',              COUNT(*) FROM available_resources_summary           UNION ALL
SELECT 'allocation_details',                       COUNT(*) FROM allocation_details                    UNION ALL
SELECT 'scheduling_performance_summary',           COUNT(*) FROM scheduling_performance_summary        UNION ALL
SELECT 'audit_activity_report',                    COUNT(*) FROM audit_activity_report;
