#pragma once

#include "variant.hpp"
#include <map>
#include <string>
#include <stdexcept>

// Evaluates a script expression using the engine's variables.
// Supports:
//   - Variables: $name$
//   - Numbers: 42, 3.14
//   - Strings: "hello" (only in comparisons and assignments, not arithmetic)
//   - Booleans: true, false
//   - Arithmetic: +, -, *, /, %, unary -, !
//   - Comparison: >, <, >=, <=, ==, !=
//   - Logical: &&, ||
//   - Parentheses for grouping.
class ExpressionEvaluator {
public:
    explicit ExpressionEvaluator(const std::map<std::string, Variant>& vars);

    // Evaluates the expression and returns a Variant.
    Variant evaluate(const std::string& expression);

    // Convenience helpers.
    bool evaluateBool(const std::string& expression);
    Variant evaluateMath(const std::string& expression);

private:
    const std::map<std::string, Variant>& variables;
    std::string expr;
    size_t pos = 0;

    void skipWhitespace();
    bool match(const std::string& op);
    bool startsWith(const std::string& s);

    Variant parseExpression();   // ||
    Variant parseConjunction();  // &&
    Variant parseEquality();     // ==, !=
    Variant parseComparison();   // >, <, >=, <=
    Variant parseAdditive();     // +, -
    Variant parseMultiplicative(); // *, /, %
    Variant parseUnary();        // !, unary -
    Variant parsePrimary();      // number, variable, string, bool, parentheses

    Variant resolveVariable(const std::string& name);
};

// Helper functions for type coercion.
bool variantToBool(const Variant& v);
int variantToInt(const Variant& v);
float variantToFloat(const Variant& v);
