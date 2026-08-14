#include "expression.hpp"
#include "common.hpp"
#include "logger.hpp"
#include <cctype>
#include <cmath>

bool variantToBool(const Variant& v) {
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) return arg;
        else if constexpr (std::is_same_v<T, int>) return arg != 0;
        else if constexpr (std::is_same_v<T, float>) return arg != 0.0f;
        else return !arg.empty();
    }, v);
}

int variantToInt(const Variant& v) {
    return std::visit([](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return arg;
        else if constexpr (std::is_same_v<T, bool>) return arg ? 1 : 0;
        else if constexpr (std::is_same_v<T, float>) return static_cast<int>(arg);
        else {
            try { return std::stoi(arg); } catch (...) { return 0; }
        }
    }, v);
}

float variantToFloat(const Variant& v) {
    return std::visit([](auto&& arg) -> float {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, float>) return arg;
        else if constexpr (std::is_same_v<T, int>) return static_cast<float>(arg);
        else if constexpr (std::is_same_v<T, bool>) return arg ? 1.0f : 0.0f;
        else {
            try { return std::stof(arg); } catch (...) { return 0.0f; }
        }
    }, v);
}

ExpressionEvaluator::ExpressionEvaluator(const std::map<std::string, Variant>& vars)
    : variables(vars) {}

Variant ExpressionEvaluator::evaluate(const std::string& expression) {
    expr = expression;
    pos = 0;
    skipWhitespace();
    Variant result = parseExpression();
    skipWhitespace();
    if (pos != expr.size()) {
        throw std::runtime_error("Unexpected token at position " + std::to_string(pos) + " in: " + expression);
    }
    return result;
}

bool ExpressionEvaluator::evaluateBool(const std::string& expression) {
    return variantToBool(evaluate(expression));
}

Variant ExpressionEvaluator::evaluateMath(const std::string& expression) {
    return evaluate(expression);
}

void ExpressionEvaluator::skipWhitespace() {
    while (pos < expr.size() && std::isspace(static_cast<unsigned char>(expr[pos]))) {
        pos++;
    }
}

bool ExpressionEvaluator::match(const std::string& op) {
    skipWhitespace();
    if (expr.compare(pos, op.size(), op) == 0) {
        pos += op.size();
        return true;
    }
    return false;
}

bool ExpressionEvaluator::startsWith(const std::string& s) {
    skipWhitespace();
    return expr.compare(pos, s.size(), s) == 0;
}

Variant ExpressionEvaluator::parseExpression() {
    Variant left = parseConjunction();
    while (true) {
        if (match("||")) {
            bool l = variantToBool(left);
            Variant right = parseConjunction();
            left = (l || variantToBool(right));
        } else {
            break;
        }
    }
    return left;
}

Variant ExpressionEvaluator::parseConjunction() {
    Variant left = parseEquality();
    while (true) {
        if (match("&&")) {
            bool l = variantToBool(left);
            Variant right = parseEquality();
            left = (l && variantToBool(right));
        } else {
            break;
        }
    }
    return left;
}

namespace {
    bool compareEqualImpl(const Variant& a, const Variant& b) {
        // Same type direct comparison
        if (a.index() == b.index()) {
            return std::visit([](auto&& arg1, auto&& arg2) -> bool {
                using T1 = std::decay_t<decltype(arg1)>;
                using T2 = std::decay_t<decltype(arg2)>;
                if constexpr (std::is_same_v<T1, T2>) {
                    return arg1 == arg2;
                }
                return false;
            }, a, b);
        }
        // Numeric cross-type comparison
        if ((std::holds_alternative<int>(a) || std::holds_alternative<float>(a)) &&
            (std::holds_alternative<int>(b) || std::holds_alternative<float>(b))) {
            return variantToFloat(a) == variantToFloat(b);
        }
        return false;
    }
}

Variant ExpressionEvaluator::parseEquality() {
    Variant left = parseComparison();
    while (true) {
        if (match("==")) {
            Variant right = parseComparison();
            left = compareEqualImpl(left, right);
        } else if (match("!=")) {
            Variant right = parseComparison();
            left = !compareEqualImpl(left, right);
        } else {
            break;
        }
    }
    return left;
}

Variant ExpressionEvaluator::parseComparison() {
    Variant left = parseAdditive();
    while (true) {
        if (match(">=")) {
            Variant right = parseAdditive();
            left = variantToFloat(left) >= variantToFloat(right);
        } else if (match("<=")) {
            Variant right = parseAdditive();
            left = variantToFloat(left) <= variantToFloat(right);
        } else if (match(">")) {
            Variant right = parseAdditive();
            left = variantToFloat(left) > variantToFloat(right);
        } else if (match("<")) {
            Variant right = parseAdditive();
            left = variantToFloat(left) < variantToFloat(right);
        } else {
            break;
        }
    }
    return left;
}

Variant ExpressionEvaluator::parseAdditive() {
    Variant left = parseMultiplicative();
    while (true) {
        if (match("+")) {
            Variant right = parseMultiplicative();
            if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
                left = variantToString(left) + variantToString(right);
            } else {
                float result = variantToFloat(left) + variantToFloat(right);
                if (std::holds_alternative<float>(left) || std::holds_alternative<float>(right)) {
                    left = result;
                } else {
                    left = static_cast<int>(result);
                }
            }
        } else if (match("-")) {
            Variant right = parseMultiplicative();
            float result = variantToFloat(left) - variantToFloat(right);
            if (std::holds_alternative<float>(left) || std::holds_alternative<float>(right)) {
                left = result;
            } else {
                left = static_cast<int>(result);
            }
        } else {
            break;
        }
    }
    return left;
}

Variant ExpressionEvaluator::parseMultiplicative() {
    Variant left = parseUnary();
    while (true) {
        if (match("*")) {
            Variant right = parseUnary();
            float result = variantToFloat(left) * variantToFloat(right);
            if (std::holds_alternative<float>(left) || std::holds_alternative<float>(right)) {
                left = result;
            } else {
                left = static_cast<int>(result);
            }
        } else if (match("/")) {
            Variant right = parseUnary();
            float divisor = variantToFloat(right);
            if (divisor == 0.0f) throw std::runtime_error("Division by zero");
            float result = variantToFloat(left) / divisor;
            if (std::holds_alternative<float>(left) || std::holds_alternative<float>(right)) {
                left = result;
            } else {
                left = static_cast<int>(result);
            }
        } else if (match("%")) {
            Variant right = parseUnary();
            int divisor = variantToInt(right);
            if (divisor == 0) throw std::runtime_error("Modulo by zero");
            left = variantToInt(left) % divisor;
        } else {
            break;
        }
    }
    return left;
}

Variant ExpressionEvaluator::parseUnary() {
    if (match("!")) {
        Variant operand = parseUnary();
        return !variantToBool(operand);
    }
    if (match("-")) {
        Variant operand = parseUnary();
        if (std::holds_alternative<float>(operand)) return -std::get<float>(operand);
        return -variantToInt(operand);
    }
    return parsePrimary();
}

Variant ExpressionEvaluator::parsePrimary() {
    skipWhitespace();
    if (pos >= expr.size()) {
        throw std::runtime_error("Unexpected end of expression");
    }

    // Parenthesized expression
    if (match("(")) {
        Variant result = parseExpression();
        if (!match(")")) {
            throw std::runtime_error("Expected ')'");
        }
        return result;
    }

    // String literal
    if (expr[pos] == '"') {
        pos++;
        std::string value;
        while (pos < expr.size() && expr[pos] != '"') {
            if (expr[pos] == '\\' && pos + 1 < expr.size()) {
                pos++;
            }
            value += expr[pos];
            pos++;
        }
        if (pos >= expr.size() || expr[pos] != '"') {
            throw std::runtime_error("Unterminated string literal");
        }
        pos++; // skip closing quote
        return value;
    }

    // Variable $name$
    if (expr[pos] == '$') {
        pos++;
        size_t end = expr.find('$', pos);
        if (end == std::string::npos) {
            throw std::runtime_error("Unclosed variable reference");
        }
        std::string name = expr.substr(pos, end - pos);
        pos = end + 1;
        if (!isValidIdentifier(name)) {
            Logger::getInstance().warn("Invalid variable name in expression: " + name);
            return 0;
        }
        return resolveVariable(name);
    }

    // Identifier / boolean literal / bare variable
    if (std::isalpha(static_cast<unsigned char>(expr[pos])) || expr[pos] == '_') {
        size_t start = pos;
        while (pos < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[pos])) || expr[pos] == '_')) {
            pos++;
        }
        std::string name = expr.substr(start, pos - start);
        if (name == "true") return true;
        if (name == "false") return false;
        if (!isValidIdentifier(name)) {
            Logger::getInstance().warn("Invalid variable name in expression: " + name);
            return 0;
        }
        return resolveVariable(name);
    }

    // Number
    if (std::isdigit(static_cast<unsigned char>(expr[pos])) || expr[pos] == '.') {
        size_t start = pos;
        bool isFloat = false;
        while (pos < expr.size() && (std::isdigit(static_cast<unsigned char>(expr[pos])) || expr[pos] == '.')) {
            if (expr[pos] == '.') isFloat = true;
            pos++;
        }
        std::string numStr = expr.substr(start, pos - start);
        if (isFloat) {
            return std::stof(numStr);
        } else {
            return std::stoi(numStr);
        }
    }

    throw std::runtime_error("Unexpected character in expression: " + std::string(1, expr[pos]));
}

Variant ExpressionEvaluator::resolveVariable(const std::string& name) {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    // Undefined variables default to 0/false/empty based on context.
    Logger::getInstance().warn("Undefined variable in expression: " + name + ", defaulting to 0");
    return 0;
}
