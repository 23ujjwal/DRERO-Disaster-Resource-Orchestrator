// core/include/drero_config.h
#pragma once
#include <bits/stdc++.h>
using namespace std;

#define DEFAULT_QUANTUM  3
#define AGING_INTERVAL   5.0
#define AGING_BONUS      10.0

typedef struct {
    string host;
    int    port;
    string user;
    string password;
    string dbname;
} db_config;

db_config load_db_config();
