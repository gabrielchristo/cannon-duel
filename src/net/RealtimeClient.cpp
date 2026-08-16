#include "RealtimeClient.h"

#include "SupabaseConfig.h"
#include "TlsCaBundle.h"
#include "../DebugLog.h"

#include <curl/curl.h>
#include <curl/websockets.h>

#include <chrono>
#include <cstring>
#include <sstream>
#include <vector>

using json = nlohmann::json;

namespace {
std::string StripUrlScheme(const std::string& url) {
    if (url.rfind("https://", 0) == 0) return url.substr(8);
    if (url.rfind("http://", 0) == 0) return url.substr(7);
    return url;
}

bool WsSendText(CURL* curl, const std::string& text) {
    size_t sent = 0;
    CURLcode rc = curl_ws_send(curl, text.data(), text.size(), &sent, 0, CURLWS_TEXT);
    if (rc != CURLE_OK) {
        DebugLogf(LOG_WARNING, "REALTIME: curl_ws_send falhou (%d): %s",
                  static_cast<int>(rc), curl_easy_strerror(rc));
        return false;
    }
    return sent == text.size();
}
} // namespace

RealtimeClient::RealtimeClient() = default;

RealtimeClient::~RealtimeClient() {
    Stop();
}

std::string RealtimeClient::MakeWsUrl() {
    std::ostringstream oss;
    oss << "wss://" << StripUrlScheme(supabase_config::URL)
        << "/realtime/v1/websocket?apikey=" << supabase_config::ANON_KEY
        << "&vsn=1.0.0";
    return oss.str();
}

void RealtimeClient::SetOnPostgres(const std::string& table, JsonHandler handler) {
    std::lock_guard lock(mu_);
    onPostgres_[table] = std::move(handler);
}

void RealtimeClient::SetOnBroadcast(JsonHandler handler) {
    std::lock_guard lock(mu_);
    onBroadcast_ = std::move(handler);
}

void RealtimeClient::SendBroadcast(const std::string& eventName, const json& payload,
                                   const std::string& coalesceSlot) {
    std::string topicCopy;
    std::string joinRefCopy;
    {
        std::lock_guard lock(mu_);
        topicCopy = topic_;
        joinRefCopy = joinRef_;
    }
    if (topicCopy.empty() || joinRefCopy.empty()) return;

    const int ref = refCounter_.fetch_add(1);
    json msg = {
        { "topic", topicCopy },
        { "event", "broadcast" },
        { "payload", {
            { "type", "broadcast" },
            { "event", eventName },
            { "payload", payload }
        }},
        { "ref", std::to_string(ref) },
        { "join_ref", joinRefCopy }
    };
    const std::string raw = msg.dump();

    std::lock_guard lock(mu_);
    if (!coalesceSlot.empty()) {
        outboxCoalesced_[coalesceSlot] = raw;
    } else {
        outbox_.push_back(raw);
    }
}

void RealtimeClient::Start(const std::string& topicSuffix, const std::vector<PostgresSub>& subs) {
    Stop();
    if (topicSuffix.empty()) return;
    stop_.store(false);
    connected_.store(false);
    ResetBackoff();
    joinedPulse_.store(false);
    topic_ = "realtime:" + topicSuffix;
    joinRef_.clear();
    thread_ = std::thread([this, topicSuffix, subs] { ThreadMain(topicSuffix, subs); });
}

void RealtimeClient::Stop() {
    stop_.store(true);
    if (thread_.joinable()) thread_.join();
    connected_.store(false);
    std::lock_guard lock(mu_);
    pendingPg_.clear();
    pendingBroadcast_.clear();
    outbox_.clear();
    outboxCoalesced_.clear();
    topic_.clear();
    joinRef_.clear();
}

void RealtimeClient::Drain() {
    std::vector<PendingPg> pg;
    std::vector<json> broadcasts;
    std::unordered_map<std::string, JsonHandler> pgHandlers;
    JsonHandler broadcastHandler;
    {
        std::lock_guard lock(mu_);
        pg.swap(pendingPg_);
        broadcasts.swap(pendingBroadcast_);
        pgHandlers = onPostgres_;
        broadcastHandler = onBroadcast_;
    }

    for (auto& e : pg) {
        auto it = pgHandlers.find(e.table);
        if (it != pgHandlers.end() && it->second) {
            json envelope = {
                { "table", e.table },
                { "type", e.type },
                { "record", std::move(e.record) }
            };
            it->second(envelope);
        }
    }
    if (broadcastHandler) {
        for (auto& b : broadcasts) broadcastHandler(b);
    }
}

void RealtimeClient::HandleServerMessage(const std::string& raw) {
    json msg;
    try {
        msg = json::parse(raw);
    } catch (...) {
        return;
    }

    const std::string event = msg.value("event", "");
    if (event == "phx_reply") {
        const auto& payload = msg["payload"];
        if (payload.is_object() && payload.value("status", "") == "ok") {
            DebugLogf(LOG_INFO, "REALTIME: canal joined OK (%s)", topic_.c_str());
            NoteChannelJoined();
        } else {
            DebugLogf(LOG_WARNING, "REALTIME: phx_reply: %s", raw.c_str());
        }
        return;
    }

    if (event == "system") {
        DebugLogf(LOG_INFO, "REALTIME: system: %s", raw.c_str());
        return;
    }

    if (event == "broadcast") {
        json payload = msg.contains("payload") ? msg["payload"] : json::object();
        // Formato: payload.event + payload.payload  (ou type/event aninhado)
        json envelope = {
            { "event", payload.value("event", "") },
            { "payload", payload.contains("payload") ? payload["payload"] : payload }
        };
        std::lock_guard lock(mu_);
        pendingBroadcast_.push_back(std::move(envelope));
        return;
    }

    if (event != "postgres_changes") return;

    json data = msg.contains("payload") && msg["payload"].is_object() &&
                msg["payload"].contains("data")
                    ? msg["payload"]["data"]
                    : msg.value("payload", json::object());

    if (!data.is_object()) return;

    const std::string type = data.contains("type") && data["type"].is_string()
                                 ? data["type"].get<std::string>()
                                 : (data.contains("eventType") && data["eventType"].is_string()
                                        ? data["eventType"].get<std::string>()
                                        : std::string());
    const std::string table = data.value("table", "");
    json record = json::object();
    if (data.contains("record") && data["record"].is_object()) {
        record = data["record"];
    } else if (data.contains("new") && data["new"].is_object()) {
        record = data["new"];
    }

    if (record.empty()) {
        DebugLogf(LOG_WARNING, "REALTIME: postgres_changes sem record: %s", raw.c_str());
        return;
    }

    DebugLogf(LOG_INFO, "REALTIME: CDC table=%s type=%s", table.c_str(), type.c_str());

    std::lock_guard lock(mu_);
    pendingPg_.push_back(PendingPg{ table, type, std::move(record) });
}

void RealtimeClient::ThreadMain(std::string topicSuffix, std::vector<PostgresSub> subs) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    while (!stop_.load()) {
        RunSocketSession(topicSuffix, subs);
        OnSocketDropped();
        if (stop_.load()) break;
        const int waitMs = TakeBackoffMs();
        DebugLogf(LOG_INFO, "REALTIME: reconectando em %d ms (realtime:%s)",
                  waitMs, topicSuffix.c_str());
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(waitMs);
        while (!stop_.load() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    DebugLogf(LOG_INFO, "REALTIME: thread encerrada (realtime:%s)", topicSuffix.c_str());
}

bool RealtimeClient::RunSocketSession(const std::string& topicSuffix,
                                      const std::vector<PostgresSub>& subs) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        DebugLogf(LOG_WARNING, "REALTIME: curl_easy_init falhou");
        return false;
    }

    const std::string url = MakeWsUrl();
    const std::string apiKeyHeader = std::string("apikey: ") + supabase_config::ANON_KEY;
    const std::string authHeader = std::string("Authorization: Bearer ") + supabase_config::ANON_KEY;
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, apiKeyHeader.c_str());
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    TlsCaBundle ca = LoadTlsCaBundle();
    if (ca.loaded) {
        ApplyTlsCaToCurl(curl, ca);
    }

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        DebugLogf(LOG_WARNING, "REALTIME: handshake WS falhou (%d): %s",
                  static_cast<int>(rc), curl_easy_strerror(rc));
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    connected_.store(true);
    DebugLogf(LOG_INFO, "REALTIME: WebSocket aberto topic=realtime:%s", topicSuffix.c_str());

    const int joinRef = refCounter_.fetch_add(1);
    std::string joinRefLocal;
    std::string topicLocal;
    {
        std::lock_guard lock(mu_);
        topic_ = "realtime:" + topicSuffix;
        joinRef_ = std::to_string(joinRef);
        topicLocal = topic_;
        joinRefLocal = joinRef_;
    }

    json pgArr = json::array();
    for (const auto& s : subs) {
        json one = {
            { "event", s.event },
            { "schema", s.schema },
            { "table", s.table }
        };
        if (!s.filter.empty()) one["filter"] = s.filter;
        pgArr.push_back(std::move(one));
    }

    json joinMsg = {
        { "topic", topicLocal },
        { "event", "phx_join" },
        { "payload", {
            { "config", {
                { "broadcast", { { "ack", false }, { "self", false } } },
                { "presence", { { "enabled", false } } },
                { "private", false },
                { "postgres_changes", pgArr }
            }},
            { "access_token", supabase_config::ANON_KEY }
        }},
        { "ref", joinRefLocal },
        { "join_ref", joinRefLocal }
    };

    if (!WsSendText(curl, joinMsg.dump())) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }
    DebugLogf(LOG_INFO, "REALTIME: join enviado %s (%zu subs)", topicLocal.c_str(), subs.size());

    std::vector<char> buf(64 * 1024);
    std::string frameAcc;
    auto lastHeartbeat = std::chrono::steady_clock::now();

    while (!stop_.load()) {
        std::vector<std::string> toSend;
        {
            std::lock_guard lock(mu_);
            toSend.swap(outbox_);
            for (auto& kv : outboxCoalesced_) toSend.push_back(std::move(kv.second));
            outboxCoalesced_.clear();
        }
        bool sendOk = true;
        for (const auto& raw : toSend) {
            if (!WsSendText(curl, raw)) {
                sendOk = false;
                break;
            }
        }
        if (!sendOk) break;

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat).count() >= 25) {
            const int href = refCounter_.fetch_add(1);
            json hb = {
                { "topic", "phoenix" },
                { "event", "heartbeat" },
                { "payload", json::object() },
                { "ref", std::to_string(href) }
            };
            if (!WsSendText(curl, hb.dump())) break;
            lastHeartbeat = now;
        }

        size_t nread = 0;
        const struct curl_ws_frame* meta = nullptr;
        rc = curl_ws_recv(curl, buf.data(), buf.size(), &nread, &meta);
        if (rc == CURLE_AGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (rc != CURLE_OK) {
            DebugLogf(LOG_WARNING, "REALTIME: curl_ws_recv falhou (%d): %s",
                      static_cast<int>(rc), curl_easy_strerror(rc));
            break;
        }
        if (!meta || nread == 0) continue;

        if (meta->flags & CURLWS_CLOSE) {
            DebugLogf(LOG_INFO, "REALTIME: peer fechou o WebSocket");
            break;
        }
        if (meta->flags & CURLWS_PING) {
            size_t sent = 0;
            curl_ws_send(curl, buf.data(), nread, &sent, 0, CURLWS_PONG);
            continue;
        }
        if (!(meta->flags & CURLWS_TEXT) && !(meta->flags & CURLWS_BINARY)) {
            continue;
        }

        frameAcc.append(buf.data(), nread);
        if (meta->bytesleft == 0) {
            HandleServerMessage(frameAcc);
            frameAcc.clear();
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return true;
}
