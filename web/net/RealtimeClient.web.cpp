// Implementação web do RealtimeClient (ver src/net/RealtimeClient.h).
//
// Sem pthreads não dá pra rodar o loop bloqueante de curl_ws_recv numa
// thread de fundo — em vez disso usamos a API nativa de callbacks
// emscripten_websocket (browser WebSocket por baixo). Os callbacks
// (OnWebOpen/OnWebMessage/OnWebClose) empurram pra dentro das mesmas filas
// (pendingPg_/pendingBroadcast_) que o Drain() do jogo já drena todo
// frame — mesma arquitetura do desktop, só trocando "thread" por "evento
// do browser". SendBroadcast/SetOnPostgres/SetOnBroadcast/
// HandleServerMessage/MakeWsUrl não mudam entre plataformas (não tocam em
// curl/websocket diretamente) e foram copiados do RealtimeClient.cpp de
// desktop.
#include "net/RealtimeClient.h"
#include "net/SupabaseConfig.h"
#include "DebugLog.h"

#include <emscripten/emscripten.h>
#include <emscripten/websocket.h>

#include <sstream>

using json = nlohmann::json;

namespace {
std::string StripUrlScheme(const std::string& url) {
    if (url.rfind("https://", 0) == 0) return url.substr(8);
    if (url.rfind("http://", 0) == 0) return url.substr(7);
    return url;
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

void RealtimeClient::SendJoinWeb() {
    const int joinRef = refCounter_.fetch_add(1);
    joinRef_ = std::to_string(joinRef);

    json pgArr = json::array();
    for (const auto& s : pendingJoinSubs_) {
        json one = {
            { "event", s.event },
            { "schema", s.schema },
            { "table", s.table }
        };
        if (!s.filter.empty()) one["filter"] = s.filter;
        pgArr.push_back(std::move(one));
    }

    json joinMsg = {
        { "topic", topic_ },
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
        { "ref", joinRef_ },
        { "join_ref", joinRef_ }
    };

    const std::string raw = joinMsg.dump();
    emscripten_websocket_send_utf8_text(wsHandle_, raw.c_str());
    DebugLogf(LOG_INFO, "REALTIME: join enviado %s (%zu subs)", topic_.c_str(), pendingJoinSubs_.size());
}

void RealtimeClient::FlushOutboxWeb() {
    std::vector<std::string> toSend;
    {
        std::lock_guard lock(mu_);
        toSend.swap(outbox_);
        for (auto& kv : outboxCoalesced_) toSend.push_back(std::move(kv.second));
        outboxCoalesced_.clear();
    }
    for (const auto& raw : toSend) {
        emscripten_websocket_send_utf8_text(wsHandle_, raw.c_str());
    }
}

EM_BOOL RealtimeClient::OnWebOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent* /*e*/, void* userData) {
    auto* self = static_cast<RealtimeClient*>(userData);
    self->connected_.store(true);
    DebugLogf(LOG_INFO, "REALTIME: WebSocket aberto topic=%s", self->topic_.c_str());
    self->SendJoinWeb();
    return EM_TRUE;
}

EM_BOOL RealtimeClient::OnWebMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent* e, void* userData) {
    auto* self = static_cast<RealtimeClient*>(userData);
    if (!e->isText) return EM_TRUE;
    std::string raw(reinterpret_cast<const char*>(e->data), e->numBytes);
    self->HandleServerMessage(raw);
    return EM_TRUE;
}

EM_BOOL RealtimeClient::OnWebClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent* /*e*/, void* userData) {
    auto* self = static_cast<RealtimeClient*>(userData);
    const bool wanted = !self->stop_.load();
    if (self->wsHandle_ > 0) {
        const int h = self->wsHandle_;
        self->wsHandle_ = 0;
        emscripten_websocket_delete(h);
    }
    self->OnSocketDropped();
    DebugLogf(LOG_INFO, "REALTIME: WebSocket fechado (%s)", self->topic_.c_str());
    if (wanted) self->ScheduleReconnect();
    return EM_TRUE;
}

EM_BOOL RealtimeClient::OnWebError(int /*eventType*/, const EmscriptenWebSocketErrorEvent* /*e*/, void* userData) {
    auto* self = static_cast<RealtimeClient*>(userData);
    DebugLogf(LOG_WARNING, "REALTIME: erro no WebSocket (%s)", self->topic_.c_str());
    const bool wanted = !self->stop_.load();
    self->OnSocketDropped();
    if (wanted) self->ScheduleReconnect();
    return EM_TRUE;
}

void RealtimeClient::ScheduleReconnect() {
    if (stop_.load() || reconnectWanted_) return;
    reconnectWanted_ = true;
    const int waitMs = TakeBackoffMs();
    reconnectAtMs_ = emscripten_get_now() + static_cast<double>(waitMs);
    DebugLogf(LOG_INFO, "REALTIME: reconectando em %d ms (%s)", waitMs, topic_.c_str());
}

void RealtimeClient::OpenWebSocket() {
    if (wsHandle_ > 0) {
        emscripten_websocket_delete(wsHandle_);
        wsHandle_ = 0;
    }
    const std::string url = MakeWsUrl();
    EmscriptenWebSocketCreateAttributes attr;
    emscripten_websocket_init_create_attributes(&attr);
    attr.url = url.c_str();
    attr.createOnMainThread = EM_TRUE;

    wsHandle_ = emscripten_websocket_new(&attr);
    if (wsHandle_ <= 0) {
        DebugLogf(LOG_WARNING, "REALTIME: emscripten_websocket_new falhou");
        wsHandle_ = 0;
        ScheduleReconnect();
        return;
    }

    emscripten_websocket_set_onopen_callback(wsHandle_, this, RealtimeClient::OnWebOpen);
    emscripten_websocket_set_onmessage_callback(wsHandle_, this, RealtimeClient::OnWebMessage);
    emscripten_websocket_set_onclose_callback(wsHandle_, this, RealtimeClient::OnWebClose);
    emscripten_websocket_set_onerror_callback(wsHandle_, this, RealtimeClient::OnWebError);
}

void RealtimeClient::TryReconnectWeb() {
    if (stop_.load() || connected_.load() || !reconnectWanted_) return;
    if (emscripten_get_now() < reconnectAtMs_) return;
    reconnectWanted_ = false;
    if (topic_.empty() || pendingJoinSubs_.empty()) return;
    OpenWebSocket();
}

void RealtimeClient::Start(const std::string& topicSuffix, const std::vector<PostgresSub>& subs) {
    Stop();
    if (topicSuffix.empty()) return;
    stop_.store(false);
    connected_.store(false);
    ResetBackoff();
    joinedPulse_.store(false);
    reconnectWanted_ = false;
    topic_ = "realtime:" + topicSuffix;
    joinRef_.clear();
    pendingJoinSubs_ = subs;
    OpenWebSocket();
}

void RealtimeClient::Stop() {
    stop_.store(true);
    reconnectWanted_ = false;
    if (wsHandle_ > 0) {
        emscripten_websocket_close(wsHandle_, 1000, "bye");
        emscripten_websocket_delete(wsHandle_);
        wsHandle_ = 0;
    }
    connected_.store(false);
    std::lock_guard lock(mu_);
    pendingPg_.clear();
    pendingBroadcast_.clear();
    outbox_.clear();
    outboxCoalesced_.clear();
    topic_.clear();
    joinRef_.clear();
    pendingJoinSubs_.clear();
}

void RealtimeClient::Drain() {
    TryReconnectWeb();
    if (connected_.load() && wsHandle_ > 0) {
        double now = emscripten_get_now();
        if (now - lastHeartbeatMs_ >= 25000.0) {
            const int href = refCounter_.fetch_add(1);
            json hb = {
                { "topic", "phoenix" },
                { "event", "heartbeat" },
                { "payload", json::object() },
                { "ref", std::to_string(href) }
            };
            const std::string raw = hb.dump();
            emscripten_websocket_send_utf8_text(wsHandle_, raw.c_str());
            lastHeartbeatMs_ = now;
        }
        FlushOutboxWeb();
    }

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
