-- Additive upgrade: preserves application rows and existing staff passwords.
-- Run as administrator from the project root. The backend hashes legacy
-- password values on startup after this restricted column grant is installed.
USE drero_db;
CREATE TABLE IF NOT EXISTS staff_sessions (
    token_hash CHAR(64) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,
    user_id INT UNSIGNED NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    expires_at DATETIME NOT NULL,
    revoked_at DATETIME DEFAULT NULL,
    FOREIGN KEY fk_session_user (user_id) REFERENCES users(user_id) ON DELETE CASCADE,
    INDEX idx_session_expiry (expires_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
GRANT SELECT, INSERT, UPDATE ON drero_db.staff_sessions TO 'drero_application_role';
GRANT UPDATE (password_hash) ON drero_db.users TO 'drero_application_role';
SELECT COUNT(*) AS session_table_installed FROM information_schema.tables
WHERE table_schema='drero_db' AND table_name='staff_sessions';
