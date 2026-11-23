// REST API entry point: connection, HTTP setup, route registration, listener.
#include "api.h"
#include "operations.h"

int main(int argc, char **argv) {
    int port = 8080;
    if (argc > 1) port = atoi(argv[1]);

    Database db;
    if (!db.connect()) {
        cerr << "[ERROR] Cannot connect to MySQL. Check credentials.\n";
        return 1;
    }
    initialize_api_auth(db);
    cout << "[OK] Database connected.\n";

    httplib::Server svr;
    // Keep a single backend listener per port; duplicate launchers are rejected.
    svr.set_socket_options([](socket_t socket) {
        httplib::default_socket_options(socket);
        int enabled = 1;
        setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
#ifdef SO_REUSEPORT
        int disabled = 0;
        setsockopt(socket, SOL_SOCKET, SO_REUSEPORT, &disabled, sizeof(disabled));
#endif
    });
    // The shared MySQL Connector/C++ connection is not thread-safe.
    // Process HTTP handlers sequentially to prevent concurrent DB access.
    svr.new_task_queue = []() { return new httplib::ThreadPool(1); };

    // ── CORS & Preflight ───────────────────────────────────────
    svr.set_pre_routing_handler([](const httplib::Request &req, httplib::Response &res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        if (req.method == "OPTIONS") {
            res.status = 204;
            return httplib::Server::HandlerResponse::Handled;
        }
        return httplib::Server::HandlerResponse::Unhandled;
    });

    // ── Health Check ───────────────────────────────────────────
    svr.Get("/api/health", [&](const httplib::Request &, httplib::Response &res) {
        res.set_content(json{{"status", "ok"}, {"db_connected", db.is_connected()}}.dump(), "application/json");
    });

    // Route handlers reference these objects throughout the server lifetime.
    Operations operations(db);
    register_operations_routes(svr, db, operations);
    register_public_routes(svr, db);
    register_auth_routes(svr, db);
    register_dispatcher_routes(svr, db);
    register_scheduling_routes(svr, db);
    register_administration_routes(svr, db);

    if (!svr.bind_to_port("0.0.0.0", port)) {
        cerr << "[ERROR] Cannot bind API port " << port
             << ". Stop the existing backend before starting another instance.\n";
        return 1;
    }

    cout << "========================================================\n";
    cout << "  DRERO REST API Server running on http://0.0.0.0:" << port << "\n";
    cout << "  Connected to MySQL: " << db.cfg.dbname << " (" << db.cfg.host << ")\n";
    cout << "========================================================\n";

    return svr.listen_after_bind() ? 0 : 1;
}
