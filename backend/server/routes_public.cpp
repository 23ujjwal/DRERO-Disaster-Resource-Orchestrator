#include "api.h"

void register_public_routes(httplib::Server &svr, Database &db) {
    svr.Get("/api/public/disasters", [&](const httplib::Request &, httplib::Response &res) {
        vector<disaster> list = db.get_active_disasters();
        json arr = json::array();
        for (const auto &d : list) {
            arr.push_back({
                {"disaster_id",   d.disaster_id},
                {"disaster_name", d.disaster_name},
                {"disaster_type", d.disaster_type},
                {"status",        d.status},
                {"start_time",    d.start_time}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // 2. Get cities
    svr.Get("/api/public/cities", [&](const httplib::Request &, httplib::Response &res) {
        vector<city_t> list = db.get_cities();
        json arr = json::array();
        for (const auto &c : list) {
            arr.push_back({
                {"city_id",    c.id},
                {"city_name",  c.name},
                {"state_name", c.state}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // 3. Get localities for a city
    svr.Get("/api/public/localities", [&](const httplib::Request &req, httplib::Response &res) {
        int city_id = req.has_param("city_id") ? atoi(req.get_param_value("city_id").c_str()) : 1;
        vector<locality_t> list = db.get_localities(city_id);
        json arr = json::array();
        for (const auto &l : list) {
            arr.push_back({
                {"locality_id",   l.id},
                {"locality_name", l.name},
                {"zone_id",       l.zone_id}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // 4. Get assistance types
    svr.Get("/api/public/assistance-types", [&](const httplib::Request &, httplib::Response &res) {
        vector<asst_type_t> list = db.get_assistance_types();
        json arr = json::array();
        for (const auto &a : list) {
            arr.push_back({
                {"assistance_type_id", a.id},
                {"type_name",          a.name}
            });
        }
        res.set_content(arr.dump(), "application/json");
    });

    // 5. Submit emergency rescue request
    svr.Post("/api/public/requests", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            json body = json::parse(req.body);
            pcb p;
            p.disaster_id        = body.value("disaster_id", 1);
            p.locality_id        = body.value("locality_id", 1);
            p.victim_name        = body.value("victim_name", "Anonymous");
            p.contact_number     = body.value("contact_number", "");
            p.reported_location  = body.value("reported_location", "");
            p.assistance_type_id = body.value("assistance_type_id", 1);
            p.people_affected    = body.value("people_affected", 1);
            p.details            = body.value("details", "");

            if (p.victim_name.empty() || p.reported_location.empty()) {
                res.status = 400;
                res.set_content(json{{"error", "Victim name and location details are required"}}.dump(), "application/json");
                return;
            }

            int new_id = db.insert_request(p);
            if (new_id > 0) {
                pcb created = db.get_request_by_id(new_id);
                res.status = 201;
                res.set_content(json{
                    {"success",           true},
                    {"request_id",        new_id},
                    {"public_ref",        created.public_ref},
                    {"severity_snapshot", created.severity_snapshot},
                    {"zone_name",         created.zone_name},
                    {"city_name",         created.city_name},
                    {"status",            created.status},
                    {"message",           "Emergency request submitted successfully. Staff will dispatch resources."}
                }.dump(), "application/json");
            } else {
                res.status = 500;
                res.set_content(json{{"error", "Failed to insert rescue request. Check locality and disaster status."}}.dump(), "application/json");
            }
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", string("Invalid request payload: ") + e.what()}}.dump(), "application/json");
        }
    });

    // 6. Track request status by public reference or ID
    svr.Get("/api/public/track", [&](const httplib::Request &req, httplib::Response &res) {
        string ref = req.has_param("ref") ? req.get_param_value("ref") : "";
        if (ref.empty()) {
            res.status = 400;
            res.set_content(json{{"error", "Missing 'ref' parameter"}}.dump(), "application/json");
            return;
        }

        pcb p;
        p.request_id = 0;
        // If numeric, try id first
        bool is_num = true;
        for (char c : ref) { if (!isdigit(c)) { is_num = false; break; } }
        if (is_num && !ref.empty()) {
            p = db.get_request_by_id(atoi(ref.c_str()));
        }
        if (p.request_id == 0) {
            p = db.get_request_by_ref(ref);
        }

        if (p.request_id == 0) {
            res.status = 404;
            res.set_content(json{{"error", "Rescue request not found for reference: " + ref}}.dump(), "application/json");
            return;
        }

        json out = pcb_to_json(p);
        vector<status_history> hist = db.get_request_history(p.request_id);
        json hist_arr = json::array();
        for (const auto &h : hist) hist_arr.push_back(history_to_json(h));
        out["status_history"] = hist_arr;

        vector<allocation> allocs = db.get_allocations_for_request(p.request_id);
        json alloc_arr = json::array();
        for (const auto &a : allocs) alloc_arr.push_back(allocation_to_json(a));
        out["allocations"] = alloc_arr;

        res.set_content(out.dump(), "application/json");
    });

    // ============================================================
    //  AUTHENTICATION ENDPOINTS
    // ============================================================

}
