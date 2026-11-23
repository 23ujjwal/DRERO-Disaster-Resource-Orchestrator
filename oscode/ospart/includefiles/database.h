// core/include/database.h
#pragma once
#include <bits/stdc++.h>
#include "drero_config.h"
#include "models.h"
#include "auth.h"
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

// Shared interface; implementations are grouped in core/src/database_*.cpp.
class Database {
public:
    sql::Connection *conn;
    db_config cfg;

    // Connection
    Database();
    ~Database();
    int connect();
    void disconnect();
    int is_connected();
    bool verify_application_role();
    bool initialize_security();
    int check_password(string entered, string stored_value);
    bool create_session(const string &token, int user_id, int ttl_seconds);
    int session_user(const string &token);
    void revoke_session(const string &token);
    int authenticate(string username, string password);
    string get_role(int user_id);
    string get_username(int user_id);
    int get_user_zone(int user_id);

    // Requests
    string gen_ref();
    void fill_pcb(pcb &p, sql::ResultSet *rs);
    vector<pcb> get_pending_requests(string *error = nullptr);
    vector<pcb> get_all_requests(int limit);
    pcb get_request_by_id(int id, string *error = nullptr);
    pcb get_request_by_ref(string ref);
    int insert_request(pcb &req);
    int update_request_status(int id, string new_status, int user_id, string remarks);
    vector<status_history> get_request_history(int request_id);

    // Resources
    void fill_resource(resource &r, sql::ResultSet *rs);
    vector<resource> get_available_resources();
    vector<resource> get_all_resources(string *error = nullptr);
    resource get_resource_by_id(int id);
    int insert_resource(resource r, int user_id);
    int update_resource_status(int id, string new_status, int user_id, string remarks);
    int allocate_resource(int request_id, int resource_id, int user_id, string &err);
    int release_allocation(int alloc_id, int user_id, string &err);
    vector<allocation> get_allocations_for_request(int request_id);
    vector<allocation> get_active_allocations();
    vector<allocation> get_all_allocations(int limit);
    vector<status_history> get_resource_history(int resource_id);

    // Reporting
    int insert_run(string algorithm, int quantum, int user_id);
    int finish_run(int run_id);
    int insert_log_entry(int run_id, pcb p);
    vector<sched_metrics> get_run_summary();
    vector<sched_log_entry> get_run_log(int run_id, string *error = nullptr);
    void audit(int user_id, string action, string entity_type, int entity_id, string details);
    vector<audit_entry> get_audit_log(int limit);
    int insert_deadlock(string cycle_details);
    int resolve_deadlock(int event_id, string action);

    // Catalog
    vector<disaster> get_active_disasters();
    vector<disaster> get_all_disasters();
    vector<zone_severity> get_zone_risks(int zone_id, string *error = nullptr);
    int update_zone_risk(int disaster_id, int zone_id, int risk_level,
                         string risk_description, int user_id, string &error);
    int get_severity_for_zone(int disaster_id, int zone_id);
    int get_zone_for_locality(int locality_id);
    vector<city_t> get_cities();
    vector<locality_t> get_localities(int city_id);
    vector<asst_type_t> get_assistance_types();
    vector<res_type_t> get_resource_types();
};
