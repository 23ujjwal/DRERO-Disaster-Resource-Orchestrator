-- Repeatable MySQL-client migration. Run from the project root as a DB
-- administrator after checking SHOW TRIGGERS and applying the risk-level
-- migration when upgrading a database created from the older schema.
USE drero_db;

-- Verified with the backend TCP settings using CURRENT_USER().
-- Explicitly reset the target so a stale session variable cannot select another account.
SET @drero_app_host = '172.24.143.176';

CREATE TABLE IF NOT EXISTS scheduling_performance_summary_cache (
    run_id                 INT UNSIGNED NOT NULL PRIMARY KEY,
    algorithm              ENUM('FCFS','SJF','RR','PRIORITY') NOT NULL,
    time_quantum           SMALLINT UNSIGNED DEFAULT NULL,
    requests_scheduled     INT UNSIGNED NOT NULL DEFAULT 0,
    avg_waiting_time       DECIMAL(12,3) DEFAULT NULL,
    avg_turnaround_time    DECIMAL(12,3) DEFAULT NULL,
    avg_response_time      DECIMAL(12,3) DEFAULT NULL,
    throughput             DECIMAL(16,6) NOT NULL DEFAULT 0,
    starvation_count       INT UNSIGNED NOT NULL DEFAULT 0,
    started_at             DATETIME NOT NULL,
    finished_at            DATETIME DEFAULT NULL,
    FOREIGN KEY fk_summary_run (run_id)
        REFERENCES scheduling_runs(run_id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4
  COMMENT='Precomputed scheduling report summary; refreshed when runs finish';

SOURCE database/schema_routines.sql
