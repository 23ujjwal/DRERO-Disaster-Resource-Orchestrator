-- Apply once to an existing DRERO database before rebuilding/running the updated API.
-- Initialize the new independent risk value from the previous severity representation.
ALTER TABLE disaster_zone_status
    ADD COLUMN risk_level TINYINT UNSIGNED NULL AFTER severity_level;

UPDATE disaster_zone_status
SET risk_level = severity_level
WHERE risk_level IS NULL;

ALTER TABLE disaster_zone_status
    MODIFY COLUMN risk_level TINYINT UNSIGNED NOT NULL DEFAULT 1,
    ADD CONSTRAINT chk_dzs_risk_level CHECK (risk_level BETWEEN 1 AND 5);
