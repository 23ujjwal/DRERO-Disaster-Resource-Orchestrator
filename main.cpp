
#include "database.h"
#include "scheduler.h"
#include "resource_manager.h"
#include "rag.h"
#include <bits/stdc++.h>
#include <thread>
#include <mutex>
using namespace std;
void clr()  {
    // Print clean spacing instead of system("clear") so terminal scrollback buffer is not wiped
    cout << "\n\n";
}
void sep()  { cout << string(65, '-') << "\n"; }
void dsep() { cout << string(65, '=') << "\n"; }

void pause_key() {
    cout << "\n  Press Enter to continue...";
    cin.ignore(1000, '\n');
    cin.get();
}

int read_int(const string &prompt) {
    int v = 0;
    cout << prompt;
    while (!(cin >> v)) {
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "  Please enter a number: ";
    }
    cin.ignore();
    return v;
}

string read_str(const string &prompt) {
    string s;
    cout << prompt;
    getline(cin, s);
    return s;
}

// ── session state ─────────────────────────────────────────────
// Holds who is logged in right now
struct Session {
    int    user_id;
    string username;
    string role;        // "USER", "DISPATCHER", "ZONE_ADMIN", "AUDITOR"
    bool   logged_in;

    Session() : user_id(0), logged_in(false) {}

    bool is_staff() const {
        return role == "DISPATCHER" ||
               role == "ZONE_ADMIN" ||
               role == "AUDITOR";
    }
};

// ── global objects ────────────────────────────────────────────
Database db;
Session  session;

// ── banner ────────────────────────────────────────────────────
void show_banner() {
    clr();
    dsep();
    cout << "  DRERO  -  Disaster Resource Orchestrator\n";
    cout << "  [ACADEMIC SIMULATION - Graphic Era University]\n";
    cout << "  [NOT a real emergency service]\n";
    dsep();
}

// ============================================================
//  STEP 0: LOGIN SCREEN
//  Asks: Are you a victim or staff?
// ============================================================
void do_login() {
    while (true) {
        show_banner();
        cout << "\n  Who are you?\n\n";
        cout << "  1. User  (submit a rescue request)\n";
        cout << "  2. Staff            (dispatcher / zone admin / auditor)\n";
        cout << "  0. Exit\n\n";
        int ch = read_int("  Choice: ");

        if (ch == 0) { cout << "\n  Goodbye.\n"; db.disconnect(); exit(0); }

        if (ch == 1) {
            // Public user - no password needed
            session.user_id  = 0;
            session.username = "public";
            session.role     = "USER";
            session.logged_in = true;
            return;
        }

        if (ch == 2) {
            cout << "\n  Staff Login\n";
            sep();
            string uname = read_str("  Username : ");
            string pass  = read_str("  Password : ");

            int uid = db.authenticate(uname, pass);
            if (uid <= 0) {
                cout << "\n  [!] Invalid credentials. Try again.\n";
                sleep(1);
                continue;
            }
            session.user_id   = uid;
            session.username  = db.get_username(uid);
            session.role      = db.get_role(uid);
            session.logged_in = true;
            cout << "\n  Logged in as " << session.username
                 << "  [" << session.role << "]\n";
            sleep(1);
            return;
        }

        cout << "\n  Invalid choice.\n";
        sleep(1);
    }
}

// ============================================================
//  STEP 1 (USER): SUBMIT RESCUE REQUEST
//  Guided form - severity is fetched from DB automatically
// ============================================================

// Small struct to hold display items during form
struct DisasterItem  { int id; string name; string type; };
struct CityItem      { int id; string name; string state; };
struct LocalityItem  { int id; string name; };
struct AssistItem    { int id; string name; };

void user_submit_request() {
    show_banner();
    cout << "\n  STEP 1: Submit Rescue Request\n";
    cout << "  Fill in the details below. Severity is auto-assigned by the system.\n\n";
    sep();

    // -- Pick active disaster --
    vector<disaster> disasters = db.get_active_disasters();
    if (disasters.empty()) {
        cout << "\n  No active disasters right now. Check back later.\n";
        pause_key();
        return;
    }
    cout << "  Active Disasters:\n\n";
    for (int i = 0; i < (int)disasters.size(); i++) {
        cout << "    [" << disasters[i].disaster_id << "]  "
             << disasters[i].disaster_name
             << "  (" << disasters[i].disaster_type << ")\n";
    }
    int dis_id = read_int("\n  Select Disaster ID: ");

    // -- Pick city --
    vector<city_t> cities = db.get_cities();
    cout << "\n  Cities:\n\n";
    for (int i = 0; i < (int)cities.size(); i++) {
        cout << "    [" << cities[i].id << "]  "
             << cities[i].name << ",  " << cities[i].state << "\n";
    }
    int city_id = read_int("\n  Select City ID: ");

    // -- Pick locality --
    vector<locality_t> locs = db.get_localities(city_id);
    if (locs.empty()) {
        cout << "\n  No localities found for that city.\n";
        pause_key();
        return;
    }
    cout << "\n  Localities:\n\n";
    for (int i = 0; i < (int)locs.size(); i++) {
        cout << "    [" << locs[i].id << "]  " << locs[i].name << "\n";
    }
    int loc_id = read_int("\n  Select Locality ID: ");

    // -- Pick assistance type --
    vector<asst_type_t> types = db.get_assistance_types();
    cout << "\n  Assistance Types:\n\n";
    for (int i = 0; i < (int)types.size(); i++) {
        cout << "    [" << types[i].id << "]  " << types[i].name << "\n";
    }
    int asst_id = read_int("\n  Select Assistance Type ID: ");

    // -- Personal details --
    cout << "\n";
    sep();
    cout << "  Personal Details\n";
    sep();

    pcb req;
    req.disaster_id        = dis_id;
    req.locality_id        = loc_id;
    req.assistance_type_id = asst_id;
    req.zone_id            = 0;
    req.request_id         = 0;
    req.victim_name        = read_str("  Your Full Name      : ");
    req.contact_number     = read_str("  Contact Number     : ");
    req.reported_location  = read_str("  Exact Location     : ");
    req.people_affected    = read_int("  People Affected    : ");
    req.details            = read_str("  Additional Details : ");

    // -- Submit --
    cout << "\n  Submitting request...\n";
    int new_id = db.insert_request(req);

    if (new_id <= 0) {
        cout << "\n  [!] Submission failed.\n";
        cout << "      Check that the disaster and locality combination is valid.\n";
        pause_key();
        return;
    }

    // -- Fetch saved record to show auto-assigned severity --
    pcb saved = db.get_request_by_id(new_id);

    // ── STEP 2: Data Processing output ───────────────────────
    clr();
    dsep();
    cout << "  STEP 2: Data Processing Complete\n";
    dsep();
    cout << "\n  Your request has been recorded in the system.\n\n";
    sep();
    printf("  %-20s : %d\n",   "Request ID",    saved.request_id);
    printf("  %-20s : %s\n",   "Public Ref",     saved.public_ref.c_str());
    printf("  %-20s : %s\n",   "Name",           saved.victim_name.c_str());
    printf("  %-20s : %s\n",   "Disaster",       saved.disaster_name.c_str());
    printf("  %-20s : %s\n",   "Zone",           saved.zone_name.c_str());
    printf("  %-20s : %s\n",   "Locality",       saved.locality_name.c_str());
    printf("  %-20s : %d\n",   "People Affected",saved.people_affected);
    printf("  %-20s : %d  (auto-assigned from zone/disaster)\n",
                               "Severity",       saved.severity_snapshot);
    printf("  %-20s : %s\n",   "Status",         saved.status.c_str());
    sep();
    cout << "\n  The system has queued your request.\n";
    cout << "  A dispatcher will review and allocate resources shortly.\n";
    cout << "\n  Save this reference: " << saved.public_ref << "\n";

    pause_key();
}

// ============================================================
//  STEP 2 (STAFF): DISPATCHER DASHBOARD
//  Shows pending requests, resources, zone severity
//  Uses DB views and aggregate queries
// ============================================================
void show_dashboard() {
    show_banner();
    cout << "\n  DISPATCHER DASHBOARD  [" << session.username << "]\n\n";

    // -- Pending requests (via pending_rescue_requests view) --
    vector<pcb> pending = db.get_pending_requests();
    cout << "  Pending Rescue Requests: " << pending.size() << "\n";
    sep();
    printf("  %-6s %-14s %-18s %-10s %-5s %-7s %-14s\n",
           "ID", "Ref", "Name", "Zone", "Sev", "People", "Status");
    sep();
    for (int i = 0; i < (int)pending.size(); i++) {
        pcb &p = pending[i];
        printf("  %-6d %-14s %-18s %-10s %-5d %-7d %-14s\n",
               p.request_id,
               p.public_ref.c_str(),
               p.victim_name.c_str(),
               p.zone_name.c_str(),
               p.severity_snapshot,
               p.people_affected,
               p.status.c_str());
    }
    sep();

    // -- Available resources --
    vector<resource> avail = db.get_available_resources();
    cout << "\n  Available Resources: " << avail.size() << "\n";
    sep();
    printf("  %-5s %-22s %-14s %-14s %-8s\n",
           "ID", "Name", "Type", "Zone", "Capacity");
    sep();
    for (int i = 0; i < (int)avail.size(); i++) {
        resource &r = avail[i];
        printf("  %-5d %-22s %-14s %-14s %d %s\n",
               r.resource_id,
               r.resource_name.c_str(),
               r.resource_type.c_str(),
               r.zone_name.c_str(),
               r.capacity,
               r.capacity_unit.c_str());
    }
    sep();

    // -- Active allocations --
    vector<allocation> active = db.get_active_allocations();
    cout << "\n  Active Allocations: " << active.size() << "\n";
    if (!active.empty()) {
        sep();
        printf("  %-8s %-14s %-22s %-14s\n",
               "AllocID", "Request", "Resource", "Since");
        sep();
        for (int i = 0; i < (int)active.size(); i++) {
            allocation &a = active[i];
            printf("  %-8d %-14s %-22s %-14s\n",
                   a.allocation_id,
                   a.public_ref.c_str(),
                   a.resource_name.c_str(),
                   a.allocated_at.c_str());
        }
        sep();
    }

    // -- Deadlock check --
    rag_build(pending, active);
    cycle_info ci = rag_detect_cycle();
    if (ci.has_cycle) {
        cout << "\n  [!] DEADLOCK DETECTED: " << ci.description << "\n";
    } else {
        cout << "\n  [OK] No deadlock detected in current allocation graph.\n";
    }
}

// ============================================================
//  STEP 3: OS SCHEDULER
//  Runs all 4 algorithms simultaneously using threads
//  Saves each run to scheduling_runs + scheduling_log tables
// ============================================================

// Struct to hold one algorithm's result coming back from a thread
struct AlgoResult {
    string       name;
    sched_result result;
    bool         done;

    AlgoResult() : done(false) {}
};

// Thread worker — runs one algorithm and stores result
// typedef for the function pointer type each thread uses
typedef sched_result (*AlgoFn)(vector<pcb>);

void run_algo_thread(AlgoFn fn, vector<pcb> requests,
                     AlgoResult *out, mutex *print_mtx) {
    out->result = fn(requests);
    out->done   = true;

    // Thread-safe print when done
    lock_guard<mutex> lk(*print_mtx);
    cout << "  [done] " << out->name
         << "  avg_wait=" << fixed << setprecision(2)
         << out->result.metrics.avg_waiting_time
         << "  avg_TAT=" << out->result.metrics.avg_turnaround_time
         << "\n";
}

// RR needs quantum — wrapper to make it match AlgoFn signature
// We store quantum in a global so the wrapper can read it
static int rr_quantum_global = 3;

sched_result rr_wrapper(vector<pcb> requests) {
    return run_rr(requests, rr_quantum_global);
}

// Save a completed run to DB
void save_run_to_db(AlgoResult &ar, int quantum, int user_id) {
    string algo_key = ar.name;
    // algo_key must match ENUM: FCFS / SJF / RR / PRIORITY
    if (algo_key == "Round Robin") algo_key = "RR";
    if (algo_key == "Priority")    algo_key = "PRIORITY";

    int run_id = db.insert_run(algo_key, quantum, user_id);
    if (run_id <= 0) return;

    for (int i = 0; i < (int)ar.result.scheduled.size(); i++) {
        db.insert_log_entry(run_id, ar.result.scheduled[i]);
    }
    db.finish_run(run_id);
    ar.result.run_id = run_id;
}

// Print a full schedule table for one algorithm
void print_algo_table(AlgoResult &ar) {
    cout << "\n  -- " << ar.name << " --\n";
    sep();
    printf("  %-4s %-14s %-5s %-8s %-8s %-8s %-8s %-8s\n",
           "Pos", "Ref", "Sev", "Arr", "Start", "Comp", "Wait", "TAT");
    sep();
    for (int i = 0; i < (int)ar.result.scheduled.size(); i++) {
        pcb &p = ar.result.scheduled[i];
        printf("  %-4d %-14s %-5d %-8.1f %-8.1f %-8.1f %-8.1f %-8.1f\n",
               p.queue_position,
               p.public_ref.c_str(),
               p.severity_snapshot,
               p.arrival_time,
               p.start_time,
               p.completion_time,
               p.waiting_time,
               p.turnaround_time);
    }
    sep();
    sched_metrics &m = ar.result.metrics;
    printf("  Avg Wait=%.2f  Avg TAT=%.2f  Avg Resp=%.2f"
           "  Throughput=%.4f  Starvation=%d\n",
           m.avg_waiting_time, m.avg_turnaround_time,
           m.avg_response_time, m.throughput, m.starvation_count);
}

// Run all 4 simultaneously and return their results
vector<AlgoResult> run_all_schedulers(vector<pcb> &requests, int quantum) {
    rr_quantum_global = quantum;

    // 4 result slots
    AlgoResult fcfs_r, sjf_r, rr_r, pri_r;
    fcfs_r.name = "FCFS";
    sjf_r.name  = "SJF";
    rr_r.name   = "Round Robin";
    pri_r.name  = "Priority";

    mutex print_mtx;

    cout << "\n  Running all 4 algorithms simultaneously...\n\n";

    thread t1(run_algo_thread, run_fcfs,   requests, &fcfs_r, &print_mtx);
    thread t2(run_algo_thread, run_sjf,    requests, &sjf_r,  &print_mtx);
    thread t3(run_algo_thread, rr_wrapper, requests, &rr_r,   &print_mtx);
    thread t4(run_algo_thread, run_priority, requests, &pri_r, &print_mtx);

    t1.join();
    t2.join();
    t3.join();
    t4.join();

    // Save all runs to DB
    save_run_to_db(fcfs_r, 0,       session.user_id);
    save_run_to_db(sjf_r,  0,       session.user_id);
    save_run_to_db(rr_r,   quantum, session.user_id);
    save_run_to_db(pri_r,  0,       session.user_id);

    vector<AlgoResult> results;
    results.push_back(fcfs_r);
    results.push_back(sjf_r);
    results.push_back(rr_r);
    results.push_back(pri_r);
    return results;
}

// ============================================================
//  STEP 4: ALLOCATE RESOURCE
//  Dispatcher picks a request -> system finds best resource
//  Uses transactional allocation (prevents double booking)
// ============================================================

// Auto-picks the best available resource for a request:
// 1. Same zone AND matching assistance type
// 2. Same zone (any type)
// 3. Matching assistance type from any zone
// 4. Fallback: first available
resource pick_best_resource(int zone_id, int asst_type_id) {
    vector<resource> avail = db.get_available_resources();
    resource best;
    best.resource_id = 0;

    // 1. Same zone AND matching assistance type
    for (int i = 0; i < (int)avail.size(); i++) {
        if (avail[i].zone_id == zone_id && avail[i].resource_type_id == asst_type_id) {
            return avail[i];
        }
    }

    // 2. Same zone (any available resource)
    for (int i = 0; i < (int)avail.size(); i++) {
        if (avail[i].zone_id == zone_id) {
            return avail[i];
        }
    }

    // 3. Matching assistance type from any zone
    for (int i = 0; i < (int)avail.size(); i++) {
        if (avail[i].resource_type_id == asst_type_id) {
            return avail[i];
        }
    }

    // Fallback: any available
    if (!avail.empty()) best = avail[0];
    return best;
}

void allocate_flow(vector<pcb> &pending) {
    if (session.role != "DISPATCHER") {
        cout << "\n  [!] Only DISPATCHER can allocate resources.\n";
        return;
    }
    if (pending.empty()) {
        cout << "\n  No pending requests to allocate.\n";
        return;
    }

    cout << "\n  Which request do you want to allocate a resource to?\n";
    cout << "  (Enter Request ID from the list above, 0 to skip)\n\n";
    int req_id = read_int("  Request ID: ");
    if (req_id <= 0) return;

    // Find the request
    pcb target;
    target.request_id = 0;
    for (int i = 0; i < (int)pending.size(); i++) {
        if (pending[i].request_id == req_id) {
            target = pending[i];
            break;
        }
    }
    if (target.request_id == 0) {
        cout << "\n  [!] Request ID " << req_id << " not found in pending list.\n";
        return;
    }

    cout << "\n  Request : " << target.public_ref
         << "  Name: " << target.victim_name
         << "  Zone: " << target.zone_name
         << "  Type: " << target.assistance_type
         << "  Sev: " << target.severity_snapshot << "\n";

    // Show available resources and let dispatcher pick, or auto-pick
    vector<resource> avail = db.get_available_resources();
    if (avail.empty()) {
        cout << "\n  [!] No available resources right now.\n";
        return;
    }

    cout << "\n  Available Resources:\n\n";
    for (int i = 0; i < (int)avail.size(); i++) {
        resource &r = avail[i];
        string match_tag = "";
        if (r.zone_id == target.zone_id && r.resource_type_id == target.assistance_type_id) {
            match_tag = "<-- [EXACT LOCAL MATCH]";
        } else if (r.zone_id == target.zone_id) {
            match_tag = "<-- [LOCAL ZONE]";
        }
        string cap_str = to_string(r.capacity) + " " + r.capacity_unit;
        printf("    [%2d]  %-24s  %-14s  Zone: %-18s  Cap: %-12s %s\n",
               r.resource_id,
               r.resource_name.c_str(),
               r.resource_type.c_str(),
               r.zone_name.c_str(),
               cap_str.c_str(),
               match_tag.c_str());
    }

    cout << "\n  Enter resource ID to allocate (0 = auto-pick best): ";
    int res_id = read_int("");

    if (res_id == 0) {
        resource best = pick_best_resource(target.zone_id,
                                           target.assistance_type_id);
        if (best.resource_id == 0) {
            cout << "\n  [!] No suitable resource found.\n";
            return;
        }
        res_id = best.resource_id;
        cout << "\n  Auto-selected: " << best.resource_name
             << " (" << best.zone_name << ")\n";
    }

    // First promote request status to Ready if still Submitted/New
    if (target.status == "Submitted" || target.status == "New") {
        db.update_request_status(req_id, "Ready", session.user_id,
                                 "Promoted by dispatcher for allocation");
    }

    string err;
    int alloc_id = db.allocate_resource(req_id, res_id,
                                        session.user_id, err);
    if (alloc_id > 0) {
        cout << "\n  [OK] Resource allocated successfully!\n";
        cout << "       Allocation ID : " << alloc_id << "\n";
    } else {
        cout << "\n  [!] Allocation failed: " << err << "\n";
    }
}

// ============================================================
//  STEP 5: FINAL SYSTEM OUTPUT
//  Shows full details: status, scheduling metrics, audit trail
//  Uses DB views for cleaner output
// ============================================================
void show_final_output(vector<AlgoResult> &results, int req_id) {
    clr();
    dsep();
    cout << "  STEP 6: Final System Output\n";
    dsep();

    // -- Request status --
    if (req_id > 0) {
        pcb p = db.get_request_by_id(req_id);
        cout << "\n  Request Status\n";
        sep();
        printf("  %-20s : %s\n", "Reference",    p.public_ref.c_str());
        printf("  %-20s : %s\n", "Name",  p.victim_name.c_str());
        printf("  %-20s : %s\n", "Zone",          p.zone_name.c_str());
        printf("  %-20s : %d\n", "Severity",      p.severity_snapshot);
        printf("  %-20s : %d\n", "People",        p.people_affected);
        printf("  %-20s : %s\n", "Status",        p.status.c_str());
        sep();

        // Status history
        vector<status_history> hist = db.get_request_history(req_id);
        if (!hist.empty()) {
            cout << "\n  Status History\n";
            sep();
            for (int i = 0; i < (int)hist.size(); i++) {
                status_history &h = hist[i];
                printf("  %s  %-14s -> %-14s  %s\n",
                       h.changed_at.c_str(),
                       h.old_status.c_str(),
                       h.new_status.c_str(),
                       h.remarks.c_str());
            }
            sep();
        }

        // Allocations for this request
        vector<allocation> allocs = db.get_allocations_for_request(req_id);
        if (!allocs.empty()) {
            cout << "\n  Resource Assignments\n";
            sep();
            for (int i = 0; i < (int)allocs.size(); i++) {
                allocation &a = allocs[i];
                printf("  %-22s  Status:%-10s  At: %s\n",
                       a.resource_name.c_str(),
                       a.status.c_str(),
                       a.allocated_at.c_str());
            }
            sep();
        }
    }

    // -- Scheduling comparison table --
    if (!results.empty()) {
        cout << "\n  Scheduling Algorithm Comparison\n";
        cout << "  (all 4 ran simultaneously on the same pending queue)\n";
        sep();
        printf("  %-14s %-5s %-10s %-10s %-10s %-10s %-7s\n",
               "Algorithm", "N", "AvgWait", "AvgTAT", "AvgResp",
               "Thrput", "Starve");
        sep();
        for (int i = 0; i < (int)results.size(); i++) {
            sched_metrics &m = results[i].result.metrics;
            printf("  %-14s %-5d %-10.2f %-10.2f %-10.2f %-10.4f %-7d\n",
                   results[i].name.c_str(),
                   m.requests_scheduled,
                   m.avg_waiting_time,
                   m.avg_turnaround_time,
                   m.avg_response_time,
                   m.throughput,
                   m.starvation_count);
        }
        sep();

        // Highlight best algorithm by lowest avg wait
        int best_idx = 0;
        for (int i = 1; i < (int)results.size(); i++) {
            if (results[i].result.metrics.avg_waiting_time <
                results[best_idx].result.metrics.avg_waiting_time)
                best_idx = i;
        }
        cout << "\n  Best for this workload: "
             << results[best_idx].name
             << " (lowest avg wait = "
             << fixed << setprecision(2)
             << results[best_idx].result.metrics.avg_waiting_time
             << " units)\n";
        pause_key();
    }

    // -- Deadlock status --
    vector<pcb>        all_pending = db.get_pending_requests();
    vector<allocation> active_allocs = db.get_active_allocations();
    rag_build(all_pending, active_allocs);
    cycle_info ci = rag_detect_cycle();

    cout << "\n  Deadlock Status (RAG DFS)\n";
    sep();
    if (ci.has_cycle) {
        cout << "  [!] DEADLOCK: " << ci.description << "\n";
    } else {
        cout << "  [OK] No deadlock. " << ci.description << "\n";
    }

    // Banker's demo
    bankers_state  bs = build_demo_bankers();
    bankers_result br = check_bankers(bs);
    cout << "\n  Banker's Algorithm (demo state):\n";
    cout << "  " << br.reason << "\n";
    sep();
    pause_key();

    // -- Recent audit log --
    vector<audit_entry> audit = db.get_audit_log(10);
    if (!audit.empty()) {
        cout << "\n  Recent Audit Trail (last 10 actions)\n";
        sep();
        for (int i = 0; i < (int)audit.size(); i++) {
            audit_entry &e = audit[i];
            printf("  %-10s  %-24s  %s\n",
                   e.actor.c_str(),
                   e.action.c_str(),
                   e.details.c_str());
        }
        sep();
    }
}

// ============================================================
//  USER MAIN FLOW
// ============================================================
void user_flow() {
    user_submit_request();
    // After submit, user sees their result and exits back to login
    show_banner();
    cout << "\n  Your request has been submitted.\n";
    cout << "  Staff will review and allocate resources.\n\n";
    cout << "  1. Submit another request\n";
    cout << "  0. Back to login\n\n";
    int ch = read_int("  Choice: ");
    if (ch == 1) user_flow();
}

// ============================================================
//  STAFF MAIN FLOW (Dispatcher / Zone Admin / Auditor)
// ============================================================
void staff_flow() {
    while (true) {
        // Dashboard
        show_dashboard();

        vector<pcb> pending = db.get_pending_requests();

        cout << "\n";
        dsep();
        cout << "  What do you want to do?\n\n";
        cout << "  1. Run OS Scheduler (all 4 algorithms simultaneously)\n";
        cout << "  2. Allocate resource to a request\n";
        cout << "  3. View request details\n";
        cout << "  4. View resource history\n";
        cout << "  5. View audit log\n";
        cout << "  6. OS Concurrency demo (threads + semaphore)\n";
        cout << "  7. Add / update resource\n";
        cout << "  0. Logout\n";
        dsep();
        int ch = read_int("  Choice: ");

        // ── Option 1: Run all 4 schedulers ─────────────────
        if (ch == 1) {
            if (pending.empty()) {
                cout << "\n  No pending requests to schedule.\n";
                pause_key();
                continue;
            }
            clr();
            dsep();
            cout << "  STEP 3: OS Scheduler\n";
            cout << "  Queue size: " << pending.size() << " requests\n";
            dsep();

            cout << "\n  Time quantum for Round Robin (e.g. 3): ";
            int q = read_int("");

            vector<AlgoResult> results = run_all_schedulers(pending, q);

            // Show each algorithm's full table
            for (int i = 0; i < (int)results.size(); i++) {
                print_algo_table(results[i]);
            }

            // Ask if dispatcher wants to allocate now
            cout << "\n  Do you want to proceed to resource allocation? (1=Yes 0=No): ";
            int proceed = read_int("");
            if (proceed == 1) {
                allocate_flow(pending);
                // Reload pending after allocation
                pending = db.get_pending_requests();
                show_final_output(results, 0);
            } else {
                show_final_output(results, 0);
            }
            pause_key();
        }

        // ── Option 2: Allocate resource directly ───────────
        else if (ch == 2) {
            allocate_flow(pending);
            vector<AlgoResult> empty_results;
            show_final_output(empty_results, 0);
            pause_key();
        }

        // ── Option 3: View request details ─────────────────
        else if (ch == 3) {
            int id = read_int("\n  Request ID: ");
            pcb p = db.get_request_by_id(id);
            if (p.request_id == 0) {
                cout << "\n  Not found.\n";
            } else {
                clr();
                dsep();
                cout << "  Request Details\n";
                dsep();
                printf("  %-20s : %d\n",  "ID",       p.request_id);
                printf("  %-20s : %s\n",  "Ref",      p.public_ref.c_str());
                printf("  %-20s : %s\n",  "Name",  p.victim_name.c_str());
                printf("  %-20s : %s\n",  "Contact",  p.contact_number.c_str());
                printf("  %-20s : %s\n",  "Disaster", p.disaster_name.c_str());
                printf("  %-20s : %s\n",  "City",     p.city_name.c_str());
                printf("  %-20s : %s\n",  "Zone",     p.zone_name.c_str());
                printf("  %-20s : %s\n",  "Locality", p.locality_name.c_str());
                printf("  %-20s : %s\n",  "Location", p.reported_location.c_str());
                printf("  %-20s : %s\n",  "Assistance",p.assistance_type.c_str());
                printf("  %-20s : %d\n",  "People",   p.people_affected);
                printf("  %-20s : %d\n",  "Severity", p.severity_snapshot);
                printf("  %-20s : %s\n",  "Status",   p.status.c_str());
                sep();

                vector<status_history> hist = db.get_request_history(id);
                cout << "\n  Status History:\n";
                sep();
                for (int i = 0; i < (int)hist.size(); i++) {
                    status_history &h = hist[i];
                    printf("  %s  %s -> %s   %s\n",
                           h.changed_at.c_str(),
                           h.old_status.c_str(),
                           h.new_status.c_str(),
                           h.remarks.c_str());
                }
                sep();
            }
            pause_key();
        }

        // ── Option 4: Resource history ──────────────────────
        else if (ch == 4) {
            int id = read_int("\n  Resource ID: ");
            vector<status_history> hist = db.get_resource_history(id);
            cout << "\n  Resource #" << id << " History:\n";
            sep();
            for (int i = 0; i < (int)hist.size(); i++) {
                status_history &h = hist[i];
                printf("  %s  %-14s -> %-14s  %s\n",
                       h.changed_at.c_str(),
                       h.old_status.c_str(),
                       h.new_status.c_str(),
                       h.remarks.c_str());
            }
            sep();
            pause_key();
        }

        // ── Option 5: Audit log ─────────────────────────────
        else if (ch == 5) {
            if (session.role != "DISPATCHER" &&
                session.role != "AUDITOR") {
                cout << "\n  [!] Access denied.\n";
                pause_key();
                continue;
            }
            vector<audit_entry> entries = db.get_audit_log(50);
            clr();
            cout << "\n  Audit Log (last 50)\n";
            sep();
            printf("  %-10s %-24s %-18s  %s\n",
                   "Actor", "Action", "Time", "Details");
            sep();
            for (int i = 0; i < (int)entries.size(); i++) {
                audit_entry &e = entries[i];
                printf("  %-10s %-24s %-18s  %s\n",
                       e.actor.c_str(),
                       e.action.c_str(),
                       e.action_time.c_str(),
                       e.details.c_str());
            }
            sep();
            pause_key();
        }

        // ── Option 6: Concurrency demo ──────────────────────
        else if (ch == 6) {
            if (pending.empty()) {
                cout << "\n  No pending requests for demo.\n";
                pause_key();
                continue;
            }
            resource_manager rm(db);
            rm.load_from_db();
            clr();
            cout << "\n  OS Concurrency Demo\n";
            cout << "  2 worker threads, semaphore limit = 2\n";
            sep();
            resource_manager::work_result wr =
                rm.process_concurrent(pending, (int)pending.size());
            cout << "  Processed " << wr.processed
                 << " requests concurrently:\n\n";
            for (int i = 0; i < (int)wr.log.size(); i++) {
                cout << "    " << wr.log[i] << "\n";
            }
            sep();
            pause_key();
        }

        // ── Option 7: Add/update resource ──────────────────
        else if (ch == 7) {
            if (session.role != "ZONE_ADMIN" &&
                session.role != "DISPATCHER") {
                cout << "\n  [!] Need ZONE_ADMIN or DISPATCHER role.\n";
                pause_key();
                continue;
            }
            cout << "\n  1. Add resource\n";
            cout << "  2. Update resource status\n";
            int sub = read_int("  Choice: ");

            if (sub == 1) {
                resource r;
                vector<res_type_t> rtypes = db.get_resource_types();
                cout << "\n  Resource Types:\n";
                for (int i = 0; i < (int)rtypes.size(); i++) {
                    cout << "    [" << rtypes[i].id << "]  "
                         << rtypes[i].name << "\n";
                }
                r.resource_type_id = read_int("\n  Type ID: ");
                r.resource_name    = read_str("  Name: ");
                r.zone_id          = read_int("  Zone ID: ");
                r.capacity         = read_int("  Capacity: ");
                r.capacity_unit    = read_str("  Unit (people/kits/patients): ");
                r.status           = RES_AVAILABLE;
                r.notes            = read_str("  Notes: ");

                int new_id = db.insert_resource(r, session.user_id);
                if (new_id > 0)
                    cout << "\n  Added resource ID=" << new_id << "\n";
                else
                    cout << "\n  Failed.\n";

            } else if (sub == 2) {
                int rid     = read_int("\n  Resource ID: ");
                string ns   = read_str("  New status (Available/Maintenance/Inactive): ");
                string rmrk = read_str("  Remarks: ");
                if (db.update_resource_status(rid, ns, session.user_id, rmrk))
                    cout << "\n  Updated.\n";
                else
                    cout << "\n  Failed.\n";
            }
            pause_key();
        }

        // ── Option 0: Logout ────────────────────────────────
        else if (ch == 0) {
            session = Session();
            cout << "\n  Logged out.\n";
            sleep(1);
            return;
        }

        else {
            cout << "\n  Invalid choice.\n";
            sleep(1);
        }
    }
}

// ============================================================
//  MAIN
// ============================================================
int main() {
    // Connect to MySQL
    show_banner();
    cout << "\n  Connecting to database...\n";
    if (!db.connect()) {
        cerr << "\n  [!] Cannot connect to MySQL.\n";
        cerr << "      Check that MySQL is running on Windows.\n";
        cerr << "      Set DRERO_DB_HOST if the IP has changed.\n";
        cerr << "      Current config: see core/src/drero_config.cpp\n\n";
        return 1;
    }
    cout << "  Connected.\n";
    sleep(1);

    // Main application loop
    while (true) {
        do_login();

        if (session.role == "USER") {
            user_flow();
        } else if (session.is_staff()) {
            staff_flow();
        }
    }

    db.disconnect();
    return 0;
}
