#include "api.h"
#include "operations.h"

void register_dispatcher_routes(httplib::Server &svr, Database &db) {
    // Dispatcher Dashboard Overview
    svr.Get("/api/staff/dispatcher/dashboard", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        vector<pcb> pending = db.get_pending_requests();
        vector<resource> avail = db.get_available_resources();
        vector<allocation> active = db.get_active_allocations();

        // Deadlock detection & Banker's check
        rag_build(pending, active, db.get_all_resources());
        cycle_info ci = rag_detect_cycle();
        bankers_state bs = build_demo_bankers();
        bankers_result br = check_bankers(bs);

        json pending_arr = json::array();
        for (const auto &p : pending) pending_arr.push_back(pcb_to_json(p));

        json avail_arr = json::array();
        for (const auto &r : avail) avail_arr.push_back(resource_to_json(r));

        json active_arr = json::array();
        for (const auto &a : active) active_arr.push_back(allocation_to_json(a));

        json safe_seq_arr = json::array();
        for (const auto &lbl : br.safe_labels) safe_seq_arr.push_back(lbl);

        res.set_content(json{
            {"pending_requests",   pending_arr},
            {"available_resources", avail_arr},
            {"active_allocations", active_arr},
            {"graph", graph_to_json()},
            {"deadlock", {
                {"has_cycle",    ci.has_cycle},
                {"description",  ci.description},
                {"cycle_labels", ci.cycle_labels}
            }},
            {"bankers", {
                {"is_safe",       br.is_safe},
                {"reason",        br.reason},
                {"safe_sequence", safe_seq_arr}
            }}
        }.dump(), "application/json");
    });

    // Dispatcher request list (includes completed and cancelled requests).
    svr.Get("/api/staff/dispatcher/requests", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        int limit = req.has_param("limit") ? atoi(req.get_param_value("limit").c_str()) : 200;
        if (limit < 1 || limit > 500) {
            res.status = 400;
            res.set_content(json{{"error", "limit must be between 1 and 500"}}.dump(), "application/json");
            return;
        }

        json requests = json::array();
        for (const auto &p : db.get_all_requests(limit)) requests.push_back(pcb_to_json(p));
        res.set_content(requests.dump(), "application/json");
    });

    // Dispatcher request state change; the core validates the allowed transition.
    svr.Patch("/api/staff/dispatcher/request/status", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        try {
            json body = json::parse(req.body);
            int request_id = body.value("request_id", 0);
            string new_status = body.value("status", "");
            string remarks = body.value("remarks", "Updated by dispatcher");
            if (request_id <= 0 || new_status.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "request_id and status are required"}}.dump(), "application/json");
                return;
            }

            pcb current = db.get_request_by_id(request_id);
            if (current.request_id == 0) {
                res.status = 404;
                res.set_content(json{{"error", "Request not found"}}.dump(), "application/json");
                return;
            }
            if (!valid_transition(current.status, new_status)) {
                res.status = 400;
                res.set_content(json{{"error", "This request status transition is not allowed"}}.dump(), "application/json");
                return;
            }
            if (!db.update_request_status(request_id, new_status, s.user_id, remarks)) {
                res.status = 400;
                res.set_content(json{{"error", "Request status could not be updated"}}.dump(), "application/json");
                return;
            }

            res.set_content(json{
                {"success", true},
                {"request", pcb_to_json(db.get_request_by_id(request_id))},
                {"message", "Request status updated"}
            }.dump(), "application/json");
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    svr.Post("/api/staff/dispatcher/risk-preview", [&](const httplib::Request &req, httplib::Response &res) {
        Session session;
        if(!require_role(req,res,{"DISPATCHER","ZONE_ADMIN"},session))return;
        try {
            json body=json::parse(req.body);
            int disaster=body.value("disaster_id",0), zone=body.value("zone_id",0), risk=body.value("risk_level",0);
            if(session.role=="ZONE_ADMIN") {
                if(session.zone_id<=0 || zone!=session.zone_id) { res.status=403;res.set_content(json{{"error","Preview is restricted to your assigned zone"}}.dump(),"application/json");return; }
            }
            if(disaster<=0 || zone<=0 || risk<1 || risk>5)throw runtime_error("Choose a disaster, zone and risk from 1 to 5");
            string error;auto pending=db.get_pending_requests(&error);
            if(!error.empty())throw runtime_error("Could not load pending requests");
            if(session.role=="ZONE_ADMIN")pending.erase(remove_if(pending.begin(),pending.end(),[&](const pcb &p){return p.zone_id!=session.zone_id;}),pending.end());
            auto baseline=run_priority(pending);int affected=0;
            for(auto &p:pending)if(p.disaster_id==disaster && p.zone_id==zone){p.risk_level=risk;affected++;}
            auto preview=run_priority(pending);
            auto serialize=[](const sched_result &r){json items=json::array();for(const auto &p:r.scheduled)items.push_back(pcb_to_json(p));return json{{"scheduled",items},{"metrics",metrics_to_json(r.metrics)},{"timeline",timeline_to_json(r)}};};
            res.set_content(json{{"preview_only",true},{"affected_requests",affected},{"baseline",serialize(baseline)},{"preview",serialize(preview)}}.dump(),"application/json");
        }catch(const exception &e){res.status=400;res.set_content(json{{"error",e.what()}}.dump(),"application/json");}
    });

    // Allocate a previously displayed scheduler run using its persisted queue order.
    svr.Post("/api/staff/dispatcher/allocate-scheduled", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        try {
            json body = json::parse(req.body);
            int run_id = body.value("run_id", 0);
            if (run_id <= 0) {
                res.status = 400;
                res.set_content(json{{"error", "Valid run_id is required"}}.dump(), "application/json");
                return;
            }

            string run_error;
            vector<sched_log_entry> run_log = db.get_run_log(run_id, &run_error);
            if (!run_error.empty()) {
                res.status = 500;
                res.set_content(json{{"error", "Could not load the saved scheduling order"}}.dump(), "application/json");
                return;
            }
            if (run_log.empty()) {
                res.status = 404;
                res.set_content(json{{"error", "Scheduling run not found or it contains no requests"}}.dump(), "application/json");
                return;
            }

            vector<pcb> ordered_requests;
            ordered_requests.reserve(run_log.size());
            for (const auto &entry : run_log) {
                string request_error;
                pcb request = db.get_request_by_id(entry.request_id, &request_error);
                if (!request_error.empty()) {
                    res.status = 500;
                    res.set_content(json{{"error", "Could not load a request from the saved scheduling run"}}.dump(), "application/json");
                    return;
                }
                if (request.request_id == 0) {
                    request.public_ref = entry.public_ref;
                    request.status.clear();
                }
                request.request_id = entry.request_id;
                request.queue_position = entry.queue_position;
                ordered_requests.push_back(request);
            }

            json allocation = allocate_scheduled_requests(db, ordered_requests, s.user_id);
            res.set_content(json{
                {"success", true},
                {"run_id", run_id},
                {"algorithm", run_log.front().algorithm},
                {"allocation", allocation}
            }.dump(), "application/json");
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Read-only resource selection explanation, revalidated by allocation under locks.
    svr.Post("/api/staff/dispatcher/allocation-preview", [&](const httplib::Request &req, httplib::Response &res) {
        Session session;if(!require_role(req,res,{"DISPATCHER"},session))return;
        try {
            auto body=json::parse(req.body);int request_id=body.value("request_id",0),resource_id=body.value("resource_id",0);
            pcb request=db.get_request_by_id(request_id);
            if(request.request_id==0)throw runtime_error("Request not found");
            string reason; resource candidate{};
            if(resource_id>0)candidate=db.get_resource_by_id(resource_id);else candidate=pick_best_resource_db(db,request,reason,session.user_id);
            if(candidate.resource_id==0) {res.set_content(json{{"eligible",false},{"explanation",reason.empty()?"Resource not found":reason}}.dump(),"application/json");return;}
            bool compatible=resource_supports_assistance(request.assistance_type,candidate.resource_type);
            bool capacity=resource_has_capacity_for_request(request,candidate);
            bool available=candidate.status==RES_AVAILABLE;
            res.set_content(json{{"eligible",compatible&&capacity&&available},{"resource",resource_to_json(candidate)},{"checks",{{"compatible",compatible},{"capacity",capacity},{"available",available}}},{"explanation",describe_resource_match(request,candidate)}}.dump(),"application/json");
        }catch(const exception &e){res.status=400;res.set_content(json{{"error",e.what()}}.dump(),"application/json");}
    });

    // Allocate Resource
    svr.Post("/api/staff/dispatcher/allocate", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        try {
            json body = json::parse(req.body);
            int req_id = body.value("request_id", 0);
            int res_id = body.value("resource_id", 0);

            if (req_id <= 0) {
                res.status = 400;
                res.set_content(json{{"error", "Valid request_id is required"}}.dump(), "application/json");
                return;
            }

            pcb target = db.get_request_by_id(req_id);
            if (target.request_id == 0) {
                res.status = 404;
                res.set_content(json{{"error", "Request not found"}}.dump(), "application/json");
                return;
            }

            // Auto-pick if 0; the database repeats eligibility checks under row locks.
            if (res_id == 0) {
                string reason;
                resource best = pick_best_resource_db(db, target, reason, s.user_id);
                if (best.resource_id == 0) {
                    res.status = reason.rfind("Could not load", 0) == 0 ? 500 : 400;
                    res.set_content(json{{"error", reason}}.dump(), "application/json");
                    return;
                }
                res_id = best.resource_id;
            }

            string err;
            int alloc_id = db.allocate_resource(req_id, res_id, s.user_id, err);
            if (alloc_id > 0) {
                res.set_content(json{
                    {"success",       true},
                    {"allocation_id", alloc_id},
                    {"request_id",    req_id},
                    {"resource_id",   res_id},
                    {"message",       "Resource successfully allocated"},
                    {"explanation", describe_resource_match(target, db.get_resource_by_id(res_id)) + " Transaction committed successfully."}
                }.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{{"error", err.empty() ? "Allocation failed" : err}}.dump(), "application/json");
            }
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Release Allocation
    svr.Post("/api/staff/dispatcher/release", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        try {
            json body = json::parse(req.body);
            int alloc_id = body.value("allocation_id", 0);
            if (alloc_id <= 0) {
                res.status = 400;
                res.set_content(json{{"error", "Valid allocation_id required"}}.dump(), "application/json");
                return;
            }

            string err;
            if (db.release_allocation(alloc_id, s.user_id, err)) {
                res.set_content(json{{"success", true}, {"message", "Allocation released successfully"}}.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{{"error", err.empty() ? "Release failed" : err}}.dump(), "application/json");
            }
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Deadlock Status & Banker's Algorithm
    svr.Get("/api/staff/dispatcher/deadlock", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER", "ZONE_ADMIN", "AUDITOR"}, s)) return;

        vector<pcb> pending = db.get_pending_requests();
        vector<allocation> active = db.get_active_allocations();
        rag_build(pending, active, db.get_all_resources());
        cycle_info ci = rag_detect_cycle();

        bankers_state bs = build_demo_bankers();
        bankers_result br = check_bankers(bs);

        json safe_seq = json::array();
        for (const auto &l : br.safe_labels) safe_seq.push_back(l);

        json active_arr = json::array();
        for (const auto &a : active) active_arr.push_back(allocation_to_json(a));

        res.set_content(json{
            {"graph", graph_to_json()},
            {"deadlock", {
                {"has_cycle",    ci.has_cycle},
                {"description",  ci.description},
                {"cycle_labels", ci.cycle_labels}
            }},
            {"bankers", {
                {"is_safe",       br.is_safe},
                {"reason",        br.reason},
                {"safe_sequence", safe_seq}
            }},
            {"active_allocations", active_arr}
        }.dump(), "application/json");
    });

    // ============================================================
    //  ZONE ADMINISTRATOR ENDPOINTS
    //  (Requires role == ZONE_ADMIN or DISPATCHER)
    // ============================================================

}
