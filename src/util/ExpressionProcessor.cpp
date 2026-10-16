/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"
#include <vector>
#include <cstring>
#include <cctype>
#include <algorithm>

#include <list>
#include <map>

#include "../log.h"
#include "ExpressionProcessor.h"

#include "tinyexpr.h"

// tinyexpr's own tokenizer (next_token() in tinyexpr.c) only starts an
// identifier on a lowercase a-z (not '_', not uppercase - see the
// `s->next[0] >= 'a' && s->next[0] <= 'z'` check), then allows a-z/0-9/_.
// Not true of names like "mqtt-some/topic" (hyphens, slashes), nor of an
// uppercase-led User Variable name. The
// "%%name%%" substitution form below is a plain map lookup and never cares
// about any of this; only the tinyexpr ("=..."/"==...==") math forms do.
static bool isValidExprIdentifier(const std::string& name) {
    if (name.empty() || name[0] < 'a' || name[0] > 'z') {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

// A character that continues a tinyexpr token - a name or a number - so a
// variable name with one of these next to it is only part of a token.
static bool isExprWordChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
}

// A variable named "2" or "1.5" can't be told apart from the number it
// spells, so aliasing it would hijack that literal in every expression. Such a
// variable is still reachable as "%%2%%".
static bool isNumericLiteral(const std::string& name) {
    // Digit- or point-led only: strtod() also takes "Inf", "NaN" and leading
    // blanks, and those names still need their alias.
    if (name.empty() || !((name[0] >= '0' && name[0] <= '9') || name[0] == '.')) {
        return false;
    }
    const char* start = name.c_str();
    char* end = nullptr;
    strtod(start, &end);
    return end != start && *end == 0;
}

ExpressionProcessor::ExpressionVariable::ExpressionVariable(const std::string& n) :
    name(n) {
}
ExpressionProcessor::ExpressionVariable::~ExpressionVariable() {
}

void ExpressionProcessor::ExpressionVariable::setValue(const std::string& s) {
    sValue = s;
    try {
        dValue = std::stod(s);
    } catch (...) {
        dValue = 0;
    }
}

class EvalStep {
public:
    EvalStep() {}
    virtual ~EvalStep() {}

    virtual std::string eval() = 0;
};

class TinyExprEvalStep : public EvalStep {
public:
    // tinyexpr recurses once per nesting level when it parses ("((((1))))",
    // "abs abs abs 1"), and again per tree level when it evaluates and frees
    // - so even a flat "1+1+1+..." becomes a left-deep tree as deep as it is
    // long. With no limit, a ~60KB expression in a Set Variable or If command
    // ran the calling thread out of stack. Every level costs at least one
    // character, so capping the length caps all three; real expressions are
    // a few dozen characters.
    static constexpr size_t MAX_EXPRESSION_LENGTH = 4096;

    TinyExprEvalStep(const std::string& s, std::vector<te_variable>& exprVars) {
        if (s.size() > MAX_EXPRESSION_LENGTH) {
            LogWarn(VB_GENERAL, "Expression is %d characters long, the limit is %d - not evaluating it\n",
                    (int)s.size(), (int)MAX_EXPRESSION_LENGTH);
            return;
        }
        int err = 0;
        expr = te_compile(s.c_str(), exprVars.data(), exprVars.size(), &err);
    }
    ~TinyExprEvalStep() {
        if (expr) {
            te_free(expr);
        }
    }

    bool ok() const { return expr != nullptr; }

    virtual std::string eval() override {
        if (expr) {
            double d = te_eval(expr);
            // "%lf" of a large value is long - 1e30 prints 31 digits before
            // the point, and DBL_MAX over 300 - so a fixed buffer truncated
            // it, and the zero-stripping below then returned a number with
            // the wrong magnitude. Size the buffer to what snprintf needs.
            int need = snprintf(nullptr, 0, "%lf", d);
            if (need <= 0) {
                return "0";
            }
            std::string buf(need + 1, '\0');
            snprintf(buf.data(), buf.size(), "%lf", d);
            buf.resize(need);
            for (int x = need - 1; x >= 0; x--) {
                if (buf[x] == '.' || buf[x] == ',') {
                    buf.resize(x);
                    return buf;
                }
                if (buf[x] != '0') {
                    return buf;
                }
                buf.resize(x);
            }
            // if it gets here, everything is 0
            return "0";
        }
        return "";
    }

    te_expr* expr = nullptr;
};
class TextEvalStep : public EvalStep {
public:
    TextEvalStep(const std::string& s) :
        str(s) {}
    ~TextEvalStep() {}

    virtual std::string eval() override {
        return str;
    }

    std::string str;
};
class VariableEvalStep : public EvalStep {
public:
    VariableEvalStep(ExpressionProcessor::ExpressionVariable* v) :
        var(v) {}
    ~VariableEvalStep() {}

    virtual std::string eval() override {
        if (var) {
            return var->getValue();
        }
        return "";
    }

    ExpressionProcessor::ExpressionVariable* var;
};

class ExpressionProcessorData {
public:
    ExpressionProcessorData() {}
    ~ExpressionProcessorData() {
        clear();
    }

    // Steps first: a compiled tinyexpr tree only holds the variables'
    // addresses, never the names, but nothing should outlive what it was
    // compiled against.
    void clear() {
        for (auto a : steps) {
            delete a;
        }
        steps.clear();
        for (auto& a : exprVars) {
            free((void*)a.name);
        }
        exprVars.clear();
    }

    void bind(ExpressionProcessor::ExpressionVariable* var) {
        variables[var->getName()] = var;
    }
    bool compile(const std::string& s) {
        // A second compile() replaces the first rather than appending to
        // its steps and leaking its strdup'd names.
        clear();

        // The "==...=="/"%%...%%" scan below re-copies the rest of the
        // string at every marker, so it is quadratic: a 1MB input took over
        // a second before the per-expression cap in TinyExprEvalStep was
        // ever reached. Nothing legitimate is this long.
        if (s.size() > TinyExprEvalStep::MAX_EXPRESSION_LENGTH) {
            LogWarn(VB_GENERAL, "Expression is %d characters long, the limit is %d - not evaluating it\n",
                    (int)s.size(), (int)TinyExprEvalStep::MAX_EXPRESSION_LENGTH);
            return false;
        }

        exprVars.resize(variables.size());
        // Names that aren't valid tinyexpr identifiers get a safe alias for
        // tinyexpr's benefit only - the real name is still what "%%name%%"
        // and bind()/variables map use.
        std::map<std::string, std::string> aliasForName;
        int cur = 0;
        for (auto& a : variables) {
            const std::string& name = a.second->getName();
            std::string boundName = name;
            if (!isValidExprIdentifier(name) && !isNumericLiteral(name)) {
                // Must itself satisfy isValidExprIdentifier() - lowercase-led.
                boundName = "ev" + std::to_string(cur);
                aliasForName[name] = boundName;
            }
            exprVars[cur].name = strdup(boundName.c_str());
            exprVars[cur].address = &a.second->dValue;
            exprVars[cur].type = TE_VARIABLE;
            cur++;
        }

        // Longest-name-first so aliasing one name can't be clobbered by a
        // shorter name that happens to be a substring of it.
        std::vector<std::string> aliasedNames;
        aliasedNames.reserve(aliasForName.size());
        for (auto& kv : aliasForName) {
            aliasedNames.push_back(kv.first);
        }
        std::sort(aliasedNames.begin(), aliasedNames.end(),
                  [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
        // Only a whole token is replaced: a match with a word character
        // right before or after it is part of a longer name or number. A
        // plain find() turned "12" into "1ev0" for a variable named "2", and
        // could rewrite the digits of an alias already substituted.
        auto aliasExpr = [&](std::string expr) {
            for (auto& name : aliasedNames) {
                const std::string& alias = aliasForName[name];
                std::size_t pos = 0;
                while ((pos = expr.find(name, pos)) != std::string::npos) {
                    std::size_t end = pos + name.size();
                    if ((pos > 0 && isExprWordChar(expr[pos - 1])) ||
                        (end < expr.size() && isExprWordChar(expr[end]))) {
                        pos++;
                        continue;
                    }
                    expr.replace(pos, name.size(), alias);
                    pos += alias.size();
                }
            }
            return expr;
        };

        bool success = true;

        if (s.size() > 1 && s[0] == '=' && s[1] != '=') {
            // simple math expression
            auto step = new TinyExprEvalStep(aliasExpr(s.substr(1)), exprVars);
            success = step->ok();
            steps.push_back(step);
        } else {
            std::string cur = s;
            for (int x = 1; x < cur.size(); x++) {
                if (cur[x - 1] == cur[x]) {
                    if (cur[x] == '=' || cur[x] == '%') {
                        for (int y = x + 2; y < cur.size(); y++) {
                            if (cur[y - 1] == cur[y] && cur[y] == cur[x]) {
                                int key = cur[x];
                                std::string start = cur.substr(0, x - 1);
                                if (start.size() > 0) {
                                    steps.push_back(new TextEvalStep(start));
                                }
                                std::string expr = cur.substr(x + 1, y - x - 2);
                                if (key == '%') {
                                    // find(), not operator[]: that inserted a
                                    // null variable, which the next compile()
                                    // dereferenced when binding.
                                    auto it = variables.find(expr);
                                    if (it == variables.end()) {
                                        success = false;
                                    }
                                    steps.push_back(new VariableEvalStep(it == variables.end() ? nullptr : it->second));
                                } else if (key == '=') {
                                    auto step = new TinyExprEvalStep(aliasExpr(expr), exprVars);
                                    if (!step->ok()) {
                                        success = false;
                                    }
                                    steps.push_back(step);
                                } else {
                                    steps.push_back(new TextEvalStep(expr));
                                }
                                cur = cur.substr(y + 1);
                                x = 0;
                                y = cur.size();
                            }
                        }
                    }
                }
            }
            if (cur.size()) {
                steps.push_back(new TextEvalStep(cur));
            }
        }
        return success;
    }

    std::string evaluate(const std::string& type) {
        std::string s;
        for (auto step : steps) {
            s += step->eval();
        }
        return s;
    }

    std::map<std::string, ExpressionProcessor::ExpressionVariable*> variables;
    std::vector<te_variable> exprVars;
    std::list<EvalStep*> steps;
};

ExpressionProcessor::ExpressionProcessor() {
    data = new ExpressionProcessorData();
}
ExpressionProcessor::~ExpressionProcessor() {
    if (data) {
        delete data;
    }
}

void ExpressionProcessor::bindVariable(ExpressionVariable* var) {
    data->bind(var);
}

bool ExpressionProcessor::compile(const std::string& s) {
    return data->compile(s);
}

std::string ExpressionProcessor::evaluate(const std::string& type) {
    return data->evaluate(type);
}
