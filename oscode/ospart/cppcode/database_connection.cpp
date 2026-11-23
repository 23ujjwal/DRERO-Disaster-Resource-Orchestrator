#include "database.h"

Database::Database() {
    cfg  = load_db_config();
    conn = NULL;
}

Database::~Database() {
    if (conn != NULL) { delete conn; conn = NULL; }
}

int Database::connect() {
    try {
        sql::mysql::MySQL_Driver *drv = sql::mysql::get_mysql_driver_instance();
        string url = "tcp://" + cfg.host + ":" + to_string(cfg.port);
        conn = drv->connect(url, cfg.user, cfg.password);
        conn->setSchema(cfg.dbname);
        conn->setAutoCommit(true);
        if (!verify_application_role() || !initialize_security()) {
            disconnect();
            return 0;
        }
        return 1;
    } catch (sql::SQLException &e) {
        cerr << "[connect error] " << e.what() << "\n";
        conn = NULL;
        return 0;
    }
}

void Database::disconnect() {
    if (conn != NULL) { delete conn; conn = NULL; }
}

int Database::is_connected() { return conn != NULL ? 1 : 0; }

bool Database::verify_application_role() {
    sql::Statement *st = NULL;
    sql::ResultSet *rs = NULL;
    bool active = false;
    bool granted = false;
    bool least_privileged = true;
    try {
        st = conn->createStatement();
        rs = st->executeQuery("SELECT CURRENT_ROLE()");
        if (rs->next()) {
            string roles = rs->getString(1);
            active = roles.find("drero_application_role") != string::npos &&
                     roles.find(",") == string::npos;
        }
        delete rs;
        rs = NULL;

        rs = st->executeQuery("SHOW GRANTS");
        while (rs->next()) {
            string grant = rs->getString(1);
            if (grant.find("drero_application_role") != string::npos)
                granted = true;
            if ((grant.find("ON *.*") != string::npos &&
                 grant.find("GRANT USAGE ON *.*") == string::npos) ||
                grant.find("ON `drero_db`.*") != string::npos ||
                grant.find("ON drero_db.*") != string::npos)
                least_privileged = false;
        }
        delete rs;
        delete st;
    } catch (sql::SQLException &e) {
        if (rs) delete rs;
        if (st) delete st;
        cerr << "[role verification error] " << e.what() << "\n";
        return false;
    }

    if (!active || !granted || !least_privileged) {
        cerr << "[role verification error] drero_application_role is not "
             << "active with least privilege for this connection\n";
        return false;
    }
    return true;
}

bool Database::initialize_security() {
    try {
        unique_ptr<sql::Statement> st(conn->createStatement());
        unique_ptr<sql::ResultSet> check(st->executeQuery("SELECT token_hash FROM staff_sessions LIMIT 0"));
        check.reset();
        unique_ptr<sql::ResultSet> rs(st->executeQuery("SELECT user_id,password_hash FROM users"));
        vector<pair<int,string>> legacy;
        while(rs->next()) {
            string stored=rs->getString("password_hash");
            if(!drero_auth::is_password_hash(stored))legacy.push_back({rs->getInt("user_id"),stored});
        }
        rs.reset();
        for(const auto &entry:legacy) {
            unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
                "UPDATE users SET password_hash=? WHERE user_id=? AND password_hash=?"));
            ps->setString(1,drero_auth::hash_password(entry.second));
            ps->setInt(2,entry.first); ps->setString(3,entry.second); ps->executeUpdate();
        }
        return true;
    } catch(const exception &) {
        cerr << "[security setup] Apply database/migrations/20261115_persistent_staff_sessions.sql as administrator before starting.\n";
        return false;
    }
}

int Database::check_password(string entered, string stored_value) {
    return drero_auth::verify_password(entered, stored_value) ? 1 : 0;
}

bool Database::create_session(const string &token, int user_id, int ttl_seconds) {
    unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "INSERT INTO staff_sessions(token_hash,user_id,expires_at) VALUES (?,?,TIMESTAMPADD(SECOND,?,NOW()))"));
    ps->setString(1,drero_auth::token_hash(token));ps->setInt(2,user_id);ps->setInt(3,ttl_seconds);
    return ps->executeUpdate()==1;
}

int Database::session_user(const string &token) {
    if(token.size()!=64)return 0;
    unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "SELECT s.user_id FROM staff_sessions s JOIN users u ON u.user_id=s.user_id "
        "WHERE s.token_hash=? AND s.expires_at>NOW() AND s.revoked_at IS NULL AND u.is_active=1"));
    ps->setString(1,drero_auth::token_hash(token));
    unique_ptr<sql::ResultSet> rs(ps->executeQuery());
    return rs->next()?rs->getInt(1):0;
}

void Database::revoke_session(const string &token) {
    unique_ptr<sql::PreparedStatement> ps(conn->prepareStatement(
        "UPDATE staff_sessions SET revoked_at=NOW() WHERE token_hash=? AND revoked_at IS NULL"));
    ps->setString(1,drero_auth::token_hash(token));ps->executeUpdate();
}

int Database::authenticate(string username, string password) {
    if (!is_connected()) return -1;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    int uid = -1;
    try {
        ps = conn->prepareStatement(
            "SELECT user_id, password_hash FROM users WHERE username=? AND is_active=1");
        ps->setString(1, username);
        rs = ps->executeQuery();
        if (rs->next()) {
            uid = rs->getInt("user_id");
            string hash = rs->getString("password_hash");
            if (!check_password(password, hash)) uid = -1;
        }
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[authenticate] " << e.what() << "\n";
    }
    return uid;
}

string Database::get_role(int user_id) {
    if (!is_connected()) return "";
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    string role = "";
    try {
        ps = conn->prepareStatement("SELECT role FROM users WHERE user_id=?");
        ps->setInt(1, user_id);
        rs = ps->executeQuery();
        if (rs->next()) role = rs->getString("role");
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_role] " << e.what() << "\n";
    }
    return role;
}

string Database::get_username(int user_id) {
    if (!is_connected()) return "";
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    string name = "";
    try {
        ps = conn->prepareStatement("SELECT username FROM users WHERE user_id=?");
        ps->setInt(1, user_id);
        rs = ps->executeQuery();
        if (rs->next()) name = rs->getString("username");
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_username] " << e.what() << "\n";
    }
    return name;
}

int Database::get_user_zone(int user_id) {
    if (!is_connected()) return 0;
    sql::PreparedStatement *ps;
    sql::ResultSet *rs;
    int zid = 0;
    try {
        ps = conn->prepareStatement("SELECT zone_id FROM users WHERE user_id=?");
        ps->setInt(1, user_id);
        rs = ps->executeQuery();
        if (rs->next() && !rs->isNull("zone_id")) zid = rs->getInt("zone_id");
        delete rs; delete ps;
    } catch (sql::SQLException &e) {
        cerr << "[get_user_zone] " << e.what() << "\n";
    }
    return zid;
}

