/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2026 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "../Variables.h"
#include "../common.h"
#include "../log.h"
#include "../util/ExpressionProcessor.h"

#include "VariableCommands.h"

#include <cstdlib>
#include <map>

// std::to_string(double) always pads to 6 decimals ("45.000000"); use a
// format that drops trailing zeros so integer-valued results stay readable.
static std::string DoubleToCleanString(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    return std::string(buf);
}

SetVariableCommand::SetVariableCommand() :
    Command("Set Variable") {
    args.push_back(CommandArg("name", "string", "Variable Name")
                       .setHelp("The name this value is stored and looked up under - reference it "
                                "elsewhere as %VAR:name% (in any command's text fields) or by picking "
                                "it directly in an If command's Check."));
    args.push_back(CommandArg("type", "string", "Set To")
                       .setContentList({ "Value", "Increment", "Random", "Expression" })
                       .setDefaultValue("Value")
                       .setChildren({ { "Value", { "value" } },
                                      { "Increment", { "delta" } },
                                      { "Random", { "min", "max" } },
                                      { "Expression", { "expression" } } })
                       .setHelp("<ul class='mb-0 ps-3 text-start'>"
                                "<li><b>Value</b>: store the text/number below as-is.</li>"
                                "<li><b>Increment</b>: add Amount to the variable's current value (e.g. "
                                "a counter).</li>"
                                "<li><b>Random</b>: pick a whole number between Minimum and Maximum.</li>"
                                "<li><b>Expression</b>: compute a value from a formula, optionally "
                                "referencing other variables by name.</li>"
                                "</ul>"));
    args.push_back(CommandArg("value", "string", "Value")
                       .setHelp("The text or number to store."));
    args.push_back(CommandArg("delta", "string", "Amount")
                       .setDefaultValue("1")
                       .setHelp("Added to the variable's current value each time this command runs. Use "
                                "a negative number to count down instead of up."));
    args.push_back(CommandArg("min", "string", "Minimum").setDefaultValue("0")
                       .setHelp("Lowest whole number that can be picked."));
    args.push_back(CommandArg("max", "string", "Maximum").setDefaultValue("100")
                       .setHelp("Highest whole number that can be picked."));
    args.push_back(CommandArg("expression", "expression", "Expression")
                       .setHelp("A math formula to compute the new value, e.g. =2+3*4. Must start with "
                                "'=', and can reference other variables by name (e.g. =temp*1.8+32 to "
                                "convert temp to Fahrenheit)."));
    args.push_back(CommandArg("persist", "bool", "Persist", true)
                       .setDefaultValue("false")
                       .setHelp("If checked, this value is saved to disk and reloaded automatically "
                                "the next time FPP starts, so it keeps its value across a restart or "
                                "reboot. If unchecked (the default), the value only lives in memory and "
                                "resets to unset every time FPP restarts."));
}

std::unique_ptr<Command::Result> SetVariableCommand::run(const std::vector<std::string>& args) {
    if (args.empty() || args[0].empty()) {
        return std::make_unique<Command::ErrorResult>("Set Variable requires a Variable Name");
    }
    const std::string& name = args[0];
    std::string type = args.size() > 1 && !args[1].empty() ? args[1] : "Value";
    std::string value = args.size() > 2 ? args[2] : "";
    std::string deltaStr = args.size() > 3 ? args[3] : "1";
    std::string minStr = args.size() > 4 ? args[4] : "0";
    std::string maxStr = args.size() > 5 ? args[5] : "100";
    std::string expression = args.size() > 6 ? args[6] : "";
    bool persist = args.size() > 7 && (args[7] == "true" || args[7] == "1");

    if (type == "Increment") {
        double delta = 1.0;
        if (!deltaStr.empty()) {
            try {
                delta = std::stod(deltaStr);
            } catch (...) {
                return std::make_unique<Command::ErrorResult>("Invalid amount");
            }
        }
        double newValue = Variables::INSTANCE.incrementVariable(name, delta, persist);
        return std::make_unique<Command::Result>(DoubleToCleanString(newValue));
    } else if (type == "Random") {
        int mn = 0, mx = 100;
        try {
            mn = std::stoi(minStr.empty() ? "0" : minStr);
            mx = std::stoi(maxStr.empty() ? "100" : maxStr);
        } catch (...) {
            return std::make_unique<Command::ErrorResult>("Invalid min/max");
        }
        if (mx < mn) {
            std::swap(mn, mx);
        }
        int result = mn + (mx > mn ? (rand() % (mx - mn + 1)) : 0);
        Variables::INSTANCE.setVariable(name, result, persist);
        return std::make_unique<Command::Result>(std::to_string(result));
    } else if (type == "Expression") {
        ExpressionProcessor proc;
        // Bind every currently-known Variable by name - User Variables plus
        // the read-only fpp_/mqtt- ones - so the expression can reference
        // any of them directly (e.g. "=temp*1.8+32" or "=fpp_volume+10").
        // ExpressionVariable objects must stay alive and at a stable address
        // until evaluate() returns, since tinyexpr compiles against their
        // raw address (ExpressionProcessor.h) -- std::map is node-based, so
        // inserting more entries never invalidates already-bound pointers.
        std::map<std::string, ExpressionProcessor::ExpressionVariable> boundVars;
        for (auto const& varName : Variables::INSTANCE.getAllVariableNames()) {
            auto inserted = boundVars.try_emplace(varName, varName);
            inserted.first->second.setValue(Variables::INSTANCE.getVariable(varName));
            proc.bindVariable(&inserted.first->second);
        }
        if (!proc.compile(expression)) {
            LogWarn(VB_COMMAND, "Set Variable: expression failed to compile for \"%s\": %s\n",
                    name.c_str(), TruncateForLog(expression).c_str());
            return std::make_unique<Command::ErrorResult>("Invalid expression");
        }
        std::string result = proc.evaluate("string");
        Variables::INSTANCE.setVariable(name, result, persist);
        return std::make_unique<Command::Result>(result);
    }
    // "Value" (default/fallback)
    Variables::INSTANCE.setVariable(name, value, persist);
    return std::make_unique<Command::Result>("OK");
}
