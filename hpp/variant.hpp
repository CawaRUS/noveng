#pragma once

#include <string>
#include <variant>
#include <stdexcept>
#include "json.hpp"

using json = nlohmann::json;

// Variant type supporting the four basic scripting types.
using Variant = std::variant<int, float, bool, std::string>;

enum class VariantType { Int, Float, Bool, String };

inline VariantType variantType(const Variant& v) {
    return std::visit([](auto&& arg) -> VariantType {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return VariantType::Int;
        else if constexpr (std::is_same_v<T, float>) return VariantType::Float;
        else if constexpr (std::is_same_v<T, bool>) return VariantType::Bool;
        else return VariantType::String;
    }, v);
}

inline std::string variantTypeName(VariantType type) {
    switch (type) {
        case VariantType::Int: return "int";
        case VariantType::Float: return "float";
        case VariantType::Bool: return "bool";
        case VariantType::String: return "string";
    }
    return "unknown";
}

inline std::string variantToString(const Variant& v) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return std::to_string(arg);
        else if constexpr (std::is_same_v<T, float>) {
            std::string s = std::to_string(arg);
            // Trim trailing zeros and possible trailing dot
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s.empty() ? "0" : s;
        }
        else if constexpr (std::is_same_v<T, bool>) return arg ? "true" : "false";
        else return arg;
    }, v);
}

// Parse a value string into a Variant based on explicit type.
inline Variant parseVariant(const std::string& value, const std::string& typeName) {
    std::string t = typeName;
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (t == "int") {
        return std::stoi(value);
    } else if (t == "float") {
        return std::stof(value);
    } else if (t == "bool") {
        std::string v = value;
        for (char& c : v) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return (v == "true" || v == "1" || v == "yes");
    } else if (t == "string") {
        return value;
    }
    throw std::runtime_error("Unknown variant type: " + typeName);
}

// Auto-detect type: bool -> float -> int -> string.
inline Variant autoDetectVariant(const std::string& value) {
    std::string lower = value;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lower == "true" || lower == "false") {
        return lower == "true";
    }

    try {
        size_t pos = 0;
        int iv = std::stoi(value, &pos);
        if (pos == value.size()) return iv;
    } catch (...) {}

    try {
        size_t pos = 0;
        float fv = std::stof(value, &pos);
        if (pos == value.size()) return fv;
    } catch (...) {}

    return value;
}

// nlohmann/json serialization for Variant: store as object {"type": "...", "value": ...}
namespace nlohmann {
    template <>
    struct adl_serializer<Variant> {
        static void to_json(::nlohmann::json& j, const Variant& v) {
            VariantType type = variantType(v);
            j["type"] = variantTypeName(type);
            std::visit([&j](auto&& arg) {
                j["value"] = arg;
            }, v);
        }

        static void from_json(const ::nlohmann::json& j, Variant& v) {
            if (!j.contains("type") || !j.contains("value")) {
                // Legacy support: treat as int
                v = j.get<int>();
                return;
            }

            std::string type = j["type"];
            for (char& c : type) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

            if (type == "int") v = j["value"].get<int>();
            else if (type == "float") v = j["value"].get<float>();
            else if (type == "bool") v = j["value"].get<bool>();
            else if (type == "string") v = j["value"].get<std::string>();
            else throw std::runtime_error("Unknown variant type in JSON: " + type);
        }
    };
}
