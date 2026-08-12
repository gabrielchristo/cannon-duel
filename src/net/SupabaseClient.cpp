#include "SupabaseClient.h"


#include "SupabaseConfig.h"
#include <curl/curl.h>
#include <cstring>

namespace {
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* out = static_cast<std::string*>(userp);
    out->append(static_cast<char*>(contents), size * nmemb);
    return size * nmemb;
}
} // namespace

nlohmann::json SupabaseClient::Request(const std::string& method, const std::string& urlSuffix,
                                        const nlohmann::json* body, const char* preferHeader) {
    lastOk = false;
    CURL* curl = curl_easy_init();
    if (!curl) return nlohmann::json();

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

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode < 200 || httpCode >= 300) {
        return nlohmann::json();
    }

    lastOk = true;
    if (responseBuffer.empty()) return nlohmann::json::array();

    try {
        return nlohmann::json::parse(responseBuffer);
    } catch (...) {
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

nlohmann::json SupabaseClient::Upsert(const std::string& table, const nlohmann::json& body) {
    return Request("POST", table, &body, "Prefer: resolution=merge-duplicates,return=representation");
}

void SupabaseClient::Delete(const std::string& table, const std::string& filter) {
    Request("DELETE", table + "?" + filter, nullptr, nullptr);
}

