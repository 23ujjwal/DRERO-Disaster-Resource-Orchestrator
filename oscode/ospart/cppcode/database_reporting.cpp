#include "database.h"

int Database::insert_run(string algorithm, int quantum, int user_id) {
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

int Database::finish_run(int run_id) {
    if (!is_connected()) return 0;
    sql::PreparedStatement *update = NULL;
    sql::PreparedStatement *refresh = NULL;
    try {
        conn->setAutoCommit(false);
        update = conn->prepareStatement(
            "UPDATE scheduling_runs SET status='Completed',finished_at=NOW() "
            "WHERE run_id=?");
        update->setInt(1, run_id);
        update->executeUpdate();
        delete update;
        update = NULL;

        refresh = conn->prepareStatement(
            "CALL refresh_scheduling_run_summary(?)");
        refresh->setInt(1, run_id);
        refresh->execute();
        delete refresh;
        refresh = NULL;

        conn->commit();
        conn->setAutoCommit(true);
        return 1;
    } catch (sql::SQLException &e) {
        if (update) delete update;
        if (refresh) delete refresh;
        try {
            conn->rollback();
        } catch (sql::SQLException &rollback_error) {
            cerr << "[finish_run rollback] " << rollback_error.what() << "\n";
        }
        try {
            conn->setAutoCommit(true);
        } catch (sql::SQLException &autocommit_error) {
            cerr << "[finish_run autocommit reset] "
                 << autocommit_error.what() << "\n";
        }
        cerr << "[finish_run] " << e.what() << "\n";
    }
    return 0;
}

int Database::insert_log_entry(int run_id, pcb p) {
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

vector<sched_metrics> Database::get_run_summary() {
    vector<sched_metrics> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT cache.algorithm, cache.requests_scheduled AS n, "
            "  cache.avg_waiting_time AS avgW, "
            "  cache.avg_turnaround_time AS avgT, "
            "  cache.avg_response_time AS avgR, "
            "  cache.throughput AS throughput, "
            "  cache.starvation_count AS starvation "
            "FROM scheduling_performance_summary_cache cache "
            "JOIN scheduling_runs sr ON sr.run_id=cache.run_id "
            "WHERE sr.status='Completed' "
            "ORDER BY cache.started_at DESC LIMIT 20");
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

vector<sched_log_entry> Database::get_run_log(int run_id, string *error) {
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

void Database::audit(int user_id, string action, string entity_type, int entity_id, string details) {
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

vector<audit_entry> Database::get_audit_log(int limit) {
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

int Database::insert_deadlock(string cycle_details) {
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

int Database::resolve_deadlock(int event_id, string action) {
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

