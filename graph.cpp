// core/src/graph.cpp
// RAG and Banker's Algorithm — plain free functions, no class, no ::
#include "rag.h"
#include <bits/stdc++.h>
using namespace std;

#define MAX_NODES 400
#define TYPE_PROCESS  0
#define TYPE_RESOURCE 1

static int    node_id[MAX_NODES];
static int    node_type[MAX_NODES];
static string node_label[MAX_NODES];
static int    n_nodes = 0;

static int edge_from[MAX_NODES];
static int edge_to[MAX_NODES];
static int n_edges = 0;

void rag_clear() {
    n_nodes = 0; n_edges = 0;
}

void rag_add_process(int pid, string label) {
    node_id[n_nodes]    = pid;
    node_type[n_nodes]  = TYPE_PROCESS;
    node_label[n_nodes] = label;
    n_nodes++;
}

void rag_add_resource(int rid, string label) {
    node_id[n_nodes]    = rid + 10000;
    node_type[n_nodes]  = TYPE_RESOURCE;
    node_label[n_nodes] = label;
    n_nodes++;
}

void rag_add_request(int pid, int rid) {
    edge_from[n_edges] = pid;
    edge_to[n_edges]   = rid + 10000;
    n_edges++;
}

void rag_add_assignment(int rid, int pid) {
    edge_from[n_edges] = rid + 10000;
    edge_to[n_edges]   = pid;
    n_edges++;
}

void rag_build(vector<pcb> &pending, vector<allocation> &active) {
    rag_clear();
    for (int i = 0; i < (int)pending.size(); i++)
        rag_add_process(pending[i].request_id, pending[i].public_ref);
    set<int> seen_res;
    for (int i = 0; i < (int)active.size(); i++) {
        int rid = active[i].resource_id;
        if (!seen_res.count(rid)) {
            rag_add_resource(rid, active[i].resource_name);
            seen_res.insert(rid);
        }
        rag_add_assignment(rid, active[i].request_id);
    }
    for (int i = 0; i < (int)pending.size(); i++) {
        for (int j = 0; j < (int)active.size(); j++) {
            if (pending[i].request_id != active[j].request_id &&
                pending[i].assistance_type_id == active[j].resource_id) {
                rag_add_request(pending[i].request_id, active[j].resource_id);
            }
        }
    }
}

static int  visited[MAX_NODES];
static int  in_stack[MAX_NODES];
static int  cycle_found;
static int  cycle_start;
static int  cycle_path[MAX_NODES];
static int  cycle_path_len;

static int find_node_idx(int id) {
    for (int i = 0; i < n_nodes; i++) if (node_id[i] == id) return i;
    return -1;
}

static void dfs_visit(int node_idx) {
    if (cycle_found) return;
    visited[node_idx]  = 1;
    in_stack[node_idx] = 1;
    cycle_path[cycle_path_len++] = node_idx;

    int cur_id = node_id[node_idx];
    for (int e = 0; e < n_edges; e++) {
        if (edge_from[e] != cur_id) continue;
        int next_idx = find_node_idx(edge_to[e]);
        if (next_idx < 0) continue;
        if (!visited[next_idx]) {
            dfs_visit(next_idx);
            if (cycle_found) return;
        } else if (in_stack[next_idx]) {
            cycle_found = 1;
            cycle_start = next_idx;
            return;
        }
    }
    in_stack[node_idx] = 0;
    cycle_path_len--;
}

cycle_info rag_detect_cycle() {
    cycle_info ci;
    ci.has_cycle = 0;
    ci.description = "";

    for (int i = 0; i < n_nodes; i++) { visited[i]=0; in_stack[i]=0; }
    cycle_found = 0; cycle_path_len = 0;

    for (int i = 0; i < n_nodes && !cycle_found; i++) {
        if (!visited[i]) dfs_visit(i);
    }

    if (cycle_found) {
        ci.has_cycle = 1;
        string desc = "Cycle detected: ";
        int in_cycle = 0;
        for (int i = 0; i < cycle_path_len; i++) {
            if (cycle_path[i] == cycle_start) in_cycle = 1;
            if (in_cycle) {
                ci.cycle_labels.push_back(node_label[cycle_path[i]]);
                desc += node_label[cycle_path[i]];
                if (i + 1 < cycle_path_len) desc += " -> ";
            }
        }
        ci.cycle_labels.push_back(node_label[cycle_start]);
        desc += " -> " + node_label[cycle_start];
        ci.description = desc;
    } else {
        ci.description = "No cycle. " + to_string(n_nodes) + " nodes, "
                       + to_string(n_edges) + " edges examined.";
    }
    return ci;
}

string rag_to_text() {
    string s = "Resource Allocation Graph\n";
    s += "Nodes (" + to_string(n_nodes) + "):\n";
    for (int i = 0; i < n_nodes; i++) {
        s += "  [" + string(node_type[i]==TYPE_PROCESS?"P":"R") + "] "
           + node_label[i] + "\n";
    }
    s += "Edges (" + to_string(n_edges) + "):\n";
    for (int e = 0; e < n_edges; e++) {
        int fi = find_node_idx(edge_from[e]);
        int ti = find_node_idx(edge_to[e]);
        string fl = (fi >= 0) ? node_label[fi] : to_string(edge_from[e]);
        string tl = (ti >= 0) ? node_label[ti] : to_string(edge_to[e]);
        s += "  " + fl + " --> " + tl + "\n";
    }
    return s;
}

bankers_result check_bankers(bankers_state state) {
    bankers_result result;
    result.is_safe = 0;
    int n = state.n_processes;
    int m = state.n_resources;
    int i, j, found, count;

    vector<vector<int>> need(n, vector<int>(m));
    for (i = 0; i < n; i++)
        for (j = 0; j < m; j++)
            need[i][j] = state.max_need[i][j] - state.allocation[i][j];

    vector<int> work = state.available;
    vector<int> finish(n, 0);
    count = 0;

    while (count < n) {
        found = 0;
        for (i = 0; i < n; i++) {
            if (finish[i]) continue;
            int can = 1;
            for (j = 0; j < m; j++) if (need[i][j] > work[j]) { can=0; break; }
            if (can) {
                for (j = 0; j < m; j++) work[j] += state.allocation[i][j];
                finish[i] = 1;
                result.safe_sequence.push_back(i);
                if (i < (int)state.process_labels.size())
                    result.safe_labels.push_back(state.process_labels[i]);
                found = 1; count++;
            }
        }
        if (!found) break;
    }

    if (count == n) {
        result.is_safe = 1;
        result.reason  = "SAFE STATE - Safe sequence: ";
        for (i = 0; i < (int)result.safe_labels.size(); i++) {
            result.reason += result.safe_labels[i];
            if (i+1 < (int)result.safe_labels.size()) result.reason += " -> ";
        }
    } else {
        result.reason = "UNSAFE STATE - No safe sequence exists. Deadlock possible!";
    }
    return result;
}

bankers_state build_demo_bankers() {
    bankers_state s;
    s.n_processes = 3; s.n_resources = 3;
    s.resource_labels = {"Boat","Truck","MedTeam"};
    s.process_labels  = {"Req#1","Req#5","Req#6"};
    s.available       = {1, 1, 0};
    s.allocation      = {{1,0,1},{0,1,0},{0,0,1}};
    s.max_need        = {{2,1,1},{0,2,0},{1,1,2}};
    return s;
}
