#include "NetValidation.h"

#include <cctype>

namespace net_validation {

bool IsValidPlayerUuid(const std::string& id) {
    if (id.size() != 36) return false;
    for (size_t i = 0; i < id.size(); ++i) {
        char c = id[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-') return false;
        } else if (!std::isxdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    return true;
}

std::string SanitizeDisplayName(const std::string& raw) {
    std::string out;
    out.reserve(32);
    for (unsigned char c : raw) {
        if (c < 32 || c == 127) continue;
        if (c == '<' || c == '>' || c == '"' || c == '\\' || c == '&' || c == ';') continue;
        if (out.size() >= 24) break;
        out.push_back(static_cast<char>(c));
    }
    while (!out.empty() && out.front() == ' ') out.erase(out.begin());
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

} // namespace net_validation
