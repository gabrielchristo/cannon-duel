#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

class SupabaseClient;

// Duas filas HTTP em paralelo:
//   high  — turnos, polls, disparos
//   coalesced — presença / mira / loja / sync de lobby (só o último por slot)
class NetWorker {
public:
    NetWorker();
    ~NetWorker();

    NetWorker(const NetWorker&) = delete;
    NetWorker& operator=(const NetWorker&) = delete;

    void Post(std::function<void(SupabaseClient&)> job);
    void PostCoalesced(const std::string& slot, std::function<void(SupabaseClient&)> job);
    void ClearCoalesced();
    void CloseConnections();

    void Stop();

    // Web: drena até 2 jobs por frame (1 high + 1 coalescido). No-op no desktop.
    void Tick();

private:
    void EnsureThread();
    void ThreadMainHigh();
    void ThreadMainCoalesced();

    std::function<void(SupabaseClient&)> PopHigh();
    std::function<void(SupabaseClient&)> PopCoalesced();

    std::thread highThread_;
    std::thread coalescedThread_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::function<void(SupabaseClient&)>> highQueue_;
    std::unordered_map<std::string, std::function<void(SupabaseClient&)>> coalesced_;
    std::atomic<bool> stop_{false};
};

NetWorker& GlobalNetWorker();
