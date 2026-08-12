#include "SupabaseClient.h"

#include "SupabaseConfig.h"
#include "../AssetPath.h"
#include <curl/curl.h>
#include <raylib.h>
#include <cstring>
#include <cstdio>

namespace {
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}

// Resolve um caminho REAL de sistema de arquivos pro certificado CA,
// utilizável pelo curl — que usa fopen() puro internamente, e por isso NÃO
// enxerga dentro do APK no Android (só as funções do próprio raylib, tipo
// LoadFileData, conseguem ler o que está empacotado lá via AAssetManager).
//
// No desktop, o arquivo já é um arquivo normal em disco (assets/certs/...),
// então só devolvemos o caminho direto. No Android, extraímos o conteúdo
// uma única vez pra um caminho gravável de verdade e reaproveitamos depois.
//
// Sem isso, TODA requisição HTTPS do multiplayer online falhava em
// silêncio no Android (o curl que compilamos do zero com mbedTLS não tem
// nenhum certificado raiz embutido, ao contrário do curl do sistema usado
// no desktop) — o app parecia "conectado" normalmente, mas nunca conseguia
// de fato gravar nada no banco.
std::string ResolveCaBundlePath() {
#if CANNON_DUEL_ANDROID_BUILD
    static std::string cachedPath;
    if (!cachedPath.empty()) return cachedPath;

    const char* extractedName = "cacert_extracted.pem";
    FILE* check = fopen(extractedName, "rb");
    if (check) {
        fclose(check);
        cachedPath = extractedName;
        return cachedPath;
    }

    int bytesRead = 0;
    unsigned char* data = LoadFileData(AssetPath("certs/cacert.pem").c_str(), &bytesRead);
    if (data && bytesRead > 0) {
        FILE* out = fopen(extractedName, "wb");
        if (out) {
            fwrite(data, 1, static_cast<size_t>(bytesRead), out);
            fclose(out);
            cachedPath = extractedName;
            TraceLog(LOG_INFO, "SUPABASE: certificado CA extraído (%d bytes) -> %s", bytesRead, extractedName);
        }
        UnloadFileData(data);
    } else {
        TraceLog(LOG_WARNING, "SUPABASE: não encontrei assets/certs/cacert.pem empacotado no APK");
    }
    return cachedPath;
#else
    // No desktop, só usamos o arquivo se ele realmente existir — se ainda
    // não foi baixado (ver assets/certs/README.txt), deixamos o CAINFO
    // vazio de propósito, pra não sobrescrever/quebrar o repositório de
    // confiança padrão do sistema (que já funciona sozinho no curl do
    // desktop). Preenchemos com o arquivo real só quando ele existir.
    static std::string path;
    static bool checked = false;
    if (!checked) {
        checked = true;
        std::string candidate = AssetPath("certs/cacert.pem");
        FILE* f = fopen(candidate.c_str(), "rb");
        if (f) {
            fclose(f);
            path = candidate;
            TraceLog(LOG_INFO, "SUPABASE: usando certificado CA em %s", candidate.c_str());
        } else {
            TraceLog(LOG_WARNING, "SUPABASE: %s não encontrado — usando repositório de confiança padrão do sistema", candidate.c_str());
        }
    }
    return path;
#endif
}
} // namespace

nlohmann::json SupabaseClient::Request(const std::string& method, const std::string& urlSuffix,
                                        const nlohmann::json* body, const char* preferHeader) {
    lastOk = false;
    CURL* curl = curl_easy_init();
    if (!curl) {
        TraceLog(LOG_WARNING, "SUPABASE: curl_easy_init() falhou");
        return nlohmann::json();
    }

    std::string url = std::string(supabase_config::URL) + "/rest/v1/" + urlSuffix;
    std::string responseBuffer;
    std::string bodyStr;

    struct curl_slist* headers = nullptr;
    std::string apiKeyHeader = std::string("apikey: ") + supabase_config::ANON_KEY;
    std::string authHeader = std::string("Authorization: Bearer ") + supabase_config::ANON_KEY;
    headers = curl_slist_append(headers, apiKeyHeader.c_str());
    headers = curl_slist_append(headers, authHeader.c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");
    if (preferHeader) headers = curl_slist_append(headers, preferHeader);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);

    std::string caBundle = ResolveCaBundlePath();
    if (!caBundle.empty()) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, caBundle.c_str());
    }

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (body) {
            bodyStr = body->dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
        }
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
        if (body) {
            bodyStr = body->dump();
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
        }
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    // GET é o padrão, não precisa setar nada extra.

    // Buffer de erro detalhado do curl — pega mensagens específicas (tipo
    // falha de verificação de certificado, DNS, timeout) que CURLcode
    // sozinho não descreve bem.
    char errorBuf[CURL_ERROR_SIZE];
    errorBuf[0] = '\0';
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuf);

    TraceLog(LOG_INFO, "SUPABASE: %s %s%s%s", method.c_str(), url.c_str(),
             body ? " body=" : "", body ? bodyStr.c_str() : "");

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        TraceLog(LOG_WARNING, "SUPABASE: falha de transporte (CURLcode=%d, %s) — %s",
                 static_cast<int>(res), curl_easy_strerror(res), errorBuf);
        return nlohmann::json();
    }

    if (httpCode < 200 || httpCode >= 300) {
        TraceLog(LOG_WARNING, "SUPABASE: HTTP %ld em %s — resposta: %s",
                 httpCode, url.c_str(), responseBuffer.c_str());
        return nlohmann::json();
    }

    TraceLog(LOG_INFO, "SUPABASE: HTTP %ld OK — resposta: %s", httpCode,
             responseBuffer.empty() ? "(vazia)" : responseBuffer.c_str());

    lastOk = true;
    if (responseBuffer.empty()) return nlohmann::json::array();

    try {
        return nlohmann::json::parse(responseBuffer);
    } catch (...) {
        TraceLog(LOG_WARNING, "SUPABASE: resposta não é JSON válido: %s", responseBuffer.c_str());
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

