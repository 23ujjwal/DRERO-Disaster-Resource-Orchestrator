// core/include/scheduler.h
#pragma once
#include <bits/stdc++.h>
#include "models.h"
using namespace std;

struct execution_slice {
    int request_id;
    string public_ref;
    double start_time;
    double end_time;
};

typedef struct {
    vector<execution_slice> timeline;
    vector<pcb>   scheduled;
    sched_metrics metrics;
    int           run_id;
} sched_result;

typedef struct {
    sched_result fcfs;
    sched_result sjf;
    sched_result rr;
    sched_result priority;
} sched_comparison;

vector<pcb>      normalize_arrival(vector<pcb> requests);
sched_metrics    compute_metrics(vector<pcb> &scheduled, string algorithm);
sched_result     run_fcfs(vector<pcb> requests);
sched_result     run_sjf(vector<pcb> requests);
sched_result     run_rr(vector<pcb> requests, int quantum);
sched_result     run_priority(vector<pcb> requests);
sched_comparison compare_all(vector<pcb> requests, int quantum);

// Synthetic lab only: honor supplied numeric arrivals without changing normal scheduling.
sched_result run_synthetic_schedule(vector<pcb> requests, const string &algorithm, int quantum);
