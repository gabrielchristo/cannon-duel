#include "NetWorker.h"
#include "SupabaseClient.h"
#include "../Platform.h"
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

std::function<void(SupabaseClient&)> NetWorker::PopHigh() {
    if (highQueue_.empty()) return {};
    auto job = std::move(highQueue_.front());
    highQueue_.pop_front();
    return job;
}

std::function<void(SupabaseClient&)> NetWorker::PopCoalesced() {
    if (coalesced_.empty()) return {};
    auto it = coalesced_.begin();
    auto job = std::move(it->second);
    coalesced_.erase(it);
    return job;
}

#if CANNON_DUEL_WEB_BUILD

void NetWorker::EnsureThread() {}
void NetWorker::ThreadMainHigh() {}
void NetWorker::ThreadMainCoalesced() {}

void NetWorker::Stop() {
    std::lock_guard lock(mu_);
    highQueue_.clear();
    coalesced_.clear();
}

void NetWorker::Post(std::function<void(SupabaseClient&)> job) {
    std::lock_guard lock(mu_);
    highQueue_.push_back(std::move(job));
}

void NetWorker::PostCoalesced(const std::string& slot, std::function<void(SupabaseClient&)> job) {
    std::lock_guard lock(mu_);
    coalesced_[slot] = std::move(job);
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

void NetWorker::Tick() {
    static SupabaseClient client;
    std::function<void(SupabaseClient&)> high;
    std::function<void(SupabaseClient&)> low;
    {
        std::lock_guard lock(mu_);
        high = PopHigh();
        low = PopCoalesced();
    }
    if (high) high(client);
    if (low) low(client);
}

#else

void NetWorker::Tick() {}

void NetWorker::EnsureThread() {
    if (highThread_.joinable()) return;
    stop_.store(false);
    highThread_ = std::thread([this] { ThreadMainHigh(); });
    coalescedThread_ = std::thread([this] { ThreadMainCoalesced(); });
}

void NetWorker::Stop() {
    stop_.store(true);
    cv_.notify_all();
    if (highThread_.joinable()) highThread_.join();
    if (coalescedThread_.joinable()) coalescedThread_.join();
}

void NetWorker::Post(std::function<void(SupabaseClient&)> job) {
    EnsureThread();
    {
        std::lock_guard lock(mu_);
        highQueue_.push_back(std::move(job));
    }
    cv_.notify_all();
}

void NetWorker::PostCoalesced(const std::string& slot, std::function<void(SupabaseClient&)> job) {
    EnsureThread();
    {
        std::lock_guard lock(mu_);
        coalesced_[slot] = std::move(job);
    }
    cv_.notify_all();
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

void NetWorker::ThreadMainHigh() {
    SupabaseClient client;
    while (!stop_.load()) {
        std::function<void(SupabaseClient&)> job;
        {
            std::unique_lock lock(mu_);
            cv_.wait_for(lock, std::chrono::milliseconds(8), [this] {
                return stop_.load() || !highQueue_.empty();
            });
            if (stop_.load()) break;
            job = PopHigh();
        }
        if (job) job(client);
    }
}

void NetWorker::ThreadMainCoalesced() {
    SupabaseClient client;
    while (!stop_.load()) {
        std::function<void(SupabaseClient&)> job;
        {
            std::unique_lock lock(mu_);
            cv_.wait_for(lock, std::chrono::milliseconds(8), [this] {
                return stop_.load() || !coalesced_.empty();
            });
            if (stop_.load()) break;
            job = PopCoalesced();
        }
        if (job) job(client);
    }
}

#endif // CANNON_DUEL_WEB_BUILD
