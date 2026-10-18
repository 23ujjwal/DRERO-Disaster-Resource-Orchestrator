// core/src/models.cpp
#include "models.h"
#include <bits/stdc++.h>
using namespace std;

double pcb_service_time(pcb *p) {
    return (p->people_affected * 0.5) + (p->severity_snapshot * 2.0);
}

double pcb_priority_score(pcb *p, double current_time) {
    double waited = current_time - p->arrival_time;
    if (waited < 0) waited = 0;
    double aging_bonus = floor(waited / AGING_INTERVAL) * AGING_BONUS;
    int people = p->people_affected;
    if (people > 50) people = 50;
    return (p->severity_snapshot * 20.0) + (p->risk_level * 15.0) +
           (people * 0.6) + aging_bonus;
}

bool zone_admin_can_update_risk(const string &role, int assigned_zone_id, int target_zone_id) {
    return role == "ZONE_ADMIN" && assigned_zone_id > 0 &&
           target_zone_id > 0 && assigned_zone_id == target_zone_id;
}

static string normalized_type_name(string value) {
    transform(value.begin(), value.end(), value.begin(),
              [](unsigned char ch) { return (char)tolower(ch); });
    value.erase(value.begin(), find_if(value.begin(), value.end(),
        [](unsigned char ch) { return !isspace(ch); }));
    value.erase(find_if(value.rbegin(), value.rend(),
        [](unsigned char ch) { return !isspace(ch); }).base(), value.end());
    return value;
}

bool resource_prefers_assistance(const string &assistance_type, const string &resource_type) {
    const string assistance = normalized_type_name(assistance_type);
    const string resource = normalized_type_name(resource_type);

    if (assistance == resource) return true;
    if (assistance == "boat rescue") return resource == "boat";
    if (assistance == "relief supplies") return resource == "relief kit";
    if (assistance == "fire brigade") return resource == "fire engine";
    return false;
}

bool resource_supports_assistance(const string &assistance_type, const string &resource_type) {
    const string assistance = normalized_type_name(assistance_type);
    const string resource = normalized_type_name(resource_type);
    if (resource_prefers_assistance(assistance, resource)) return true;
    return assistance == "medical team" && resource == "ambulance";
}

bool resource_has_capacity_for_request(const pcb &request, const resource &candidate) {
    string unit = normalized_type_name(candidate.capacity_unit);
    if (unit == "people" || unit == "person" ||
        unit == "patients" || unit == "patient") {
        return candidate.capacity >= request.people_affected;
    }
    return true;
}

resource select_compatible_resource(const pcb &request, const vector<resource> &inventory,
                                    string &reason) {
    vector<resource> available;
    bool has_compatible_resource = false;
    bool has_compatible_available = false;
    bool has_available_with_capacity = false;

    for (const auto &candidate : inventory) {
        if (!resource_supports_assistance(request.assistance_type, candidate.resource_type)) continue;
        has_compatible_resource = true;
        if (candidate.status != RES_AVAILABLE) continue;
        has_compatible_available = true;
        if (!resource_has_capacity_for_request(request, candidate)) continue;
        has_available_with_capacity = true;
        available.push_back(candidate);
    }

    resource none{};
    if (!has_compatible_resource) {
        reason = "No resource type is compatible with " + request.assistance_type + ".";
        return none;
    }
    if (!has_compatible_available) {
        reason = "Compatible resources exist, but none are available.";
        return none;
    }
    if (!has_available_with_capacity) {
        reason = "Available compatible resources do not have capacity for " +
                 to_string(request.people_affected) + " people.";
        return none;
    }

    for (const auto &candidate : available) {
        if (candidate.zone_id == request.zone_id &&
            resource_prefers_assistance(request.assistance_type, candidate.resource_type)) {
            return candidate;
        }
    }
    for (const auto &candidate : available) {
        if (candidate.zone_id == request.zone_id) return candidate;
    }
    for (const auto &candidate : available) {
        if (resource_prefers_assistance(request.assistance_type, candidate.resource_type)) {
            return candidate;
        }
    }
    return available.front();
}

int valid_transition(string from, string to) {
    if (from == STATE_SUBMITTED)   { return (to==STATE_NEW     || to==STATE_CANCELLED) ? 1:0; }
    if (from == STATE_NEW)         { return (to==STATE_READY   || to==STATE_CANCELLED) ? 1:0; }
    if (from == STATE_READY)       { return (to==STATE_SCHEDULED||to==STATE_DEFERRED  ||to==STATE_CANCELLED)?1:0; }
    if (from == STATE_SCHEDULED)   { return (to==STATE_UNDER_REVIEW||to==STATE_DEFERRED||to==STATE_CANCELLED)?1:0; }
    if (from == STATE_UNDER_REVIEW){ return (to==STATE_ALLOCATED||to==STATE_DEFERRED  ||to==STATE_CANCELLED)?1:0; }
    if (from == STATE_DEFERRED)    { return (to==STATE_READY   || to==STATE_CANCELLED) ? 1:0; }
    return 0;
}
