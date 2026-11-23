#pragma once
#include <httplib.h>
#include <nlohmann/json.hpp>
#include "database.h"
#include "scheduler.h"
#include "resource_manager.h"
#include "rag.h"

using json = nlohmann::json;
class Operations;

struct Session {
    int user_id;
    string username;
    string role;
    int zone_id;
    time_t created_at;
};

void initialize_api_auth(Database &db);
int session_ttl_seconds();
string gen_token();
bool get_session_from_req(const httplib::Request &req, Session &session);
bool require_role(const httplib::Request &req, httplib::Response &res,
                  const vector<string> &allowed_roles, Session &out_session);
json pcb_to_json(const pcb &p);
json resource_to_json(const resource &r);
json allocation_to_json(const allocation &a);
json audit_to_json(const audit_entry &e);
json metrics_to_json(const sched_metrics &m);
json history_to_json(const status_history &h);
json graph_to_json();
json timeline_to_json(const sched_result &result);
resource pick_best_resource_db(Database &db, const pcb &request, string &reason, int user_id = 0);
json allocate_scheduled_requests(Database &db, const vector<pcb> &scheduled, int user_id);

void register_auth_routes(httplib::Server &svr, Database &db);
void register_public_routes(httplib::Server &svr, Database &db);
void register_dispatcher_routes(httplib::Server &svr, Database &db);
void register_scheduling_routes(httplib::Server &svr, Database &db);
void register_administration_routes(httplib::Server &svr, Database &db);
void register_operations_routes(httplib::Server &svr, Database &db, Operations &operations);
