// core/include/resource_manager.h
#pragma once
#include <bits/stdc++.h>
#include "models.h"
#include "database.h"
#include <mutex>
#include <condition_variable>
using namespace std;

class semaphore {
public:
    int                count_;
    mutex              mtx_;
    condition_variable cv_;

    semaphore(int count) { count_ = count; }

    void acquire() {
        unique_lock<mutex> lk(mtx_);
        cv_.wait(lk, [this]() { return count_ > 0; });
        count_--;
    }

    void release() {
        lock_guard<mutex> lk(mtx_);
        count_++;
        cv_.notify_one();
    }

    int get_count() {
        lock_guard<mutex> lk(mtx_);
        return count_;
    }
};

class rw_lock {
public:
    mutex              mtx_;
    condition_variable cv_;
    int                readers_;
    int                writer_active_;
    int                writers_waiting_;

    rw_lock() { readers_ = 0; writer_active_ = 0; writers_waiting_ = 0; }

    void lock_read() {
        unique_lock<mutex> lk(mtx_);
        cv_.wait(lk, [this]() { return writer_active_ == 0 && writers_waiting_ == 0; });
        readers_++;
    }

    void unlock_read() {
        lock_guard<mutex> lk(mtx_);
        readers_--;
        if (readers_ == 0) cv_.notify_all();
    }

    void lock_write() {
        unique_lock<mutex> lk(mtx_);
        writers_waiting_++;
        cv_.wait(lk, [this]() { return readers_ == 0 && writer_active_ == 0; });
        writers_waiting_--;
        writer_active_ = 1;
    }

    void unlock_write() {
        lock_guard<mutex> lk(mtx_);
        writer_active_ = 0;
        cv_.notify_all();
    }
};

class resource_manager {
public:
    Database               &db_;
    rw_lock                 rw_;
    unordered_map<int,resource> cache_;
    semaphore               worker_sem_;

    typedef struct {
        int            processed;
        vector<string> log;
    } work_result;

    resource_manager(Database &db) : db_(db), worker_sem_(2) {}

    void load_from_db() {
        rw_.lock_write();
        cache_.clear();
        vector<resource> all = db_.get_all_resources();
        for (int i = 0; i < (int)all.size(); i++) {
            cache_[all[i].resource_id] = all[i];
        }
        rw_.unlock_write();
    }

    vector<resource> get_available() {
        rw_.lock_read();
        vector<resource> out;
        for (auto &kv : cache_) {
            if (kv.second.status == RES_AVAILABLE) out.push_back(kv.second);
        }
        rw_.unlock_read();
        return out;
    }

    vector<resource> get_all() {
        rw_.lock_read();
        vector<resource> out;
        for (auto &kv : cache_) out.push_back(kv.second);
        rw_.unlock_read();
        return out;
    }

    resource get_by_id(int id) {
        rw_.lock_read();
        resource r;
        r.resource_id = 0;
        if (cache_.count(id)) r = cache_[id];
        rw_.unlock_read();
        return r;
    }

    int allocate(int request_id, int resource_id, int user_id, string &err) {
        int id = db_.allocate_resource(request_id, resource_id, user_id, err);
        if (id > 0) {
            rw_.lock_write();
            if (cache_.count(resource_id)) cache_[resource_id].status = RES_ALLOCATED;
            rw_.unlock_write();
        }
        return id;
    }

    int release(int alloc_id, int user_id, string &err) {
        int ok = db_.release_allocation(alloc_id, user_id, err);
        if (ok) load_from_db();
        return ok;
    }

    int set_status(int resource_id, string new_status, int user_id, string remarks) {
        int ok = db_.update_resource_status(resource_id, new_status, user_id, remarks);
        if (ok) {
            rw_.lock_write();
            if (cache_.count(resource_id)) cache_[resource_id].status = new_status;
            rw_.unlock_write();
        }
        return ok;
    }

    int add_resource(resource r, int user_id) {
        int id = db_.insert_resource(r, user_id);
        if (id > 0) {
            r.resource_id = id;
            rw_.lock_write();
            cache_[id] = r;
            rw_.unlock_write();
        }
        return id;
    }

    work_result process_concurrent(vector<pcb> &queue, int n_workers) {
        work_result wr;
        wr.processed = 0;
        mutex log_mtx;
        vector<thread> threads;

        int n = (int)queue.size();
        if (n > n_workers) n = n_workers;

        for (int i = 0; i < n; i++) {
            threads.push_back(thread([this, i, &queue, &wr, &log_mtx]() {
                worker_sem_.acquire();
                pcb &p = queue[i];
                string msg = "Worker thread processed REQ #"
                           + to_string(p.request_id)
                           + " (" + p.victim_name + ") sev="
                           + to_string(p.severity_snapshot);
                {
                    lock_guard<mutex> lk(log_mtx);
                    wr.log.push_back(msg);
                    wr.processed++;
                }
                worker_sem_.release();
            }));
        }
        for (int i = 0; i < (int)threads.size(); i++) threads[i].join();
        return wr;
    }
};
