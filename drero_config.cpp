// core/src/drero_config.cpp
#include "drero_config.h"
#include <bits/stdc++.h>
using namespace std;

db_config load_db_config() {
    db_config cfg;
    cfg.host     = "172.24.128.1";
    cfg.port     = 3306;
    cfg.user     = "drero_app";
    cfg.password = "";
    cfg.dbname   = "drero_db";
    char *v;
    v = getenv("DRERO_DB_HOST");     if (v) cfg.host     = string(v);
    v = getenv("DRERO_DB_PORT");     if (v) cfg.port     = atoi(v);
    v = getenv("DRERO_DB_USER");     if (v) cfg.user     = string(v);
    v = getenv("DRERO_DB_PASSWORD"); if (v) cfg.password = string(v);
    v = getenv("DRERO_DB_NAME");     if (v) cfg.dbname   = string(v);
    return cfg;
}
