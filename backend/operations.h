#pragma once
#include <nlohmann/json.hpp>
#include "database.h"
using json=nlohmann::json;

class Operations {
    Database &db;
    using statement=unique_ptr<sql::PreparedStatement>;
    statement prepare(const string &query,const json &args) {
        statement ps(db.conn->prepareStatement(query));int index=1;
        for(const auto &arg:args) {
            if(arg.is_null())ps->setNull(index,0);
            else if(arg.is_number_integer())ps->setInt64(index,arg.get<long long>());
            else ps->setString(index,arg.get<string>());
            index++;
        }
        return ps;
    }
    struct transaction {
        sql::Connection *conn;bool done=false;
        explicit transaction(sql::Connection *c):conn(c){conn->setAutoCommit(false);}
        void commit(){conn->commit();conn->setAutoCommit(true);done=true;}
        ~transaction(){if(!done)try{conn->rollback();conn->setAutoCommit(true);}catch(...) {}}
    };
    json lock_request(int id) {
        auto rows=query("SELECT JSON_OBJECT('request_id',request_id,'status',status,'assistance_type_id',assistance_type_id,'people_affected',people_affected,'zone_id',zone_id) FROM rescue_requests WHERE request_id=? FOR UPDATE",{id});
        if(rows.empty())throw runtime_error("Request not found");
        return rows[0];
    }
    void pending_only(const json &request) {
        string status=request.at("status");
        if(status=="Allocated"||status=="Completed"||status=="Cancelled")throw runtime_error("Choose a pending, unallocated request");
        if(number("SELECT COUNT(*) FROM allocations WHERE request_id=? AND status='Active'",{request["request_id"]}))throw runtime_error("Request already has active allocations");
    }
    resource lock_resource(int id) {
        auto values=query("SELECT JSON_OBJECT('id',resource_id,'name',resource_name,'type',resource_type_id,'zone',zone_id,'capacity',capacity,'unit',capacity_unit,'status',status) FROM resources WHERE resource_id=? FOR UPDATE",{id});
        if(values.empty())throw runtime_error("Resource not found");auto value=values[0];resource r{};
        r.resource_id=id;r.resource_name=value["name"];r.resource_type_id=value["type"];r.zone_id=value["zone"];r.capacity=value["capacity"];r.capacity_unit=value["unit"];r.status=value["status"];
        auto types=query("SELECT JSON_OBJECT('name',type_name) FROM resource_types WHERE resource_type_id=?",{r.resource_type_id});r.resource_type=types[0]["name"];
        return r;
    }
    string assistance_name(int id) {
        auto types=query("SELECT JSON_OBJECT('name',type_name) FROM assistance_types WHERE assistance_type_id=? AND is_active=1",{id});
        if(types.empty())throw runtime_error("Unknown or inactive assistance type");return types[0]["name"];
    }
    map<int,int> requirements(const json &request) {
        map<int,int> needed{{request["assistance_type_id"].get<int>(),1}};
        for(const auto &row:query("SELECT JSON_OBJECT('type',assistance_type_id,'units',units) FROM request_resource_needs WHERE request_id=?",{request["request_id"]}))needed[row["type"].get<int>()]=row["units"].get<int>();
        return needed;
    }
    void audit(int user,const string &action,const string &entity,int id,const string &details) {
        execute("INSERT INTO audit_log(user_id,action,entity_type,entity_id,details) VALUES(?,?,?,?,?)",{user,action,entity,id,details});
    }
public:
    explicit Operations(Database &database):db(database){}
    json query(const string &sql,const json &args=json::array()) {
        auto ps=prepare(sql,args);unique_ptr<sql::ResultSet> rs(ps->executeQuery());json result=json::array();
        while(rs->next())result.push_back(json::parse(rs->getString(1).asStdString()));return result;
    }
    long long number(const string &sql,const json &args=json::array()) {
        auto ps=prepare(sql,args);unique_ptr<sql::ResultSet> rs(ps->executeQuery());return rs->next()?rs->getInt64(1):0;
    }
    int execute(const string &sql,const json &args=json::array()) {auto ps=prepare(sql,args);return ps->executeUpdate();}
    json needs(int request_id) {
        auto request=db.get_request_by_id(request_id);if(!request.request_id)throw runtime_error("Request not found");
        json data=json::array();map<int,int> items{{request.assistance_type_id,1}};
        for(const auto &row:query("SELECT JSON_OBJECT('type',assistance_type_id,'units',units) FROM request_resource_needs WHERE request_id=?",{request_id}))items[row["type"].get<int>()]=row["units"].get<int>();
        for(auto [id,units]:items)data.push_back({{"assistance_type_id",id},{"name",assistance_name(id)},{"units",units},{"primary",id==request.assistance_type_id}});
        return data;
    }
    json set_needs(int request_id,const json &items,int user) {
        if(!items.is_array()||items.size()>12)throw runtime_error("Supply at most 12 requirements");
        transaction tx(db.conn);auto request=lock_request(request_id);pending_only(request);
        map<int,int> parsed{{request["assistance_type_id"].get<int>(),1}};
        set<int> supplied;
        for(const auto &item:items){int id=item.at("assistance_type_id"),units=item.at("units");if(id<=0||units<1||units>12)throw runtime_error("Requirement units must be 1 to 12");assistance_name(id);if(!supplied.insert(id).second)throw runtime_error("Duplicate requirement type");parsed[id]=units;}
        int total=0;for(auto [id,units]:parsed)total+=units;if(total>12)throw runtime_error("At most 12 resources per request");
        execute("DELETE FROM request_resource_needs WHERE request_id=?",{request_id});
        for(auto [id,units]:parsed)if(id!=request["assistance_type_id"].get<int>()||units!=1)execute("INSERT INTO request_resource_needs(request_id,assistance_type_id,units) VALUES(?,?,?)",{request_id,id,units});
        audit(user,"SET_RESOURCE_NEEDS","rescue_requests",request_id,"Resource units required: "+to_string(total));tx.commit();return needs(request_id);
    }
    json bundle(int request_id,const json &assignments,int user) {
        if(!assignments.is_array()||assignments.empty()||assignments.size()>12)throw runtime_error("Choose 1 to 12 resources");
        transaction tx(db.conn);auto request=lock_request(request_id);pending_only(request);auto needed=requirements(request);
        map<int,int> provided;map<int,int> selected;
        for(const auto &item:assignments){int id=item.at("resource_id"),type=item.at("assistance_type_id");if(id<=0||type<=0||selected.count(id))throw runtime_error("Choose distinct valid resources");selected[id]=type;provided[type]++;}
        if(needed!=provided)throw runtime_error("Assignments must fulfill every required assistance type and unit exactly");
        json created=json::array();
        // Sorted resource IDs give all multi-resource writers the same lock order.
        for(auto [id,type]:selected) {
            auto r=lock_resource(id);if(r.status!=RES_AVAILABLE)throw runtime_error("A selected resource is no longer available");
            string type_name=assistance_name(type);
            pcb p{};p.people_affected=request["people_affected"];p.assistance_type=type_name;p.zone_id=request["zone_id"];
            if(!resource_supports_assistance(type_name,r.resource_type)||!resource_has_capacity_for_request(p,r))throw runtime_error("Resource type or people/patient capacity is unsuitable");
            if(number("SELECT COUNT(*) FROM allocations WHERE resource_id=? AND status='Active'",{id}))throw runtime_error("Resource already allocated");
            execute("INSERT INTO allocations(request_id,resource_id,allocated_by,status) VALUES(?,?,?,'Active')",{request_id,id,user});int allocation_id=number("SELECT LAST_INSERT_ID()");
            execute("INSERT INTO allocation_assistance(allocation_id,assistance_type_id) VALUES(?,?)",{allocation_id,type});
            execute("UPDATE resources SET status='Allocated',updated_at=NOW() WHERE resource_id=?",{id});
            execute("INSERT INTO resource_status_history(resource_id,old_status,new_status,changed_by,remarks) VALUES(?,'Available','Allocated',?,'Multi-resource allocation')",{id,user});
            audit(user,"ALLOCATE_RESOURCE","allocations",allocation_id,"Multi-resource request="+to_string(request_id));
            created.push_back({{"allocation_id",allocation_id},{"resource_id",id},{"resource_name",r.resource_name},{"assistance_type",type_name}});
        }
        execute("UPDATE rescue_requests SET status='Allocated',updated_at=NOW() WHERE request_id=?",{request_id});
        execute("INSERT INTO request_status_history(request_id,old_status,new_status,changed_by,remarks) VALUES(?,?,'Allocated',?,'All required resources allocated atomically')",{request_id,request["status"],user});
        tx.commit();return created;
    }
    json reserve(int request_id,int resource_id,int seconds,int user) {
        if(seconds<30||seconds>900)throw runtime_error("Reservation duration must be 30 to 900 seconds");
        transaction tx(db.conn);auto request=lock_request(request_id);pending_only(request);auto resource=lock_resource(resource_id);
        if(resource.status!=RES_AVAILABLE)throw runtime_error("Resource is not available");
        bool compatible=false;for(auto [type,units]:requirements(request))compatible=compatible||resource_supports_assistance(assistance_name(type),resource.resource_type);
        pcb p{};p.people_affected=request["people_affected"];
        if(!compatible||!resource_has_capacity_for_request(p,resource))throw runtime_error("Resource does not match this request's requirements or capacity");
        auto holds=query("SELECT JSON_OBJECT('busy',state='Active' AND expires_at>NOW()) FROM resource_reservations WHERE resource_id=? FOR UPDATE",{resource_id});
        if(!holds.empty() && holds[0]["busy"].get<int>()!=0)throw runtime_error("Resource is already reserved; cancel or wait for expiry");
        execute("INSERT INTO resource_reservations(resource_id,request_id,user_id,expires_at,state) VALUES(?,?,?,TIMESTAMPADD(SECOND,?,NOW()),'Active') ON DUPLICATE KEY UPDATE request_id=VALUES(request_id),user_id=VALUES(user_id),created_at=NOW(),expires_at=VALUES(expires_at),state='Active'",{resource_id,request_id,user,seconds});
        audit(user,"RESERVE_RESOURCE","resources",resource_id,"Request="+to_string(request_id)+" ttl_seconds="+to_string(seconds));tx.commit();return {{"reserved",true},{"expires_in",seconds}};
    }
    void cancel_reservation(int resource_id,int user) {
        // Lock request before resource, then recheck ownership in the UPDATE.
        auto rows=query("SELECT JSON_OBJECT('request_id',request_id) FROM resource_reservations WHERE resource_id=?",{resource_id});if(rows.empty())throw runtime_error("Reservation not found");
        transaction tx(db.conn);lock_request(rows[0]["request_id"]);lock_resource(resource_id);
        if(execute("UPDATE resource_reservations SET state='Cancelled' WHERE resource_id=? AND user_id=? AND request_id=? AND state='Active'",{resource_id,user,rows[0]["request_id"]})!=1)throw runtime_error("Only the reserving dispatcher can cancel an active reservation");
        audit(user,"CANCEL_RESERVATION","resources",resource_id,"Dispatcher cancelled reservation");tx.commit();
    }
    json reservations(int user) {
        return query("SELECT JSON_OBJECT('resource_id',rr.resource_id,'request_id',rr.request_id,'public_ref',q.public_ref,'resource_name',r.resource_name,'owner',u.username,'mine',rr.user_id=?,'seconds_remaining',GREATEST(0,TIMESTAMPDIFF(SECOND,NOW(),rr.expires_at)),'expires_at',DATE_FORMAT(rr.expires_at,'%Y-%m-%d %H:%i:%s')) FROM resource_reservations rr JOIN resources r USING(resource_id) JOIN rescue_requests q ON q.request_id=rr.request_id JOIN users u ON u.user_id=rr.user_id WHERE rr.state='Active' AND rr.expires_at>NOW() ORDER BY rr.expires_at",{user});
    }
    json dispatches() {
        auto rows=query("SELECT JSON_OBJECT('allocation_id',a.allocation_id,'request_id',a.request_id,'public_ref',q.public_ref,'resource_name',r.resource_name,'status',a.status,'stage',COALESCE((SELECT stage FROM dispatch_events e WHERE e.allocation_id=a.allocation_id ORDER BY e.event_id DESC LIMIT 1),'Allocated')) FROM allocations a JOIN rescue_requests q USING(request_id) JOIN resources r USING(resource_id) WHERE a.status IN ('Active','Released') ORDER BY a.allocated_at DESC LIMIT 200");
        for(auto &row:rows)row["events"]=query("SELECT JSON_OBJECT('stage',stage,'changed_at',DATE_FORMAT(changed_at,'%Y-%m-%d %H:%i:%s'),'actor',COALESCE(u.username,'system')) FROM dispatch_events e LEFT JOIN users u ON u.user_id=e.changed_by WHERE allocation_id=? ORDER BY event_id",{row["allocation_id"]});
        return rows;
    }
    void advance(int allocation_id,const string &stage,int user) {
        auto rows=query("SELECT JSON_OBJECT('request_id',request_id,'resource_id',resource_id) FROM allocations WHERE allocation_id=?",{allocation_id});if(rows.empty())throw runtime_error("Allocation not found");
        transaction tx(db.conn);lock_request(rows[0]["request_id"]);lock_resource(rows[0]["resource_id"]);
        if(!number("SELECT COUNT(*) FROM allocations WHERE allocation_id=? AND status='Active' FOR UPDATE",{allocation_id}))throw runtime_error("Allocation is no longer active");
        auto events=query("SELECT JSON_OBJECT('stage',stage) FROM dispatch_events WHERE allocation_id=? ORDER BY event_id DESC LIMIT 1",{allocation_id});
        string current=events.empty()?"Allocated":events[0]["stage"].get<string>();map<string,string> next{{"Allocated","Dispatched"},{"Dispatched","Arrived"},{"Arrived","Working"}};
        if(!next.count(current)||next[current]!=stage)throw runtime_error("Advance one stage at a time; use the existing Release action to finish");
        execute("INSERT INTO dispatch_events(allocation_id,stage,changed_by) VALUES(?,?,?)",{allocation_id,stage,user});audit(user,"DISPATCH_STAGE","allocations",allocation_id,current+" -> "+stage);tx.commit();
    }
    json duplicates() {
        return query(R"SQL(SELECT JSON_OBJECT('request_low',a.request_id,'request_high',b.request_id,'reference_low',a.public_ref,'reference_high',b.public_ref,'name_low',a.victim_name,'name_high',b.victim_name,'reason',IF(CHAR_LENGTH(REGEXP_REPLACE(a.contact_number,'[^0-9]',''))>=7 AND REGEXP_REPLACE(a.contact_number,'[^0-9]','')=REGEXP_REPLACE(b.contact_number,'[^0-9]',''),'Same normalized phone, disaster, zone and assistance within 24 hours','Same name and locality with matching location text within 24 hours'),'disposition',COALESCE(d.disposition,'Unreviewed'))
        FROM rescue_requests a JOIN rescue_requests b ON a.request_id<b.request_id AND a.disaster_id=b.disaster_id AND a.zone_id=b.zone_id AND a.assistance_type_id=b.assistance_type_id AND ABS(TIMESTAMPDIFF(SECOND,a.arrival_time,b.arrival_time))<=86400
        LEFT JOIN request_duplicate_reviews d ON d.request_low=a.request_id AND d.request_high=b.request_id
        WHERE a.status NOT IN ('Completed','Cancelled') AND b.status NOT IN ('Completed','Cancelled') AND
        ((CHAR_LENGTH(REGEXP_REPLACE(a.contact_number,'[^0-9]',''))>=7 AND REGEXP_REPLACE(a.contact_number,'[^0-9]','')=REGEXP_REPLACE(b.contact_number,'[^0-9]','')) OR
        (LOWER(TRIM(a.victim_name))=LOWER(TRIM(b.victim_name)) AND a.locality_id=b.locality_id AND CHAR_LENGTH(TRIM(a.reported_location))>=8 AND CHAR_LENGTH(TRIM(b.reported_location))>=8 AND
        (LOCATE(LOWER(TRIM(a.reported_location)),LOWER(TRIM(b.reported_location)))>0 OR LOCATE(LOWER(TRIM(b.reported_location)),LOWER(TRIM(a.reported_location)))>0)))
        ORDER BY GREATEST(a.arrival_time,b.arrival_time) DESC LIMIT 100)SQL");
    }
    void review(int low,int high,const string &disposition,int user) {
        if(low<=0||high<=low||(disposition!="Related"&&disposition!="Distinct"))throw runtime_error("Choose a valid pair and review disposition");
        transaction tx(db.conn);lock_request(low);lock_request(high);
        execute("INSERT INTO request_duplicate_reviews(request_low,request_high,disposition,reviewed_by) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE disposition=VALUES(disposition),reviewed_by=VALUES(reviewed_by),reviewed_at=NOW()",{low,high,disposition,user});
        audit(user,"REVIEW_DUPLICATE","rescue_requests",low,"Related request="+to_string(high)+" disposition="+disposition);tx.commit();
    }
    json alerts(int user) {
        auto settings=query("SELECT JSON_OBJECT('warning_minutes',warning_minutes,'critical_minutes',critical_minutes) FROM request_alert_settings WHERE user_id=?",{user});json config=settings.empty()?json{{"warning_minutes",30},{"critical_minutes",120}}:settings[0];
        auto rows=query("SELECT JSON_OBJECT('request_id',q.request_id,'public_ref',q.public_ref,'status',q.status,'zone_name',z.zone_name,'assistance_type',t.type_name,'age_minutes',GREATEST(0,TIMESTAMPDIFF(MINUTE,q.arrival_time,NOW()))) FROM rescue_requests q JOIN zones z USING(zone_id) JOIN assistance_types t USING(assistance_type_id) WHERE q.status IN ('Submitted','New','Ready','Scheduled','Under Review','Deferred') AND NOT EXISTS(SELECT 1 FROM allocations a WHERE a.request_id=q.request_id AND a.status='Active') ORDER BY q.arrival_time LIMIT 500");
        for(auto &row:rows){int age=row["age_minutes"];row["level"]=age>=config["critical_minutes"].get<int>()?"Critical":age>=config["warning_minutes"].get<int>()?"Warning":"Within target";}
        return {{"settings",config},{"requests",rows},{"scope","Up to 500 oldest pending requests. Age uses MySQL wall-clock time, not scheduler simulation time."}};
    }
    void thresholds(int warning,int critical,int user) {
        if(warning<1||critical<=warning||critical>10080)throw runtime_error("Use 1 <= warning < critical <= 10080 minutes");
        transaction tx(db.conn);execute("INSERT INTO request_alert_settings(user_id,warning_minutes,critical_minutes) VALUES(?,?,?) ON DUPLICATE KEY UPDATE warning_minutes=VALUES(warning_minutes),critical_minutes=VALUES(critical_minutes)",{user,warning,critical});audit(user,"SET_ALERT_THRESHOLDS","users",user,"warning="+to_string(warning)+" critical="+to_string(critical));tx.commit();
    }
    json utilization(int days,int zone=0) {
        if(days<1||days>90)throw runtime_error("Choose a window from 1 to 90 days");
        auto resources=query(R"SQL(SELECT JSON_OBJECT('resource_id',r.resource_id,'resource_name',r.resource_name,'resource_type',t.type_name,'zone_name',z.zone_name,'status',r.status,
        'observed_seconds',GREATEST(0,TIMESTAMPDIFF(SECOND,GREATEST(r.created_at,TIMESTAMPADD(DAY,?,NOW())),NOW())),
        'busy_seconds',COALESCE((SELECT SUM(GREATEST(0,TIMESTAMPDIFF(SECOND,GREATEST(a.allocated_at,r.created_at,TIMESTAMPADD(DAY,?,NOW())),LEAST(COALESCE(a.released_at,NOW()),NOW())))) FROM allocations a WHERE a.resource_id=r.resource_id AND a.status IN ('Active','Released')),0),
        'allocation_count',(SELECT COUNT(*) FROM allocations a WHERE a.resource_id=r.resource_id AND a.allocated_at>=TIMESTAMPADD(DAY,?,NOW())))
        FROM resources r JOIN resource_types t USING(resource_type_id) JOIN zones z USING(zone_id) WHERE (?=0 OR r.zone_id=?) ORDER BY r.resource_id)SQL",{-days,-days,-days,zone,zone});
        for(auto &row:resources){double observed=row["observed_seconds"].get<double>(),busy=min(observed,row["busy_seconds"].get<double>());row["busy_seconds"]=busy;row["unallocated_seconds"]=observed-busy;row["utilization_percent"]=observed>0?100*busy/observed:0;}
        auto demand=query("SELECT JSON_OBJECT('zone_name',z.zone_name,'assistance_type',t.type_name,'request_count',COUNT(*),'people_affected',SUM(q.people_affected)) FROM rescue_requests q JOIN zones z USING(zone_id) JOIN assistance_types t USING(assistance_type_id) WHERE q.arrival_time>=TIMESTAMPADD(DAY,?,NOW()) AND (?=0 OR q.zone_id=?) GROUP BY z.zone_id,z.zone_name,t.type_name ORDER BY COUNT(*) DESC",{-days,zone,zone});
        return {{"days",days},{"resources",resources},{"demand",demand},{"note","Busy time is allocation overlap with the selected window since resource creation. Unallocated time includes maintenance/inactive periods; it does not imply deployable capacity. Demand uses real request arrival timestamps."}};
    }
};
