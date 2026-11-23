-- Run using the application's MySQL account after applying the feature migration.
SELECT CURRENT_USER() AS connected_account, CURRENT_ROLE() AS active_roles;
SHOW GRANTS;
