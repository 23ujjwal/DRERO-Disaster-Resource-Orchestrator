#include "api.h"

static Database *g_database = nullptr;
int session_ttl_seconds() {
    const char *value=getenv("DRERO_SESSION_TTL_SECONDS");
    int ttl=value?atoi(value):28800;
    return ttl>=1 && ttl<=604800 ? ttl : 28800;
}
string gen_token() { return drero_auth::random_token(); }

bool get_session_from_req(const httplib::Request &req, Session &session) {
    string auth=req.get_header_value("Authorization");
    if(auth.rfind("Bearer ",0)!=0 || !g_database)return false;
    int uid=g_database->session_user(auth.substr(7));
    if(uid<=0)return false;
    session={uid,g_database->get_username(uid),g_database->get_role(uid),g_database->get_user_zone(uid),time(nullptr)};
    return true;
}

bool require_role(const httplib::Request &req, httplib::Response &res,
                  const vector<string> &allowed_roles, Session &out_session) {
    Session session;
    if (!get_session_from_req(req, session)) {
        res.status = 401;
        res.set_content(json{{"error", "Unauthorized: please log in"}}.dump(), "application/json");
        return false;
    }
    if (!allowed_roles.empty()) {
        bool ok = false;
        for (const auto &r : allowed_roles) {
            if (session.role == r) { ok = true; break; }
        }
        if (!ok) {
            res.status = 403;
            res.set_content(json{{"error", "Forbidden: access denied for role " + session.role}}.dump(), "application/json");
            return false;
        }
    }
    out_session = session;
    return true;
}

void initialize_api_auth(Database &db) { g_database = &db; }

void register_auth_routes(httplib::Server &svr, Database &db) {
    // Staff Login
    svr.Post("/api/auth/login", [&](const httplib::Request &req, httplib::Response &res) {
        try {
            json body = json::parse(req.body);
            string username = body.value("username", "");
            string password = body.value("password", "");

            if (username.empty() || password.empty() || username.size()>60 || password.size()>1024) {
                res.status = 400;
                res.set_content(json{{"error", "Username and password are required"}}.dump(), "application/json");
                return;
            }

            int uid = db.authenticate(username, password);
            if (uid <= 0) {
                res.status = 401;
                res.set_content(json{{"error", "Invalid credentials"}}.dump(), "application/json");
                return;
            }

            string role = db.get_role(uid);
            string uname = db.get_username(uid);
            int zone_id = db.get_user_zone(uid);

            string token = gen_token();
            if(!db.create_session(token, uid, session_ttl_seconds()))throw runtime_error("Could not save staff session");

            res.set_content(json{
                {"success", true},
                {"token",   token},
                {"expires_in", session_ttl_seconds()},
                {"user", {
                    {"user_id",  uid},
                    {"username", uname},
                    {"role",     role},
                    {"zone_id",  zone_id}
                }}
            }.dump(), "application/json");
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", e.what()}}.dump(), "application/json");
        }
    });

    // Get current session
    svr.Get("/api/auth/me", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {}, s)) return;
        res.set_content(json{
            {"user_id",  s.user_id},
            {"username", s.username},
            {"role",     s.role},
            {"zone_id",  s.zone_id}
        }.dump(), "application/json");
    });

    // Logout
    svr.Post("/api/auth/logout", [&](const httplib::Request &req, httplib::Response &res) {
        string auth = req.get_header_value("Authorization");
        if (auth.rfind("Bearer ", 0) == 0) {
            string token = auth.substr(7);
            db.revoke_session(token);
        }
        res.set_content(json{{"success", true}}.dump(), "application/json");
    });

    // ============================================================
    //  DISPATCHER ENDPOINTS
    //  (Requires role == DISPATCHER)
    // ============================================================

}
