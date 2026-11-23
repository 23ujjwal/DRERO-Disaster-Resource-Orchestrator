#include "database.h"

vector<disaster> Database::get_active_disasters() {
    vector<disaster> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT * FROM disasters WHERE status='Active' ORDER BY disaster_id ASC");
        while (rs->next()) {
            disaster d;
            d.disaster_id   = rs->getInt("disaster_id");
            d.disaster_name = rs->getString("disaster_name");
            d.disaster_type = rs->getString("disaster_type");
            d.status        = rs->getString("status");
            d.start_time    = rs->getString("start_time");
            out.push_back(d);
        }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_active_disasters] " << e.what() << "\n";
    }
    return out;
}

vector<disaster> Database::get_all_disasters() {
    vector<disaster> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT * FROM disasters ORDER BY start_time DESC");
        while (rs->next()) {
            disaster d;
            d.disaster_id   = rs->getInt("disaster_id");
            d.disaster_name = rs->getString("disaster_name");
            d.disaster_type = rs->getString("disaster_type");
            d.status        = rs->getString("status");
            d.start_time    = rs->getString("start_time");
            out.push_back(d);
        }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_all_disasters] " << e.what() << "\n";
    }
    return out;
}

vector<zone_severity> Database::get_zone_risks(int zone_id, string *error) {
    vector<zone_severity> out;
    if (!is_connected()) {
        if (error) *error = "Database is not connected";
        return out;
    }
    sql::PreparedStatement *ps = nullptr;
    sql::ResultSet *rs;
    try {
        const string query =
            "SELECT dz.disaster_id, dz.zone_id, z.zone_name, d.disaster_name, d.status AS disaster_status, "
            "dz.severity_level, dz.risk_level, dz.risk_description, dz.updated_by, "
            "DATE_FORMAT(dz.updated_at,'%Y-%m-%d %H:%i:%s') AS updated_at "
            "FROM disaster_zone_status dz "
            "JOIN zones z ON dz.zone_id=z.zone_id "
            "JOIN disasters d ON dz.disaster_id=d.disaster_id ";
        ps = conn->prepareStatement(zone_id > 0
            ? query + "WHERE dz.zone_id=? ORDER BY d.disaster_name"
            : query + "ORDER BY d.disaster_name,z.zone_name");
        if (zone_id > 0) ps->setInt(1, zone_id);
        rs = ps->executeQuery();
        while (rs->next()) {
            zone_severity zs{};
            zs.disaster_id      = rs->getInt("disaster_id");
            zs.zone_id          = rs->getInt("zone_id");
            zs.zone_name        = rs->getString("zone_name");
            zs.disaster_name    = rs->getString("disaster_name");
            zs.disaster_status  = rs->getString("disaster_status");
            zs.severity_level   = rs->getInt("severity_level");
            zs.risk_level       = rs->getInt("risk_level");
            try { zs.risk_description = rs->getString("risk_description"); } catch (...) {}
            zs.updated_by       = rs->getInt("updated_by");
            zs.updated_at       = rs->getString("updated_at");
            out.push_back(zs);
        }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        if (ps) delete ps;
        cerr << "[get_zone_risks] " << e.what() << "\n";
        if (error) *error = e.what();
    }
    return out;
}

int Database::update_zone_risk(int disaster_id, int zone_id, int risk_level,
                         string risk_description, int user_id, string &error) {
    if (!is_connected()) { error = "Database is not connected"; return 0; }
    sql::PreparedStatement *ps = nullptr;
    sql::ResultSet *rs = nullptr;
    try {
        conn->setAutoCommit(false);

        ps = conn->prepareStatement(
            "SELECT risk_level, risk_description FROM disaster_zone_status "
            "WHERE disaster_id=? AND zone_id=? FOR UPDATE");
        ps->setInt(1, disaster_id);
        ps->setInt(2, zone_id);
        rs = ps->executeQuery();
        if (!rs->next()) {
            delete rs; rs = nullptr;
            delete ps; ps = nullptr;
            conn->rollback(); conn->setAutoCommit(true);
            error = "No risk record exists for this disaster and your assigned zone";
            return 0;
        }
        int old_risk_level = rs->getInt("risk_level");
        string old_description;
        try { old_description = rs->getString("risk_description"); } catch (...) {}
        delete rs; rs = nullptr;
        delete ps; ps = nullptr;

        ps = conn->prepareStatement(
            "UPDATE disaster_zone_status "
            "SET risk_level=?, risk_description=?, updated_by=?, updated_at=NOW() "
            "WHERE disaster_id=? AND zone_id=?");
        ps->setInt(1, risk_level);
        ps->setString(2, risk_description);
        ps->setInt(3, user_id);
        ps->setInt(4, disaster_id);
        ps->setInt(5, zone_id);
        if (ps->executeUpdate() != 1) {
            delete ps; ps = nullptr;
            throw runtime_error("Risk record changed before it could be updated");
        }
        delete ps; ps = nullptr;

        ps = conn->prepareStatement(
            "INSERT INTO audit_log "
            "(user_id,action,entity_type,entity_id,action_time,details) "
            "VALUES (?,'UPDATE_ZONE_RISK','disaster_zone_status',?,NOW(),?)");
        ps->setInt(1, user_id);
        ps->setInt(2, disaster_id);
        ps->setString(3, "zone=" + to_string(zone_id) +
            " risk_level=" + to_string(old_risk_level) + "->" + to_string(risk_level) +
            " risk_description=" + old_description + "->" + risk_description);
        ps->executeUpdate();
        delete ps; ps = nullptr;

        conn->commit();
        conn->setAutoCommit(true);
        return 1;
    } catch (sql::SQLException &e) {
        if (rs) delete rs;
        if (ps) delete ps;
        try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
        error = e.what();
        cerr << "[update_zone_risk] " << e.what() << "\n";
    } catch (exception &e) {
        if (rs) delete rs;
        if (ps) delete ps;
        try { conn->rollback(); conn->setAutoCommit(true); } catch (...) {}
        error = e.what();
        cerr << "[update_zone_risk] " << e.what() << "\n";
    }
    return 0;
}

int Database::get_severity_for_zone(int disaster_id, int zone_id) {
    if (!is_connected()) return 1;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    int sev = 1;
    try {
        ps = conn->prepareStatement(
            "SELECT severity_level FROM disaster_zone_status "
            "WHERE disaster_id=? AND zone_id=?");
        ps->setInt(1, disaster_id);
        ps->setInt(2, zone_id);
        rs = ps->executeQuery();
        if (rs->next()) sev = rs->getInt("severity_level");
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_severity_for_zone] " << e.what() << "\n";
    }
    return sev;
}

int Database::get_zone_for_locality(int locality_id) {
    if (!is_connected()) return 0;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    int zone_id = 0;
    try {
        ps = conn->prepareStatement(
            "SELECT zone_id FROM localities WHERE locality_id=?");
        ps->setInt(1, locality_id);
        rs = ps->executeQuery();
        if (rs->next()) zone_id = rs->getInt("zone_id");
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_zone_for_locality] " << e.what() << "\n";
    }
    return zone_id;
}

vector<city_t> Database::get_cities() {
    vector<city_t> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT city_id,city_name,state_name FROM cities "
            "WHERE is_active=1 ORDER BY city_id ASC");
        while (rs->next()) {
            city_t c;
            c.id    = rs->getInt("city_id");
            c.name  = rs->getString("city_name");
            c.state = rs->getString("state_name");
            out.push_back(c);
        }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_cities] " << e.what() << "\n";
    }
    return out;
}

vector<locality_t> Database::get_localities(int city_id) {
    vector<locality_t> out;
    if (!is_connected()) return out;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT l.locality_id, l.locality_name, l.zone_id "
            "FROM localities l "
            "JOIN zones z ON l.zone_id=z.zone_id "
            "WHERE z.city_id=? AND l.is_active=1 ORDER BY l.locality_name");
        ps->setInt(1, city_id);
        rs = ps->executeQuery();
        while (rs->next()) {
            locality_t l;
            l.id      = rs->getInt("locality_id");
            l.name    = rs->getString("locality_name");
            l.zone_id = rs->getInt("zone_id");
            out.push_back(l);
        }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_localities] " << e.what() << "\n";
    }
    return out;
}

vector<asst_type_t> Database::get_assistance_types() {
    vector<asst_type_t> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT assistance_type_id,type_name FROM assistance_types "
            "WHERE is_active=1 ORDER BY type_name");
        while (rs->next()) {
            asst_type_t t;
            t.id   = rs->getInt("assistance_type_id");
            t.name = rs->getString("type_name");
            out.push_back(t);
        }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_assistance_types] " << e.what() << "\n";
    }
    return out;
}

vector<res_type_t> Database::get_resource_types() {
    vector<res_type_t> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT resource_type_id,type_name FROM resource_types "
            "WHERE is_active=1 ORDER BY type_name");
        while (rs->next()) {
            res_type_t t;
            t.id   = rs->getInt("resource_type_id");
            t.name = rs->getString("type_name");
            out.push_back(t);
        }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_resource_types] " << e.what() << "\n";
    }
    return out;
}

