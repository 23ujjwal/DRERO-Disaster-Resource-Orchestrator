-- DRERO: seed.sql — minimum 10 meaningful records per table
-- Idempotent: INSERT IGNORE means safe to re-run.
-- All passwords: Demo@1234 (stored plain for academic demo — documented).
-- All data is SIMULATED for academic demonstration only.

SET FOREIGN_KEY_CHECKS = 0; --system softwar

-- ─── cities (10) ─────────────────────────────────────────────
INSERT IGNORE INTO cities (city_id, city_name, state_name, country, is_active) VALUES
(1,  'Mumbai',       'Maharashtra',    'India', 1),
(2,  'Chennai',      'Tamil Nadu',     'India', 1),
(3,  'Kolkata',      'West Bengal',    'India', 1),
(4,  'Bhubaneswar',  'Odisha',         'India', 1),
(5,  'Kochi',        'Kerala',         'India', 1),
(6,  'Guwahati',     'Assam',          'India', 1),
(7,  'Patna',        'Bihar',          'India', 1),
(8,  'Surat',        'Gujarat',        'India', 1),
(9,  'Vijayawada',   'Andhra Pradesh', 'India', 1),
(10, 'Hyderabad',    'Telangana',      'India', 1);

-- ─── zones (12) ──────────────────────────────────────────────
INSERT IGNORE INTO zones (zone_id, city_id, zone_name, is_active) VALUES
(1,  1, 'Mumbai North',        1),
(2,  1, 'Mumbai South',        1),
(3,  1, 'Mumbai Central',      1),
(4,  2, 'Chennai North',       1),
(5,  2, 'Chennai South',       1),
(6,  3, 'Kolkata East',        1),
(7,  3, 'Kolkata West',        1),
(8,  4, 'Bhubaneswar Zone A',  1),
(9,  5, 'Kochi Metro',         1),
(10, 6, 'Guwahati Zone 1',     1),
(11, 7, 'Patna Zone 1',        1),
(12, 8, 'Surat Zone 1',        1);

-- ─── localities (15) ─────────────────────────────────────────
INSERT IGNORE INTO localities (locality_id, zone_id, locality_name, pincode, is_active) VALUES
(1,  1, 'Borivali',          '400066', 1),
(2,  1, 'Dahisar',           '400068', 1),
(3,  2, 'Colaba',            '400005', 1),
(4,  2, 'Worli',             '400018', 1),
(5,  3, 'Kurla',             '400070', 1),
(6,  3, 'Dharavi',           '400017', 1),
(7,  4, 'Thiruvottiyur',     '600019', 1),
(8,  4, 'Tondiarpet',        '600081', 1),
(9,  5, 'Besant Nagar',      '600090', 1),
(10, 5, 'Velachery',         '600042', 1),
(11, 6, 'Salt Lake',         '700064', 1),
(12, 6, 'Bidhannagar',       '700091', 1),
(13, 7, 'Howrah',            '711101', 1),
(14, 8, 'Patia',             '751024', 1),
(15, 9, 'Ernakulam North',   '682018', 1);

-- ─── disasters (10) ──────────────────────────────────────────
INSERT IGNORE INTO disasters (disaster_id, disaster_name, disaster_type, description, start_time, status) VALUES
(1,  'Mumbai Floods 2024',        'Flood',       'Heavy monsoon flooding across Mumbai',          '2024-07-10 06:00:00', 'Active'),
(2,  'Chennai Cyclone Michaung',  'Cyclone',     'Simulated cyclone Michaung landfall Chennai',   '2024-12-04 08:00:00', 'Active'),
(3,  'Kolkata Flood 2024',        'Flood',       'Damodar river overflow',                        '2024-08-15 10:00:00', 'Monitoring'),
(4,  'Odisha Earthquake Sim',     'Earthquake',  'Simulated earthquake near Bhubaneswar',         '2024-09-01 03:00:00', 'Monitoring'),
(5,  'Kerala Landslide 2024',     'Landslide',   'Simulated Wayanad district landslide',          '2024-07-30 05:00:00', 'Active'),
(6,  'Assam Flood 2024',          'Flood',       'Brahmaputra river flooding',                    '2024-06-20 09:00:00', 'Closed'),
(7,  'Bihar Flood 2024',          'Flood',       'Ganga and Gandak rivers flooding',              '2024-08-01 07:00:00', 'Active'),
(8,  'Gujarat Flood 2024',        'Flood',       'Tapi river overflow in Surat',                  '2024-08-25 11:00:00', 'Monitoring'),
(9,  'Andhra Flood 2024',         'Flood',       'Krishna river flooding Vijayawada',             '2024-09-10 04:00:00', 'Active'),
(10, 'Hyderabad Flash Flood',     'Flood',       'Urban flash flooding Hyderabad',                '2024-10-01 14:00:00', 'Monitoring');

-- ─── users (10) ──────────────────────────────────────────────
-- NOTE: password_hash stores plain "Demo@1234" for academic demo.
-- In production, store bcrypt/argon2 hash. Documented in setup.md.
INSERT IGNORE INTO users (user_id, username, password_hash, full_name, role, zone_id, is_active) VALUES
(1,  'dispatcher1', 'Demo@1234', 'Arjun Sharma',    'DISPATCHER', NULL, 1),
(2,  'dispatcher2', 'Demo@1234', 'Priya Nair',       'DISPATCHER', NULL, 1),
(3,  'zone_admin1', 'Demo@1234', 'Ravi Menon',       'ZONE_ADMIN', 1,    1),
(4,  'zone_admin2', 'Demo@1234', 'Sunita Patel',     'ZONE_ADMIN', 4,    1),
(5,  'auditor1',    'Demo@1234', 'Meera Krishnan',   'AUDITOR',    NULL, 1),
(6,  'auditor2',    'Demo@1234', 'Deepak Joshi',     'AUDITOR',    NULL, 1),
(7,  'zone_admin3', 'Demo@1234', 'Fatima Sheikh',    'ZONE_ADMIN', 6,    1),
(8,  'dispatcher3', 'Demo@1234', 'Vikram Bose',      'DISPATCHER', NULL, 1),
(9,  'zone_admin4', 'Demo@1234', 'Ananya Das',       'ZONE_ADMIN', 8,    1),
(10, 'auditor3',    'Demo@1234', 'Sanjay Kumar',     'AUDITOR',    NULL, 1);

-- ─── disaster_zone_status (12) ───────────────────────────────
INSERT IGNORE INTO disaster_zone_status (disaster_id, zone_id, severity_level, risk_description, updated_by) VALUES
(1, 1, 4, 'Severe flooding in northern suburbs',          1),
(1, 2, 3, 'Moderate flooding in coastal areas',           1),
(1, 3, 5, 'Critical flooding in low-lying areas',         1),
(2, 4, 5, 'Direct cyclone impact zone',                   2),
(2, 5, 4, 'High wind and rain damage',                    2),
(3, 6, 3, 'Moderate river overflow',                      1),
(3, 7, 2, 'Low-level flooding Howrah',                    1),
(4, 8, 3, 'Simulated seismic zone risk',                  2),
(5, 9, 4, 'Landslide risk in hilly terrain',              1),
(7,11, 5, 'Critical Ganga flood zone',                    2),
(8,12, 3, 'Moderate Tapi river overflow',                 1),
(9, 1, 2, 'Low risk secondary zone',                      1);

-- ─── assistance_types (10) ───────────────────────────────────
INSERT IGNORE INTO assistance_types (assistance_type_id, type_name, description, is_active) VALUES
(1,  'Boat Rescue',         'Water rescue using motorboats',                   1),
(2,  'Medical Team',        'Emergency medical assistance and first aid',      1),
(3,  'Rescue Truck',        'Ground vehicle rescue and evacuation',            1),
(4,  'Relief Supplies',     'Food, water, and essential relief materials',     1),
(5,  'Helicopter',          'Aerial rescue for inaccessible areas',            1),
(6,  'Search and Rescue',   'Specialized teams for locating missing persons',  1),
(7,  'Fire Brigade',        'Fire suppression and rescue from burning structures', 1),
(8,  'Animal Rescue',       'Rescue and shelter for livestock and animals',    1),
(9,  'Evacuation Bus',      'Mass evacuation transport for large groups',      1),
(10, 'Water Tanker',        'Clean drinking water supply to affected areas',   1);

-- ─── resource_types (10) ─────────────────────────────────────
INSERT IGNORE INTO resource_types (resource_type_id, type_name, description, is_active) VALUES
(1,  'Boat',             'Water rescue vessel',                    1),
(2,  'Medical Team',     'Emergency medical response unit',        1),
(3,  'Rescue Truck',     'Ground evacuation vehicle',              1),
(4,  'Relief Kit',       'Supply and relief package unit',         1),
(5,  'Helicopter',       'Aerial rescue aircraft',                 1),
(6,  'Fire Engine',      'Fire suppression vehicle',               1),
(7,  'Ambulance',        'Medical transport vehicle',              1),
(8,  'Water Tanker',     'Clean water supply vehicle',             1),
(9,  'Evacuation Bus',   'Large capacity evacuation transport',    1),
(10, 'Generator Set',    'Emergency power supply unit',            1);

-- ─── resources (12) ──────────────────────────────────────────
INSERT IGNORE INTO resources
  (resource_id, resource_name, resource_type_id, zone_id, capacity, capacity_unit, status, notes, created_by)
VALUES
(1,  'Boat B-01',            1,  1,  10, 'people',   'Available',   'NDRF motorboat Mumbai North',    1),
(2,  'Boat B-02',            1,  2,  10, 'people',   'Available',   'NDRF motorboat Mumbai South',    1),
(3,  'Boat B-03',            1,  3,   8, 'people',   'Maintenance', 'Under engine servicing',         1),
(4,  'Medical Team M-01',    2,  4,   5, 'patients', 'Available',   'Chennai North medical unit',     2),
(5,  'Medical Team M-02',    2,  1,   5, 'patients', 'Available',   'Mumbai North medical team',      2),
(6,  'Rescue Truck T-01',    3,  1,  20, 'people',   'Available',   'Heavy rescue truck MH-01',       1),
(7,  'Rescue Truck T-02',    3,  4,  20, 'people',   'Available',   'Chennai North rescue truck',     2),
(8,  'Rescue Truck T-03',    3,  3,  15, 'people',   'Available',   'Medium truck Mumbai Central',    1),
(9,  'Relief Kit S-01',      4,  1,  50, 'kits',     'Available',   'Emergency relief package set',   1),
(10, 'Relief Kit S-02',      4,  4,  50, 'kits',     'Available',   'Chennai relief package set',     2),
(11, 'Boat B-04',            1,  6,  12, 'people',   'Available',   'Kolkata East rescue boat',       3),
(12, 'Medical Team M-03',    2,  8,   4, 'patients', 'Available',   'Bhubaneswar mobile medics',      4);

-- ─── rescue_requests (12) ────────────────────────────────────
INSERT IGNORE INTO rescue_requests
  (request_id, public_ref, disaster_id, locality_id, zone_id, victim_name,
   contact_number, reported_location, assistance_type_id, people_affected,
   details, severity_snapshot, arrival_time, status, created_by)
VALUES
(1,  'DRERO-001', 1,  1,  1, 'Ramesh Tiwari',   '9811001001', 'Near Borivali Station overbridge',    1,  8,  'Family trapped on rooftop',          4, '2024-07-10 07:15:00', 'Ready',       NULL),
(2,  'DRERO-002', 1,  2,  1, 'Lakshmi Devi',    '9811002002', 'Dahisar check naka, 200m east',       2,  3,  'Elderly woman needs medical help',   4, '2024-07-10 07:45:00', 'Ready',       NULL),
(3,  'DRERO-003', 1,  5,  2, 'Suresh Kadam',    '9811003003', 'Colaba bus depot area',               3, 12,  'Bus shelter flooded, people stuck',  3, '2024-07-10 08:00:00', 'Ready',       NULL),
(4,  'DRERO-004', 1,  6,  3, 'Anjali Patil',    '9811004004', 'Dharavi Sector 5 main road',          4, 25,  'Large group needs food and water',   5, '2024-07-10 08:30:00', 'Ready',       NULL),
(5,  'DRERO-005', 2,  7,  4, 'Tamil Selvan',    '9944005005', 'Thiruvottiyur fishing harbour',       1, 15,  'Fishermen boat capsized',            5, '2024-12-04 09:00:00', 'New',         NULL),
(6,  'DRERO-006', 2,  8,  4, 'Kavitha Raj',     '9944006006', 'Tondiarpet north, near temple',       2,  4,  'Injuries from fallen tree',          5, '2024-12-04 09:30:00', 'New',         NULL),
(7,  'DRERO-007', 2,  9,  5, 'Murugan S',       '9944007007', 'Besant Nagar 2nd Avenue',             3,  7,  'Road cut off by fallen debris',      4, '2024-12-04 10:00:00', 'Ready',       NULL),
(8,  'DRERO-008', 3, 11,  6, 'Subhasish Ghosh', '9800008008', 'Salt Lake Sector III, Block B',       4, 30,  'Community centre needs supplies',    3, '2024-08-15 11:00:00', 'Deferred',    NULL),
(9,  'DRERO-009', 7, 14, 11, 'Binod Kumar',     '9430009009', 'Patia village, near railway line',    1, 20,  'Village cut off by flood water',     5, '2024-08-01 08:00:00', 'Ready',       NULL),
(10, 'DRERO-010', 9, 10,  5, 'Venkat Rao',      '9966010010', 'Velachery Lakshmi Nagar',             2,  6,  'House collapsed, injured inside',    4, '2024-09-10 05:00:00', 'New',         NULL),
(11, 'DRERO-011', 1,  3,  2, 'Geeta Sawant',    '9811011011', 'Colaba Causeway near hotel',          1,  5,  'Tourists stranded on flooded road',  3, '2024-07-10 09:00:00', 'New',         NULL),
(12, 'DRERO-012', 1,  4,  2, 'Kiran Malhotra',  '9811012012', 'Worli sea face promenade',            4, 50,  'Evacuation camp low on supplies',    3, '2024-07-10 09:30:00', 'New',         NULL);

-- ─── request_status_history (12) ─────────────────────────────
INSERT IGNORE INTO request_status_history
  (history_id, request_id, old_status, new_status, changed_by, changed_at, remarks)
VALUES
(1,  1, 'Submitted', 'New',      NULL, '2024-07-10 07:16:00', 'Auto-promoted on arrival'),
(2,  1, 'New',       'Ready',    1,    '2024-07-10 07:30:00', 'Dispatcher validated request'),
(3,  2, 'Submitted', 'New',      NULL, '2024-07-10 07:46:00', 'Auto-promoted on arrival'),
(4,  2, 'New',       'Ready',    1,    '2024-07-10 08:00:00', 'Validated by dispatcher'),
(5,  3, 'Submitted', 'New',      NULL, '2024-07-10 08:01:00', 'Auto-promoted on arrival'),
(6,  3, 'New',       'Ready',    1,    '2024-07-10 08:30:00', 'Validated by dispatcher'),
(7,  4, 'Submitted', 'New',      NULL, '2024-07-10 08:31:00', 'Auto-promoted on arrival'),
(8,  4, 'New',       'Ready',    1,    '2024-07-10 09:00:00', 'High severity confirmed'),
(9,  5, 'Submitted', 'New',      NULL, '2024-12-04 09:01:00', 'Auto-promoted on arrival'),
(10, 8, 'Submitted', 'New',      NULL, '2024-08-15 11:01:00', 'Auto-promoted on arrival'),
(11, 8, 'New',       'Deferred', 2,    '2024-08-15 12:00:00', 'No available resources in zone'),
(12, 9, 'Submitted', 'New',      NULL, '2024-08-01 08:01:00', 'Auto-promoted on arrival');

-- ─── allocations (10) ────────────────────────────────────────
-- NOTE: All resources start Available above; allocations reference
-- historical events. We update status separately below.
INSERT IGNORE INTO allocations
  (allocation_id, request_id, resource_id, allocated_by, allocated_at, released_at, status, remarks)
VALUES
(1,  1,  1, 1, '2024-07-10 08:00:00', '2024-07-10 14:00:00', 'Released', 'Boat B-01 to Borivali rescue - completed'),
(2,  2,  5, 1, '2024-07-10 08:15:00', '2024-07-10 10:30:00', 'Released', 'Medical M-02 released after completion'),
(3,  3,  8, 1, '2024-07-10 09:30:00', '2024-07-10 13:00:00', 'Released', 'Truck T-03 to Colaba - completed'),
(4,  4,  9, 1, '2024-07-10 10:00:00', '2024-07-10 16:00:00', 'Released', 'Relief S-01 to Dharavi camp'),
(5,  1,  6, 1, '2024-07-10 08:05:00', '2024-07-10 12:00:00', 'Released', 'Truck T-01 additional support Borivali'),
(6,  2, 10, 2, '2024-07-10 08:20:00', '2024-07-10 11:00:00', 'Released', 'Relief kits to Dahisar'),
(7,  7,  4, 2, '2024-12-04 10:30:00', '2024-12-05 06:00:00', 'Released', 'Medical M-01 Chennai dispatch'),
(8,  9,  2, 8, '2024-08-01 09:00:00', '2024-08-02 08:00:00', 'Released', 'Boat B-02 Patna flood ops'),
(9,  6,  7, 2, '2024-12-04 10:00:00', '2024-12-04 14:00:00', 'Released', 'Truck T-02 Tondiarpet'),
(10,11, 11, 3, '2024-07-10 09:30:00', '2024-07-10 15:00:00', 'Released', 'Boat B-04 Colaba Causeway');

-- All allocations are Released so all resources remain Available
-- (consistent with initial resource status = Available)

-- ─── resource_status_history (10) ────────────────────────────
INSERT IGNORE INTO resource_status_history
  (history_id, resource_id, old_status, new_status, changed_by, changed_at, remarks)
VALUES
(1,  1, 'Available', 'Allocated', 1, '2024-07-10 08:00:00', 'Allocated to DRERO-001'),
(2,  1, 'Allocated', 'Available', 1, '2024-07-10 14:00:00', 'Released after Borivali rescue'),
(3,  5, 'Available', 'Allocated', 1, '2024-07-10 08:15:00', 'Allocated to DRERO-002'),
(4,  5, 'Allocated', 'Available', 1, '2024-07-10 10:30:00', 'Released after mission complete'),
(5,  3, 'Available', 'Maintenance',1,'2024-07-09 06:00:00', 'Scheduled engine maintenance'),
(6,  6, 'Available', 'Allocated', 1, '2024-07-10 08:05:00', 'Additional support Borivali'),
(7,  6, 'Allocated', 'Available', 1, '2024-07-10 12:00:00', 'Released after Borivali ops'),
(8,  9, 'Available', 'Allocated', 1, '2024-07-10 10:00:00', 'Allocated to DRERO-004'),
(9,  2, 'Available', 'Allocated', 8, '2024-08-01 09:00:00', 'Patna flood deployment'),
(10, 2, 'Allocated', 'Available', 8, '2024-08-02 08:00:00', 'Returned from Patna ops');

-- ─── scheduling_runs (10) — fixed seed only, no accumulated runs ──
INSERT IGNORE INTO scheduling_runs
  (run_id, algorithm, time_quantum, started_by, started_at, finished_at, status)
VALUES
(1,  'FCFS',     NULL, 1, '2024-07-10 08:00:00', '2024-07-10 08:00:05', 'Completed'),
(2,  'SJF',      NULL, 1, '2024-07-10 08:01:00', '2024-07-10 08:01:04', 'Completed'),
(3,  'RR',       3,    1, '2024-07-10 08:02:00', '2024-07-10 08:02:08', 'Completed'),
(4,  'PRIORITY', NULL, 1, '2024-07-10 08:03:00', '2024-07-10 08:03:06', 'Completed'),
(5,  'FCFS',     NULL, 2, '2024-12-04 10:00:00', '2024-12-04 10:00:04', 'Completed'),
(6,  'PRIORITY', NULL, 2, '2024-12-04 10:01:00', '2024-12-04 10:01:05', 'Completed'),
(7,  'SJF',      NULL, 8, '2024-08-01 09:00:00', '2024-08-01 09:00:03', 'Completed'),
(8,  'RR',       5,    8, '2024-08-01 09:01:00', '2024-08-01 09:01:06', 'Completed'),
(9,  'PRIORITY', NULL, 1, '2024-07-10 09:00:00', '2024-07-10 09:00:07', 'Completed'),
(10, 'FCFS',     NULL, 1, '2024-09-10 06:00:00', '2024-09-10 06:00:03', 'Completed');

-- ─── scheduling_log (12) ─────────────────────────────────────
-- Hand-computed examples. Service time = people*0.5 + severity*2.0 units.
-- DRERO-001: 8*0.5 + 4*2 = 12  DRERO-002: 3*0.5+4*2=9.5  DRERO-004: 25*0.5+5*2=22.5
INSERT IGNORE INTO scheduling_log
  (log_id, run_id, request_id, queue_position, priority_score,
   start_time, completion_time, waiting_time, turnaround_time, response_time)
VALUES
(1,  1, 1, 1, NULL,   0,    12,    0,    12,    0),
(2,  1, 2, 2, NULL,  12,  21.5,   12,  20.5,   12),
(3,  1, 3, 3, NULL,21.5,  37.5, 21.5,  34.5,  21.5),
(4,  1, 4, 4, NULL,37.5,  60,   37.5,  52,    37.5),
(5,  2, 2, 1, NULL,   0,   9.5,   0,    9.5,   0),
(6,  2, 3, 2, NULL, 9.5,  25.5,  9.5,  22.5,  9.5),
(7,  4, 4, 1, 100.0, 0,   22.5,   0,   22.5,   0),
(8,  4, 1, 2,  82.3,22.5, 34.5, 22.5,  27,   22.5),
(9,  4, 9, 3,  91.0,34.5, 58.5, 34.5,  48.5, 34.5),
(10, 5, 5, 1, NULL,   0,   18.5,  0,   18.5,   0),
(11, 5, 6, 2, NULL,18.5,  27,   18.5,  17.5, 18.5),
(12, 6, 5, 1,  98.2,  0,   18.5,  0,   18.5,   0);

-- ─── audit_log (12) ──────────────────────────────────────────
INSERT IGNORE INTO audit_log
  (audit_id, user_id, action, entity_type, entity_id, action_time, details)
VALUES
(1,  NULL, 'REQUEST_SUBMITTED',     'rescue_request', 1,  '2024-07-10 07:15:00', 'Public submission DRERO-001'),
(2,  1,    'REQUEST_STATUS_UPDATE', 'rescue_request', 1,  '2024-07-10 08:00:00', 'Status: New -> Ready'),
(3,  1,    'RESOURCE_ALLOCATED',    'resource',       1,  '2024-07-10 08:00:00', 'Boat B-01 allocated to DRERO-001'),
(4,  NULL, 'REQUEST_SUBMITTED',     'rescue_request', 2,  '2024-07-10 07:45:00', 'Public submission DRERO-002'),
(5,  1,    'SCHEDULING_RUN',        'scheduling_run', 1,  '2024-07-10 08:00:00', 'FCFS run completed, 4 requests'),
(6,  1,    'SCHEDULING_RUN',        'scheduling_run', 4,  '2024-07-10 08:03:00', 'PRIORITY run completed, 3 requests'),
(7,  1,    'RESOURCE_RELEASED',     'resource',       1,  '2024-07-10 14:00:00', 'Boat B-01 released after Borivali ops'),
(8,  1,    'RESOURCE_RELEASED',     'resource',       5,  '2024-07-10 10:30:00', 'Medical M-02 released'),
(9,  2,    'REQUEST_STATUS_UPDATE', 'rescue_request', 8,  '2024-08-15 12:00:00', 'DRERO-008 deferred: no resource in zone'),
(10, 1,    'SEVERITY_UPDATED',      'disaster_zone',  3,  '2024-07-10 07:00:00', 'Zone 3 severity set to 5 for Mumbai Floods'),
(11, NULL, 'REQUEST_SUBMITTED',     'rescue_request', 5,  '2024-12-04 09:00:00', 'Public submission DRERO-005'),
(12, 8,    'RESOURCE_ALLOCATED',    'resource',       2,  '2024-08-01 09:00:00', 'Boat B-02 allocated for Patna ops');

-- ─── deadlock_events (10) ────────────────────────────────────
INSERT IGNORE INTO deadlock_events
  (event_id, detected_at, cycle_details, resolution_action, resolved_at, status)
VALUES
(1,  '2024-07-10 08:05:00',
     'P1(DRERO-001)->R1(Boat B-01)->P2(DRERO-003)->R2(Truck T-01)->P1(DRERO-001)',
     'Deferred DRERO-003, released R2 hold',       '2024-07-10 08:06:00', 'Resolved'),
(2,  '2024-07-10 09:00:00',
     'P3(DRERO-004)->R3(Truck T-03)->P4(DRERO-011)->R1(Boat B-01)->P3',
     'Preempted lower-priority request P4',         '2024-07-10 09:01:00', 'Resolved'),
(3,  '2024-12-04 10:05:00',
     'P5(DRERO-005)->R4(Medical M-01)->P6(DRERO-006)->R5(Truck T-02)->P5',
     'Rejected DRERO-006 allocation attempt',       '2024-12-04 10:06:00', 'Resolved'),
(4,  '2024-08-01 09:10:00',
     'P7(DRERO-009)->R2(Boat B-02)->P8(DRERO-008)->R6(Relief S-02)->P7',
     'Deferred DRERO-008',                          '2024-08-01 09:11:00', 'Resolved'),
(5,  '2024-07-10 10:00:00',
     'Simulated test cycle: P1->R10->P2->R11->P1',
     'Demo cycle: resolved by P2 deferral',         '2024-07-10 10:01:00', 'Simulated'),
(6,  '2024-08-15 12:30:00',
     'P9(DRERO-008)->R9(Relief S-01)->P10(DRERO-012)->R2(Boat B-02)->P9',
     'Released hold on R9 for P10',                 '2024-08-15 12:31:00', 'Resolved'),
(7,  '2024-09-10 05:30:00',
     'P10(DRERO-010)->R4(Medical M-01)->P11(DRERO-007)->R7(Truck T-02)->P10',
     'P11 deferred, R7 freed',                      '2024-09-10 05:31:00', 'Resolved'),
(8,  '2024-07-10 11:00:00',
     'P12(DRERO-012)->R1(Boat B-01)->P1(DRERO-001)->R9(Relief S-01)->P12',
     'Cancelled P12 relief hold pending review',    '2024-07-10 11:02:00', 'Resolved'),
(9,  '2024-12-04 11:00:00',
     'Banker algorithm unsafe state: no safe sequence for 3 processes',
     'Denied allocation request',                   '2024-12-04 11:01:00', 'Resolved'),
(10, '2024-10-01 15:00:00',
     'Simulated multi-resource cycle: P1->R1->P2->R2->P3->R3->P1',
     'Demo: P3 deferred to break cycle',            '2024-10-01 15:01:00', 'Simulated');

SET FOREIGN_KEY_CHECKS = 1;
