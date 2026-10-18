// core/include/rag.h
#pragma once
#include <bits/stdc++.h>
#include "models.h"
using namespace std;

typedef struct {
    int            has_cycle;
    vector<string> cycle_labels;
    string         description;
} cycle_info;

typedef struct {
    int                  n_processes;
    int                  n_resources;
    vector<int>          available;
    vector<vector<int>>  allocation;
    vector<vector<int>>  max_need;
    vector<string>       process_labels;
    vector<string>       resource_labels;
} bankers_state;

typedef struct {
    int            is_safe;
    vector<int>    safe_sequence;
    vector<string> safe_labels;
    string         reason;
} bankers_result;

void           rag_clear();
void           rag_add_process(int pid, string label);
void           rag_add_resource(int rid, string label);
void           rag_add_request(int pid, int rid);
void           rag_add_assignment(int rid, int pid);
void           rag_build(vector<pcb> &pending, vector<allocation> &active);
cycle_info     rag_detect_cycle();
string         rag_to_text();
bankers_result check_bankers(bankers_state state);
bankers_state  build_demo_bankers();
