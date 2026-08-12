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

// Fila HTTP com prioridade: turnos/disparos na frente; mira ao vivo coalescida
// (só o último estado) para não acumular centenas de PATCH atrás de polls.
class NetWorker {
public:
    NetWorker();
    ~NetWorker();

    NetWorker(const NetWorker&) = delete;
    NetWorker& operator=(const NetWorker&) = delete;

    // Turnos, polls, disparos — processados antes de qualquer job de baixa prioridade.
    void Post(std::function<void(SupabaseClient&)> job);

    // Heartbeat / limpeza de mira — descarta jobs antigos do mesmo slot.
    void PostCoalesced(const std::string& slot, std::function<void(SupabaseClient&)> job);
    void ClearCoalesced();
    void CloseConnections();

    void Stop();

private:
    void EnsureThread();
    void ThreadMain();

    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::deque<std::function<void(SupabaseClient&)>> highQueue_;
    std::unordered_map<std::string, std::function<void(SupabaseClient&)>> coalesced_;
    std::atomic<bool> stop_{false};
};

NetWorker& GlobalNetWorker();
