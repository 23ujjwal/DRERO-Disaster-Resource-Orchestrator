#include "api.h"

void register_scheduling_routes(httplib::Server &svr, Database &db) {
    // Run a selected OS scheduler, optionally allocating in its returned request order.
    svr.Post("/api/staff/dispatcher/schedule", [&](const httplib::Request &req, httplib::Response &res) {
        Session s;
        if (!require_role(req, res, {"DISPATCHER"}, s)) return;

        int quantum = 3;
        string selected_algorithm;
        try {
            if (!req.body.empty()) {
                json body = json::parse(req.body);
                quantum = body.value("quantum", 3);
                selected_algorithm = body.value("algorithm", "");
            }
        } catch (exception &e) {
            res.status = 400;
            res.set_content(json{{"error", string("Invalid scheduling payload: ") + e.what()}}.dump(), "application/json");
            return;
        }
        if (quantum < 1 || quantum > 1000) {
            res.status = 400;
            res.set_content(json{{"error", "Quantum must be between 1 and 1000"}}.dump(), "application/json");
            return;
        }
        if (!selected_algorithm.empty() &&
            selected_algorithm != "FCFS" &&
            selected_algorithm != "SJF" &&
            selected_algorithm != "ROUND_ROBIN" &&
            selected_algorithm != "PRIORITY") {
            res.status = 400;
            res.set_content(json{{"error", "algorithm must be FCFS, SJF, ROUND_ROBIN, or PRIORITY"}}.dump(), "application/json");
            return;
        }
        string pending_error;
        vector<pcb> pending = db.get_pending_requests(&pending_error);
        if (!pending_error.empty()) {
            res.status = 500;
            res.set_content(json{{"error", "Could not load pending requests from the database"}}.dump(), "application/json");
            return;
        }
        if (pending.empty()) {
            res.status = 400;
            res.set_content(json{{"error", "No pending rescue requests to schedule"}}.dump(), "application/json");
            return;
        }

        if (!selected_algorithm.empty()) {
            sched_result result;
            string database_algorithm;
            string display_algorithm;
            if (selected_algorithm == "FCFS") {
                result = run_fcfs(pending);
                database_algorithm = "FCFS";
                display_algorithm = "FCFS";
            } else if (selected_algorithm == "SJF") {
                result = run_sjf(pending);
                database_algorithm = "SJF";
                display_algorithm = "SJF";
            } else if (selected_algorithm == "ROUND_ROBIN") {
                result = run_rr(pending, quantum);
                database_algorithm = "RR";
                display_algorithm = "Round Robin";
            } else {
                result = run_priority(pending);
                database_algorithm = "PRIORITY";
                display_algorithm = "Priority with Aging";
            }

            int run_id = db.insert_run(database_algorithm,
                selected_algorithm == "ROUND_ROBIN" ? quantum : 0, s.user_id);
            if (run_id <= 0) {
                res.status = 500;
                res.set_content(json{{"error", "Scheduling ran but could not be saved to the database"}}.dump(), "application/json");
                return;
            }
            for (const auto &p : result.scheduled) {
                if (!db.insert_log_entry(run_id, p)) {
                    res.status = 500;
                    res.set_content(json{{"error", "Scheduling ran but its results could not be fully saved"}}.dump(), "application/json");
                    return;
                }
            }
            if (!db.finish_run(run_id)) {
                res.status = 500;
                res.set_content(json{{"error", "Scheduling ran but could not be completed and summarized"}}.dump(), "application/json");
                return;
            }

            json scheduled = json::array();
            for (const auto &p : result.scheduled) scheduled.push_back(pcb_to_json(p));

            json response{
                {"success", true},
                {"algorithm", display_algorithm},
                {"quantum", quantum},
                {"run_id", run_id},
                {"metrics", metrics_to_json(result.metrics)},
                {"timeline", timeline_to_json(result)},
                {"scheduled", scheduled}
            };

            res.set_content(response.dump(), "application/json");
            return;
        }

        // Run all 4 algorithms
        sched_result fcfs = run_fcfs(pending);
        sched_result sjf  = run_sjf(pending);
        sched_result rr   = run_rr(pending, quantum);
        sched_result pri  = run_priority(pending);

        // A partial save must never be reported as a successful comparison.
        auto save_result = [&](const string &algorithm,int time_quantum,const sched_result &result) {
            int id=db.insert_run(algorithm,time_quantum,s.user_id);
            if(id<=0)return false;
            for(const auto &p:result.scheduled)if(!db.insert_log_entry(id,p))return false;
            return db.finish_run(id)!=0;
        };
        bool runs_finished=save_result("FCFS",0,fcfs);
        runs_finished=save_result("SJF",0,sjf)&&runs_finished;
        runs_finished=save_result("RR",quantum,rr)&&runs_finished;
        runs_finished=save_result("PRIORITY",0,pri)&&runs_finished;

        if (!runs_finished) {
            res.status = 500;
            res.set_content(json{{"error", "One or more scheduling summaries could not be refreshed"}}.dump(), "application/json");
            return;
        }

        // Determine best algorithm
        vector<pair<string, double>> waits = {
            {"FCFS", fcfs.metrics.avg_waiting_time},
            {"SJF", sjf.metrics.avg_waiting_time},
            {"Round Robin", rr.metrics.avg_waiting_time},
            {"Priority with Aging", pri.metrics.avg_waiting_time}
        };
        string best_algo = waits[0].first;
        double min_wait = waits[0].second;
        for (const auto &w : waits) {
            if (w.second < min_wait) {
                min_wait = w.second;
                best_algo = w.first;
            }
        }

        auto serialize_sched = [](const sched_result &sr, const string &name) {
            json arr = json::array();
            for (const auto &p : sr.scheduled) arr.push_back(pcb_to_json(p));
            return json{
                {"algorithm", name},
                {"metrics",   metrics_to_json(sr.metrics)},
                {"timeline", timeline_to_json(sr)},
                {"scheduled", arr}
            };
        };

        json comparison = json::array({
            metrics_to_json(fcfs.metrics),
            metrics_to_json(sjf.metrics),
            metrics_to_json(rr.metrics),
            metrics_to_json(pri.metrics)
        });

        res.set_content(json{
            {"success",     true},
            {"quantum",     quantum},
            {"comparison",  comparison},
            {"best_algorithm", {
                {"name",     best_algo},
                {"avg_wait", min_wait}
            }},
            {"algorithms", {
                {"fcfs",        serialize_sched(fcfs, "FCFS")},
                {"sjf",         serialize_sched(sjf,  "SJF")},
                {"round_robin", serialize_sched(rr,   "Round Robin")},
                {"priority",    serialize_sched(pri,  "Priority with Aging")}
            }}
        }.dump(), "application/json");
    });

    // Read-only simulation: loads live pending requests, runs in memory, never persists.
}
