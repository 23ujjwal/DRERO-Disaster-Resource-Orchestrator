#pragma once
// Pure, local simulation data: no Database reference and no live RAG mutation.
inline json deadlock_lab(int count,bool recover) {
    if(count<2||count>5)throw runtime_error("Choose 2 to 5 simulated processes");
    json nodes=json::array(),original=json::array(),edges=json::array();
    for(int i=1;i<=count;i++) {
        nodes.push_back({{"id","p:"+to_string(i)},{"label","Simulated P"+to_string(i)},{"kind","request"}});
        nodes.push_back({{"id","r:"+to_string(i)},{"label","Simulated R"+to_string(i)},{"kind","resource"}});
        original.push_back({{"from","r:"+to_string(i)},{"to","p:"+to_string(i)},{"kind","assignment"}});
        original.push_back({{"from","p:"+to_string(i)},{"to","r:"+to_string(i%count+1)},{"kind","request"}});
    }
    for(const auto &edge:original)if(!recover || (edge["from"]!="p:"+to_string(count) && edge["from"]!="r:"+to_string(count)))edges.push_back(edge);
    auto has_cycle=[](const json &links) {
        map<string,vector<string>> adjacency;map<string,int> state;
        for(const auto &edge:links)adjacency[edge["from"].get<string>()].push_back(edge["to"].get<string>());
        function<bool(string)> visit=[&](string node){state[node]=1;for(const auto &next:adjacency[node]){if(state[next]==1)return true;if(state[next]==0&&visit(next))return true;}state[node]=2;return false;};
        vector<string> keys;for(const auto &entry:adjacency)keys.push_back(entry.first);
        for(const auto &key:keys)if(!state[key]&&visit(key))return true;
        return false;
    };
    json progress=json::array();if(recover)for(int i=count-1;i>=1;i--)progress.push_back("P"+to_string(i));
    return {{"simulation_only",true},{"initial_cycle",has_cycle(original)},{"has_cycle",has_cycle(edges)},
        {"conditions",{{"mutual_exclusion",true},{"hold_and_wait",true},{"no_preemption",!recover},{"circular_wait",!recover}}},
        {"recovery",recover?"Deferred simulated P"+to_string(count)+" and preempted its resource. Remaining processes can progress.":"No recovery applied. Each process holds one resource and waits for the next."},
        {"progress_sequence",progress},{"graph",{{"nodes",nodes},{"edges",edges},{"scope","Isolated teaching simulation. Solid resource-to-process arrows are holds; process-to-resource arrows are explicit waits. No live requests, resources, allocations or audit records are changed."}}}};
}
