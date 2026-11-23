
USE drero_db;

-- History recording is the responsibility of the calling code.
DROP TRIGGER IF EXISTS record_request_status_change;
DROP TRIGGER IF EXISTS record_resource_status_change;

-- ─────────────────────────────────────────────────────────────
-- Trigger 1: audit_new_resource_creation
-- Fires AFTER a new row is inserted into the resources table.
-- Writes one audit_log row so we always know when and by whom
-- a resource was added to the system.
-- Uses NEW to read the just-inserted values.
-- ─────────────────────────────────────────────────────────────
DROP TRIGGER IF EXISTS audit_new_resource_creation;
DELIMITER $$
CREATE TRIGGER audit_new_resource_creation
AFTER INSERT ON resources
FOR EACH ROW
BEGIN
    INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
    VALUES (NEW.created_by, 'INSERT_RESOURCE', 'resources', NEW.resource_id,
            CONCAT('Added: ', NEW.resource_name));
END$$
DELIMITER ;

-- ─────────────────────────────────────────────────────────────
-- Trigger 2: audit_new_disaster_creation
-- Fires AFTER a new row is inserted into the disasters table.
-- Writes one audit_log row so there is always a record of
-- which disaster was declared and what type it is.
-- Uses NEW to read the just-inserted values.
-- ─────────────────────────────────────────────────────────────
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
CREATE ROLE IF NOT EXISTS 'drero_application_role';

GRANT SELECT ON drero_db.cities TO 'drero_application_role';
GRANT SELECT ON drero_db.zones TO 'drero_application_role';
GRANT SELECT ON drero_db.localities TO 'drero_application_role';
GRANT SELECT ON drero_db.disasters TO 'drero_application_role';
GRANT SELECT ON drero_db.disaster_zone_status TO 'drero_application_role';
GRANT SELECT ON drero_db.users TO 'drero_application_role';
GRANT SELECT ON drero_db.assistance_types TO 'drero_application_role';
GRANT SELECT ON drero_db.rescue_requests TO 'drero_application_role';
GRANT SELECT, INSERT ON drero_db.request_status_history TO 'drero_application_role';
GRANT SELECT ON drero_db.resource_types TO 'drero_application_role';
GRANT SELECT ON drero_db.resources TO 'drero_application_role';
GRANT SELECT ON drero_db.allocations TO 'drero_application_role';
GRANT SELECT, INSERT ON drero_db.resource_status_history TO 'drero_application_role';
GRANT SELECT ON drero_db.scheduling_runs TO 'drero_application_role';
GRANT SELECT ON drero_db.scheduling_log TO 'drero_application_role';
GRANT SELECT, INSERT ON drero_db.audit_log TO 'drero_application_role';
GRANT SELECT, INSERT, UPDATE ON drero_db.deadlock_events TO 'drero_application_role';
GRANT SELECT ON drero_db.pending_rescue_requests TO 'drero_application_role';
GRANT SELECT ON drero_db.available_resources_summary TO 'drero_application_role';
GRANT SELECT ON drero_db.allocation_details TO 'drero_application_role';
GRANT SELECT, INSERT, UPDATE
    ON drero_db.scheduling_performance_summary_cache TO 'drero_application_role';
GRANT SELECT ON drero_db.audit_activity_report TO 'drero_application_role';

GRANT INSERT, UPDATE ON drero_db.rescue_requests TO 'drero_application_role';
GRANT INSERT, UPDATE ON drero_db.allocations TO 'drero_application_role';
GRANT INSERT, UPDATE ON drero_db.scheduling_runs TO 'drero_application_role';
GRANT INSERT ON drero_db.scheduling_log TO 'drero_application_role';
GRANT INSERT, UPDATE ON drero_db.resources TO 'drero_application_role';
GRANT UPDATE ON drero_db.disaster_zone_status TO 'drero_application_role';

-- Set this session variable before SOURCE when MySQL selects a host-specific account.
SET @drero_app_host = COALESCE(@drero_app_host, '172.24.143.176');
SET @drero_app_grantee = CONCAT(QUOTE('drero_app'), '@', QUOTE(@drero_app_host));
-- GRANT OPTION is revoked separately; ALL PRIVILEGES excludes it.
SELECT IF(COUNT(*) > 0,
    CONCAT('REVOKE GRANT OPTION ON drero_db.* FROM ', @drero_app_grantee),
    'SELECT 1') INTO @drero_revoke_option
FROM information_schema.SCHEMA_PRIVILEGES
WHERE GRANTEE = @drero_app_grantee AND TABLE_SCHEMA = 'drero_db' AND IS_GRANTABLE = 'YES';
PREPARE drero_option_stmt FROM @drero_revoke_option;
EXECUTE drero_option_stmt;
DEALLOCATE PREPARE drero_option_stmt;

-- GRANT OPTION is revoked separately; ALL PRIVILEGES excludes it.
SELECT IF(COUNT(*) > 0,
    CONCAT('REVOKE GRANT OPTION ON *.* FROM ', @drero_app_grantee),
    'SELECT 1') INTO @drero_revoke_option
FROM information_schema.USER_PRIVILEGES
WHERE GRANTEE = @drero_app_grantee AND IS_GRANTABLE = 'YES';
PREPARE drero_option_stmt FROM @drero_revoke_option;
EXECUTE drero_option_stmt;
DEALLOCATE PREPARE drero_option_stmt;

SELECT IF(
    COUNT(*) > 0,
    CONCAT('REVOKE ALL PRIVILEGES ON drero_db.* FROM ', @drero_app_grantee),
    'SELECT 1'
) INTO @drero_revoke_schema_grants
FROM information_schema.SCHEMA_PRIVILEGES
WHERE GRANTEE = @drero_app_grantee AND TABLE_SCHEMA = 'drero_db';
PREPARE drero_revoke_stmt FROM @drero_revoke_schema_grants;
EXECUTE drero_revoke_stmt;
DEALLOCATE PREPARE drero_revoke_stmt;

SELECT IF(
    COUNT(*) > 0,
    CONCAT('REVOKE ALL PRIVILEGES ON *.* FROM ', @drero_app_grantee),
    'SELECT 1'
) INTO @drero_revoke_global_grants
FROM information_schema.USER_PRIVILEGES
WHERE GRANTEE = @drero_app_grantee AND PRIVILEGE_TYPE <> 'USAGE';
PREPARE drero_revoke_global_stmt FROM @drero_revoke_global_grants;
EXECUTE drero_revoke_global_stmt;
DEALLOCATE PREPARE drero_revoke_global_stmt;

SET @drero_role_sql = CONCAT('GRANT ''drero_application_role'' TO ', @drero_app_grantee);
PREPARE drero_role_stmt FROM @drero_role_sql;
EXECUTE drero_role_stmt;
DEALLOCATE PREPARE drero_role_stmt;
SET @drero_role_sql = CONCAT('SET DEFAULT ROLE ''drero_application_role'' TO ', @drero_app_grantee);
PREPARE drero_role_stmt FROM @drero_role_sql;
EXECUTE drero_role_stmt;
DEALLOCATE PREPARE drero_role_stmt;

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

-- ─────────────────────────────────────────────────────────────
-- Function 1: get_severity_for_zone
-- Returns the numeric severity level (1–5) for a given disaster
-- and zone pair by reading the disaster_zone_status table.
-- If no row exists for that combination, it defaults to 1.
-- Called by demo_queries.sql and the C++ scheduler.
-- ─────────────────────────────────────────────────────────────
DROP FUNCTION IF EXISTS get_severity_for_zone;
DELIMITER $$
CREATE FUNCTION get_severity_for_zone(
    p_disaster_id INT UNSIGNED,   -- the disaster to look up
    p_zone_id     INT UNSIGNED    -- the zone to look up
)
RETURNS TINYINT
NOT DETERMINISTIC   -- result may differ each call (reads from a live table)
READS SQL DATA      -- tells MySQL this function only reads, never writes
SQL SECURITY INVOKER
COMMENT 'Returns severity_level for (disaster, zone) pair; defaults to 1 if not found'
BEGIN
    -- Declare a variable to store the result; default is 1 (lowest severity)
    DECLARE v_sev TINYINT DEFAULT 1;

    -- Look up the severity for this specific disaster-zone combination
    SELECT severity_level INTO v_sev
    FROM disaster_zone_status
    WHERE disaster_id = p_disaster_id AND zone_id = p_zone_id
    LIMIT 1;

    -- COALESCE returns v_sev if it has a value, or 1 if no row was found
    RETURN COALESCE(v_sev, 1);
END$$
DELIMITER ;

-- ─────────────────────────────────────────────────────────────
-- Function 2: calculate_request_service_time
-- Calculates how long (in simulated time units) one rescue
-- request will take to service.
-- Formula: (people * 0.5) + (severity * 2.0)
-- This mirrors the C++ function pcb_service_time().
-- 1 time unit = 10 simulated minutes.
-- ─────────────────────────────────────────────────────────────
DROP FUNCTION IF EXISTS calculate_request_service_time;
DELIMITER $$
CREATE FUNCTION calculate_request_service_time(
    p_people   INT,     -- number of people affected
    p_severity TINYINT  -- severity level 1-5
)
RETURNS DECIMAL(8,2)
DETERMINISTIC   -- same inputs always give the same output
NO SQL          -- does not access the database at all (pure math)
SQL SECURITY INVOKER
COMMENT 'Simulated OS service time. 1 unit = 10 sim minutes. Mirrors C++ pcb_service_time()'
BEGIN
    -- Multiply people by 0.5 and add severity * 2.0, then round to 2 decimals
    RETURN ROUND((p_people * 0.5) + (p_severity * 2.0), 2);
END$$
DELIMITER ;

-- ─────────────────────────────────────────────────────────────
-- Function 3: calculate_priority_score
-- Calculates a priority number for scheduling.
-- Higher score = serve sooner.
-- Formula: (severity * 20) + (min(people, 50) * 0.6) + (waiting * 0.4)
-- This mirrors the C++ function pcb_priority_score().
-- LEAST(people, 50) caps the people count so one large request
-- does not dominate the queue forever.
-- ─────────────────────────────────────────────────────────────
DROP FUNCTION IF EXISTS calculate_priority_score;
DELIMITER $$
CREATE FUNCTION calculate_priority_score(
    p_severity      TINYINT,        -- severity level 1-5
    p_people        INT,            -- number of people affected
    p_waiting_units DECIMAL(8,2)    -- how long the request has been waiting
)
RETURNS DECIMAL(10,4)
DETERMINISTIC   -- same inputs always give the same output
NO SQL          -- pure math, no database access
SQL SECURITY INVOKER
COMMENT 'Priority score: severity*20 + min(people,50)*0.6 + waiting*0.4. Mirrors C++ pcb_priority_score()'
BEGIN
    -- Add the three weighted components and round to 4 decimal places
    RETURN ROUND(
        (p_severity * 20.0)
        + (LEAST(p_people, 50) * 0.6)   -- cap people at 50 to avoid overflow
        + (p_waiting_units * 0.4)        -- reward requests that have waited longer
    , 4);
END$$
DELIMITER ;

-- ============================================================
-- STORED PROCEDURES — complex multi-step operations
-- ============================================================

-- ─────────────────────────────────────────────────────────────
-- Procedure 1: submit_rescue_request
-- Called when a victim submits a new request for help.
-- Steps:
--   1. Look up which zone the given locality belongs to.
--   2. Look up the current severity for that zone + disaster.
--   3. Generate a unique public reference number (e.g. REQ-45231-261017).
--   4. Insert the rescue request row inside a transaction.
--   5. Insert the first status-history row (old=NONE, new=Submitted).
--   6. Insert an audit record.
--   7. COMMIT: all three inserts succeed together or none do.
-- ─────────────────────────────────────────────────────────────
DROP PROCEDURE IF EXISTS submit_rescue_request;
DELIMITER $$
CREATE PROCEDURE submit_rescue_request(
    IN  p_disaster_id        INT UNSIGNED,   -- which disaster this request belongs to
    IN  p_locality_id        INT UNSIGNED,   -- where the victim is located
    IN  p_victim_name        VARCHAR(150),
    IN  p_contact            VARCHAR(20),
    IN  p_location           VARCHAR(255),
    IN  p_assistance_type_id INT UNSIGNED,
    IN  p_people             SMALLINT UNSIGNED,
    IN  p_details            TEXT,
    OUT p_request_id         INT UNSIGNED,   -- returns the new request ID
    OUT p_public_ref         VARCHAR(20),    -- returns the human-readable ref
    OUT p_severity           TINYINT,        -- returns the auto-assigned severity
    OUT p_zone_id            INT UNSIGNED    -- returns the locality's zone
)
SQL SECURITY INVOKER
COMMENT 'Submit a rescue request: resolves zone+severity, inserts, logs submission.'
BEGIN
    -- Local variables to hold values we compute along the way
    DECLARE v_zone_id  INT UNSIGNED;
    DECLARE v_severity TINYINT DEFAULT 1;
    DECLARE v_ref      VARCHAR(20);

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    START TRANSACTION;

    -- Step 1: Find which zone this locality belongs to.
    --         The locality must be active. If not found, signal an error.
    SELECT zone_id INTO v_zone_id
    FROM localities
    WHERE locality_id = p_locality_id AND is_active = 1
    LIMIT 1;

    IF v_zone_id IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT = 'Invalid or inactive locality';
    END IF;

    -- This function returns 1 when no severity row exists, matching the C++ lookup.
    SET v_severity = get_severity_for_zone(p_disaster_id, v_zone_id);

    -- Step 3: Generate a readable public reference like REQ-45231-261017.
    --         RAND() produces a random decimal; we scale it to a 5-digit number.
    --         DATE_FORMAT gives today's date as YYMMDD.
    SET v_ref = CONCAT('REQ-', LPAD(FLOOR(RAND() * 90000 + 10000), 5, '0'),
                       '-', DATE_FORMAT(NOW(), '%y%m%d'));

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
    SET p_zone_id    = v_zone_id;

    INSERT INTO request_status_history
        (request_id, old_status, new_status, changed_by, remarks)
    VALUES (p_request_id, 'NONE', 'Submitted', NULL,
            'Public submission via CLI/API');

    INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
    VALUES (NULL, 'REQUEST_SUBMITTED', 'rescue_request', p_request_id,
            CONCAT('Public request submitted: ', v_ref));

    COMMIT;
END$$
DELIMITER ;

-- ─────────────────────────────────────────────────────────────
-- Procedure 2: allocate_resource_to_request
-- Assigns a resource (boat, truck, medical team, etc.) to a request.
-- Uses FOR UPDATE locks so two dispatchers cannot assign the
-- same resource simultaneously (concurrency control).
-- Steps:
--   1. Lock resource row and read its status.
--   2. Lock request row and read its status.
--   3. Count existing active allocations for this resource.
--   4. Validate: resource must be Available, request must be in a
--      workable state, resource must have no existing active allocation.
--   5. If any check fails: ROLLBACK and return a FAIL message.
--   6. If all checks pass: insert allocation, update both statuses,
--      write history rows, write audit row, COMMIT.
-- ─────────────────────────────────────────────────────────────
DROP PROCEDURE IF EXISTS allocate_resource_to_request;
DELIMITER $$
CREATE PROCEDURE allocate_resource_to_request(
    IN  p_request_id  INT UNSIGNED,   -- the request that needs a resource
    IN  p_resource_id INT UNSIGNED,   -- the resource being assigned
    IN  p_user_id     INT UNSIGNED,   -- the dispatcher doing the assignment
    OUT p_allocation_id INT UNSIGNED,
    OUT p_result        VARCHAR(255)
)
SQL SECURITY INVOKER
COMMENT 'Atomically allocates a resource. Prevents double-allocation with FOR UPDATE + transaction.'
BEGIN
    DECLARE v_res_status       VARCHAR(20) DEFAULT NULL;
    DECLARE v_req_status       VARCHAR(30) DEFAULT NULL;
    DECLARE v_request_type_id  INT UNSIGNED DEFAULT NULL;
    DECLARE v_people_affected  SMALLINT UNSIGNED DEFAULT NULL;
    DECLARE v_res_type_id      INT UNSIGNED DEFAULT NULL;
    DECLARE v_capacity         SMALLINT UNSIGNED DEFAULT NULL;
    DECLARE v_capacity_unit    VARCHAR(40) DEFAULT NULL;
    DECLARE v_assistance_type  VARCHAR(100) DEFAULT NULL;
    DECLARE v_resource_type    VARCHAR(100) DEFAULT NULL;
    DECLARE v_active_alloc_id  INT UNSIGNED DEFAULT NULL;
    DECLARE v_old_alloc_id     INT UNSIGNED DEFAULT NULL;
    DECLARE v_alloc_id         INT UNSIGNED DEFAULT NULL;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    SET p_allocation_id = NULL;
    SET p_result = NULL;
    START TRANSACTION;

    -- Lock request before resource; all allocation writers use this order.
    SELECT status, assistance_type_id, people_affected
    INTO v_req_status, v_request_type_id, v_people_affected
    FROM rescue_requests
    WHERE request_id = p_request_id
    FOR UPDATE;

    IF v_req_status IS NULL THEN
        SET p_result = 'Request not found';
    ELSEIF v_req_status NOT IN
        ('Submitted', 'New', 'Ready', 'Scheduled', 'Under Review', 'Deferred') THEN
        SET p_result = CONCAT(
            'Request is not eligible for allocation (status=', v_req_status, ')');
    ELSE
        SET v_active_alloc_id = NULL;
        SELECT allocation_id INTO v_active_alloc_id
        FROM allocations
        WHERE request_id = p_request_id AND status = 'Active'
        LIMIT 1 FOR UPDATE;

        IF v_active_alloc_id IS NOT NULL THEN
            SET p_result = 'Request already has an active allocation';
        ELSE
            SELECT status, resource_type_id, capacity, capacity_unit
            INTO v_res_status, v_res_type_id, v_capacity, v_capacity_unit
            FROM resources
            WHERE resource_id = p_resource_id
            FOR UPDATE;

            IF v_res_status IS NULL THEN
                SET p_result = 'Resource not found';
            ELSEIF v_res_status != 'Available' THEN
                SET p_result = CONCAT(
                    'Resource not available (status=', v_res_status, ')');
            ELSE
                SELECT type_name INTO v_assistance_type
                FROM assistance_types
                WHERE assistance_type_id = v_request_type_id;

                SELECT type_name INTO v_resource_type
                FROM resource_types
                WHERE resource_type_id = v_res_type_id;

                IF NOT (
                    LOWER(TRIM(v_assistance_type)) = LOWER(TRIM(v_resource_type))
                    OR (LOWER(TRIM(v_assistance_type)) = 'boat rescue'
                        AND LOWER(TRIM(v_resource_type)) = 'boat')
                    OR (LOWER(TRIM(v_assistance_type)) = 'relief supplies'
                        AND LOWER(TRIM(v_resource_type)) = 'relief kit')
                    OR (LOWER(TRIM(v_assistance_type)) = 'fire brigade'
                        AND LOWER(TRIM(v_resource_type)) = 'fire engine')
                    OR (LOWER(TRIM(v_assistance_type)) = 'medical team'
                        AND LOWER(TRIM(v_resource_type)) = 'ambulance')
                ) THEN
                    SET p_result = CONCAT(
                        'Resource type ', v_resource_type,
                        ' is not compatible with assistance type ', v_assistance_type);
                ELSEIF LOWER(TRIM(v_capacity_unit)) IN
                    ('people', 'person', 'patients', 'patient')
                    AND v_capacity < v_people_affected THEN
                    SET p_result =
                        'Resource capacity is below the number of affected people';
                ELSE
                    SET v_active_alloc_id = NULL;
                    SELECT allocation_id INTO v_active_alloc_id
                    FROM allocations
                    WHERE resource_id = p_resource_id AND status = 'Active'
                    LIMIT 1 FOR UPDATE;

                    IF v_active_alloc_id IS NOT NULL THEN
                        SET p_result = 'Resource already has an active allocation';
                    ELSE
                        INSERT INTO allocations
                            (request_id, resource_id, allocated_by, status)
                        VALUES (p_request_id, p_resource_id, p_user_id, 'Active');
                        SET v_alloc_id = LAST_INSERT_ID();

                        UPDATE resources
                        SET status = 'Allocated', updated_at = NOW()
                        WHERE resource_id = p_resource_id;

                        UPDATE rescue_requests
                        SET status = 'Allocated', updated_at = NOW()
                        WHERE request_id = p_request_id;

                        INSERT INTO resource_status_history
                            (resource_id, old_status, new_status, changed_by, remarks)
                        VALUES (p_resource_id, 'Available', 'Allocated', p_user_id,
                                CONCAT('Allocated to request #', p_request_id));

                        INSERT INTO request_status_history
                            (request_id, old_status, new_status, changed_by, remarks)
                        VALUES (p_request_id, v_req_status, 'Allocated', p_user_id,
                                CONCAT('Resource #', p_resource_id, ' assigned'));

                        INSERT INTO audit_log
                            (user_id, action, entity_type, entity_id, details)
                        VALUES (p_user_id, 'ALLOCATE_RESOURCE', 'allocations',
                                v_alloc_id,
                                CONCAT('req=', p_request_id, ' res=', p_resource_id));

                        SET p_allocation_id = v_alloc_id;
                        SET p_result = CONCAT('OK: allocation_id=', v_alloc_id);
                    END IF;
                END IF;
            END IF;
        END IF;
    END IF;

    IF p_allocation_id IS NULL THEN
        ROLLBACK;
    ELSE
        COMMIT;
    END IF;
END$$
DELIMITER ;

-- ─────────────────────────────────────────────────────────────
-- Procedure 3: release_resource_allocation
-- Called when a rescue operation is finished.
-- Marks the allocation as Released, returns the resource to
-- Available status, and marks the request as Completed.
-- Steps:
--   1. Lock and read the allocation row (FOR UPDATE).
--   2. If no active allocation found, ROLLBACK and return error.
--   3. Update allocation, resource, and request statuses.
--   4. Write one history row each for resource and request.
--   5. Write one audit row, then COMMIT.
-- ─────────────────────────────────────────────────────────────
DROP PROCEDURE IF EXISTS release_resource_allocation;
DELIMITER $$
CREATE PROCEDURE release_resource_allocation(
    IN  p_allocation_id INT UNSIGNED,   -- the allocation record to close
    IN  p_user_id       INT UNSIGNED,   -- the user performing the release
    OUT p_result        VARCHAR(255)
)
SQL SECURITY INVOKER
COMMENT 'Releases one resource; completes request after its last active allocation.'
BEGIN
    DECLARE v_resource_id INT UNSIGNED DEFAULT NULL;
    DECLARE v_request_id  INT UNSIGNED DEFAULT NULL;
    DECLARE v_locked_allocation_id INT UNSIGNED DEFAULT NULL;
    DECLARE v_request_status VARCHAR(30) DEFAULT NULL;
    DECLARE v_resource_status VARCHAR(20) DEFAULT NULL;
    DECLARE v_remaining INT DEFAULT 0;

    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    SET p_result = NULL;
    START TRANSACTION;

    -- Read the references first, then acquire locks in allocation's request/resource order.
    SELECT resource_id, request_id
    INTO v_resource_id, v_request_id
    FROM allocations
    WHERE allocation_id = p_allocation_id AND status = 'Active'
    LIMIT 1;

    IF v_resource_id IS NULL THEN
        SET p_result = 'Allocation not found or not active';
    ELSE
        SELECT status INTO v_request_status
        FROM rescue_requests
        WHERE request_id = v_request_id
        FOR UPDATE;

        SELECT status INTO v_resource_status
        FROM resources
        WHERE resource_id = v_resource_id
        FOR UPDATE;

        SELECT allocation_id INTO v_locked_allocation_id
        FROM allocations
        WHERE allocation_id = p_allocation_id AND status = 'Active'
        FOR UPDATE;

        IF v_locked_allocation_id IS NULL THEN
            SET p_result = 'Allocation not found or not active';
        ELSE
            UPDATE allocations
            SET status = 'Released', released_at = NOW()
            WHERE allocation_id = p_allocation_id AND status = 'Active';

            INSERT IGNORE INTO dispatch_events(allocation_id,stage,changed_at,changed_by)
            VALUES(p_allocation_id,'Released',NOW(),p_user_id);

            UPDATE resources
            SET status = 'Available', updated_at = NOW()
            WHERE resource_id = v_resource_id;

            SELECT COUNT(*) INTO v_remaining FROM allocations WHERE request_id=v_request_id AND status='Active' FOR UPDATE;
            IF v_remaining=0 THEN
                UPDATE rescue_requests SET status='Completed',updated_at=NOW() WHERE request_id=v_request_id;
            END IF;

            INSERT INTO resource_status_history
                (resource_id, old_status, new_status, changed_by, remarks)
            VALUES (v_resource_id, v_resource_status, 'Available', p_user_id,
                    CONCAT('Released from allocation #', p_allocation_id));

            IF v_remaining=0 THEN
                INSERT INTO request_status_history(request_id,old_status,new_status,changed_by,remarks)
                VALUES(v_request_id,v_request_status,'Completed',p_user_id,CONCAT('Last resource released, allocation #',p_allocation_id));
            END IF;

            INSERT INTO audit_log
                (user_id, action, entity_type, entity_id, details)
            VALUES (p_user_id, 'RELEASE_ALLOCATION', 'allocations',
                    p_allocation_id, 'released');

            SET p_result = CONCAT('OK: resource=', v_resource_id,
                                  ' request=', v_request_id, IF(v_remaining=0,' Completed',' still allocated'));
        END IF;
    END IF;

    IF LEFT(p_result, 3) = 'OK:' THEN
        COMMIT;
    ELSE
        ROLLBACK;
    END IF;
END$$
DELIMITER ;

-- ─────────────────────────────────────────────────────────────
-- Procedure 4: change_request_status
-- Moves a request through its defined lifecycle.
-- Acts like a state machine: only certain transitions are valid.
-- Allowed transitions:
--   Submitted    -> New, Cancelled
--   New          -> Ready, Cancelled
--   Ready        -> Scheduled, Deferred, Cancelled
--   Scheduled    -> Under Review, Deferred, Cancelled
--   Under Review -> Allocated, Deferred, Cancelled
--   Deferred     -> Ready, Cancelled
-- Steps:
--   1. Lock and read the current status (FOR UPDATE).
--   2. Check each allowed transition using IF / ELSEIF.
--   3. If not a valid transition, ROLLBACK and return a FAIL message.
--   4. If valid: UPDATE status, SAVEPOINT, write history, audit, COMMIT.
-- ─────────────────────────────────────────────────────────────
DROP PROCEDURE IF EXISTS change_request_status;
DELIMITER $$
CREATE PROCEDURE change_request_status(
    IN  p_request_id INT UNSIGNED,   -- which request to update
    IN  p_new_status VARCHAR(20),    -- the status we want to move to
    IN  p_user_id    INT UNSIGNED,   -- the staff member making the change
    IN  p_remarks    VARCHAR(255),   -- optional notes for the history record
    OUT p_result     VARCHAR(255)
)
SQL SECURITY INVOKER
COMMENT 'Changes request status with transition validation, SAVEPOINT, history and audit.'
BEGIN
    DECLARE v_old_status VARCHAR(20) DEFAULT NULL;  -- current status in DB
    DECLARE v_valid      INT         DEFAULT 0;     -- 1=allowed, 0=blocked

    -- If any SQL error occurs, roll back and return an error message
    DECLARE EXIT HANDLER FOR SQLEXCEPTION
    BEGIN
        ROLLBACK;
        RESIGNAL;
    END;

    START TRANSACTION;

    -- Step 1: Read the current status and lock the row so no other
    --         session can change it while we are working.
    SELECT status INTO v_old_status
    FROM rescue_requests
    WHERE request_id = p_request_id
    FOR UPDATE;

    IF v_old_status IS NULL THEN
        -- Request ID does not exist in the table
        ROLLBACK;
        SET p_result = 'FAIL: request not found';
    ELSE
        -- Step 2: Check whether the requested transition is in the
        --         allowed list. Each ELSEIF is one valid path.
        IF v_old_status = 'Submitted' AND p_new_status IN ('New', 'Cancelled') THEN
            SET v_valid = 1;
        ELSEIF v_old_status = 'New' AND p_new_status IN ('Ready', 'Cancelled') THEN
            SET v_valid = 1;
        ELSEIF v_old_status = 'Ready' AND p_new_status IN ('Scheduled', 'Deferred', 'Cancelled') THEN
            SET v_valid = 1;
        ELSEIF v_old_status = 'Scheduled' AND p_new_status IN ('Under Review', 'Deferred', 'Cancelled') THEN
            SET v_valid = 1;
        ELSEIF v_old_status = 'Under Review' AND p_new_status IN ('Allocated', 'Deferred', 'Cancelled') THEN
            SET v_valid = 1;
        ELSEIF v_old_status = 'Deferred' AND p_new_status IN ('Ready', 'Cancelled') THEN
            SET v_valid = 1;
        ELSE
            SET v_valid = 0;  -- any other combination is not allowed
        END IF;

        -- Step 3: Reject if the transition is not in the allowed list
        IF v_valid = 0 THEN
            ROLLBACK;
            SET p_result = CONCAT('FAIL: invalid transition ', v_old_status, ' -> ', p_new_status);
        ELSE
            -- Step 4: Apply the change

            -- Update the status column on the request row
            UPDATE rescue_requests
            SET status = p_new_status, updated_at = NOW()
            WHERE request_id = p_request_id;

            -- SAVEPOINT marks a point we could partially roll back to.
            -- This demonstrates TCL (Transaction Control Language).
            SAVEPOINT after_status_update;

            -- Write the status change into the history table
            INSERT INTO request_status_history
                (request_id, old_status, new_status, changed_by, remarks)
            VALUES (p_request_id, v_old_status, p_new_status, p_user_id, p_remarks);

            -- Write the event into the audit log
            INSERT INTO audit_log (user_id, action, entity_type, entity_id, details)
            VALUES (p_user_id, 'UPDATE_STATUS', 'rescue_requests', p_request_id,
                    CONCAT(v_old_status, '->', p_new_status));

            COMMIT;
            SET p_result = CONCAT('OK: ', v_old_status, ' -> ', p_new_status);
        END IF;
    END IF;
END$$
DELIMITER ;

-- Refresh one completed scheduler run's precomputed reporting row.
-- The caller owns the transaction so the run status and cache update commit together.
DROP PROCEDURE IF EXISTS refresh_scheduling_run_summary;
DELIMITER $$
CREATE PROCEDURE refresh_scheduling_run_summary(IN p_run_id INT UNSIGNED)
SQL SECURITY INVOKER
COMMENT 'Refresh the cached summary for one scheduler run.'
BEGIN
    INSERT INTO scheduling_performance_summary_cache
        (run_id, algorithm, time_quantum, requests_scheduled,
         avg_waiting_time, avg_turnaround_time, avg_response_time,
         throughput, starvation_count, started_at, finished_at)
    SELECT sr.run_id, sr.algorithm, sr.time_quantum,
           COUNT(sl.log_id),
           AVG(sl.waiting_time),
           AVG(sl.turnaround_time),
           AVG(sl.response_time),
           CASE WHEN COALESCE(MAX(sl.completion_time), 0) > 0
                THEN COUNT(sl.log_id) / MAX(sl.completion_time) ELSE 0 END,
           COALESCE(SUM(CASE WHEN sl.waiting_time > 20 THEN 1 ELSE 0 END), 0),
           sr.started_at, sr.finished_at
    FROM scheduling_runs sr
    LEFT JOIN scheduling_log sl ON sl.run_id = sr.run_id
    WHERE sr.run_id = p_run_id
    GROUP BY sr.run_id, sr.algorithm, sr.time_quantum,
             sr.started_at, sr.finished_at
    ON DUPLICATE KEY UPDATE
        algorithm = VALUES(algorithm),
        time_quantum = VALUES(time_quantum),
        requests_scheduled = VALUES(requests_scheduled),
        avg_waiting_time = VALUES(avg_waiting_time),
        avg_turnaround_time = VALUES(avg_turnaround_time),
        avg_response_time = VALUES(avg_response_time),
        throughput = VALUES(throughput),
        starvation_count = VALUES(starvation_count),
        started_at = VALUES(started_at),
        finished_at = VALUES(finished_at);
END$$
DELIMITER ;

-- Initial backfill for fresh installs and repeatable upgrades.
INSERT INTO scheduling_performance_summary_cache
    (run_id, algorithm, time_quantum, requests_scheduled,
     avg_waiting_time, avg_turnaround_time, avg_response_time,
     throughput, starvation_count, started_at, finished_at)
SELECT sr.run_id, sr.algorithm, sr.time_quantum,
       COUNT(sl.log_id),
       AVG(sl.waiting_time),
       AVG(sl.turnaround_time),
       AVG(sl.response_time),
       CASE WHEN COALESCE(MAX(sl.completion_time), 0) > 0
            THEN COUNT(sl.log_id) / MAX(sl.completion_time) ELSE 0 END,
       COALESCE(SUM(CASE WHEN sl.waiting_time > 20 THEN 1 ELSE 0 END), 0),
       sr.started_at, sr.finished_at
FROM scheduling_runs sr
LEFT JOIN scheduling_log sl ON sl.run_id = sr.run_id
GROUP BY sr.run_id, sr.algorithm, sr.time_quantum,
         sr.started_at, sr.finished_at
ON DUPLICATE KEY UPDATE
    algorithm = VALUES(algorithm),
    time_quantum = VALUES(time_quantum),
    requests_scheduled = VALUES(requests_scheduled),
    avg_waiting_time = VALUES(avg_waiting_time),
    avg_turnaround_time = VALUES(avg_turnaround_time),
    avg_response_time = VALUES(avg_response_time),
    throughput = VALUES(throughput),
    starvation_count = VALUES(starvation_count),
    started_at = VALUES(started_at),
    finished_at = VALUES(finished_at);

GRANT EXECUTE ON PROCEDURE drero_db.submit_rescue_request TO 'drero_application_role';
GRANT EXECUTE ON PROCEDURE drero_db.allocate_resource_to_request TO 'drero_application_role';
GRANT EXECUTE ON PROCEDURE drero_db.release_resource_allocation TO 'drero_application_role';
GRANT EXECUTE ON PROCEDURE drero_db.change_request_status TO 'drero_application_role';
GRANT EXECUTE ON PROCEDURE drero_db.refresh_scheduling_run_summary TO 'drero_application_role';
GRANT EXECUTE ON FUNCTION drero_db.get_severity_for_zone TO 'drero_application_role';

