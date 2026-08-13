// Implementação web do SupabaseClient (ver src/net/SupabaseClient.h).
//
// WASM sem pthreads não tem socket bloqueante, então em vez de libcurl
// usamos fetch() do browser. EM_ASYNC_JS + build com -sASYNCIFY deixam a
// chamada "parecer" síncrona pro C++ (o Request() abaixo devolve o JSON já
// pronto, igual ao SupabaseClient.cpp de desktop/Android) — nenhum call
// site em NetMatch.cpp/OnlineLobby*.cpp precisa saber a diferença.
//
// TLS é responsabilidade do browser (fetch já exige HTTPS); não há
// cacert.pem/TlsCaBundle aqui.
#include "net/SupabaseClient.h"
#include "net/SupabaseConfig.h"
#include "DebugLog.h"

#include <emscripten.h>
#include <cstdlib>
#include <mutex>

namespace {

// method/url/headersJson(objeto JSON)/body(pode ser null) entram; devolve
// uma string malloc'd (o C++ dá free) com o corpo da resposta, e escreve o
// status HTTP em *outStatus (0 = falha de transporte, ex.: rede caiu).
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

} // namespace

SupabaseClient::SupabaseClient() = default;
SupabaseClient::~SupabaseClient() = default;

void SupabaseClient::Close() {
    // Nada a fechar — cada fetch() é uma requisição independente, o
    // browser cuida de keep-alive/conexões por baixo dos panos.
}

void SupabaseClient::EnsureCurl() {}
void SupabaseClient::ResetCurlOptions() {}

void SupabaseClient::ProbeCaBundle() {
    // TLS é do browser; nada pra checar aqui.
}

nlohmann::json SupabaseClient::Request(const std::string& method, const std::string& urlSuffix,
                                        const nlohmann::json* body, const char* preferHeader) {
    lastOk = false;

    const std::string url = std::string(supabase_config::URL) + "/rest/v1/" + urlSuffix;

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
    const std::string headersStr = headers.dump();

    std::string bodyStr;
    const char* bodyArg = nullptr;
    if (body) {
        bodyStr = body->dump();
        bodyArg = bodyStr.c_str();
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

    DebugLogf(LOG_INFO, "SUPABASE: HTTP %d OK — resposta: %s", status,
              responseBuffer.empty() ? "(vazia)" : responseBuffer.c_str());

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
