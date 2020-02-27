/*
*   ExpressionProcessor - wrapper around tinyexpr
*
*   The Falcon Player (FPP) is free software; you can redistribute it
*   and/or modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2 of
*   the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "ExpressionProcessor.h"

#include <map>
#include <vector>
#include <list>
#include <string.h>

#include "tinyexpr.h"

ExpressionProcessor::ExpressionVariable::ExpressionVariable(const std::string &n) : name(n) {
}
ExpressionProcessor::ExpressionVariable::~ExpressionVariable() {
}

void ExpressionProcessor::ExpressionVariable::setValue(const std::string &s) {
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
    TinyExprEvalStep(const std::string &s, std::vector<te_variable> &exprVars) {
        int err = 0;
        expr = te_compile(s.c_str(), &exprVars[0], exprVars.size(), &err);
    }
    ~TinyExprEvalStep() {
        if (expr) {
            te_free(expr);
        }
    }
    
    virtual std::string eval() override {
        if (expr) {
            double d = te_eval(expr);
            char buf[30];
            sprintf(buf, "%lf", d);
            int len = strlen(buf);
            for (int x = len-1; x >= 0; x--) {
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

    
    te_expr *expr = nullptr;
};
class TextEvalStep : public EvalStep {
public:
    TextEvalStep(const std::string &s) : str(s) {}
    ~TextEvalStep() {}
    
    virtual std::string eval() override {
        return str;
    }

    std::string str;
};
class VariableEvalStep : public EvalStep {
public:
    VariableEvalStep(ExpressionProcessor::ExpressionVariable *v) : var(v) {}
    ~VariableEvalStep() {}
    
    virtual std::string eval() override {
        if (var) {
            return var->getValue();
        }
        return "";
    }

    ExpressionProcessor::ExpressionVariable *var;
};

class ExpressionProcessorData {
public:
    ExpressionProcessorData() {}
    ~ExpressionProcessorData() {
        for (auto a : steps) {
            delete a;
        }
        for (auto &a : exprVars) {
            free((void*)a.name);
        }
    }
    
    void bind(ExpressionProcessor::ExpressionVariable *var) {
        variables[var->getName()] = var;
    }
    bool compile(const std::string &s) {
        exprVars.resize(variables.size());
        int cur = 0;
        for (auto &a: variables) {
            exprVars[cur].name = strdup(a.second->getName().c_str());
            exprVars[cur].address = &a.second->dValue;
            exprVars[cur].type = TE_VARIABLE;
            cur++;
        }
        
        if (s.size() > 1
            && s[0] == '='
            && s[1] != '=') {
            //simple math expression
            steps.push_back(new TinyExprEvalStep(s.substr(1), exprVars));
        } else {
            std::string cur = s;
            for (int x = 1; x < cur.size(); x++) {
                if (cur[x-1] == cur[x]) {
                    if (cur[x] == '=' || cur[x] == '%') {
                        for (int y = x + 2; y < cur.size(); y++) {
                            if (cur[y-1] == cur[y] && cur[y] == cur[x]) {
                                int key = cur[x];
                                std::string start = cur.substr(0, x-1);
                                if (start.size() > 0) {
                                    steps.push_back(new TextEvalStep(start));
                                }
                                std::string expr = cur.substr(x+1, y - x - 2);
                                if (key == '%') {
                                    steps.push_back(new VariableEvalStep(variables[expr]));
                                } else if (key == '=') {
                                    steps.push_back(new TinyExprEvalStep(expr, exprVars));
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
        return false;
    }
    
    
    std::string evaluate(const std::string &type) {
        std::string s;
        for (auto step : steps) {
            s += step->eval();
        }
        return s;
    }

    std::map<std::string,ExpressionProcessor::ExpressionVariable *> variables;
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


void ExpressionProcessor::bindVariable(ExpressionVariable *var) {
    data->bind(var);
}

bool ExpressionProcessor::compile(const std::string &s) {
    return data->compile(s);
}

std::string ExpressionProcessor::evaluate(const std::string &type) {
    return data->evaluate(type);
}
