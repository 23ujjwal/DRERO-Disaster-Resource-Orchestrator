// core/include/models.h
#pragma once
#include <bits/stdc++.h>
#include "drero_config.h"
using namespace std;

#define STATE_SUBMITTED    "Submitted"
#define STATE_NEW          "New"
#define STATE_READY        "Ready"
#define STATE_SCHEDULED    "Scheduled"
#define STATE_UNDER_REVIEW "Under Review"
#define STATE_ALLOCATED    "Allocated"
#define STATE_DEFERRED     "Deferred"
#define STATE_COMPLETED    "Completed"
#define STATE_CANCELLED    "Cancelled"

#define RES_AVAILABLE   "Available"
#define RES_ALLOCATED   "Allocated"
#define RES_MAINTENANCE "Maintenance"
#define RES_INACTIVE    "Inactive"

int valid_transition(string from, string to);

typedef struct {
    int    request_id;
    string public_ref;
    int    disaster_id;
    int    locality_id;
    int    zone_id;
    string victim_name;
    string contact_number;
    string reported_location;
    int    assistance_type_id;
    string assistance_type;
    int    people_affected;
    string details;
    int    severity_snapshot;
    int    risk_level;
    string arrival_time_str;
    string status;
    double arrival_time;
    double service_time;
    double priority_score;
    double waiting_time;
    double response_time;
    double turnaround_time;
    double start_time;
    double completion_time;
    int    queue_position;
    string city_name;
    string zone_name;
    string locality_name;
    string disaster_name;
} pcb;

double pcb_service_time(pcb *p);
double pcb_priority_score(pcb *p, double current_time);
bool zone_admin_can_update_risk(const string &role, int assigned_zone_id, int target_zone_id);

typedef struct {
    int    resource_id;
    string resource_name;
    int    resource_type_id;
    string resource_type;
    int    zone_id;
    string zone_name;
    string city_name;
    int    capacity;
    string capacity_unit;
    string status;
    string notes;
} resource;

bool resource_prefers_assistance(const string &assistance_type, const string &resource_type);
bool resource_supports_assistance(const string &assistance_type, const string &resource_type);
bool resource_has_capacity_for_request(const pcb &request, const resource &candidate);
resource select_compatible_resource(const pcb &request, const vector<resource> &inventory,
                                    string &reason);

typedef struct {
    int    allocation_id;
    int    request_id;
    int    resource_id;
    string public_ref;
    string resource_name;
    string status;
    string allocated_at;
    string released_at;
    string remarks;
} allocation;

typedef struct {
    int    disaster_id;
    string disaster_name;
    string disaster_type;
    string status;
    string start_time;
} disaster;

typedef struct {
    int    disaster_id;
    int    zone_id;
    string zone_name;
    string disaster_name;
    string disaster_status;
    int    severity_level;
    int    risk_level;
    string risk_description;
    int    updated_by;
    string updated_at;
} zone_severity;

typedef struct {
    string algorithm;
    int    requests_scheduled;
    double avg_waiting_time;
    double avg_turnaround_time;
    double avg_response_time;
    double throughput;
    int    starvation_count;
} sched_metrics;
