#pragma once
#include "../Platform.h"


#include <string>
#include <nlohmann/json.hpp>

// Wrapper fino sobre a API REST automática do Supabase (PostgREST). Cada
// tabela do banco vira um endpoint HTTP; aqui só montamos as URLs/headers
// certos e (de)serializamos JSON. Sem nenhuma lógica de jogo aqui dentro —
// só transporte.
class SupabaseClient {
public:
    // GET /rest/v1/<table>?<query>   (ex.: "select=*&status=eq.idle")
    nlohmann::json Select(const std::string& table, const std::string& query = "select=*");

    // POST /rest/v1/<table>  — insere; com Prefer: return=representation,
    // retorna a(s) linha(s) criada(s).
    nlohmann::json Insert(const std::string& table, const nlohmann::json& body);

    // PATCH /rest/v1/<table>?<filter>   (ex.: "id=eq.<uuid>")
    nlohmann::json Update(const std::string& table, const std::string& filter, const nlohmann::json& body);

    // Upsert via header Prefer: resolution=merge-duplicates (precisa de uma
    // constraint UNIQUE/PK na coluna de conflito, que já configuramos no
    // schema.sql pra players/lobby_presence).
    nlohmann::json Upsert(const std::string& table, const nlohmann::json& body);

    // DELETE /rest/v1/<table>?<filter>
    void Delete(const std::string& table, const std::string& filter);

    // true se a última requisição terminou sem erro de transporte/HTTP.
    bool LastRequestOk() const { return lastOk; }

private:
    bool lastOk = true;

    nlohmann::json Request(const std::string& method, const std::string& urlSuffix,
                            const nlohmann::json* body, const char* preferHeader);
};

