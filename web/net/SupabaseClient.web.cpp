// Implementação web do SupabaseClient (ver src/net/SupabaseClient.h).
//
// fetch() do browser, sem libcurl. O NetWorker usa replay assíncrono:
// cada Request dispara o fetch e lança WebHttpYield — o frame continua.
// Quando a resposta chega, o job é reexecutado e as chamadas já resolvidas
// saem do cache. Chamadas no thread do jogo (clique) ainda usam EM_ASYNC_JS.
#include "net/SupabaseClient.h"
#include "net/SupabaseConfig.h"
#include "DebugLog.h"

#include <emscripten.h>
#include <cstdlib>
#include <string>

namespace {

EM_JS(int, cannon_http_start, (const char* methodPtr, const char* urlPtr,
                               const char* headersJsonPtr, const char* bodyPtr), {
    if (!Module.CannonHttp) Module.CannonHttp = { nextId: 1, pending: {} };
    const id = Module.CannonHttp.nextId++;
    const method = UTF8ToString(methodPtr);
    const url = UTF8ToString(urlPtr);
    const headers = JSON.parse(UTF8ToString(headersJsonPtr));
    const opts = { method: method, headers: headers };
    if (bodyPtr) opts.body = UTF8ToString(bodyPtr);
    fetch(url, opts).then(function(resp) {
        return resp.text().then(function(text) {
            Module.CannonHttp.pending[id] = { status: resp.status, text: text };
        });
    }).catch(function(e) {
        console.warn('SUPABASE fetch falhou:', e);
        Module.CannonHttp.pending[id] = { status: 0, text: "" };
    });
    return id;
});

EM_JS(int, cannon_http_ready, (int id), {
    return (Module.CannonHttp && Module.CannonHttp.pending[id]) ? 1 : 0;
});

EM_JS(char*, cannon_http_take, (int id, int* outStatus), {
    const slot = Module.CannonHttp.pending[id];
    delete Module.CannonHttp.pending[id];
    HEAP32[outStatus >> 2] = slot.status;
    return stringToNewUTF8(slot.text);
});

EM_ASYNC_JS(char*, web_supabase_request, (const char* methodPtr, const char* urlPtr,
                                           const char* headersJsonPtr, const char* bodyPtr,
                                           int* outStatus), {
    const method = UTF8ToString(methodPtr);
    const url = UTF8ToString(urlPtr);
    const headers = JSON.parse(UTF8ToString(headersJsonPtr));
    const opts = { method, headers };
    if (bodyPtr) {
        opts.body = UTF8ToString(bodyPtr);
    }
    try {
        const resp = await fetch(url, opts);
        const text = await resp.text();
        HEAP32[outStatus >> 2] = resp.status;
        return stringToNewUTF8(text);
    } catch (e) {
        console.warn('SUPABASE fetch falhou:', e);
        HEAP32[outStatus >> 2] = 0;
        return stringToNewUTF8("");
    }
});

std::string BuildUrl(const std::string& urlSuffix) {
    return std::string(supabase_config::URL) + "/rest/v1/" + urlSuffix;
}

std::string BuildHeaders(const char* preferHeader) {
    nlohmann::json headers = {
        { "apikey", supabase_config::ANON_KEY },
        { "Authorization", std::string("Bearer ") + supabase_config::ANON_KEY },
        { "Content-Type", "application/json" },
    };
    if (preferHeader) {
        std::string ph(preferHeader);
        size_t sep = ph.find(": ");
        if (sep != std::string::npos) {
            headers[ph.substr(0, sep)] = ph.substr(sep + 2);
        }
    }
    return headers.dump();
}

nlohmann::json ParseHttpBody(int status, const std::string& responseBuffer, bool& ok) {
    ok = false;
    if (status == 0) return nlohmann::json();
    if (status < 200 || status >= 300) return nlohmann::json();
    ok = true;
    if (responseBuffer.empty()) return nlohmann::json::array();
    try {
        return nlohmann::json::parse(responseBuffer);
    } catch (...) {
        ok = false;
        return nlohmann::json();
    }
}

} // namespace

SupabaseClient::SupabaseClient() = default;
SupabaseClient::~SupabaseClient() = default;

void SupabaseClient::Close() {}
void SupabaseClient::EnsureCurl() {}
void SupabaseClient::ResetCurlOptions() {}
void SupabaseClient::ProbeCaBundle() {}

void SupabaseClient::WebEnableAsyncReplay(bool enable) {
    webAsyncReplay_ = enable;
}

void SupabaseClient::WebResetReplay() {
    webReplayIndex_ = 0;
}

void SupabaseClient::WebClearJob() {
    webReplayIndex_ = 0;
    webInFlightId_ = 0;
    webCache_.clear();
}

bool SupabaseClient::WebPollInFlight() {
    if (webInFlightId_ == 0) return false;
    if (!cannon_http_ready(webInFlightId_)) return true;

    int status = 0;
    char* respRaw = cannon_http_take(webInFlightId_, &status);
    std::string responseBuffer = respRaw ? respRaw : "";
    if (respRaw) free(respRaw);
    webInFlightId_ = 0;

    WebCachedResult cached;
    cached.json = ParseHttpBody(status, responseBuffer, cached.ok);
    if (status == 0) {
        DebugLogf(LOG_WARNING, "SUPABASE: falha de transporte (fetch async)");
    } else if (!cached.ok) {
        DebugLogf(LOG_WARNING, "SUPABASE: HTTP %d (async) — %s", status, responseBuffer.c_str());
    }
    webCache_.push_back(std::move(cached));
    return false;
}

nlohmann::json SupabaseClient::WebStartOrReplay(const std::string& method, const std::string& url,
                                                const std::string& headersStr, const char* bodyArg) {
    if (webReplayIndex_ < static_cast<int>(webCache_.size())) {
        const WebCachedResult& cached = webCache_[static_cast<size_t>(webReplayIndex_++)];
        lastOk = cached.ok;
        return cached.json;
    }

    DebugLogf(LOG_INFO, "SUPABASE: %s %s (async)", method.c_str(), url.c_str());
    webInFlightId_ = cannon_http_start(method.c_str(), url.c_str(), headersStr.c_str(), bodyArg);
    throw WebHttpYield{};
}

nlohmann::json SupabaseClient::Request(const std::string& method, const std::string& urlSuffix,
                                        const nlohmann::json* body, const char* preferHeader) {
    lastOk = false;

    const std::string url = BuildUrl(urlSuffix);
    const std::string headersStr = BuildHeaders(preferHeader);

    std::string bodyStr;
    const char* bodyArg = nullptr;
    if (body) {
        bodyStr = body->dump();
        bodyArg = bodyStr.c_str();
    }

    if (webAsyncReplay_) {
        return WebStartOrReplay(method, url, headersStr, bodyArg);
    }

    DebugLogf(LOG_INFO, "SUPABASE: %s %s%s%s", method.c_str(), url.c_str(),
              body ? " body=" : "", body ? bodyStr.c_str() : "");

    int status = 0;
    char* respRaw = web_supabase_request(method.c_str(), url.c_str(), headersStr.c_str(), bodyArg, &status);
    std::string responseBuffer = respRaw ? respRaw : "";
    if (respRaw) free(respRaw);

    if (status == 0) {
        DebugLogf(LOG_WARNING, "SUPABASE: falha de transporte (fetch) em %s", url.c_str());
        return nlohmann::json();
    }
    if (status < 200 || status >= 300) {
        DebugLogf(LOG_WARNING, "SUPABASE: HTTP %d em %s — resposta: %s",
                  status, url.c_str(), responseBuffer.c_str());
        return nlohmann::json();
    }

    lastOk = true;
    if (responseBuffer.empty()) return nlohmann::json::array();
    try {
        return nlohmann::json::parse(responseBuffer);
    } catch (...) {
        DebugLogf(LOG_WARNING, "SUPABASE: resposta não é JSON válido: %s", responseBuffer.c_str());
        lastOk = false;
        return nlohmann::json();
    }
}

nlohmann::json SupabaseClient::Select(const std::string& table, const std::string& query) {
    return Request("GET", table + "?" + query, nullptr, nullptr);
}

nlohmann::json SupabaseClient::Insert(const std::string& table, const nlohmann::json& body) {
    return Request("POST", table, &body, "Prefer: return=representation");
}

nlohmann::json SupabaseClient::Update(const std::string& table, const std::string& filter, const nlohmann::json& body) {
    return Request("PATCH", table + "?" + filter, &body, "Prefer: return=representation");
}

nlohmann::json SupabaseClient::Upsert(const std::string& table, const nlohmann::json& body, const std::string& onConflictColumn) {
    std::string path = table + "?on_conflict=" + onConflictColumn;
    return Request("POST", path, &body, "Prefer: resolution=merge-duplicates,return=representation");
}

void SupabaseClient::Delete(const std::string& table, const std::string& filter) {
    Request("DELETE", table + "?" + filter, nullptr, nullptr);
}
