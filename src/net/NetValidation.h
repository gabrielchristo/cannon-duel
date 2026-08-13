#pragma once

#include <string>

namespace net_validation {

// Valida UUID v4 gerado localmente (PlayerIdentity) antes de usar em filtros PostgREST.
bool IsValidPlayerUuid(const std::string& id);

// Nome exibido no lobby: remove controles/perigosos, limita tamanho.
// Retorna vazio se nada sobrar após sanitização.
std::string SanitizeDisplayName(const std::string& raw);

} // namespace net_validation
