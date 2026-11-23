#include "api.h"
#include "operations.h"

// ── JSON Helpers ───────────────────────────────────────────────
json pcb_to_json(const pcb &p) {
    auto priority=explain_priority(p,p.start_time);
    return json{
        {"priority_breakdown", {{"severity",priority.severity},{"risk",priority.risk},{"people",priority.people},{"aging",priority.aging},{"waited",priority.waited},{"total",priority.total}}},
        {"request_id",         p.request_id},
        {"public_ref",         p.public_ref},
        {"disaster_id",        p.disaster_id},
        {"disaster_name",      p.disaster_name},
        {"city_name",          p.city_name},
        {"zone_id",            p.zone_id},
        {"zone_name",          p.zone_name},
        {"locality_id",        p.locality_id},
        {"locality_name",      p.locality_name},
        {"victim_name",        p.victim_name},
        {"contact_number",     p.contact_number},
        {"reported_location",  p.reported_location},
        {"assistance_type_id", p.assistance_type_id},
        {"assistance_type",    p.assistance_type},
        {"people_affected",    p.people_affected},
        {"details",            p.details},
        {"severity_snapshot",  p.severity_snapshot},
        {"risk_level",         p.risk_level},
        {"arrival_time_str",   p.arrival_time_str},
        {"status",             p.status},
        {"arrival_time",       p.arrival_time},
        {"service_time",       p.service_time},
        {"priority_score",     p.priority_score},
        {"waiting_time",       p.waiting_time},
        {"response_time",      p.response_time},
        {"turnaround_time",    p.turnaround_time},
        {"start_time",         p.start_time},
        {"completion_time",    p.completion_time},
        {"queue_position",     p.queue_position}
    };
}

json resource_to_json(const resource &r) {
    return json{
        {"resource_id",      r.resource_id},
        {"resource_name",    r.resource_name},
        {"resource_type_id", r.resource_type_id},
        {"resource_type",    r.resource_type},
        {"zone_id",          r.zone_id},
        {"zone_name",        r.zone_name},
        {"city_name",        r.city_name},
        {"capacity",         r.capacity},
        {"capacity_unit",    r.capacity_unit},
        {"status",           r.status},
        {"notes",            r.notes}
    };
}

json allocation_to_json(const allocation &a) {
    return json{
        {"allocation_id", a.allocation_id},
        {"request_id",    a.request_id},
        {"resource_id",   a.resource_id},
        {"public_ref",    a.public_ref},
        {"resource_name", a.resource_name},
        {"status",        a.status},
        {"allocated_at",  a.allocated_at},
        {"released_at",   a.released_at},
        {"remarks",       a.remarks}
    };
}

json audit_to_json(const audit_entry &e) {
    return json{
        {"audit_id",    e.audit_id},
        {"actor",       e.actor},
        {"action",      e.action},
        {"entity_type", e.entity_type},
        {"entity_id",   e.entity_id},
        {"action_time", e.action_time},
        {"details",     e.details}
    };
}

json metrics_to_json(const sched_metrics &m) {
    return json{
        {"algorithm",           m.algorithm},
        {"requests_scheduled",  m.requests_scheduled},
        {"avg_waiting_time",    m.avg_waiting_time},
        {"avg_turnaround_time", m.avg_turnaround_time},
        {"avg_response_time",   m.avg_response_time},
        {"throughput",          m.throughput},
        {"starvation_count",    m.starvation_count}
    };
}

json history_to_json(const status_history &h) {
    return json{
        {"old_status", h.old_status},
        {"new_status", h.new_status},
        {"changed_at", h.changed_at},
        {"remarks",    h.remarks}
    };
}

json graph_to_json() {
    auto graph=rag_get_snapshot();json nodes=json::array(),edges=json::array();
    for(const auto &node:graph.nodes)nodes.push_back({{"id",node.id},{"label",node.label},{"kind",node.kind}});
    for(const auto &edge:graph.edges)edges.push_back({{"from",edge.from},{"to",edge.to},{"kind",edge.kind}});
    return {{"nodes",nodes},{"edges",edges},{"scope","Assignment arrows are recorded allocations. Dashed candidate arrows show compatible inventory, not confirmed resource waits. The application does not record multi-resource hold-and-wait dependencies."}};
}

json timeline_to_json(const sched_result &result) {
    json timeline=json::array();
    for(const auto &slice:result.timeline)timeline.push_back({{"request_id",slice.request_id},{"public_ref",slice.public_ref},{"start_time",slice.start_time},{"end_time",slice.end_time}});
    return timeline;
}

// ── Smart Resource Matcher ─────────────────────────────────────
resource pick_best_resource_db(Database &db, const pcb &request, string &reason, int user_id) {
    string inventory_error;
    vector<resource> inventory = db.get_all_resources(&inventory_error);
    if (!inventory_error.empty()) {
        reason = "Could not load resource inventory from the database.";
        return resource{};
    }
    Operations operations(db);
    auto held=operations.query("SELECT JSON_OBJECT('id',resource_id) FROM resource_reservations WHERE state='Active' AND expires_at>NOW() AND NOT(request_id=? AND user_id=?)",{request.request_id,user_id});
    set<int> blocked;for(const auto &row:held)blocked.insert(row["id"].get<int>());
    inventory.erase(remove_if(inventory.begin(),inventory.end(),[&](const resource &r){return blocked.count(r.resource_id);}),inventory.end());
    return select_compatible_resource(request, inventory, reason);
}

json allocate_scheduled_requests(Database &db, const vector<pcb> &scheduled, int user_id) {
    json results = json::array();
    int allocated_count = 0;

    for (const auto &request : scheduled) {
        json outcome{
            {"queue_position", request.queue_position},
            {"request_id", request.request_id},
            {"public_ref", request.public_ref},
            {"status", "Unallocated"}
        };
        string reason;
        if (request.status.empty()) {
            outcome["reason"] = "Request no longer exists.";
        } else {
            resource selected = pick_best_resource_db(db, request, reason, user_id);
            if (selected.resource_id == 0) {
                outcome["reason"] = reason;
            } else {
                string allocation_error;
                int allocation_id = db.allocate_resource(
                    request.request_id, selected.resource_id, user_id, allocation_error);
                if (allocation_id > 0) {
                    outcome["status"] = "Allocated";
                    outcome["allocation_id"] = allocation_id;
                    outcome["resource_id"] = selected.resource_id;
                    outcome["resource_name"] = selected.resource_name;
                    outcome["explanation"] = reason;
                    allocated_count++;
                } else {
                    outcome["reason"] = allocation_error.empty()
                        ? "Allocation failed without a backend error message."
                        : allocation_error;
                }
            }
        }
        results.push_back(outcome);
    }

    return {
        {"attempted", scheduled.size()},
        {"allocated", allocated_count},
        {"unallocated", (int)scheduled.size() - allocated_count},
        {"results", results}
    };
}

