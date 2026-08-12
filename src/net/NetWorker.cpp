#include "NetWorker.h"
#include "SupabaseClient.h"
#include <chrono>
#include <thread>

NetWorker& GlobalNetWorker() {
    static NetWorker worker;
    return worker;
}

NetWorker::NetWorker() = default;

NetWorker::~NetWorker() {
    Stop();
}

void NetWorker::EnsureThread() {
    if (thread_.joinable()) return;
    stop_.store(false);
    thread_ = std::thread([this] { ThreadMain(); });
}

void NetWorker::Stop() {
    stop_.store(true);
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void NetWorker::Post(std::function<void(SupabaseClient&)> job) {
    EnsureThread();
    {
        std::lock_guard lock(mu_);
        highQueue_.push_back(std::move(job));
    }
    cv_.notify_one();
}

void NetWorker::PostCoalesced(const std::string& slot, std::function<void(SupabaseClient&)> job) {
    EnsureThread();
    {
        std::lock_guard lock(mu_);
        coalesced_[slot] = std::move(job);
    }
    cv_.notify_one();
}

void NetWorker::ClearCoalesced() {
    std::lock_guard lock(mu_);
    coalesced_.clear();
}

void NetWorker::CloseConnections() {
    ClearCoalesced();
    Post([](SupabaseClient& client) {
        client.Close();
    });
}

void NetWorker::ThreadMain() {
    SupabaseClient client;
    while (!stop_.load()) {
        std::function<void(SupabaseClient&)> job;
        {
            std::unique_lock lock(mu_);
            cv_.wait_for(lock, std::chrono::milliseconds(80), [this] {
                return stop_.load() || !highQueue_.empty() || !coalesced_.empty();
            });
            if (stop_.load()) break;
            if (!highQueue_.empty()) {
                job = std::move(highQueue_.front());
                highQueue_.pop_front();
            } else if (!coalesced_.empty()) {
                auto it = coalesced_.begin();
                job = std::move(it->second);
                coalesced_.erase(it);
            } else {
                continue;
            }
        }
        if (job) job(client);
    }
}
