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
    TinyExprEvalStep(const std::string& s, std::vector<te_variable>& exprVars) {
        int err = 0;
        expr = te_compile(s.c_str(), &exprVars[0], exprVars.size(), &err);
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
            char buf[30];
            snprintf(buf, sizeof(buf), "%lf", d);
            int len = strlen(buf);
            for (int x = len - 1; x >= 0; x--) {
                if (buf[x] == '.' || buf[x] == ',') {
                    buf[x] = 0;
                    return buf;
                }
                if (buf[x] != '0') {
                    return buf;
                }
                buf[x] = 0;
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
        for (auto a : steps) {
            delete a;
        }
        for (auto& a : exprVars) {
            free((void*)a.name);
        }
    }

    void bind(ExpressionProcessor::ExpressionVariable* var) {
        variables[var->getName()] = var;
    }
    bool compile(const std::string& s) {
        exprVars.resize(variables.size());
        // Names that aren't valid tinyexpr identifiers get a safe alias for
        // tinyexpr's benefit only - the real name is still what "%%name%%"
        // and bind()/variables map use.
        std::map<std::string, std::string> aliasForName;
        int cur = 0;
        for (auto& a : variables) {
            const std::string& name = a.second->getName();
            std::string boundName = name;
            if (!isValidExprIdentifier(name)) {
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
        auto aliasExpr = [&](std::string expr) {
            for (auto& name : aliasedNames) {
                const std::string& alias = aliasForName[name];
                std::size_t pos = 0;
                while ((pos = expr.find(name, pos)) != std::string::npos) {
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
                                    if (variables.find(expr) == variables.end()) {
                                        success = false;
                                    }
                                    steps.push_back(new VariableEvalStep(variables[expr]));
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
