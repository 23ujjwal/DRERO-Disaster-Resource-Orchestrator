// core/src/scheduling.cpp
// FCFS, SJF, Round Robin, Priority with Aging — all plain free functions
#include "scheduler.h"
#include <bits/stdc++.h>
using namespace std;

vector<pcb> normalize_arrival(vector<pcb> requests) {
    int n = requests.size();
    int i, j;
    for (i = 0; i < n - 1; i++) {
        for (j = 0; j < n - i - 1; j++) {
            if (requests[j].arrival_time_str > requests[j+1].arrival_time_str) {
                pcb tmp = requests[j]; requests[j] = requests[j+1]; requests[j+1] = tmp;
            }
        }
    }
    for (i = 0; i < n; i++) {
        requests[i].arrival_time = (double)i;
        requests[i].service_time = pcb_service_time(&requests[i]);
    }
    return requests;
}

sched_metrics compute_metrics(vector<pcb> &v, string algo) {
    sched_metrics m;
    m.algorithm           = algo;
    m.requests_scheduled  = (int)v.size();
    m.starvation_count    = 0;
    m.avg_waiting_time    = 0;
    m.avg_turnaround_time = 0;
    m.avg_response_time   = 0;
    m.throughput          = 0;
    if (v.empty()) return m;
    double sum_w = 0, sum_t = 0, sum_r = 0, max_c = 0;
    int n = (int)v.size();
    for (int i = 0; i < n; i++) {
        sum_w += v[i].waiting_time;
        sum_t += v[i].turnaround_time;
        sum_r += v[i].response_time;
        if (v[i].completion_time > max_c) max_c = v[i].completion_time;
        if (v[i].waiting_time > 20.0) m.starvation_count++;
    }
    m.avg_waiting_time    = sum_w / n;
    m.avg_turnaround_time = sum_t / n;
    m.avg_response_time   = sum_r / n;
    m.throughput          = (max_c > 0) ? (double)n / max_c : 0;
    return m;
}

static vector<pcb> prepare_arrivals(vector<pcb> requests,bool preserve) {
    if(!preserve)return normalize_arrival(requests);
    stable_sort(requests.begin(),requests.end(),[](const pcb &a,const pcb &b){return a.arrival_time<b.arrival_time;});
    for(auto &p:requests)p.service_time=pcb_service_time(&p);
    return requests;
}

static sched_result run_fcfs_prepared(vector<pcb> requests, bool preserve) {
    sched_result res;
    res.run_id = 0;
    int n = (int)requests.size();
    int i, pos;
    double t = 0;
    requests = prepare_arrivals(requests,preserve);
    pos = 1;
    for (i = 0; i < n; i++) {
        if (t < requests[i].arrival_time) t = requests[i].arrival_time;
        requests[i].start_time      = t;
        requests[i].response_time   = t - requests[i].arrival_time;
        requests[i].completion_time = t + requests[i].service_time;
        requests[i].turnaround_time = requests[i].completion_time - requests[i].arrival_time;
        requests[i].waiting_time    = requests[i].turnaround_time - requests[i].service_time;
        requests[i].queue_position  = pos++;
        t = requests[i].completion_time;
        res.timeline.push_back({requests[i].request_id, requests[i].public_ref, requests[i].start_time, requests[i].completion_time});
        res.scheduled.push_back(requests[i]);
    }
    res.metrics = compute_metrics(res.scheduled, "FCFS");
    return res;
}

static sched_result run_sjf_prepared(vector<pcb> requests, bool preserve) {
    sched_result res;
    res.run_id = 0;
    int n = (int)requests.size();
    int i, best, pos;
    vector<int> done(n, 0);
    double t = 0;
    requests = prepare_arrivals(requests,preserve);
    pos = 1;
    while ((int)res.scheduled.size() < n) {
        best = -1;
        for (i = 0; i < n; i++) {
            if (done[i] || requests[i].arrival_time > t) continue;
            if (best == -1) { best = i; continue; }
            if (requests[i].service_time < requests[best].service_time) best = i;
            else if (requests[i].service_time == requests[best].service_time &&
                     requests[i].request_id   <  requests[best].request_id) best = i;
        }
        if (best == -1) {
            double next_t = 1e18;
            for (i = 0; i < n; i++)
                if (!done[i] && requests[i].arrival_time < next_t) next_t = requests[i].arrival_time;
            t = next_t; continue;
        }
        requests[best].start_time      = t;
        requests[best].response_time   = t - requests[best].arrival_time;
        requests[best].completion_time = t + requests[best].service_time;
        requests[best].turnaround_time = requests[best].completion_time - requests[best].arrival_time;
        requests[best].waiting_time    = requests[best].turnaround_time - requests[best].service_time;
        requests[best].queue_position  = pos++;
        t = requests[best].completion_time;
        done[best] = 1;
        res.timeline.push_back({requests[best].request_id, requests[best].public_ref, requests[best].start_time, requests[best].completion_time});
        res.scheduled.push_back(requests[best]);
    }
    res.metrics = compute_metrics(res.scheduled, "SJF");
    return res;
}

static sched_result run_rr_prepared(vector<pcb> requests, int quantum, bool preserve) {
    sched_result res;
    res.run_id = 0;
    int n = (int)requests.size();
    int i;
    vector<double> remaining(n), first_run(n), finish(n);
    if (quantum <= 0) quantum = DEFAULT_QUANTUM;
    requests = prepare_arrivals(requests,preserve);
    for (i = 0; i < n-1; i++)
        for (int j = 0; j < n-i-1; j++)
            if (requests[j].arrival_time > requests[j+1].arrival_time) {
                pcb tmp = requests[j]; requests[j]=requests[j+1]; requests[j+1]=tmp;
            }
    for (i = 0; i < n; i++) { remaining[i]=requests[i].service_time; first_run[i]=-1; finish[i]=0; }
    deque<int> q; int next_arr=0;
    double t = 0;
    while (next_arr < n && requests[next_arr].arrival_time <= t) q.push_back(next_arr++);
    while (!q.empty() || next_arr<n) {
        if(q.empty()) {
            t=max(t,requests[next_arr].arrival_time);
            while(next_arr<n && requests[next_arr].arrival_time<=t)q.push_back(next_arr++);
        }
        int idx = q.front(); q.pop_front();
        double slice = (remaining[idx] < quantum) ? remaining[idx] : (double)quantum;
        if (first_run[idx] < 0) first_run[idx] = t;
        res.timeline.push_back({requests[idx].request_id, requests[idx].public_ref, t, t+slice});
        t += slice; remaining[idx] -= slice;
        while (next_arr < n && requests[next_arr].arrival_time <= t) q.push_back(next_arr++);
        if (remaining[idx] > 0.001) q.push_back(idx); else finish[idx] = t;
    }
    for (i = 0; i < n; i++) {
        requests[i].start_time      = first_run[i];
        requests[i].completion_time = finish[i];
        requests[i].response_time   = first_run[i] - requests[i].arrival_time;
        requests[i].turnaround_time = finish[i]    - requests[i].arrival_time;
        requests[i].waiting_time    = requests[i].turnaround_time - requests[i].service_time;
        requests[i].queue_position  = i + 1;
    }
    res.scheduled = requests;
    res.metrics   = compute_metrics(res.scheduled, "RR(q=" + to_string(quantum) + ")");
    return res;
}

static sched_result run_priority_prepared(vector<pcb> requests, bool preserve) {
    sched_result res;
    res.run_id = 0;
    int n = (int)requests.size();
    int i, best, pos;
    vector<int> done(n, 0);
    double t = 0;
    requests = prepare_arrivals(requests,preserve);
    pos = 1;
    while ((int)res.scheduled.size() < n) {
        best = -1;
        double best_score = -1e18;
        for (i = 0; i < n; i++) {
            if (done[i] || requests[i].arrival_time > t) continue;
            double score = pcb_priority_score(&requests[i], t);
            if (best == -1 || score > best_score ||
                (score == best_score && requests[i].request_id < requests[best].request_id)) {
                best_score = score; best = i;
            }
        }
        if (best == -1) {
            double next_t = 1e18;
            for (i = 0; i < n; i++)
                if (!done[i] && requests[i].arrival_time < next_t) next_t = requests[i].arrival_time;
            t = next_t; continue;
        }
        requests[best].priority_score  = best_score;
        requests[best].start_time      = t;
        requests[best].response_time   = t - requests[best].arrival_time;
        requests[best].completion_time = t + requests[best].service_time;
        requests[best].turnaround_time = requests[best].completion_time - requests[best].arrival_time;
        requests[best].waiting_time    = requests[best].turnaround_time - requests[best].service_time;
        requests[best].queue_position  = pos++;
        t = requests[best].completion_time;
        done[best] = 1;
        res.timeline.push_back({requests[best].request_id, requests[best].public_ref, requests[best].start_time, requests[best].completion_time});
        res.scheduled.push_back(requests[best]);
    }
    res.metrics = compute_metrics(res.scheduled, "PRIORITY");
    return res;
}

sched_result run_fcfs(vector<pcb> requests) { return run_fcfs_prepared(requests,false); }
sched_result run_sjf(vector<pcb> requests) { return run_sjf_prepared(requests,false); }
sched_result run_rr(vector<pcb> requests,int quantum) { return run_rr_prepared(requests,quantum,false); }
sched_result run_priority(vector<pcb> requests) { return run_priority_prepared(requests,false); }
sched_result run_synthetic_schedule(vector<pcb> requests,const string &algorithm,int quantum) {
    if(algorithm=="FCFS")return run_fcfs_prepared(requests,true);
    if(algorithm=="SJF")return run_sjf_prepared(requests,true);
    if(algorithm=="ROUND_ROBIN")return run_rr_prepared(requests,quantum,true);
    if(algorithm=="PRIORITY")return run_priority_prepared(requests,true);
    throw invalid_argument("Unknown simulation algorithm");
}

sched_comparison compare_all(vector<pcb> requests, int quantum) {
    sched_comparison c;
    c.fcfs     = run_fcfs(requests);
    c.sjf      = run_sjf(requests);
    c.rr       = run_rr(requests, quantum);
    c.priority = run_priority(requests);
    return c;
}
