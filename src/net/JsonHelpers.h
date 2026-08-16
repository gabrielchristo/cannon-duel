#pragma once

#include <nlohmann/json.hpp>

#include <string>

namespace json_helpers {

inline int Int(const nlohmann::json& j, const char* key, int fallback = 0) {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    const auto& v = j[key];
    if (v.is_number_integer()) return v.get<int>();
    if (v.is_number_float()) return static_cast<int>(v.get<double>());
    if (v.is_string()) {
        try { return std::stoi(v.get<std::string>()); } catch (...) { return fallback; }
    }
    if (v.is_boolean()) return v.get<bool>() ? 1 : 0;
    return fallback;
}

inline float Float(const nlohmann::json& j, const char* key, float fallback = 0.0f) {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    const auto& v = j[key];
    if (v.is_number()) return static_cast<float>(v.get<double>());
    if (v.is_string()) {
        try { return std::stof(v.get<std::string>()); } catch (...) { return fallback; }
    }
    return fallback;
}

inline long long Int64(const nlohmann::json& j, const char* key, long long fallback = 0) {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    const auto& v = j[key];
    if (v.is_number_integer()) return v.get<long long>();
    if (v.is_number_float()) return static_cast<long long>(v.get<double>());
    return fallback;
}

inline std::string Str(const nlohmann::json& j, const char* key, const std::string& fallback = "") {
    if (!j.contains(key) || j[key].is_null()) return fallback;
    if (j[key].is_string()) return j[key].get<std::string>();
    return fallback;
}

} // namespace json_helpers
