#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

// Supabase Realtime (Phoenix) via libcurl WebSocket (curl_ws_*).
// - postgres_changes (CDC) para tabelas
// - broadcast para mira / eventos leves
// Poll HTTP fica só como fallback lento.
class RealtimeClient {
public:
    using JsonHandler = std::function<void(const nlohmann::json& payload)>;

    struct PostgresSub {
        std::string event = "*"; // INSERT | UPDATE | DELETE | *
        std::string schema = "public";
        std::string table;
        std::string filter; // opcional, ex: "match_id=eq.uuid"
    };

    RealtimeClient();
    ~RealtimeClient();

    RealtimeClient(const RealtimeClient&) = delete;
    RealtimeClient& operator=(const RealtimeClient&) = delete;

    // topicSuffix vira "realtime:<topicSuffix>" (ex: "match:<uuid>" ou "lobby").
    void Start(const std::string& topicSuffix, const std::vector<PostgresSub>& subs);
    void Stop();

    bool IsConnected() const { return connected_.load(); }

    void SetOnPostgres(const std::string& table, JsonHandler handler);
    void SetOnBroadcast(JsonHandler handler); // payload = { "event": "...", "payload": {...} }

    // Enfileira broadcast. slot não-vazio = coalescido (só o último por slot).
    void SendBroadcast(const std::string& eventName, const nlohmann::json& payload,
                       const std::string& coalesceSlot = "");

    // Drena eventos da thread WS → thread do jogo.
    void Drain();

private:
    void ThreadMain(std::string topicSuffix, std::vector<PostgresSub> subs);
    void HandleServerMessage(const std::string& raw);
    static std::string MakeWsUrl();

    std::thread thread_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> connected_{false};
    std::atomic<int> refCounter_{1};

    std::string topic_; // "realtime:..."
    std::string joinRef_;

    mutable std::mutex mu_;
    std::unordered_map<std::string, JsonHandler> onPostgres_;
    JsonHandler onBroadcast_;

    struct PendingPg {
        std::string table;
        std::string type;
        nlohmann::json record;
    };
    std::vector<PendingPg> pendingPg_;
    std::vector<nlohmann::json> pendingBroadcast_;

    std::vector<std::string> outbox_;
    std::unordered_map<std::string, std::string> outboxCoalesced_;
};
