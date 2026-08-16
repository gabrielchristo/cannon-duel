#pragma once
#include "../Platform.h"

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

// Wrapper fino sobre a API REST automática do Supabase (PostgREST). Cada
// tabela do banco vira um endpoint HTTP; aqui só montamos as URLs/headers
// certos e (de)serializamos JSON. Sem nenhuma lógica de jogo aqui dentro —
// só transporte.
//
// Reutiliza um handle libcurl por instância (keep-alive) enquanto a instância
// existir; chame Close() ao sair do lobby/partida para liberar conexões.
class SupabaseClient {
public:
    SupabaseClient();
    ~SupabaseClient();

    SupabaseClient(const SupabaseClient&) = delete;
    SupabaseClient& operator=(const SupabaseClient&) = delete;

    nlohmann::json Select(const std::string& table, const std::string& query = "select=*");
    nlohmann::json Insert(const std::string& table, const nlohmann::json& body);
    nlohmann::json Update(const std::string& table, const std::string& filter, const nlohmann::json& body);
    nlohmann::json Upsert(const std::string& table, const nlohmann::json& body, const std::string& onConflictColumn);
    void Delete(const std::string& table, const std::string& filter);

    bool LastRequestOk() const { return lastOk; }

    static void ProbeCaBundle();
    void Close();

#if CANNON_DUEL_WEB_BUILD
    // NetWorker web: HTTP sem pausar o frame (fetch no browser, replay do job).
    struct WebHttpYield {};
    void WebEnableAsyncReplay(bool enable);
    bool WebPollInFlight();
    void WebResetReplay();
    void WebClearJob();
#endif

private:
    bool lastOk = true;
    void* curl_ = nullptr;

    void EnsureCurl();
    void ResetCurlOptions();
    nlohmann::json Request(const std::string& method, const std::string& urlSuffix,
                            const nlohmann::json* body, const char* preferHeader);

#if CANNON_DUEL_WEB_BUILD
    bool webAsyncReplay_ = false;
    int webReplayIndex_ = 0;
    int webInFlightId_ = 0;
    struct WebCachedResult {
        bool ok = false;
        nlohmann::json json;
    };
    std::vector<WebCachedResult> webCache_;
    nlohmann::json WebStartOrReplay(const std::string& method, const std::string& url,
                                    const std::string& headersStr, const char* bodyArg);
#endif
};
