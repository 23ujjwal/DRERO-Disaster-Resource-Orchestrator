-- Additive optional operations tools. Preserves existing application records.
USE drero_db;
CREATE TABLE IF NOT EXISTS request_resource_needs (
  request_id INT UNSIGNED NOT NULL,
  assistance_type_id INT UNSIGNED NOT NULL,
  units SMALLINT UNSIGNED NOT NULL DEFAULT 1 CHECK(units BETWEEN 1 AND 12),
  PRIMARY KEY(request_id,assistance_type_id),
  FOREIGN KEY(request_id) REFERENCES rescue_requests(request_id) ON DELETE CASCADE,
  FOREIGN KEY(assistance_type_id) REFERENCES assistance_types(assistance_type_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS allocation_assistance (
  allocation_id INT UNSIGNED PRIMARY KEY,
  assistance_type_id INT UNSIGNED NOT NULL,
  FOREIGN KEY(allocation_id) REFERENCES allocations(allocation_id) ON DELETE CASCADE,
  FOREIGN KEY(assistance_type_id) REFERENCES assistance_types(assistance_type_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS resource_reservations (
  resource_id INT UNSIGNED PRIMARY KEY,
  request_id INT UNSIGNED NOT NULL,
  user_id INT UNSIGNED NOT NULL,
  created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  expires_at DATETIME NOT NULL,
  state ENUM('Active','Cancelled','Consumed') NOT NULL DEFAULT 'Active',
  FOREIGN KEY(resource_id) REFERENCES resources(resource_id),
  FOREIGN KEY(request_id) REFERENCES rescue_requests(request_id),
  FOREIGN KEY(user_id) REFERENCES users(user_id),
  INDEX idx_reservation_expiry(state,expires_at)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS dispatch_events (
  event_id BIGINT UNSIGNED AUTO_INCREMENT PRIMARY KEY,
  allocation_id INT UNSIGNED NOT NULL,
  stage ENUM('Allocated','Dispatched','Arrived','Working','Released') NOT NULL,
  changed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  changed_by INT UNSIGNED DEFAULT NULL,
  UNIQUE KEY unique_allocation_stage(allocation_id,stage),
  FOREIGN KEY(allocation_id) REFERENCES allocations(allocation_id),
  FOREIGN KEY(changed_by) REFERENCES users(user_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS request_duplicate_reviews (
  request_low INT UNSIGNED NOT NULL,
  request_high INT UNSIGNED NOT NULL,
  disposition ENUM('Related','Distinct') NOT NULL,
  reviewed_by INT UNSIGNED NOT NULL,
  reviewed_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY(request_low,request_high),
  CHECK(request_low<request_high),
  FOREIGN KEY(request_low) REFERENCES rescue_requests(request_id),
  FOREIGN KEY(request_high) REFERENCES rescue_requests(request_id),
  FOREIGN KEY(reviewed_by) REFERENCES users(user_id)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS request_alert_settings (
  user_id INT UNSIGNED PRIMARY KEY,
  warning_minutes INT UNSIGNED NOT NULL DEFAULT 30,
  critical_minutes INT UNSIGNED NOT NULL DEFAULT 120,
  CHECK(warning_minutes>=1 AND critical_minutes>warning_minutes AND critical_minutes<=10080),
  FOREIGN KEY(user_id) REFERENCES users(user_id)
) ENGINE=InnoDB;
GRANT SELECT,INSERT,UPDATE,DELETE ON drero_db.request_resource_needs TO 'drero_application_role';
GRANT SELECT,INSERT ON drero_db.allocation_assistance TO 'drero_application_role';
GRANT SELECT,INSERT,UPDATE ON drero_db.resource_reservations TO 'drero_application_role';
GRANT SELECT,INSERT ON drero_db.dispatch_events TO 'drero_application_role';
GRANT SELECT,INSERT,UPDATE ON drero_db.request_duplicate_reviews TO 'drero_application_role';
GRANT SELECT,INSERT,UPDATE ON drero_db.request_alert_settings TO 'drero_application_role';

-- Backfill known stages without inventing dispatch/arrival/work timestamps.
INSERT IGNORE INTO dispatch_events(allocation_id,stage,changed_at,changed_by)
SELECT allocation_id,'Allocated',allocated_at,allocated_by FROM allocations;
INSERT IGNORE INTO dispatch_events(allocation_id,stage,changed_at,changed_by)
SELECT allocation_id,'Released',released_at,allocated_by FROM allocations WHERE status='Released' AND released_at IS NOT NULL;

DROP TRIGGER IF EXISTS enforce_resource_reservation;
DROP TRIGGER IF EXISTS protect_reserved_resource;
DROP TRIGGER IF EXISTS cancel_terminal_reservations;
DROP TRIGGER IF EXISTS record_allocation_dispatch_start;
DROP TRIGGER IF EXISTS record_allocation_dispatch_release;
DELIMITER $$
CREATE TRIGGER enforce_resource_reservation BEFORE INSERT ON allocations FOR EACH ROW
BEGIN
  DECLARE held_request INT UNSIGNED DEFAULT NULL;
  DECLARE held_user INT UNSIGNED DEFAULT NULL;
  DECLARE held_state VARCHAR(20) DEFAULT NULL;
  DECLARE held_until DATETIME DEFAULT NULL;
  IF NEW.status='Active' THEN
    -- A current locking read is essential after waiting for the resource row.
    SELECT request_id,user_id,state,expires_at INTO held_request,held_user,held_state,held_until
      FROM resource_reservations WHERE resource_id=NEW.resource_id FOR UPDATE;
    IF held_state='Active' AND held_until>NOW() AND
      (held_request<>NEW.request_id OR NOT(held_user<=>NEW.allocated_by)) THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Resource has an active reservation for another request or dispatcher';
    END IF;
    UPDATE resource_reservations SET state='Consumed'
      WHERE resource_id=NEW.resource_id AND request_id=NEW.request_id AND user_id=NEW.allocated_by AND state='Active' AND expires_at>NOW();
  END IF;
END$$
CREATE TRIGGER protect_reserved_resource BEFORE UPDATE ON resources FOR EACH ROW
BEGIN
  DECLARE held_state VARCHAR(20) DEFAULT NULL;
  DECLARE held_until DATETIME DEFAULT NULL;
  IF OLD.status='Available' AND NEW.status<>OLD.status THEN
    SELECT state,expires_at INTO held_state,held_until
      FROM resource_reservations WHERE resource_id=NEW.resource_id FOR UPDATE;
    IF held_state='Active' AND held_until>NOW() THEN
      SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Resource is reserved; cancel the reservation or wait for expiry';
    END IF;
  END IF;
END$$
CREATE TRIGGER cancel_terminal_reservations AFTER UPDATE ON rescue_requests FOR EACH ROW
BEGIN
  IF NEW.status IN ('Cancelled','Completed') AND OLD.status<>NEW.status THEN
    UPDATE resource_reservations SET state='Cancelled' WHERE request_id=NEW.request_id AND state='Active';
  END IF;
END$$
CREATE TRIGGER record_allocation_dispatch_start AFTER INSERT ON allocations FOR EACH ROW
BEGIN
  INSERT IGNORE INTO dispatch_events(allocation_id,stage,changed_at,changed_by) VALUES(NEW.allocation_id,'Allocated',NEW.allocated_at,NEW.allocated_by);
END$$
DELIMITER ;

-- Compatible single/multiple-resource release; same procedure signature.
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
SELECT 6 AS optional_operations_tables_installed;
