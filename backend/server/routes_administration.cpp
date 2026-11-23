#include "api.h"

void register_administration_routes(httplib::Server &svr, Database &db) {
    svr.Get("/api/staff/zone/data", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"ZONE_ADMIN", "DISPATCHER"}, s)) return;
        if (s.role == "ZONE_ADMIN" && s.zone_id <= 0) {
            res.status = 403;
            res.set_content(json{{"error", "Zone administrator account is not assigned to a zone"}}.dump(), "application/json");
            return;
        }

        int user_zone = s.zone_id;
        string risk_error;
        vector<zone_severity> risks = db.get_zone_risks(user_zone, &risk_error);
        if (!risk_error.empty()) {
            res.status = 500;
            res.set_content(json{{"error", "Could not load zone risk records"}}.dump(), "application/json");
            return;
        }
        vector<resource> all_res = db.get_all_resources();
        vector<pcb> all_reqs = db.get_all_requests(50);
        vector<res_type_t> rtypes = db.get_resource_types();

        json res_arr = json::array();
        for (const auto &r : all_res) {
            if (user_zone == 0 || r.zone_id == user_zone) {
                res_arr.push_back(resource_to_json(r));
            }
        }

        json req_arr = json::array();
        for (const auto &p : all_reqs) {
            if (user_zone == 0 || p.zone_id == user_zone) {
                req_arr.push_back(pcb_to_json(p));
            }
        }

        json type_arr = json::array();
        for (const auto &t : rtypes) {
            type_arr.push_back({{"id", t.id}, {"name", t.name}});
        }

        json risk_arr = json::array();
        for (const auto &risk : risks) {
            risk_arr.push_back({
                {"disaster_id", risk.disaster_id},
                {"disaster_name", risk.disaster_name},
                {"disaster_status", risk.disaster_status},
                {"zone_id", risk.zone_id},
                {"zone_name", risk.zone_name},
                {"severity_level", risk.severity_level},
                {"risk_level", risk.risk_level},
                {"risk_description", risk.risk_description},
                {"updated_by", risk.updated_by},
                {"updated_at", risk.updated_at}
            });
        }

        res.set_content(json{
            {"zone_id",        user_zone},
            {"resources",      res_arr},
            {"requests",       req_arr},
            {"resource_types", type_arr},
            {"zone_risks",     risk_arr}
        }.dump(), "application/json");
    });

    // Update zonal risk for the authenticated administrator's assigned zone only.
    svr.Patch("/api/staff/zone/risk", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"ZONE_ADMIN"}, s)) return;
        if (s.zone_id <= 0) {
            res.status = 403;
            res.set_content(json{{"error", "Zone administrator account is not assigned to a zone"}}.dump(), "application/json");
            return;
        }

        try {
            json body = json::parse(req.body);
            int disaster_id = body.value("disaster_id", 0);
            int risk_level = body.value("risk_level", 0);
            string risk_description = body.value("risk_description", "");

            if (disaster_id <= 0 || risk_level < 1 || risk_level > 5) {
                res.status = 400;
                res.set_content(json{{"error", "A valid disaster_id and risk_level from 1 to 5 are required"}}.dump(), "application/json");
                return;
            }
            if (risk_description.size() > 255) {
                res.status = 400;
                res.set_content(json{{"error", "Risk description must be 255 characters or fewer"}}.dump(), "application/json");
                return;
            }
            if (!zone_admin_can_update_risk(s.role, s.zone_id, s.zone_id)) {
                res.status = 403;
                res.set_content(json{{"error", "Zone administrators may only manage risk for their assigned zone"}}.dump(), "application/json");
                return;
            }

            string update_error;
            if (!db.update_zone_risk(disaster_id, s.zone_id, risk_level,
                                     risk_description, s.user_id, update_error)) {
                res.status = update_error.find("No risk record exists") == 0 ? 404 : 500;
                res.set_content(json{{"error", update_error}}.dump(), "application/json");
                return;
            }
            res.set_content(json{
                {"success", true},
                {"disaster_id", disaster_id},
                {"zone_id", s.zone_id},
                {"risk_level", risk_level},
                {"updated_by", s.user_id},
                {"message", "Zone risk updated. Pending requests will use this value on the next scheduling run."}
            }.dump(), "application/json");
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Register / Add Resource
    svr.Post("/api/staff/zone/resource", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"ZONE_ADMIN", "DISPATCHER"}, s)) return;

        try {
            json body = json::parse(req.body);
            resource r;
            r.resource_name    = body.value("resource_name", "");
            r.resource_type_id = body.value("resource_type_id", 1);
            r.zone_id          = body.value("zone_id", s.zone_id > 0 ? s.zone_id : 1);
            r.capacity         = body.value("capacity", 10);
            r.capacity_unit    = body.value("capacity_unit", "people");
            r.status           = RES_AVAILABLE;
            r.notes            = body.value("notes", "");

            if (r.resource_name.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "Resource name is required"}}.dump(), "application/json");
                return;
            }
            if (r.capacity <= 0) {
                res.status = 400;
                res.set_content(json{{"error", "Resource capacity must be greater than zero"}}.dump(), "application/json");
                return;
            }
            if (s.role == "ZONE_ADMIN") {
                if (s.zone_id <= 0) {
                    res.status = 403;
                    res.set_content(json{{"error", "Zone administrator account is not assigned to a zone"}}.dump(), "application/json");
                    return;
                }
                r.zone_id = s.zone_id;
            }

            int new_id = db.insert_resource(r, s.user_id);
            if (new_id > 0) {
                res.status = 201;
                res.set_content(json{{"success", true}, {"resource_id", new_id}, {"message", "Resource registered successfully"}}.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to insert resource"}}.dump(), "application/json");
            }
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Update Resource Status (Available / Maintenance / Inactive)
    svr.Patch("/api/staff/zone/resource/status", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"ZONE_ADMIN", "DISPATCHER"}, s)) return;

        try {
            json body = json::parse(req.body);
            int res_id = body.value("resource_id", 0);
            string new_status = body.value("status", "");
            string remarks = body.value("remarks", "Updated by Zone Administrator");

            if (res_id <= 0 || new_status.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "resource_id and status are required"}}.dump(), "application/json");
                return;
            }
            if (new_status != RES_AVAILABLE && new_status != RES_MAINTENANCE && new_status != RES_INACTIVE) {
                res.status = 400;
                res.set_content(json{{"error", "status must be Available, Maintenance, or Inactive"}}.dump(), "application/json");
                return;
            }
            resource existing = db.get_resource_by_id(res_id);
            if (existing.resource_id == 0) {
                res.status = 404;
                res.set_content(json{{"error", "Resource not found"}}.dump(), "application/json");
                return;
            }
            if (s.role == "ZONE_ADMIN" &&
                (s.zone_id <= 0 || existing.zone_id != s.zone_id)) {
                res.status = 403;
                res.set_content(json{{"error", "Zone administrators may only update resources in their assigned zone"}}.dump(), "application/json");
                return;
            }
            if (existing.status == RES_ALLOCATED) {
                res.status = 409;
                res.set_content(json{{"error", "An allocated resource cannot have its status changed"}}.dump(), "application/json");
                return;
            }

            if (db.update_resource_status(res_id, new_status, s.user_id, remarks)) {
                res.set_content(json{{"success", true}, {"message", "Resource status updated"}}.dump(), "application/json");
            } else {
                res.status = 400;
                res.set_content(json{{"error", "Failed to update resource status"}}.dump(), "application/json");
            }
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // ============================================================
    //  AUDITOR ENDPOINTS
    //  (Read-only access for AUDITOR, DISPATCHER, ZONE_ADMIN)
    // ============================================================

    // Audit Log
    svr.Get("/api/staff/auditor/audit-log", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"AUDITOR", "DISPATCHER", "ZONE_ADMIN"}, s)) return;

        int limit = req.has_param("limit") ? atoi(req.get_param_value("limit").c_str()) : 50;
        vector<audit_entry> logs = db.get_audit_log(limit);
        json arr = json::array();
        for (const auto &l : logs) arr.push_back(audit_to_json(l));
        res.set_content(arr.dump(), "application/json");
    });

    // Scheduling Runs Summary
    svr.Get("/api/staff/auditor/scheduling-history", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"AUDITOR", "DISPATCHER", "ZONE_ADMIN"}, s)) return;

        vector<sched_metrics> runs = db.get_run_summary();
        json arr = json::array();
        for (const auto &m : runs) arr.push_back(metrics_to_json(m));
        res.set_content(arr.dump(), "application/json");
    });

    // Scheduling Run Detailed Log
    svr.Get("/api/staff/auditor/run-log", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"AUDITOR", "DISPATCHER", "ZONE_ADMIN"}, s)) return;

        int run_id = req.has_param("run_id") ? atoi(req.get_param_value("run_id").c_str()) : 1;
        vector<sched_log_entry> entries = db.get_run_log(run_id);
        json arr = json::array();
        for (const auto &e : entries) {
            arr.push_back({
                {"log_id",          e.log_id},
                {"run_id",          e.run_id},
                {"algorithm",       e.algorithm},
                {"public_ref",      e.public_ref},
                {"queue_position",  e.queue_position},
                {"priority_score",  e.priority_score},
                {"waiting_time",    e.waiting_time},
                {"turnaround_time", e.turnaround_time},
                {"response_time",   e.response_time}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // All Allocations
    svr.Get("/api/staff/auditor/allocations", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"AUDITOR", "DISPATCHER", "ZONE_ADMIN"}, s)) return;

        int limit = req.has_param("limit") ? atoi(req.get_param_value("limit").c_str()) : 100;
        if (limit < 1 || limit > 500) {
            res.status = 400;
            res.set_content(json{{"error", "limit must be between 1 and 500"}}.dump(), "application/json");
            return;
        }
        vector<allocation> list = db.get_all_allocations(limit);
        json arr = json::array();
        for (const auto &a : list) arr.push_back(allocation_to_json(a));
        res.set_content(arr.dump(), "application/json");
    });

}
