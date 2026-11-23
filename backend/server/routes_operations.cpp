#include "api.h"
#include "operations.h"
#include "labs.h"

void register_operations_routes(httplib::Server &svr, Database &db, Operations &operations) {
    using optional_handler=function<json(const httplib::Request&,const Session&)>;
    auto optional_route=[&](const string &method,const string &path,const vector<string> &roles,optional_handler handler) {
        auto wrapped=[&,roles,handler](const httplib::Request &req,httplib::Response &res) {
            Session session;if(!require_role(req,res,roles,session))return;
            try {res.set_content(handler(req,session).dump(),"application/json");}
            catch(const sql::SQLException &e) {res.status=(e.getErrorCode()==1146||e.getErrorCode()==1142)?503:409;res.set_content(json{{"error",res.status==503?"Install database/migrations/20261115_optional_operations.sql as administrator":string(e.what())}}.dump(),"application/json");}
            catch(const exception &e){res.status=400;res.set_content(json{{"error",e.what()}}.dump(),"application/json");}
        };
        if(method=="GET")svr.Get(path,wrapped);else if(method=="PUT")svr.Put(path,wrapped);else svr.Post(path,wrapped);
    };
    optional_route("GET","/api/staff/operations/overview",{"DISPATCHER"},[&](const auto &,const Session &s){
        json requests=json::array(),resources=json::array(),types=json::array();
        for(const auto &p:db.get_pending_requests())requests.push_back(pcb_to_json(p));
        for(const auto &r:db.get_all_resources())resources.push_back(resource_to_json(r));
        for(const auto &t:db.get_assistance_types())types.push_back({{"id",t.id},{"name",t.name}});
        return json{{"requests",requests},{"resources",resources},{"assistance_types",types},{"reservations",operations.reservations(s.user_id)},{"dispatches",operations.dispatches()},{"duplicates",operations.duplicates()},{"alerts",operations.alerts(s.user_id)}};
    });
    optional_route("GET","/api/staff/operations/needs",{"DISPATCHER"},[&](const auto &req,const Session &){
        int id=req.has_param("request_id")?stoi(req.get_param_value("request_id")):0;return json{{"needs",operations.needs(id)}};
    });
    optional_route("PUT","/api/staff/operations/needs",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);return json{{"needs",operations.set_needs(body.at("request_id"),body.at("needs"),s.user_id)}};
    });
    optional_route("POST","/api/staff/operations/allocate-bundle",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);return json{{"allocations",operations.bundle(body.at("request_id"),body.at("assignments"),s.user_id)}};
    });
    optional_route("POST","/api/staff/operations/reserve",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);return operations.reserve(body.at("request_id"),body.at("resource_id"),body.value("seconds",300),s.user_id);
    });
    optional_route("POST","/api/staff/operations/cancel-reservation",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);operations.cancel_reservation(body.at("resource_id"),s.user_id);return json{{"success",true}};
    });
    optional_route("POST","/api/staff/operations/dispatch-stage",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);operations.advance(body.at("allocation_id"),body.at("stage"),s.user_id);return json{{"success",true}};
    });
    optional_route("POST","/api/staff/operations/duplicate-review",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);operations.review(body.at("request_low"),body.at("request_high"),body.at("disposition"),s.user_id);return json{{"success",true}};
    });
    optional_route("PUT","/api/staff/operations/alert-settings",{"DISPATCHER"},[&](const auto &req,const Session &s){
        auto body=json::parse(req.body);operations.thresholds(body.at("warning_minutes"),body.at("critical_minutes"),s.user_id);return operations.alerts(s.user_id);
    });
    optional_route("GET","/api/staff/operations/utilization",{"DISPATCHER","ZONE_ADMIN","AUDITOR"},[&](const auto &req,const Session &s){
        int days=req.has_param("days")?stoi(req.get_param_value("days")):7;
        if(s.role=="ZONE_ADMIN"&&s.zone_id<=0)throw runtime_error("Account has no assigned zone");
        return operations.utilization(days,s.role=="ZONE_ADMIN"?s.zone_id:0);
    });
    optional_route("POST","/api/staff/labs/deadlock",{"DISPATCHER","ZONE_ADMIN","AUDITOR"},[&](const auto &req,const Session &){
        auto body=json::parse(req.body);return deadlock_lab(body.value("processes",3),body.value("recover",false));
    });
    optional_route("POST","/api/staff/labs/scheduling",{"DISPATCHER","ZONE_ADMIN","AUDITOR"},[&](const auto &req,const Session &){
        auto body=json::parse(req.body);int count=body.value("count",10),quantum=body.value("quantum",3),gap=body.value("arrival_gap",2),severity=body.value("max_severity",5),people=body.value("max_people",20),seed=body.value("seed",42);string pattern=body.value("pattern","staggered");
        if(count<1||count>30||quantum<1||quantum>100||gap<0||gap>20||severity<1||severity>5||people<1||people>50||seed<0||seed>1000000)throw runtime_error("Invalid lab bounds (requests 1-30, quantum 1-100, gap 0-20, severity 1-5, people 1-50, seed 0-1000000)");
        if(pattern!="staggered"&&pattern!="burst"&&pattern!="random")throw runtime_error("Choose staggered, burst or random arrivals");
        mt19937 random(seed);vector<pcb> workload;
        for(int i=0;i<count;i++){pcb p{};p.request_id=i+1;p.public_ref="SIM-"+to_string(i+1);p.people_affected=uniform_int_distribution<int>(1,people)(random);p.severity_snapshot=uniform_int_distribution<int>(1,severity)(random);p.risk_level=3;p.arrival_time=pattern=="staggered"?i*gap:pattern=="burst"?(i/5)*gap:uniform_int_distribution<int>(0,count*gap)(random);p.status="Ready";workload.push_back(p);}
        json results=json::object(),comparison=json::array(),requests=json::array();for(const auto &p:workload)requests.push_back(pcb_to_json(p));
        for(const auto &entry:vector<pair<string,string>>{{"fcfs","FCFS"},{"sjf","SJF"},{"round_robin","ROUND_ROBIN"},{"priority","PRIORITY"}}){auto result=run_synthetic_schedule(workload,entry.second,quantum);json scheduled=json::array();for(const auto &p:result.scheduled)scheduled.push_back(pcb_to_json(p));results[entry.first]={{"algorithm",entry.second},{"scheduled",scheduled},{"metrics",metrics_to_json(result.metrics)},{"timeline",timeline_to_json(result)}};comparison.push_back(metrics_to_json(result.metrics));}
        return json{{"simulation_only",true},{"seed",seed},{"workload",requests},{"algorithms",results},{"comparison",comparison}};
    });

    // ============================================================
    //  PUBLIC CITIZEN / VICTIM PORTAL ENDPOINTS
    //  (No staff login required)
    // ============================================================

    // 1. Get active disasters
}
