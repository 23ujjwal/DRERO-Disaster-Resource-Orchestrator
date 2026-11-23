-- Current locking count prevents stale snapshots during simultaneous releases.
USE drero_db;
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
GRANT EXECUTE ON PROCEDURE drero_db.release_resource_allocation TO 'drero_application_role';
SELECT 'release_completion_fix_installed' AS result;
