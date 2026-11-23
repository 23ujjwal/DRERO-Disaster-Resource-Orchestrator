#include "rag.h"

static rag_snapshot graph;
static string process_key(int id) { return "p:"+to_string(id); }
static string resource_key(int id) { return "r:"+to_string(id); }
static void add_node(string id,string label,string kind) {
    for(const auto &node:graph.nodes)if(node.id==id)return;
    graph.nodes.push_back({id,label,kind});
}
void rag_clear() { graph={}; }
void rag_add_process(int pid,string label) { add_node(process_key(pid),label,"request"); }
void rag_add_resource(int rid,string label) { add_node(resource_key(rid),label,"resource"); }
void rag_add_request(int pid,int rid) { graph.edges.push_back({process_key(pid),resource_key(rid),"request"}); }
void rag_add_assignment(int rid,int pid) { graph.edges.push_back({resource_key(rid),process_key(pid),"assignment"}); }
rag_snapshot rag_get_snapshot() { return graph; }

void rag_build(vector<pcb> &pending,vector<allocation> &active,const vector<resource> &inventory) {
    rag_clear();
    for(const auto &p:pending)rag_add_process(p.request_id,p.public_ref);
    for(const auto &r:inventory)rag_add_resource(r.resource_id,r.resource_name);
    for(const auto &a:active) {
        rag_add_process(a.request_id,a.public_ref.empty()?"Request #"+to_string(a.request_id):a.public_ref);
        rag_add_resource(a.resource_id,a.resource_name);
        rag_add_assignment(a.resource_id,a.request_id);
    }
    // Compatibility is determined by type names and real capacity, never by
    // comparing unrelated assistance-type and resource IDs. Candidate links
    // are not confirmed waits and are excluded from deadlock detection.
    for(const auto &p:pending)for(const auto &r:inventory)
        if((r.status==RES_AVAILABLE || r.status==RES_ALLOCATED) &&
           resource_supports_assistance(p.assistance_type,r.resource_type) && resource_has_capacity_for_request(p,r))
            graph.edges.push_back({process_key(p.request_id),resource_key(r.resource_id),"candidate"});
}

cycle_info rag_detect_cycle() {
    cycle_info result{};
    unordered_map<string,int> state;
    vector<string> stack;
    unordered_map<string,string> labels;
    for(const auto &node:graph.nodes)labels[node.id]=node.label;
    function<bool(string)> visit=[&](string node) {
        state[node]=1;stack.push_back(node);
        for(const auto &edge:graph.edges) {
            if(edge.from!=node || edge.kind=="candidate" || !labels.count(edge.to))continue;
            if(state[edge.to]==1) {
                auto begin=find(stack.begin(),stack.end(),edge.to);
                for(auto it=begin;it!=stack.end();++it)result.cycle_labels.push_back(labels[*it]);
                result.cycle_labels.push_back(labels[edge.to]);return true;
            }
            if(state[edge.to]==0 && visit(edge.to))return true;
        }
        stack.pop_back();state[node]=2;return false;
    };
    for(const auto &node:graph.nodes)if(state[node.id]==0 && visit(node.id)){result.has_cycle=1;break;}
    result.description=result.has_cycle?"Cycle detected in recorded request/assignment edges.":"No cycle in recorded request/assignment edges. Compatibility links are not confirmed waits.";
    return result;
}

string rag_to_text() {
    string out="Resource Allocation Graph\nNodes:\n";
    for(const auto &node:graph.nodes)out+="  ["+node.kind+"] "+node.label+"\n";
    out+="Edges:\n";
    for(const auto &edge:graph.edges)out+="  "+edge.from+" -> "+edge.to+" ("+edge.kind+")\n";
    return out;
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
