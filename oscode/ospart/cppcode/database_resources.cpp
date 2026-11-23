#include "database.h"

void Database::fill_resource(resource &r, sql::ResultSet *rs) {
    r.resource_id      = rs->getInt("resource_id");
    r.resource_name    = rs->getString("resource_name");
    r.resource_type_id = rs->getInt("resource_type_id");
    r.zone_id          = rs->getInt("zone_id");
    r.capacity         = rs->getInt("capacity");
    r.capacity_unit    = rs->getString("capacity_unit");
    r.status           = rs->getString("status");
    try { r.resource_type = rs->getString("type_name");  } catch (...) {}
    try { r.zone_name     = rs->getString("zone_name");  } catch (...) {}
    try { r.city_name     = rs->getString("city_name");  } catch (...) {}
    try { r.notes         = rs->getString("notes");      } catch (...) {}
}

vector<resource> Database::get_available_resources() {
    vector<resource> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT r.*, v.resource_type AS type_name, v.zone_name, v.city_name "
            "FROM available_resources_summary v "
            "JOIN resources r ON r.resource_id=v.resource_id "
            "WHERE NOT EXISTS(SELECT 1 FROM resource_reservations rr WHERE rr.resource_id=r.resource_id AND rr.state='Active' AND rr.expires_at>NOW()) "
            "ORDER BY v.resource_name");
        while (rs->next()) { resource r; fill_resource(r, rs); out.push_back(r); }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_available_resources] " << e.what() << "\n";
    }
    return out;
}

vector<resource> Database::get_all_resources(string *error) {
    vector<resource> out;
    if (!is_connected()) {
        if (error) *error = "Database is not connected";
        return out;
    }
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT r.*, t.type_name, z.zone_name, c.city_name "
            "FROM resources r "
            "LEFT JOIN resource_types t ON r.resource_type_id=t.resource_type_id "
            "LEFT JOIN zones z ON r.zone_id=z.zone_id "
            "LEFT JOIN cities c ON z.city_id=c.city_id "
            "ORDER BY r.status, r.resource_name");
        while (rs->next()) { resource r; fill_resource(r, rs); out.push_back(r); }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_all_resources] " << e.what() << "\n";
        if (error) *error = e.what();
    }
    return out;
}

resource Database::get_resource_by_id(int id) {
    resource r; r.resource_id = 0;
    if (!is_connected()) return r;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT r.*, t.type_name, z.zone_name, c.city_name "
            "FROM resources r "
            "LEFT JOIN resource_types t ON r.resource_type_id=t.resource_type_id "
            "LEFT JOIN zones z ON r.zone_id=z.zone_id "
            "LEFT JOIN cities c ON z.city_id=c.city_id "
            "WHERE r.resource_id=?");
        ps->setInt(1, id);
        rs = ps->executeQuery();
        if (rs->next()) fill_resource(r, rs);
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_resource_by_id] " << e.what() << "\n";
    }
    return r;
}

int Database::insert_resource(resource r, int user_id) {
    if (!is_connected()) return -1;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    sql::Statement *st;
    int new_id = -1;
    try {
        ps = conn->prepareStatement(
            "INSERT INTO resources "
            "(resource_name,resource_type_id,zone_id,capacity,capacity_unit,"
            " status,notes,created_by,created_at,updated_at) "
            "VALUES (?,?,?,?,?,?,?,?,NOW(),NOW())");
        ps->setString(1, r.resource_name);
        ps->setInt(2, r.resource_type_id);
        ps->setInt(3, r.zone_id);
        ps->setInt(4, r.capacity);
        ps->setString(5, r.capacity_unit);
        ps->setString(6, r.status.empty() ? RES_AVAILABLE : r.status);
        ps->setString(7, r.notes);
        if (user_id > 0) ps->setInt(8, user_id); else ps->setNull(8, 0);
        ps->executeUpdate();
        delete ps;

        st = conn->createStatement();
        rs = st->executeQuery("SELECT LAST_INSERT_ID() AS id");
        if (rs->next()) new_id = rs->getInt("id");
        delete rs; delete st;

        // audit_new_resource_creation owns the creation audit entry.
    } catch (sql::SQLException &e) {
        cerr << "[insert_resource] " << e.what() << "\n";
    }
    return new_id;
}

int Database::update_resource_status(int id, string new_status, int user_id, string remarks) {
    if (!is_connected()) return 0;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    string old_status = "";
    try {
        ps = conn->prepareStatement(
            "SELECT status FROM resources WHERE resource_id=?");
        ps->setInt(1, id);
        rs = ps->executeQuery();
        if (rs->next()) old_status = rs->getString("status");
        delete rs; delete ps;
        if (old_status.empty()) return 0;

        ps = conn->prepareStatement(
            "UPDATE resources SET status=?,updated_at=NOW() WHERE resource_id=?");
        ps->setString(1, new_status);
        ps->setInt(2, id);
        ps->executeUpdate();
        delete ps;

        ps = conn->prepareStatement(
            "INSERT INTO resource_status_history "
            "(resource_id,old_status,new_status,changed_by,changed_at,remarks) "
            "VALUES (?,?,?,?,NOW(),?)");
        ps->setInt(1, id);
        ps->setString(2, old_status);
        ps->setString(3, new_status);
        if (user_id > 0) ps->setInt(4, user_id); else ps->setNull(4, 0);
        ps->setString(5, remarks);
        ps->executeUpdate();
        delete ps;

        audit(user_id, "UPDATE_RESOURCE_STATUS", "resources", id,
              old_status + "->" + new_status);
        return 1;
    } catch (sql::SQLException &e) {
        cerr << "[update_resource_status] " << e.what() << "\n";
    }
    return 0;
}

int Database::allocate_resource(int request_id, int resource_id, int user_id, string &err) {
    if (!is_connected()) { err = "not connected"; return -1; }
    try {
        unique_ptr<sql::PreparedStatement> needs(conn->prepareStatement("SELECT COUNT(*) FROM request_resource_needs WHERE request_id=?"));needs->setInt(1,request_id);
        unique_ptr<sql::ResultSet> rows(needs->executeQuery());
        if(rows->next()&&rows->getInt(1)>0){err="This request has multiple resource requirements. Use the optional multi-resource allocation tool.";return -1;}
    }catch(const sql::SQLException &){err="Optional operations migration is not installed";return -1;}
    sql::PreparedStatement *call = NULL;
    sql::Statement *st = NULL;
    sql::ResultSet *rs = NULL;
    int alloc_id = 0;
    try {
        st = conn->createStatement();
        st->execute("SET @drero_allocation_id=NULL, @drero_allocation_result=NULL");
        delete st;
        st = NULL;

        call = conn->prepareStatement(
            "CALL allocate_resource_to_request(?,?,?,"
            "@drero_allocation_id,@drero_allocation_result)");
        call->setInt(1, request_id);
        call->setInt(2, resource_id);
        if (user_id > 0) call->setInt(3, user_id); else call->setNull(3, 0);
        call->execute();
        delete call;
        call = NULL;

        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT @drero_allocation_id AS allocation_id, "
            "@drero_allocation_result AS result");
        string result;
        if (rs->next()) {
            alloc_id = rs->getInt("allocation_id");
            result = rs->getString("result");
        }
        delete rs;
        delete st;
        if (alloc_id <= 0) {
            err = result.empty() ? "Allocation failed" : result;
            return -1;
        }
        err.clear();
        return alloc_id;
    } catch (sql::SQLException &e) {
        if (rs) delete rs;
        if (call) delete call;
        if (st) delete st;
        err = "Allocation failed: " + string(e.what());
        cerr << "[allocate_resource] " << e.what() << "\n";
    }
    return -1;
}

int Database::release_allocation(int alloc_id, int user_id, string &err) {
    if (!is_connected()) { err = "not connected"; return 0; }
    sql::PreparedStatement *call = NULL;
    sql::Statement *st = NULL;
    sql::ResultSet *rs = NULL;
    try {
        st = conn->createStatement();
        st->execute("SET @drero_release_result=NULL");
        delete st;
        st = NULL;

        call = conn->prepareStatement(
            "CALL release_resource_allocation(?,?,@drero_release_result)");
        call->setInt(1, alloc_id);
        if (user_id > 0) call->setInt(2, user_id); else call->setNull(2, 0);
        call->execute();
        delete call;
        call = NULL;

        st = conn->createStatement();
        rs = st->executeQuery("SELECT @drero_release_result AS result");
        string result;
        if (rs->next()) result = rs->getString("result");
        delete rs;
        delete st;
        if (result.rfind("OK:", 0) == 0) {
            err.clear();
            return 1;
        }
        err = result.empty() ? "Allocation release failed" : result;
    } catch (sql::SQLException &e) {
        if (rs) delete rs;
        if (call) delete call;
        if (st) delete st;
        err = "Allocation release failed: " + string(e.what());
        cerr << "[release_allocation] " << e.what() << "\n";
    }
    return 0;
}

vector<allocation> Database::get_allocations_for_request(int request_id) {
    vector<allocation> out;
    if (!is_connected()) return out;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT a.*, r.public_ref, res.resource_name "
            "FROM allocations a "
            "JOIN rescue_requests r ON a.request_id=r.request_id "
            "JOIN resources res ON a.resource_id=res.resource_id "
            "WHERE a.request_id=? ORDER BY a.allocated_at DESC");
        ps->setInt(1, request_id);
        rs = ps->executeQuery();
        while (rs->next()) {
            allocation al;
            al.allocation_id = rs->getInt("allocation_id");
            al.request_id    = rs->getInt("request_id");
            al.resource_id   = rs->getInt("resource_id");
            al.public_ref    = rs->getString("public_ref");
            al.resource_name = rs->getString("resource_name");
            al.status        = rs->getString("status");
            al.allocated_at  = rs->getString("allocated_at");
            try { al.released_at = rs->getString("released_at"); } catch (...) {}
            try { al.remarks     = rs->getString("remarks");     } catch (...) {}
            out.push_back(al);
        }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_allocations_for_request] " << e.what() << "\n";
    }
    return out;
}

vector<allocation> Database::get_active_allocations() {
    vector<allocation> out;
    if (!is_connected()) return out;
    sql::Statement *st;
    sql::ResultSet *rs;
    try {
        st = conn->createStatement();
        rs = st->executeQuery(
            "SELECT a.*, r.public_ref, res.resource_name "
            "FROM allocations a "
            "JOIN rescue_requests r ON a.request_id=r.request_id "
            "JOIN resources res ON a.resource_id=res.resource_id "
            "WHERE a.status='Active' ORDER BY a.allocated_at DESC");
        while (rs->next()) {
            allocation al;
            al.allocation_id = rs->getInt("allocation_id");
            al.request_id    = rs->getInt("request_id");
            al.resource_id   = rs->getInt("resource_id");
            al.public_ref    = rs->getString("public_ref");
            al.resource_name = rs->getString("resource_name");
            al.status        = rs->getString("status");
            al.allocated_at  = rs->getString("allocated_at");
            out.push_back(al);
        }
        delete rs; delete st;
    } catch (sql::SQLException &e) {
        cerr << "[get_active_allocations] " << e.what() << "\n";
    }
    return out;
}

vector<allocation> Database::get_all_allocations(int limit) {
    vector<allocation> out;
    if (!is_connected()) return out;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT a.*, v.public_ref, v.resource_name "
            "FROM allocations a "
            "JOIN allocation_details v ON v.allocation_id=a.allocation_id "
            "ORDER BY a.allocated_at DESC LIMIT ?");
        ps->setInt(1, limit);
        rs = ps->executeQuery();
        while (rs->next()) {
            allocation al;
            al.allocation_id = rs->getInt("allocation_id");
            al.request_id    = rs->getInt("request_id");
            al.resource_id   = rs->getInt("resource_id");
            al.public_ref    = rs->getString("public_ref");
            al.resource_name = rs->getString("resource_name");
            al.status        = rs->getString("status");
            al.allocated_at  = rs->getString("allocated_at");
            try { al.released_at = rs->getString("released_at"); } catch (...) {}
            try { al.remarks     = rs->getString("remarks");     } catch (...) {}
            out.push_back(al);
        }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_all_allocations] " << e.what() << "\n";
    }
    return out;
}

vector<status_history> Database::get_resource_history(int resource_id) {
    vector<status_history> out;
    if (!is_connected()) return out;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    try {
        ps = conn->prepareStatement(
            "SELECT old_status,new_status,"
            "  DATE_FORMAT(changed_at,'%Y-%m-%d %H:%i:%s') AS changed_at, remarks "
            "FROM resource_status_history WHERE resource_id=? ORDER BY changed_at");
        ps->setInt(1, resource_id);
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
        cerr << "[get_resource_history] " << e.what() << "\n";
    }
    return out;
}

