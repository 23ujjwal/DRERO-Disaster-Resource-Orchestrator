#include "database.h"

string Database::gen_ref() {
    srand((unsigned)time(NULL) ^ (unsigned)(size_t)this);
    int num = 10000 + rand() % 90000;
    time_t t = time(NULL);
    tm *tm_s = localtime(&t);
    char buf[8];
    strftime(buf, sizeof(buf), "%y%m%d", tm_s);
    return "REQ-" + to_string(num) + "-" + string(buf);
}

void Database::fill_pcb(pcb &p, sql::ResultSet *rs) {
    p.request_id         = rs->getInt("request_id");
    p.public_ref         = rs->getString("public_ref");
    p.disaster_id        = rs->getInt("disaster_id");
    p.locality_id        = rs->getInt("locality_id");
    p.zone_id            = rs->getInt("zone_id");
    p.victim_name        = rs->getString("victim_name");
    p.contact_number     = rs->getString("contact_number");
    p.reported_location  = rs->getString("reported_location");
    p.assistance_type_id = rs->getInt("assistance_type_id");
    p.people_affected    = rs->getInt("people_affected");
    p.severity_snapshot  = rs->getInt("severity_snapshot");
    p.risk_level         = 1;
    try { p.risk_level   = rs->getInt("risk_level"); } catch (...) {}
    p.status             = rs->getString("status");
    try { p.arrival_time_str = rs->getString("arrival_time_str"); } catch (...) {}
    try { p.assistance_type  = rs->getString("assistance_type");  } catch (...) {}
    try { p.disaster_name    = rs->getString("disaster_name");    } catch (...) {}
    try { p.city_name        = rs->getString("city_name");        } catch (...) {}
    try { p.zone_name        = rs->getString("zone_name");        } catch (...) {}
    try { p.locality_name    = rs->getString("locality_name");    } catch (...) {}
    try { p.details          = rs->getString("details");          } catch (...) {}
    p.arrival_time = 0; p.service_time = 0; p.priority_score = 0;
    p.waiting_time = 0; p.response_time = 0; p.turnaround_time = 0;
    p.start_time   = 0; p.completion_time = 0; p.queue_position = 0;
}

vector<pcb> Database::get_pending_requests(string *error) {
    vector<pcb> out;
    if (!is_connected()) {
        if (error) *error = "Database is not connected";
        return out;
    }
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT r.*, "
            "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
            "  COALESCE(dzs.risk_level,1) AS risk_level, "
            "  v.assistance_type, v.disaster_name, v.city_name, v.zone_name, v.locality_name "
            "FROM pending_rescue_requests v "
            "JOIN rescue_requests r ON r.request_id=v.request_id "
            "LEFT JOIN disaster_zone_status dzs "
            "  ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
            "WHERE NOT EXISTS ("
            "  SELECT 1 FROM allocations al "
            "  WHERE al.request_id=r.request_id AND al.status='Active'"
            ") "
            "UNION ALL "
            "SELECT r.*, "
            "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
            "  COALESCE(dzs.risk_level,1) AS risk_level, "
            "  a.type_name AS assistance_type, "
            "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
            "FROM rescue_requests r "
            "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
            "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
            "LEFT JOIN zones z ON r.zone_id=z.zone_id "
            "LEFT JOIN localities l ON r.locality_id=l.locality_id "
            "LEFT JOIN cities c ON z.city_id=c.city_id "
            "LEFT JOIN disaster_zone_status dzs "
            "  ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
            "WHERE r.status='Under Review' "
            "AND NOT EXISTS ("
            "  SELECT 1 FROM allocations al "
            "  WHERE al.request_id=r.request_id AND al.status='Active'"
            ") "
            "ORDER BY severity_snapshot DESC, arrival_time ASC");
        while (rs->next()) { pcb p; fill_pcb(p, rs); out.push_back(p); }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_pending_requests] " << e.what() << "\n";
        if (error) *error = e.what();
    }
    return out;
}

vector<pcb> Database::get_all_requests(int limit) {
    vector<pcb> out;
    if (!is_connected()) return out;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT r.*, "
            "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
            "  COALESCE(dzs.risk_level,1) AS risk_level, "
            "  a.type_name AS assistance_type, "
            "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
            "FROM rescue_requests r "
            "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
            "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
            "LEFT JOIN zones z ON r.zone_id=z.zone_id "
            "LEFT JOIN localities l ON r.locality_id=l.locality_id "
            "LEFT JOIN cities c ON z.city_id=c.city_id "
            "LEFT JOIN disaster_zone_status dzs ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
            "ORDER BY r.arrival_time DESC LIMIT ?");
        ps->setInt(1, limit);
        rs = ps->executeQuery();
        while (rs->next()) { pcb p; fill_pcb(p, rs); out.push_back(p); }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_all_requests] " << e.what() << "\n";
    }
    return out;
}

pcb Database::get_request_by_id(int id, string *error) {
    pcb p; p.request_id = 0;
    if (!is_connected()) {
        if (error) *error = "Database is not connected";
        return p;
    }
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT r.*, "
            "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
            "  COALESCE(dzs.risk_level,1) AS risk_level, "
            "  a.type_name AS assistance_type, "
            "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
            "FROM rescue_requests r "
            "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
            "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
            "LEFT JOIN zones z ON r.zone_id=z.zone_id "
            "LEFT JOIN localities l ON r.locality_id=l.locality_id "
            "LEFT JOIN cities c ON z.city_id=c.city_id "
            "LEFT JOIN disaster_zone_status dzs ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
            "WHERE r.request_id=?");
        ps->setInt(1, id);
        rs = ps->executeQuery();
        if (rs->next()) fill_pcb(p, rs);
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_request_by_id] " << e.what() << "\n";
        if (error) *error = e.what();
    }
    return p;
}

pcb Database::get_request_by_ref(string ref) {
    pcb p; p.request_id = 0;
    if (!is_connected()) return p;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT r.*, "
            "  DATE_FORMAT(r.arrival_time,'%Y-%m-%d %H:%i:%s') AS arrival_time_str, "
            "  COALESCE(dzs.risk_level,1) AS risk_level, "
            "  a.type_name AS assistance_type, "
            "  d.disaster_name, c.city_name, z.zone_name, l.locality_name "
            "FROM rescue_requests r "
            "LEFT JOIN assistance_types a ON r.assistance_type_id=a.assistance_type_id "
            "LEFT JOIN disasters d ON r.disaster_id=d.disaster_id "
            "LEFT JOIN zones z ON r.zone_id=z.zone_id "
            "LEFT JOIN localities l ON r.locality_id=l.locality_id "
            "LEFT JOIN cities c ON z.city_id=c.city_id "
            "LEFT JOIN disaster_zone_status dzs ON dzs.disaster_id=r.disaster_id AND dzs.zone_id=r.zone_id "
            "WHERE r.public_ref=?");
        ps->setString(1, ref);
        rs = ps->executeQuery();
        if (rs->next()) fill_pcb(p, rs);
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_request_by_ref] " << e.what() << "\n";
    }
    return p;
}

int Database::insert_request(pcb &req) {
    if (!is_connected()) return -1;
    sql::PreparedStatement *call = NULL;
    sql::ResultSet *rs = NULL;
    sql::Statement *st = NULL;
    int new_id = -1;
    try {
        st = conn->createStatement();
        st->execute("SET @drero_request_id=NULL, @drero_public_ref=NULL, "
                    "@drero_severity=NULL, @drero_zone_id=NULL");
        delete st;
        st = NULL;

        call = conn->prepareStatement(
            "CALL submit_rescue_request(?,?,?,?,?,?,?, ?,"
            "@drero_request_id,@drero_public_ref,@drero_severity,@drero_zone_id)");
        call->setInt(1, req.disaster_id);
        call->setInt(2, req.locality_id);
        call->setString(3, req.victim_name);
        call->setString(4, req.contact_number);
        call->setString(5, req.reported_location);
        call->setInt(6, req.assistance_type_id);
        call->setInt(7, req.people_affected);
        call->setString(8, req.details);
        call->execute();
        delete call;
        call = NULL;

        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT @drero_request_id AS request_id, "
            "@drero_public_ref AS public_ref, "
            "@drero_severity AS severity, @drero_zone_id AS zone_id");
        if (rs->next()) {
            new_id = rs->getInt("request_id");
            req.public_ref = rs->getString("public_ref");
            req.severity_snapshot = rs->getInt("severity");
            req.zone_id = rs->getInt("zone_id");
            req.status = "Submitted";
        }
        delete rs;
        delete st;
    } catch (sql::SQLException &e) {
        if (rs) delete rs;
        if (call) delete call;
        if (st) delete st;
        cerr << "[insert_request] " << e.what() << "\n";
        new_id = -1;
    }
    req.request_id = new_id;
    return new_id;
}

int Database::update_request_status(int id, string new_status, int user_id, string remarks) {
    if (!is_connected()) return 0;
    sql::PreparedStatement *call = NULL;
    sql::Statement *st = NULL;
    sql::ResultSet *rs = NULL;
    try {
        st = conn->createStatement();
        st->execute("SET @drero_status_result=NULL");
        delete st;
        st = NULL;

        call = conn->prepareStatement(
            "CALL change_request_status(?,?,?,?,@drero_status_result)");
        call->setInt(1, id);
        call->setString(2, new_status);
        if (user_id > 0) call->setInt(3, user_id); else call->setNull(3, 0);
        call->setString(4, remarks);
        call->execute();
        delete call;
        call = NULL;

        st = conn->createStatement();
        rs = st->executeQuery("SELECT @drero_status_result AS result");
        string result;
        if (rs->next()) result = rs->getString("result");
        delete rs;
        delete st;
        return result.rfind("OK:", 0) == 0 ? 1 : 0;
    } catch (sql::SQLException &e) {
        if (rs) delete rs;
        if (call) delete call;
        if (st) delete st;
        cerr << "[update_request_status] " << e.what() << "\n";
    }
    return 0;
}

vector<status_history> Database::get_request_history(int request_id) {
    vector<status_history> out;
    if (!is_connected()) return out;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT old_status,new_status,"
            "  DATE_FORMAT(changed_at,'%Y-%m-%d %H:%i:%s') AS changed_at, remarks "
            "FROM request_status_history WHERE request_id=? ORDER BY changed_at");
        ps->setInt(1, request_id);
        rs = ps->executeQuery();
        while (rs->next()) {
            status_history h;
            h.old_status = rs->getString("old_status");
            h.new_status = rs->getString("new_status");
            h.changed_at = rs->getString("changed_at");
            try { h.remarks = rs->getString("remarks"); } catch (...) {}
            out.push_back(h);
        }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_request_history] " << e.what() << "\n";
    }
    return out;
}

