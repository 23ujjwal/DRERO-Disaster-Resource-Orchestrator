// core/include/database.h
// Database class — ALL method bodies inline, no :: in any .cpp
#pragma once
#include <bits/stdc++.h>
#include "drero_config.h"
#include "models.h"
#include <mysql_driver.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/exception.h>
using namespace std;

typedef struct { int id; string name; string state; } city_t;
typedef struct { int id; string name; int zone_id;  } locality_t;
typedef struct { int id; string name;               } asst_type_t;
typedef struct { int id; string name;               } res_type_t;

typedef struct {
    string old_status, new_status, changed_at, remarks;
} status_history;

typedef struct {
    int    audit_id;
    string actor, action, entity_type, action_time, details;
    int    entity_id;
} audit_entry;

typedef struct {
    int    log_id, run_id, request_id, queue_position;
    string algorithm, public_ref;
    double priority_score, waiting_time, turnaround_time, response_time;
} sched_log_entry;

class Database {
public:
    sql::Connection *conn;
    db_config        cfg;

    // ── constructor / destructor ───────────────────────────────
    Database() {
        cfg  = load_db_config();
        conn = NULL;
    }

    ~Database() {
        if (conn != NULL) { delete conn; conn = NULL; }
    }

    // ── connection ─────────────────────────────────────────────
    int connect() {
        try {
            sql::mysql::MySQL_Driver *drv = sql::mysql::get_mysql_driver_instance();
            string url = "tcp://" + cfg.host + ":" + to_string(cfg.port);
            conn = drv->connect(url, cfg.user, cfg.password);
            conn->setSchema(cfg.dbname);
            conn->setAutoCommit(true);
            return 1;
        } catch (sql::SQLException &e) {
            cerr << "[connect error] " << e.what() << "\n";
            conn = NULL;
            return 0;
        }
    }

    void disconnect() {
        if (conn != NULL) { delete conn; conn = NULL; }
    }

    int is_connected() { return conn != NULL ? 1 : 0; }

    // ── gen public ref ─────────────────────────────────────────
    string gen_ref() {
        srand((unsigned)time(NULL) ^ (unsigned)(size_t)this);
        int num = 10000 + rand() % 90000;
        time_t t = time(NULL);
        tm *tm_s = localtime(&t);
        char buf[8];
        strftime(buf, sizeof(buf), "%y%m%d", tm_s);
        return "REQ-" + to_string(num) + "-" + string(buf);
    }

    // ── authentication ─────────────────────────────────────────
    int check_password(string entered, string stored_value) {
        return (entered == stored_value) ? 1 : 0;
    }

    int authenticate(string username, string password) {
        if (!is_connected()) return -1;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        int uid = -1;
        try {
            ps = conn->prepareStatement(
                "SELECT user_id, password_hash FROM users WHERE username=? AND is_active=1");
            ps->setString(1, username);
            rs = ps->executeQuery();
            if (rs->next()) {
                uid = rs->getInt("user_id");
                string hash = rs->getString("password_hash");
                if (!check_password(password, hash)) uid = -1;
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[authenticate] " << e.what() << "\n";
        }
        return uid;
    }

    string get_role(int user_id) {
        if (!is_connected()) return "";
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        string role = "";
        try {
            ps = conn->prepareStatement("SELECT role FROM users WHERE user_id=?");
            ps->setInt(1, user_id);
            rs = ps->executeQuery();
            if (rs->next()) role = rs->getString("role");
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_role] " << e.what() << "\n";
        }
        return role;
    }

    string get_username(int user_id) {
        if (!is_connected()) return "";
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        string name = "";
        try {
            ps = conn->prepareStatement("SELECT username FROM users WHERE user_id=?");
            ps->setInt(1, user_id);
            rs = ps->executeQuery();
            if (rs->next()) name = rs->getString("username");
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_username] " << e.what() << "\n";
        }
        return name;
    }

    int get_user_zone(int user_id) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        int zid = 0;
        try {
            ps = conn->prepareStatement("SELECT zone_id FROM users WHERE user_id=?");
            ps->setInt(1, user_id);
            rs = ps->executeQuery();
            if (rs->next() && !rs->isNull("zone_id")) zid = rs->getInt("zone_id");
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_user_zone] " << e.what() << "\n";
        }
        return zid;
    }

    // ── helper: fill pcb from result set ──────────────────────
    void fill_pcb(pcb &p, sql::ResultSet *rs) {
        p.request_id         = rs->getInt("request_id");
        p.public_ref         = rs->getString("public_ref");
        p.disaster_id        = rs->getInt("disaster_id");
        p.locality_id        = rs->getInt("locality_id");
        p.zone_id            = rs->getInt("zone_id");
        p.victim_name        = rs->getString("victim_name");
        p.contact_number     = rs->getString("contact_number");
        p.reported_location  = rs->getString("reported_location");
        p.assistance_type_id = rs->getInt("assistance_type_id");
        p.people_affected    = rs->getInt("people_affected");
        p.severity_snapshot  = rs->getInt("severity_snapshot");
        p.risk_level         = 1;
        try { p.risk_level   = rs->getInt("risk_level"); } catch (...) {}
        p.status             = rs->getString("status");
        try { p.arrival_time_str = rs->getString("arrival_time_str"); } catch (...) {}
        try { p.assistance_type  = rs->getString("assistance_type");  } catch (...) {}
        try { p.disaster_name    = rs->getString("disaster_name");    } catch (...) {}
        try { p.city_name        = rs->getString("city_name");        } catch (...) {}
        try { p.zone_name        = rs->getString("zone_name");        } catch (...) {}
        try { p.locality_name    = rs->getString("locality_name");    } catch (...) {}
        try { p.details          = rs->getString("details");          } catch (...) {}
        p.arrival_time = 0; p.service_time = 0; p.priority_score = 0;
        p.waiting_time = 0; p.response_time = 0; p.turnaround_time = 0;
        p.start_time   = 0; p.completion_time = 0; p.queue_position = 0;
    }

    // ── requests ───────────────────────────────────────────────
    vector<pcb> get_pending_requests(string *error = nullptr) {
        vector<pcb> out;
        if (!is_connected()) {
            if (error) *error = "Database is not connected";
            return out;
        }
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT r.*, "
                "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
                "  COALESCE(dzs.risk_level,1) AS risk_level, "
                "  v.assistance_type, v.disaster_name, v.city_name, v.zone_name, v.locality_name "
                "FROM pending_rescue_requests v "
                "JOIN rescue_requests r ON r.request_id=v.request_id "
                "LEFT JOIN disaster_zone_status dzs "
                "  ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
                "WHERE NOT EXISTS ("
                "  SELECT 1 FROM allocations al "
                "  WHERE al.request_id=r.request_id AND al.status='Active'"
                ") "
                "UNION ALL "
                "SELECT r.*, "
                "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
                "  COALESCE(dzs.risk_level,1) AS risk_level, "
                "  a.type_name AS assistance_type, "
                "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
                "FROM rescue_requests r "
                "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
                "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
                "LEFT JOIN zones z ON r.zone_id=z.zone_id "
                "LEFT JOIN localities l ON r.locality_id=l.locality_id "
                "LEFT JOIN cities c ON z.city_id=c.city_id "
                "LEFT JOIN disaster_zone_status dzs "
                "  ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
                "WHERE r.status='Under Review' "
                "AND NOT EXISTS ("
                "  SELECT 1 FROM allocations al "
                "  WHERE al.request_id=r.request_id AND al.status='Active'"
                ") "
                "ORDER BY severity_snapshot DESC, arrival_time ASC");
            while (rs->next()) { pcb p; fill_pcb(p, rs); out.push_back(p); }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_pending_requests] " << e.what() << "\n";
            if (error) *error = e.what();
        }
        return out;
    }

    vector<pcb> get_all_requests(int limit) {
        vector<pcb> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT r.*, "
                "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
                "  COALESCE(dzs.risk_level,1) AS risk_level, "
                "  a.type_name AS assistance_type, "
                "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
                "FROM rescue_requests r "
                "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
                "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
                "LEFT JOIN zones z ON r.zone_id=z.zone_id "
                "LEFT JOIN localities l ON r.locality_id=l.locality_id "
                "LEFT JOIN cities c ON z.city_id=c.city_id "
                "LEFT JOIN disaster_zone_status dzs ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
                "ORDER BY r.arrival_time DESC LIMIT ?");
            ps->setInt(1, limit);
            rs = ps->executeQuery();
            while (rs->next()) { pcb p; fill_pcb(p, rs); out.push_back(p); }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_all_requests] " << e.what() << "\n";
        }
        return out;
    }

    pcb get_request_by_id(int id, string *error = nullptr) {
        pcb p; p.request_id = 0;
        if (!is_connected()) {
            if (error) *error = "Database is not connected";
            return p;
        }
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT r.*, "
                "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
                "  COALESCE(dzs.risk_level,1) AS risk_level, "
                "  a.type_name AS assistance_type, "
                "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
                "FROM rescue_requests r "
                "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
                "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
                "LEFT JOIN zones z ON r.zone_id=z.zone_id "
                "LEFT JOIN localities l ON r.locality_id=l.locality_id "
                "LEFT JOIN cities c ON z.city_id=c.city_id "
                "LEFT JOIN disaster_zone_status dzs ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
                "WHERE r.request_id=?");
            ps->setInt(1, id);
            rs = ps->executeQuery();
            if (rs->next()) fill_pcb(p, rs);
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_request_by_id] " << e.what() << "\n";
            if (error) *error = e.what();
        }
        return p;
    }

    pcb get_request_by_ref(string ref) {
        pcb p; p.request_id = 0;
        if (!is_connected()) return p;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT r.*, "
                "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
                "  COALESCE(dzs.risk_level,1) AS risk_level, "
                "  a.type_name AS assistance_type, "
                "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
                "FROM rescue_requests r "
                "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
                "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
                "LEFT JOIN zones z ON r.zone_id=z.zone_id "
                "LEFT JOIN localities l ON r.locality_id=l.locality_id "
                "LEFT JOIN cities c ON z.city_id=c.city_id "
                "LEFT JOIN disaster_zone_status dzs ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
                "WHERE r.public_ref=?");
            ps->setString(1, ref);
            rs = ps->executeQuery();
            if (rs->next()) fill_pcb(p, rs);
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_request_by_ref] " << e.what() << "\n";
        }
        return p;
    }

    int insert_request(pcb &req) {
        if (!is_connected()) return -1;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        sql::Statement *st;
        int new_id = -1;
        try {
            // Resolve zone from locality (read-only, outside transaction)
            int zone_id = get_zone_for_locality(req.locality_id);
            if (zone_id <= 0) { cerr << "[insert_request] bad locality\n"; return -1; }

            // Retrieve disaster-specific severity (never trust caller's value)
            int sev = get_severity_for_zone(req.disaster_id, zone_id);
            if (sev <= 0) sev = 1;

            string ref = gen_ref();
            req.public_ref = ref;
            req.zone_id = zone_id;
            req.severity_snapshot = sev;
            req.status = "Submitted";

            // Begin transaction: insert + history + audit are atomic
            conn->setAutoCommit(false);

            ps = conn->prepareStatement(
                "INSERT INTO rescue_requests "
                "(public_ref,disaster_id,locality_id,zone_id,victim_name,contact_number,"
                " reported_location,assistance_type_id,people_affected,details,"
                " severity_snapshot,status,created_at,arrival_time,updated_at) "
                "VALUES (?,?,?,?,?,?,?,?,?,?,?,'Submitted',NOW(),NOW(),NOW())");
            ps->setString(1, ref);
            ps->setInt(2, req.disaster_id);
            ps->setInt(3, req.locality_id);
            ps->setInt(4, zone_id);
            ps->setString(5, req.victim_name);
            ps->setString(6, req.contact_number);
            ps->setString(7, req.reported_location);
            ps->setInt(8, req.assistance_type_id);
            ps->setInt(9, req.people_affected);
            ps->setString(10, req.details);
            ps->setInt(11, sev);
            ps->executeUpdate();
            delete ps;

            st = conn->createStatement();
            rs = st->executeQuery("SELECT LAST_INSERT_ID() AS id");
            if (rs->next()) new_id = rs->getInt("id");
            delete rs; delete st;
            req.request_id = new_id;

            if (new_id > 0) {
                // Record initial status history in same transaction
                ps = conn->prepareStatement(
                    "INSERT INTO request_status_history "
                    "(request_id,old_status,new_status,changed_at,remarks) "
                    "VALUES (?,?,?,NOW(),?)");
                ps->setInt(1, new_id);
                ps->setString(2, "NONE");
                ps->setString(3, "Submitted");
                ps->setString(4, "Public submission via CLI/API");
                ps->executeUpdate();
                delete ps;

                // Audit entry in same transaction
                ps = conn->prepareStatement(
                    "INSERT INTO audit_log "
                    "(user_id,action,entity_type,entity_id,action_time,details) "
                    "VALUES (NULL,'REQUEST_SUBMITTED','rescue_request',?,NOW(),?)");
                ps->setInt(1, new_id);
                ps->setString(2, "Public request submitted: " + ref);
                ps->executeUpdate();
                delete ps;
            }

            conn->commit();
            conn->setAutoCommit(true);
        } catch (sql::SQLException &e) {
            try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
            cerr << "[insert_request] " << e.what() << "\n";
            new_id = -1;
        }
        return new_id;
    }

    int update_request_status(int id, string new_status, int user_id, string remarks) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        string old_status = "";
        try {
            ps = conn->prepareStatement(
                "SELECT status FROM rescue_requests WHERE request_id=?");
            ps->setInt(1, id);
            rs = ps->executeQuery();
            if (rs->next()) old_status = rs->getString("status");
            delete rs; delete ps;

            if (old_status.empty()) return 0;
            if (!valid_transition(old_status, new_status)) return 0;

            ps = conn->prepareStatement(
                "UPDATE rescue_requests SET status=?,updated_at=NOW() WHERE request_id=?");
            ps->setString(1, new_status);
            ps->setInt(2, id);
            ps->executeUpdate();
            delete ps;

            ps = conn->prepareStatement(
                "INSERT INTO request_status_history "
                "(request_id,old_status,new_status,changed_by,changed_at,remarks) "
                "VALUES (?,?,?,?,NOW(),?)");
            ps->setInt(1, id);
            ps->setString(2, old_status);
            ps->setString(3, new_status);
            if (user_id > 0) ps->setInt(4, user_id); else ps->setNull(4, 0);
            ps->setString(5, remarks);
            ps->executeUpdate();
            delete ps;

            audit(user_id, "UPDATE_STATUS", "rescue_requests", id,
                  old_status + "->" + new_status);
            return 1;
        } catch (sql::SQLException &e) {
            cerr << "[update_request_status] " << e.what() << "\n";
        }
        return 0;
    }

    // ── resources ──────────────────────────────────────────────
    void fill_resource(resource &r, sql::ResultSet *rs) {
        r.resource_id      = rs->getInt("resource_id");
        r.resource_name    = rs->getString("resource_name");
        r.resource_type_id = rs->getInt("resource_type_id");
        r.zone_id          = rs->getInt("zone_id");
        r.capacity         = rs->getInt("capacity");
        r.capacity_unit    = rs->getString("capacity_unit");
        r.status           = rs->getString("status");
        try { r.resource_type = rs->getString("type_name");  } catch (...) {}
        try { r.zone_name     = rs->getString("zone_name");  } catch (...) {}
        try { r.city_name     = rs->getString("city_name");  } catch (...) {}
        try { r.notes         = rs->getString("notes");      } catch (...) {}
    }

    vector<resource> get_available_resources() {
        vector<resource> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT r.*, v.resource_type AS type_name, v.zone_name, v.city_name "
                "FROM available_resources_summary v "
                "JOIN resources r ON r.resource_id=v.resource_id "
                "ORDER BY v.resource_name");
            while (rs->next()) { resource r; fill_resource(r, rs); out.push_back(r); }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_available_resources] " << e.what() << "\n";
        }
        return out;
    }

    vector<resource> get_all_resources(string *error = nullptr) {
        vector<resource> out;
        if (!is_connected()) {
            if (error) *error = "Database is not connected";
            return out;
        }
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT r.*, t.type_name, z.zone_name, c.city_name "
                "FROM resources r "
                "LEFT JOIN resource_types t ON r.resource_type_id=t.resource_type_id "
                "LEFT JOIN zones z ON r.zone_id=z.zone_id "
                "LEFT JOIN cities c ON z.city_id=c.city_id "
                "ORDER BY r.status, r.resource_name");
            while (rs->next()) { resource r; fill_resource(r, rs); out.push_back(r); }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_all_resources] " << e.what() << "\n";
            if (error) *error = e.what();
        }
        return out;
    }

    resource get_resource_by_id(int id) {
        resource r; r.resource_id = 0;
        if (!is_connected()) return r;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT r.*, t.type_name, z.zone_name, c.city_name "
                "FROM resources r "
                "LEFT JOIN resource_types t ON r.resource_type_id=t.resource_type_id "
                "LEFT JOIN zones z ON r.zone_id=z.zone_id "
                "LEFT JOIN cities c ON z.city_id=c.city_id "
                "WHERE r.resource_id=?");
            ps->setInt(1, id);
            rs = ps->executeQuery();
            if (rs->next()) fill_resource(r, rs);
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_resource_by_id] " << e.what() << "\n";
        }
        return r;
    }

    int insert_resource(resource r, int user_id) {
        if (!is_connected()) return -1;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        sql::Statement *st;
        int new_id = -1;
        try {
            ps = conn->prepareStatement(
                "INSERT INTO resources "
                "(resource_name,resource_type_id,zone_id,capacity,capacity_unit,"
                " status,notes,created_by,created_at,updated_at) "
                "VALUES (?,?,?,?,?,?,?,?,NOW(),NOW())");
            ps->setString(1, r.resource_name);
            ps->setInt(2, r.resource_type_id);
            ps->setInt(3, r.zone_id);
            ps->setInt(4, r.capacity);
            ps->setString(5, r.capacity_unit);
            ps->setString(6, r.status.empty() ? RES_AVAILABLE : r.status);
            ps->setString(7, r.notes);
            if (user_id > 0) ps->setInt(8, user_id); else ps->setNull(8, 0);
            ps->executeUpdate();
            delete ps;

            st = conn->createStatement();
            rs = st->executeQuery("SELECT LAST_INSERT_ID() AS id");
            if (rs->next()) new_id = rs->getInt("id");
            delete rs; delete st;

            if (new_id > 0)
                audit(user_id, "INSERT_RESOURCE", "resources", new_id,
                      "Added: " + r.resource_name);
        } catch (sql::SQLException &e) {
            cerr << "[insert_resource] " << e.what() << "\n";
        }
        return new_id;
    }

    int update_resource_status(int id, string new_status, int user_id, string remarks) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        string old_status = "";
        try {
            ps = conn->prepareStatement(
                "SELECT status FROM resources WHERE resource_id=?");
            ps->setInt(1, id);
            rs = ps->executeQuery();
            if (rs->next()) old_status = rs->getString("status");
            delete rs; delete ps;
            if (old_status.empty()) return 0;

            ps = conn->prepareStatement(
                "UPDATE resources SET status=?,updated_at=NOW() WHERE resource_id=?");
            ps->setString(1, new_status);
            ps->setInt(2, id);
            ps->executeUpdate();
            delete ps;

            ps = conn->prepareStatement(
                "INSERT INTO resource_status_history "
                "(resource_id,old_status,new_status,changed_by,changed_at,remarks) "
                "VALUES (?,?,?,?,NOW(),?)");
            ps->setInt(1, id);
            ps->setString(2, old_status);
            ps->setString(3, new_status);
            if (user_id > 0) ps->setInt(4, user_id); else ps->setNull(4, 0);
            ps->setString(5, remarks);
            ps->executeUpdate();
            delete ps;

            audit(user_id, "UPDATE_RESOURCE_STATUS", "resources", id,
                  old_status + "->" + new_status);
            return 1;
        } catch (sql::SQLException &e) {
            cerr << "[update_resource_status] " << e.what() << "\n";
        }
        return 0;
    }

    // ── allocation ─────────────────────────────────────────────
    int allocate_resource(int request_id, int resource_id, int user_id, string &err) {
        if (!is_connected()) { err = "not connected"; return -1; }
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        sql::Statement *st;
        int alloc_id = -1;
        try {
            conn->setAutoCommit(false);

            // Lock request first so duplicate attempts for one request serialize.
            ps = conn->prepareStatement(
                "SELECT status, assistance_type_id, people_affected "
                "FROM rescue_requests WHERE request_id=? FOR UPDATE");
            ps->setInt(1, request_id);
            rs = ps->executeQuery();
            string request_status;
            int assistance_type_id = 0;
            int people_affected = 0;
            if (rs->next()) {
                request_status = rs->getString("status");
                assistance_type_id = rs->getInt("assistance_type_id");
                people_affected = rs->getInt("people_affected");
            }
            delete rs; delete ps;

            if (request_status.empty()) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Request not found";
                return -1;
            }
            if (request_status != "Submitted" && request_status != "New" &&
                request_status != "Ready" && request_status != "Scheduled" &&
                request_status != "Under Review" && request_status != "Deferred") {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Request is not eligible for allocation (status=" + request_status + ")";
                return -1;
            }

            ps = conn->prepareStatement(
                "SELECT allocation_id FROM allocations "
                "WHERE request_id=? AND status='Active' LIMIT 1 FOR UPDATE");
            ps->setInt(1, request_id);
            rs = ps->executeQuery();
            bool request_already_allocated = rs->next();
            delete rs; delete ps;
            if (request_already_allocated) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Request already has an active allocation";
                return -1;
            }

            // Lock resource before checking its status and compatibility.
            ps = conn->prepareStatement(
                "SELECT r.status, r.resource_type_id, r.capacity, r.capacity_unit, rt.type_name "
                "FROM resources r "
                "JOIN resource_types rt ON r.resource_type_id=rt.resource_type_id "
                "WHERE r.resource_id=? FOR UPDATE");
            ps->setInt(1, resource_id);
            rs = ps->executeQuery();
            resource candidate;
            candidate.resource_id = 0;
            if (rs->next()) {
                candidate.resource_id = resource_id;
                candidate.status = rs->getString("status");
                candidate.resource_type_id = rs->getInt("resource_type_id");
                candidate.capacity = rs->getInt("capacity");
                candidate.capacity_unit = rs->getString("capacity_unit");
                candidate.resource_type = rs->getString("type_name");
            }
            delete rs; delete ps;
            if (candidate.resource_id == 0) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Resource not found";
                return -1;
            }
            if (candidate.status != RES_AVAILABLE) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Resource not available (status=" + candidate.status + ")";
                return -1;
            }

            string assistance_type;
            ps = conn->prepareStatement(
                "SELECT type_name FROM assistance_types WHERE assistance_type_id=?");
            ps->setInt(1, assistance_type_id);
            rs = ps->executeQuery();
            if (rs->next()) assistance_type = rs->getString("type_name");
            delete rs; delete ps;

            if (!resource_supports_assistance(assistance_type, candidate.resource_type)) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Resource type " + candidate.resource_type +
                      " is not compatible with assistance type " + assistance_type;
                return -1;
            }
            pcb request;
            request.people_affected = people_affected;
            if (!resource_has_capacity_for_request(request, candidate)) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Resource capacity is below the number of affected people";
                return -1;
            }

            ps = conn->prepareStatement(
                "SELECT allocation_id FROM allocations "
                "WHERE resource_id=? AND status='Active' LIMIT 1 FOR UPDATE");
            ps->setInt(1, resource_id);
            rs = ps->executeQuery();
            bool resource_already_allocated = rs->next();
            delete rs; delete ps;
            if (resource_already_allocated) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Resource already has an active allocation";
                return -1;
            }

            // insert allocation
            ps = conn->prepareStatement(
                "INSERT INTO allocations "
                "(request_id,resource_id,allocated_by,allocated_at,status) "
                "VALUES (?,?,?,NOW(),'Active')");
            ps->setInt(1, request_id);
            ps->setInt(2, resource_id);
            if (user_id > 0) ps->setInt(3, user_id); else ps->setNull(3, 0);
            ps->executeUpdate();
            delete ps;

            st = conn->createStatement();
            rs = st->executeQuery("SELECT LAST_INSERT_ID() AS id");
            if (rs->next()) alloc_id = rs->getInt("id");
            delete rs; delete st;

            ps = conn->prepareStatement(
                "INSERT INTO request_status_history "
                "(request_id,old_status,new_status,changed_by,changed_at,remarks) "
                "VALUES (?,?,'Allocated',?,NOW(),'Allocated by dispatcher')");
            ps->setInt(1, request_id);
            ps->setString(2, request_status);
            if (user_id > 0) ps->setInt(3, user_id); else ps->setNull(3, 0);
            ps->executeUpdate();
            delete ps;

            ps = conn->prepareStatement(
                "INSERT INTO resource_status_history "
                "(resource_id,old_status,new_status,changed_by,changed_at,remarks) "
                "VALUES (?,?,'Allocated',?,NOW(),'Allocated to rescue request')");
            ps->setInt(1, resource_id);
            ps->setString(2, candidate.status);
            if (user_id > 0) ps->setInt(3, user_id); else ps->setNull(3, 0);
            ps->executeUpdate();
            delete ps;

            ps = conn->prepareStatement(
                "UPDATE resources SET status='Allocated',updated_at=NOW() "
                "WHERE resource_id=? AND status='Available'");
            ps->setInt(1, resource_id);
            if (ps->executeUpdate() != 1) {
                delete ps;
                throw runtime_error("Resource became unavailable during allocation");
            }
            delete ps;

            ps = conn->prepareStatement(
                "UPDATE rescue_requests SET status='Allocated',updated_at=NOW() "
                "WHERE request_id=? AND status=?");
            ps->setInt(1, request_id);
            ps->setString(2, request_status);
            if (ps->executeUpdate() != 1) {
                delete ps;
                throw runtime_error("Request status changed during allocation");
            }
            delete ps;

            ps = conn->prepareStatement(
                "INSERT INTO audit_log "
                "(user_id,action,entity_type,entity_id,action_time,details) "
                "VALUES (?, 'ALLOCATE_RESOURCE', 'allocations', ?, NOW(), ?)");
            if (user_id > 0) ps->setInt(1, user_id); else ps->setNull(1, 0);
            ps->setInt(2, alloc_id);
            ps->setString(3, "req=" + to_string(request_id) + " res=" + to_string(resource_id));
            ps->executeUpdate();
            delete ps;

            conn->commit();
            conn->setAutoCommit(true);
        } catch (sql::SQLException &e) {
            try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
            err = string(e.what());
            cerr << "[allocate_resource] " << e.what() << "\n";
        } catch (exception &e) {
            try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
            err = string(e.what());
            cerr << "[allocate_resource] " << e.what() << "\n";
        }
        return alloc_id;
    }

    int release_allocation(int alloc_id, int user_id, string &err) {
        if (!is_connected()) { err = "not connected"; return 0; }
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            conn->setAutoCommit(false);

            int resource_id = 0, request_id = 0;
            ps = conn->prepareStatement(
                "SELECT resource_id, request_id FROM allocations "
                "WHERE allocation_id=? AND status='Active'");
            ps->setInt(1, alloc_id);
            rs = ps->executeQuery();
            if (rs->next()) {
                resource_id = rs->getInt("resource_id");
                request_id  = rs->getInt("request_id");
            }
            delete rs; delete ps;

            if (resource_id == 0) {
                conn->rollback(); conn->setAutoCommit(true);
                err = "Allocation not found or not active";
                return 0;
            }

            ps = conn->prepareStatement(
                "UPDATE allocations SET status='Released',released_at=NOW() "
                "WHERE allocation_id=?");
            ps->setInt(1, alloc_id);
            ps->executeUpdate();
            delete ps;

            ps = conn->prepareStatement(
                "UPDATE resources SET status='Available',updated_at=NOW() "
                "WHERE resource_id=?");
            ps->setInt(1, resource_id);
            ps->executeUpdate();
            delete ps;

            ps = conn->prepareStatement(
                "UPDATE rescue_requests SET status='Completed',updated_at=NOW() "
                "WHERE request_id=?");
            ps->setInt(1, request_id);
            ps->executeUpdate();
            delete ps;

            conn->commit();
            conn->setAutoCommit(true);

            audit(user_id, "RELEASE_ALLOCATION", "allocations", alloc_id, "released");
            return 1;
        } catch (sql::SQLException &e) {
            try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
            err = string(e.what());
            cerr << "[release_allocation] " << e.what() << "\n";
        }
        return 0;
    }

    vector<allocation> get_allocations_for_request(int request_id) {
        vector<allocation> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT a.*, r.public_ref, res.resource_name "
                "FROM allocations a "
                "JOIN rescue_requests r ON a.request_id=r.request_id "
                "JOIN resources res ON a.resource_id=res.resource_id "
                "WHERE a.request_id=? ORDER BY a.allocated_at DESC");
            ps->setInt(1, request_id);
            rs = ps->executeQuery();
            while (rs->next()) {
                allocation al;
                al.allocation_id = rs->getInt("allocation_id");
                al.request_id    = rs->getInt("request_id");
                al.resource_id   = rs->getInt("resource_id");
                al.public_ref    = rs->getString("public_ref");
                al.resource_name = rs->getString("resource_name");
                al.status        = rs->getString("status");
                al.allocated_at  = rs->getString("allocated_at");
                try { al.released_at = rs->getString("released_at"); } catch (...) {}
                try { al.remarks     = rs->getString("remarks");     } catch (...) {}
                out.push_back(al);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_allocations_for_request] " << e.what() << "\n";
        }
        return out;
    }

    vector<allocation> get_active_allocations() {
        vector<allocation> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT a.*, r.public_ref, res.resource_name "
                "FROM allocations a "
                "JOIN rescue_requests r ON a.request_id=r.request_id "
                "JOIN resources res ON a.resource_id=res.resource_id "
                "WHERE a.status='Active' ORDER BY a.allocated_at DESC");
            while (rs->next()) {
                allocation al;
                al.allocation_id = rs->getInt("allocation_id");
                al.request_id    = rs->getInt("request_id");
                al.resource_id   = rs->getInt("resource_id");
                al.public_ref    = rs->getString("public_ref");
                al.resource_name = rs->getString("resource_name");
                al.status        = rs->getString("status");
                al.allocated_at  = rs->getString("allocated_at");
                out.push_back(al);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_active_allocations] " << e.what() << "\n";
        }
        return out;
    }

    vector<allocation> get_all_allocations(int limit) {
        vector<allocation> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT a.*, v.public_ref, v.resource_name "
                "FROM allocations a "
                "JOIN allocation_details v ON v.allocation_id=a.allocation_id "
                "ORDER BY a.allocated_at DESC LIMIT ?");
            ps->setInt(1, limit);
            rs = ps->executeQuery();
            while (rs->next()) {
                allocation al;
                al.allocation_id = rs->getInt("allocation_id");
                al.request_id    = rs->getInt("request_id");
                al.resource_id   = rs->getInt("resource_id");
                al.public_ref    = rs->getString("public_ref");
                al.resource_name = rs->getString("resource_name");
                al.status        = rs->getString("status");
                al.allocated_at  = rs->getString("allocated_at");
                try { al.released_at = rs->getString("released_at"); } catch (...) {}
                try { al.remarks     = rs->getString("remarks");     } catch (...) {}
                out.push_back(al);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_all_allocations] " << e.what() << "\n";
        }
        return out;
    }

    // ── disasters ──────────────────────────────────────────────
    vector<disaster> get_active_disasters() {
        vector<disaster> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT * FROM disasters WHERE status='Active' ORDER BY disaster_id ASC");
            while (rs->next()) {
                disaster d;
                d.disaster_id   = rs->getInt("disaster_id");
                d.disaster_name = rs->getString("disaster_name");
                d.disaster_type = rs->getString("disaster_type");
                d.status        = rs->getString("status");
                d.start_time    = rs->getString("start_time");
                out.push_back(d);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_active_disasters] " << e.what() << "\n";
        }
        return out;
    }

    vector<disaster> get_all_disasters() {
        vector<disaster> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT * FROM disasters ORDER BY start_time DESC");
            while (rs->next()) {
                disaster d;
                d.disaster_id   = rs->getInt("disaster_id");
                d.disaster_name = rs->getString("disaster_name");
                d.disaster_type = rs->getString("disaster_type");
                d.status        = rs->getString("status");
                d.start_time    = rs->getString("start_time");
                out.push_back(d);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_all_disasters] " << e.what() << "\n";
        }
        return out;
    }

    vector<zone_severity> get_zone_risks(int zone_id, string *error = nullptr) {
        vector<zone_severity> out;
        if (!is_connected()) {
            if (error) *error = "Database is not connected";
            return out;
        }
        sql::PreparedStatement *ps = nullptr;
        sql::ResultSet *rs;
        try {
            const string query =
                "SELECT dz.disaster_id, dz.zone_id, z.zone_name, d.disaster_name, d.status AS disaster_status, "
                "dz.severity_level, dz.risk_level, dz.risk_description, dz.updated_by, "
                "DATE_FORMAT(dz.updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at "
                "FROM disaster_zone_status dz "
                "JOIN zones z ON dz.zone_id=z.zone_id "
                "JOIN disasters d ON dz.disaster_id=d.disaster_id ";
            ps = conn->prepareStatement(zone_id > 0
                ? query + "WHERE dz.zone_id=? ORDER BY d.disaster_name"
                : query + "ORDER BY d.disaster_name,z.zone_name");
            if (zone_id > 0) ps->setInt(1, zone_id);
            rs = ps->executeQuery();
            while (rs->next()) {
                zone_severity zs{};
                zs.disaster_id      = rs->getInt("disaster_id");
                zs.zone_id          = rs->getInt("zone_id");
                zs.zone_name        = rs->getString("zone_name");
                zs.disaster_name    = rs->getString("disaster_name");
                zs.disaster_status  = rs->getString("disaster_status");
                zs.severity_level   = rs->getInt("severity_level");
                zs.risk_level       = rs->getInt("risk_level");
                try { zs.risk_description = rs->getString("risk_description"); } catch (...) {}
                zs.updated_by       = rs->getInt("updated_by");
                zs.updated_at       = rs->getString("updated_at");
                out.push_back(zs);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            if (ps) delete ps;
            cerr << "[get_zone_risks] " << e.what() << "\n";
            if (error) *error = e.what();
        }
        return out;
    }

    int update_zone_risk(int disaster_id, int zone_id, int risk_level,
                         string risk_description, int user_id, string &error) {
        if (!is_connected()) { error = "Database is not connected"; return 0; }
        sql::PreparedStatement *ps = nullptr;
        sql::ResultSet *rs = nullptr;
        try {
            conn->setAutoCommit(false);

            ps = conn->prepareStatement(
                "SELECT risk_level, risk_description FROM disaster_zone_status "
                "WHERE disaster_id=? AND zone_id=? FOR UPDATE");
            ps->setInt(1, disaster_id);
            ps->setInt(2, zone_id);
            rs = ps->executeQuery();
            if (!rs->next()) {
                delete rs; rs = nullptr;
                delete ps; ps = nullptr;
                conn->rollback(); conn->setAutoCommit(true);
                error = "No risk record exists for this disaster and your assigned zone";
                return 0;
            }
            int old_risk_level = rs->getInt("risk_level");
            string old_description;
            try { old_description = rs->getString("risk_description"); } catch (...) {}
            delete rs; rs = nullptr;
            delete ps; ps = nullptr;

            ps = conn->prepareStatement(
                "UPDATE disaster_zone_status "
                "SET risk_level=?, risk_description=?, updated_by=?, updated_at=NOW() "
                "WHERE disaster_id=? AND zone_id=?");
            ps->setInt(1, risk_level);
            ps->setString(2, risk_description);
            ps->setInt(3, user_id);
            ps->setInt(4, disaster_id);
            ps->setInt(5, zone_id);
            if (ps->executeUpdate() != 1) {
                delete ps; ps = nullptr;
                throw runtime_error("Risk record changed before it could be updated");
            }
            delete ps; ps = nullptr;

            ps = conn->prepareStatement(
                "INSERT INTO audit_log "
                "(user_id,action,entity_type,entity_id,action_time,details) "
                "VALUES (?,'UPDATE_ZONE_RISK','disaster_zone_status',?,NOW(),?)");
            ps->setInt(1, user_id);
            ps->setInt(2, disaster_id);
            ps->setString(3, "zone=" + to_string(zone_id) +
                " risk_level=" + to_string(old_risk_level) + "->" + to_string(risk_level) +
                " risk_description=" + old_description + "->" + risk_description);
            ps->executeUpdate();
            delete ps; ps = nullptr;

            conn->commit();
            conn->setAutoCommit(true);
            return 1;
        } catch (sql::SQLException &e) {
            if (rs) delete rs;
            if (ps) delete ps;
            try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
            error = e.what();
            cerr << "[update_zone_risk] " << e.what() << "\n";
        } catch (exception &e) {
            if (rs) delete rs;
            if (ps) delete ps;
            try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
            error = e.what();
            cerr << "[update_zone_risk] " << e.what() << "\n";
        }
        return 0;
    }

    int get_severity_for_zone(int disaster_id, int zone_id) {
        if (!is_connected()) return 1;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        int sev = 1;
        try {
            ps = conn->prepareStatement(
                "SELECT severity_level FROM disaster_zone_status "
                "WHERE disaster_id=? AND zone_id=?");
            ps->setInt(1, disaster_id);
            ps->setInt(2, zone_id);
            rs = ps->executeQuery();
            if (rs->next()) sev = rs->getInt("severity_level");
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_severity_for_zone] " << e.what() << "\n";
        }
        return sev;
    }

    int get_zone_for_locality(int locality_id) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        int zone_id = 0;
        try {
            ps = conn->prepareStatement(
                "SELECT zone_id FROM localities WHERE locality_id=?");
            ps->setInt(1, locality_id);
            rs = ps->executeQuery();
            if (rs->next()) zone_id = rs->getInt("zone_id");
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_zone_for_locality] " << e.what() << "\n";
        }
        return zone_id;
    }

    // ── scheduling runs ────────────────────────────────────────
    int insert_run(string algorithm, int quantum, int user_id) {
        if (!is_connected()) return -1;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        sql::Statement *st;
        int run_id = -1;
        try {
            ps = conn->prepareStatement(
                "INSERT INTO scheduling_runs "
                "(algorithm,time_quantum,started_by,started_at,status) "
                "VALUES (?,?,?,NOW(),'Running')");
            ps->setString(1, algorithm);
            if (quantum > 0) ps->setInt(2, quantum); else ps->setNull(2, 0);
            if (user_id > 0) ps->setInt(3, user_id); else ps->setNull(3, 0);
            ps->executeUpdate();
            delete ps;

            st = conn->createStatement();
            rs = st->executeQuery("SELECT LAST_INSERT_ID() AS id");
            if (rs->next()) run_id = rs->getInt("id");
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[insert_run] " << e.what() << "\n";
        }
        return run_id;
    }

    int finish_run(int run_id) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        try {
            ps = conn->prepareStatement(
                "UPDATE scheduling_runs SET status='Completed',finished_at=NOW() "
                "WHERE run_id=?");
            ps->setInt(1, run_id);
            ps->executeUpdate();
            delete ps;
            return 1;
        } catch (sql::SQLException &e) {
            cerr << "[finish_run] " << e.what() << "\n";
        }
        return 0;
    }

    int insert_log_entry(int run_id, pcb p) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        try {
            ps = conn->prepareStatement(
                "INSERT INTO scheduling_log "
                "(run_id,request_id,queue_position,priority_score,"
                " start_time,completion_time,waiting_time,turnaround_time,response_time) "
                "VALUES (?,?,?,?,?,?,?,?,?)");
            ps->setInt(1, run_id);
            ps->setInt(2, p.request_id);
            ps->setInt(3, p.queue_position);
            ps->setDouble(4, p.priority_score);
            ps->setDouble(5, p.start_time);
            ps->setDouble(6, p.completion_time);
            ps->setDouble(7, p.waiting_time);
            ps->setDouble(8, p.turnaround_time);
            ps->setDouble(9, p.response_time);
            ps->executeUpdate();
            delete ps;
            return 1;
        } catch (sql::SQLException &e) {
            cerr << "[insert_log_entry] " << e.what() << "\n";
        }
        return 0;
    }

    vector<sched_metrics> get_run_summary() {
        vector<sched_metrics> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT v.algorithm, v.requests_scheduled AS n, "
                "  v.avg_waiting_time AS avgW, "
                "  v.avg_turnaround_time AS avgT, "
                "  v.avg_response_time AS avgR, "
                "  CASE WHEN COALESCE(extra.max_completion,0)>0 "
                "       THEN v.requests_scheduled/extra.max_completion ELSE 0 END AS throughput, "
                "  COALESCE(extra.starvation,0) AS starvation "
                "FROM scheduling_performance_summary v "
                "JOIN scheduling_runs sr ON sr.run_id=v.run_id "
                "LEFT JOIN ("
                "  SELECT run_id, MAX(completion_time) AS max_completion, "
                "    SUM(CASE WHEN waiting_time>20 THEN 1 ELSE 0 END) AS starvation "
                "  FROM scheduling_log GROUP BY run_id"
                ") extra ON extra.run_id=v.run_id "
                "WHERE sr.status='Completed' "
                "ORDER BY v.started_at DESC LIMIT 20");
            while (rs->next()) {
                sched_metrics m;
                m.algorithm            = rs->getString("algorithm");
                m.requests_scheduled   = rs->getInt("n");
                m.avg_waiting_time     = rs->getDouble("avgW");
                m.avg_turnaround_time  = rs->getDouble("avgT");
                m.avg_response_time    = rs->getDouble("avgR");
                m.throughput           = rs->getDouble("throughput");
                m.starvation_count     = rs->getInt("starvation");
                out.push_back(m);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_run_summary] " << e.what() << "\n";
        }
        return out;
    }

    vector<sched_log_entry> get_run_log(int run_id, string *error = nullptr) {
        vector<sched_log_entry> out;
        if (!is_connected()) {
            if (error) *error = "Database is not connected";
            return out;
        }
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT sl.*, r.public_ref, sr.algorithm "
                "FROM scheduling_log sl "
                "JOIN rescue_requests r ON sl.request_id=r.request_id "
                "JOIN scheduling_runs sr ON sl.run_id=sr.run_id "
                "WHERE sl.run_id=? ORDER BY sl.queue_position");
            ps->setInt(1, run_id);
            rs = ps->executeQuery();
            while (rs->next()) {
                sched_log_entry e;
                e.log_id         = rs->getInt("log_id");
                e.run_id         = rs->getInt("run_id");
                e.request_id     = rs->getInt("request_id");
                e.queue_position = rs->getInt("queue_position");
                e.priority_score = rs->getDouble("priority_score");
                e.waiting_time   = rs->getDouble("waiting_time");
                e.turnaround_time= rs->getDouble("turnaround_time");
                e.response_time  = rs->getDouble("response_time");
                e.public_ref     = rs->getString("public_ref");
                e.algorithm      = rs->getString("algorithm");
                out.push_back(e);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_run_log] " << e.what() << "\n";
            if (error) *error = e.what();
        }
        return out;
    }

    // ── audit + history ────────────────────────────────────────
    void audit(int user_id, string action, string entity_type, int entity_id, string details) {
        if (!is_connected()) return;
        sql::PreparedStatement *ps;
        try {
            ps = conn->prepareStatement(
                "INSERT INTO audit_log "
                "(user_id,action,entity_type,entity_id,action_time,details) "
                "VALUES (?,?,?,?,NOW(),?)");
            if (user_id > 0) ps->setInt(1, user_id); else ps->setNull(1, 0);
            ps->setString(2, action);
            ps->setString(3, entity_type);
            ps->setInt(4, entity_id);
            ps->setString(5, details);
            ps->executeUpdate();
            delete ps;
        } catch (...) {}
    }

    vector<audit_entry> get_audit_log(int limit) {
        vector<audit_entry> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT audit_id, actor, action, entity_type, entity_id, action_time, details "
                "FROM audit_activity_report "
                "ORDER BY action_time DESC LIMIT ?");
            ps->setInt(1, limit);
            rs = ps->executeQuery();
            while (rs->next()) {
                audit_entry e;
                e.audit_id   = rs->getInt("audit_id");
                e.actor      = rs->getString("actor");
                e.action     = rs->getString("action");
                e.entity_type= rs->getString("entity_type");
                e.entity_id  = rs->getInt("entity_id");
                e.action_time= rs->getString("action_time");
                e.details    = rs->getString("details");
                out.push_back(e);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_audit_log] " << e.what() << "\n";
        }
        return out;
    }

    vector<status_history> get_request_history(int request_id) {
        vector<status_history> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT old_status,new_status,"
                "  DATE_FORMAT(changed_at,'%Y-%m-%d %H:%i:%s') AS changed_at, remarks "
                "FROM request_status_history WHERE request_id=? ORDER BY changed_at");
            ps->setInt(1, request_id);
            rs = ps->executeQuery();
            while (rs->next()) {
                status_history h;
                h.old_status = rs->getString("old_status");
                h.new_status = rs->getString("new_status");
                h.changed_at = rs->getString("changed_at");
                try { h.remarks = rs->getString("remarks"); } catch (...) {}
                out.push_back(h);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_request_history] " << e.what() << "\n";
        }
        return out;
    }

    vector<status_history> get_resource_history(int resource_id) {
        vector<status_history> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT old_status,new_status,"
                "  DATE_FORMAT(changed_at,'%Y-%m-%d %H:%i:%s') AS changed_at, remarks "
                "FROM resource_status_history WHERE resource_id=? ORDER BY changed_at");
            ps->setInt(1, resource_id);
            rs = ps->executeQuery();
            while (rs->next()) {
                status_history h;
                h.old_status = rs->getString("old_status");
                h.new_status = rs->getString("new_status");
                h.changed_at = rs->getString("changed_at");
                try { h.remarks = rs->getString("remarks"); } catch (...) {}
                out.push_back(h);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_resource_history] " << e.what() << "\n";
        }
        return out;
    }

    // ── deadlock events ────────────────────────────────────────
    int insert_deadlock(string cycle_details) {
        if (!is_connected()) return -1;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        sql::Statement *st;
        int new_id = -1;
        try {
            ps = conn->prepareStatement(
                "INSERT INTO deadlock_events "
                "(detected_at,cycle_details,status) VALUES (NOW(),?,'Detected')");
            ps->setString(1, cycle_details);
            ps->executeUpdate();
            delete ps;
            st = conn->createStatement();
            rs = st->executeQuery("SELECT LAST_INSERT_ID() AS id");
            if (rs->next()) new_id = rs->getInt("id");
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[insert_deadlock] " << e.what() << "\n";
        }
        return new_id;
    }

    int resolve_deadlock(int event_id, string action) {
        if (!is_connected()) return 0;
        sql::PreparedStatement *ps;
        try {
            ps = conn->prepareStatement(
                "UPDATE deadlock_events "
                "SET resolution_action=?,resolved_at=NOW(),status='Resolved' "
                "WHERE event_id=?");
            ps->setString(1, action);
            ps->setInt(2, event_id);
            ps->executeUpdate();
            delete ps;
            return 1;
        } catch (sql::SQLException &e) {
            cerr << "[resolve_deadlock] " << e.what() << "\n";
        }
        return 0;
    }

    // ── reference data ─────────────────────────────────────────
    vector<city_t> get_cities() {
        vector<city_t> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT city_id,city_name,state_name FROM cities "
                "WHERE is_active=1 ORDER BY city_id ASC");
            while (rs->next()) {
                city_t c;
                c.id    = rs->getInt("city_id");
                c.name  = rs->getString("city_name");
                c.state = rs->getString("state_name");
                out.push_back(c);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_cities] " << e.what() << "\n";
        }
        return out;
    }

    vector<locality_t> get_localities(int city_id) {
        vector<locality_t> out;
        if (!is_connected()) return out;
        sql::PreparedStatement *ps;
        sql::ResultSet *rs;
        try {
            ps = conn->prepareStatement(
                "SELECT l.locality_id, l.locality_name, l.zone_id "
                "FROM localities l "
                "JOIN zones z ON l.zone_id=z.zone_id "
                "WHERE z.city_id=? AND l.is_active=1 ORDER BY l.locality_name");
            ps->setInt(1, city_id);
            rs = ps->executeQuery();
            while (rs->next()) {
                locality_t l;
                l.id      = rs->getInt("locality_id");
                l.name    = rs->getString("locality_name");
                l.zone_id = rs->getInt("zone_id");
                out.push_back(l);
            }
            delete rs; delete ps;
        } catch (sql::SQLException &e) {
            cerr << "[get_localities] " << e.what() << "\n";
        }
        return out;
    }

    vector<asst_type_t> get_assistance_types() {
        vector<asst_type_t> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT assistance_type_id,type_name FROM assistance_types "
                "WHERE is_active=1 ORDER BY type_name");
            while (rs->next()) {
                asst_type_t t;
                t.id   = rs->getInt("assistance_type_id");
                t.name = rs->getString("type_name");
                out.push_back(t);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_assistance_types] " << e.what() << "\n";
        }
        return out;
    }

    vector<res_type_t> get_resource_types() {
        vector<res_type_t> out;
        if (!is_connected()) return out;
        sql::Statement *st;
        sql::ResultSet *rs;
        try {
            st = conn->createStatement();
            rs = st->executeQuery(
                "SELECT resource_type_id,type_name FROM resource_types "
                "WHERE is_active=1 ORDER BY type_name");
            while (rs->next()) {
                res_type_t t;
                t.id   = rs->getInt("resource_type_id");
                t.name = rs->getString("type_name");
                out.push_back(t);
            }
            delete rs; delete st;
        } catch (sql::SQLException &e) {
            cerr << "[get_resource_types] " << e.what() << "\n";
        }
        return out;
    }
};
