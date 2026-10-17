-- ============================================================
-- DRERO — schema_routines.sql
-- Triggers, Stored Functions, Stored Procedures, MySQL Roles.
--
-- HOW TO RUN (Windows MySQL, run as root or admin user):
--
--   Step 1 (run ONCE as root in Windows MySQL Workbench or cmd):
--     SET GLOBAL log_bin_trust_function_creators = 1;
--
--   Step 2 (run this file):
--     SOURCE /path/to/schema_routines.sql;
--   or:
--     mysql -u root -p drero_db < schema_routines.sql
--
-- WHY: MySQL requires SUPER privilege or log_bin_trust_function_creators=1
--      to create stored routines/triggers when binary logging is enabled.
-- ============================================================

USE drero_db;

-- ============================================================
-- TRIGGERS — automatic actions on table events
-- ============================================================

-- Automatically records every rescue request status change in history
DROP TRIGGER IF EXISTS record_request_status_change;--syetem
DELIMITER $$
CREATE TRIGGER record_request_status_change
AFTER UPDATE ON rescue_requests
FOR EACH ROW
BEGIN
    IF OLD.status != NEW.status THEN
        INSERT INTO request_status_history
            (request_id, old_status, new_status, changed_by, remarks)
        VALUES
            (NEW.request_id, OLD.status, NEW.status, NULL,
             'Auto-recorded by trigger on direct UPDATE');
    END IF;
END$$
DELIMITER ;

-- Automatically records every resource availability change in history
DROP TRIGGER IF EXISTS record_resource_status_change;
DELIMITER $$
CREATE TRIGGER record_resource_status_change
AFTER UPDATE ON resources
FOR EACH ROW
BEGIN
    IF OLD.status != NEW.status THEN
        INSERT INTO resource_status_history
            (resource_id, old_status, new_status, changed_by, remarks)
        VALUES
            (NEW.resource_id, OLD.status, NEW.status, NULL,
             'Auto-recorded by trigger on direct UPDATE');
    END IF;
END$$
DELIMITER ;

-- Writes an audit log entry whenever a new resource is created
DROP TRIGGER IF EXISTS audit_new_resource_creation;
DELIMITER $$
CREATE TRIGGER audit_new_resource_creation
AFTER INSERT ON resources
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
    VALUES (NEW.created_by, 'RESOURCE_CREATED', 'resource', NEW.resource_id,
            CONCAT('name=', NEW.resource_name, ' zone=', NEW.zone_id));
END$$
DELIMITER ;

-- Writes an audit log entry whenever a new disaster event is created
DROP TRIGGER IF EXISTS audit_new_disaster_creation;
DELIMITER $$
CREATE TRIGGER audit_new_disaster_creation
AFTER INSERT ON disasters
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
    VALUES (NEW.created_by, 'DISASTER_CREATED', 'disaster', NEW.disaster_id,
            CONCAT('name=', NEW.disaster_name, ' type=', NEW.disaster_type));
END$$
DELIMITER ;

-- ============================================================
-- MySQL ROLES — role-based access control (DCL)
-- ============================================================
CREATE ROLE IF NOT EXISTS 'disaster_dispatcher';
CREATE ROLE IF NOT EXISTS 'zone_administrator';
CREATE ROLE IF NOT EXISTS 'system_auditor';

-- Dispatcher: manage requests, allocations, run scheduling, view resources
GRANT SELECT, INSERT, UPDATE ON drero_db.rescue_requests   TO 'disaster_dispatcher';
GRANT SELECT, INSERT, UPDATE ON drero_db.allocations       TO 'disaster_dispatcher';
GRANT SELECT, INSERT         ON drero_db.scheduling_runs   TO 'disaster_dispatcher';
GRANT SELECT, INSERT         ON drero_db.scheduling_log    TO 'disaster_dispatcher';
GRANT SELECT                 ON drero_db.resources         TO 'disaster_dispatcher';
GRANT SELECT                 ON drero_db.audit_log         TO 'disaster_dispatcher';

-- Zone Admin: manage resources and zones in their area
GRANT SELECT, INSERT, UPDATE ON drero_db.resources         TO 'zone_administrator';
GRANT SELECT, INSERT, UPDATE ON drero_db.zones             TO 'zone_administrator';
GRANT SELECT                 ON drero_db.rescue_requests   TO 'zone_administrator';
GRANT SELECT                 ON drero_db.allocations       TO 'zone_administrator';

-- Auditor: read-only access to everything
GRANT SELECT                 ON drero_db.*                 TO 'system_auditor';

-- ============================================================
-- STORED FUNCTIONS — reusable calculations
-- ============================================================

-- Returns the severity level for a given disaster and zone combination
DROP FUNCTION IF EXISTS get_severity_for_zone;
DELIMITER $$
CREATE FUNCTION get_severity_for_zone(
    p_disaster_id INT UNSIGNED,
    p_zone_id     INT UNSIGNED
)
RETURNS TINYINT
NOT DETERMINISTIC
READS SQL DATA
SQL SECURITY INVOKER
COMMENT 'Returns severity_level for (disaster, zone) pair; defaults to 1 if not found'
BEGIN
    DECLARE v_sev TINYINT DEFAULT 1;
    SELECT severity_level INTO v_sev
    FROM disaster_zone_status
    WHERE disaster_id = p_disaster_id AND zone_id = p_zone_id
    LIMIT 1;
    RETURN COALESCE(v_sev, 1);
END$$
DELIMITER ;

-- Calculates estimated processing time for a rescue request (OS simulation)
-- Formula mirrors C++ pcb_service_time(): people*0.5 + severity*2.0 units
-- 1 unit = 10 simulated minutes
DROP FUNCTION IF EXISTS calculate_request_service_time;
DELIMITER $$
CREATE FUNCTION calculate_request_service_time(
    p_people   INT,
    p_severity TINYINT
)
RETURNS DECIMAL(8,2)
DETERMINISTIC
NO SQL
SQL SECURITY INVOKER
COMMENT 'Simulated OS service time. 1 unit = 10 sim minutes. Mirrors C++ pcb_service_time()'
BEGIN
    RETURN ROUND((p_people * 0.5) + (p_severity * 2.0), 2);
END$$
DELIMITER ;

-- Calculates scheduling priority score (mirrors C++ pcb_priority_score())
DROP FUNCTION IF EXISTS calculate_priority_score;
DELIMITER $$
CREATE FUNCTION calculate_priority_score(
    p_severity      TINYINT,
    p_people        INT,
    p_waiting_units DECIMAL(8,2)
)
RETURNS DECIMAL(10,4)
DETERMINISTIC
NO SQL
SQL SECURITY INVOKER
COMMENT 'Priority score: severity*20 + min(people,50)*0.6 + waiting*0.4. Mirrors C++ pcb_priority_score()'
BEGIN
    RETURN ROUND(
        (p_severity * 20.0)
        + (LEAST(p_people, 50) * 0.6)
        + (p_waiting_units * 0.4)
    , 4);
END$$
DELIMITER ;

-- ============================================================
-- STORED PROCEDURES — complex multi-step operations
-- ============================================================

-- Submits a new rescue request with full transaction safety
DROP PROCEDURE IF EXISTS submit_rescue_request;
DELIMITER $$
CREATE PROCEDURE submit_rescue_request(
    IN  p_disaster_id        INT UNSIGNED,
    IN  p_locality_id        INT UNSIGNED,
    IN  p_victim_name        VARCHAR(120),
    IN  p_contact            VARCHAR(20),
    IN  p_location           VARCHAR(255),
    IN  p_assistance_type_id INT UNSIGNED,
    IN  p_people             SMALLINT UNSIGNED,
    IN  p_details            TEXT,
    OUT p_request_id         INT UNSIGNED,
    OUT p_public_ref         VARCHAR(20),
    OUT p_severity           TINYINT
)
SQL SECURITY INVOKER
COMMENT 'Submit a rescue request: resolves zone+severity, inserts, logs submission.'
BEGIN
    DECLARE v_zone_id  INT UNSIGNED;
    DECLARE v_severity TINYINT DEFAULT 1;
    DECLARE v_ref      VARCHAR(20);
    DECLARE v_attempt  INT DEFAULT 0;
    DECLARE v_exists   INT DEFAULT 1;

    SELECT zone_id INTO v_zone_id
    FROM localities WHERE locality_id = p_locality_id AND is_active = 1 LIMIT 1;

    IF v_zone_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Invalid or inactive locality';
    END IF;

    SELECT severity_level INTO v_severity
    FROM disaster_zone_status
    WHERE disaster_id = p_disaster_id AND zone_id = v_zone_id LIMIT 1;
    SET v_severity = COALESCE(v_severity, 1);

    generate_ref: LOOP
        SET v_attempt = v_attempt + 1;
        SET v_ref = CONCAT('REQ-', LPAD(FLOOR(RAND()*90000+10000),5,'0'),
                           '-', DATE_FORMAT(NOW(),'%y%m%d'));
        SELECT COUNT(*) INTO v_exists FROM rescue_requests WHERE public_ref = v_ref;
        IF v_exists = 0 OR v_attempt >= 5 THEN LEAVE generate_ref; END IF;
    END LOOP;

    START TRANSACTION;
        INSERT INTO rescue_requests
            (public_ref, disaster_id, locality_id, zone_id,
             victim_name, contact_number, reported_location,
             assistance_type_id, people_affected, details,
             severity_snapshot, status, arrival_time)
        VALUES
            (v_ref, p_disaster_id, p_locality_id, v_zone_id,
             p_victim_name, p_contact, p_location,
             p_assistance_type_id, p_people, p_details,
             v_severity, 'Submitted', NOW());

        SET p_request_id = LAST_INSERT_ID();
        SET p_public_ref = v_ref;
        SET p_severity   = v_severity;

        INSERT INTO request_status_history
            (request_id, old_status, new_status, changed_by, remarks)
        VALUES (p_request_id, 'NONE', 'Submitted', NULL,
                'Initial submission via submit_rescue_request');

        INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
        VALUES (NULL, 'REQUEST_SUBMITTED', 'rescue_request', p_request_id,
                CONCAT('ref=', v_ref, ' disaster=', p_disaster_id,
                       ' zone=', v_zone_id, ' severity=', v_severity));
    COMMIT;
END$$
DELIMITER ;

-- Atomically allocates a resource with double-allocation prevention
DROP PROCEDURE IF EXISTS allocate_resource_to_request;
DELIMITER $$
CREATE PROCEDURE allocate_resource_to_request(
    IN  p_request_id  INT UNSIGNED,
    IN  p_resource_id INT UNSIGNED,
    IN  p_user_id     INT UNSIGNED,
    OUT p_result      VARCHAR(120)
)
SQL SECURITY INVOKER
COMMENT 'Atomically allocates a resource. Prevents double-allocation with FOR UPDATE + transaction.'
BEGIN
    DECLARE v_res_status  VARCHAR(20) DEFAULT NULL;
    DECLARE v_req_status  VARCHAR(20) DEFAULT NULL;
    DECLARE v_alloc_count INT DEFAULT 0;
    DECLARE v_alloc_id    INT UNSIGNED;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        SET p_result = 'ERROR: transaction rolled back due to SQL exception';
    END;

    START TRANSACTION;

    SELECT status INTO v_res_status
    FROM resources WHERE resource_id = p_resource_id FOR UPDATE;

    SELECT status INTO v_req_status
    FROM rescue_requests WHERE request_id = p_request_id FOR UPDATE;

    SELECT COUNT(*) INTO v_alloc_count
    FROM allocations WHERE resource_id = p_resource_id AND status = 'Active';

    IF v_res_status IS NULL THEN
        ROLLBACK; SET p_result = 'FAIL: resource not found';
    ELSEIF v_res_status != 'Available' THEN
        ROLLBACK; SET p_result = CONCAT('FAIL: resource status is ', v_res_status);
    ELSEIF v_req_status IS NULL THEN
        ROLLBACK; SET p_result = 'FAIL: request not found';
    ELSEIF v_req_status NOT IN ('Ready','Scheduled','Under Review') THEN
        ROLLBACK; SET p_result = CONCAT('FAIL: request status is ', v_req_status);
    ELSEIF v_alloc_count > 0 THEN
        ROLLBACK; SET p_result = 'FAIL: double-allocation prevented';
    ELSE
        INSERT INTO allocations (request_id, resource_id, allocated_by, status)
        VALUES (p_request_id, p_resource_id, p_user_id, 'Active');
        SET v_alloc_id = LAST_INSERT_ID();

        UPDATE resources SET status='Allocated', updated_at=NOW()
        WHERE resource_id = p_resource_id;

        UPDATE rescue_requests SET status='Allocated', updated_at=NOW()
        WHERE request_id = p_request_id;

        SAVEPOINT after_status_updates;

        INSERT INTO resource_status_history
            (resource_id, old_status, new_status, changed_by, remarks)
        VALUES (p_resource_id, 'Available', 'Allocated', p_user_id,
                CONCAT('Allocated to request #', p_request_id));

        INSERT INTO request_status_history
            (request_id, old_status, new_status, changed_by, remarks)
        VALUES (p_request_id, v_req_status, 'Allocated', p_user_id,
                CONCAT('Resource #', p_resource_id, ' assigned'));

        INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
        VALUES (p_user_id, 'RESOURCE_ALLOCATED', 'allocation', v_alloc_id,
                CONCAT('request=', p_request_id, ' resource=', p_resource_id));

        COMMIT;
        SET p_result = CONCAT('OK: allocation_id=', v_alloc_id);
    END IF;
END$$
DELIMITER ;

-- Releases an active allocation, marks resource Available and request Completed
DROP PROCEDURE IF EXISTS release_resource_allocation;
DELIMITER $$
CREATE PROCEDURE release_resource_allocation(
    IN  p_allocation_id INT UNSIGNED,
    IN  p_user_id       INT UNSIGNED,
    OUT p_result        VARCHAR(120)
)
SQL SECURITY INVOKER
COMMENT 'Releases active allocation; marks resource Available and request Completed.'
BEGIN
    DECLARE v_resource_id INT UNSIGNED DEFAULT NULL;
    DECLARE v_request_id  INT UNSIGNED DEFAULT NULL;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN ROLLBACK; SET p_result = 'ERROR: transaction rolled back'; END;

    START TRANSACTION;

    SELECT resource_id, request_id INTO v_resource_id, v_request_id
    FROM allocations WHERE allocation_id = p_allocation_id AND status = 'Active'
    FOR UPDATE;

    IF v_resource_id IS NULL THEN
        ROLLBACK; SET p_result = 'FAIL: active allocation not found';
    ELSE
        UPDATE allocations SET status='Released', released_at=NOW()
        WHERE allocation_id = p_allocation_id;

        UPDATE resources SET status='Available', updated_at=NOW()
        WHERE resource_id = v_resource_id;

        UPDATE rescue_requests SET status='Completed', updated_at=NOW()
        WHERE request_id = v_request_id;

        INSERT INTO resource_status_history
            (resource_id, old_status, new_status, changed_by, remarks)
        VALUES (v_resource_id, 'Allocated', 'Available', p_user_id,
                CONCAT('Released from allocation #', p_allocation_id));

        INSERT INTO request_status_history
            (request_id, old_status, new_status, changed_by, remarks)
        VALUES (v_request_id, 'Allocated', 'Completed', p_user_id,
                CONCAT('Resource released, allocation #', p_allocation_id));

        INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
        VALUES (p_user_id, 'RESOURCE_RELEASED', 'allocation', p_allocation_id,
                CONCAT('resource=', v_resource_id, ' request=', v_request_id));

        COMMIT;
        SET p_result = CONCAT('OK: resource=', v_resource_id,
                              ' request=', v_request_id, ' Completed');
    END IF;
END$$
DELIMITER ;

-- Changes request status with validation and history recording
DROP PROCEDURE IF EXISTS change_request_status;
DELIMITER $$
CREATE PROCEDURE change_request_status(
    IN  p_request_id INT UNSIGNED,
    IN  p_new_status VARCHAR(20),
    IN  p_user_id    INT UNSIGNED,
    IN  p_remarks    VARCHAR(255),
    OUT p_result     VARCHAR(120)
)
SQL SECURITY INVOKER
COMMENT 'Changes request status with transition validation, SAVEPOINT, history and audit.'
BEGIN
    DECLARE v_old_status VARCHAR(20) DEFAULT NULL;
    DECLARE v_valid      INT DEFAULT 0;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN ROLLBACK; SET p_result = 'ERROR: transaction rolled back'; END;

    START TRANSACTION;

    SELECT status INTO v_old_status
    FROM rescue_requests WHERE request_id = p_request_id FOR UPDATE;

    IF v_old_status IS NULL THEN
        ROLLBACK; SET p_result = 'FAIL: request not found';
    ELSE
        SET v_valid = CASE
            WHEN v_old_status='Submitted'    AND p_new_status IN ('New','Cancelled')                   THEN 1
            WHEN v_old_status='New'          AND p_new_status IN ('Ready','Cancelled')                 THEN 1
            WHEN v_old_status='Ready'        AND p_new_status IN ('Scheduled','Deferred','Cancelled')  THEN 1
            WHEN v_old_status='Scheduled'    AND p_new_status IN ('Under Review','Deferred','Cancelled') THEN 1
            WHEN v_old_status='Under Review' AND p_new_status IN ('Allocated','Deferred','Cancelled')  THEN 1
            WHEN v_old_status='Deferred'     AND p_new_status IN ('Ready','Cancelled')                 THEN 1
            ELSE 0
        END;

        IF v_valid = 0 THEN
            ROLLBACK;
            SET p_result = CONCAT('FAIL: invalid transition ', v_old_status, ' -> ', p_new_status);
        ELSE
            UPDATE rescue_requests
            SET status = p_new_status, updated_at = NOW()
            WHERE request_id = p_request_id;

            SAVEPOINT after_status_update;

            INSERT INTO request_status_history
                (request_id, old_status, new_status, changed_by, remarks)
            VALUES (p_request_id, v_old_status, p_new_status, p_user_id, p_remarks);

            INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
            VALUES (p_user_id, 'STATUS_CHANGED', 'rescue_request', p_request_id,
                    CONCAT(v_old_status, ' -> ', p_new_status));

            COMMIT;
            SET p_result = CONCAT('OK: ', v_old_status, ' -> ', p_new_status);
        END IF;
    END IF;
END$$
DELIMITER ;
