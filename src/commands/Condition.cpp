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

#include "fpp-json.h"

#include "Condition.h"
#include "../SunRise.h"
#include "../Variables.h"
#include "../gpio.h"
#include "../log.h"
#include "../mqtt.h"
#include "../sensors/Sensors.h"
#include "../settings.h"
#include "../util/ExpressionProcessor.h"
#include "../util/GPIOUtils.h"

#include <cmath>
#include <ctime>
#include <map>

class GroupNode : public ConditionNode {
public:
    bool isAnd = true;
    bool negate = false;
    std::vector<std::unique_ptr<ConditionNode>> children;

    bool evaluate() const override {
        bool result = isAnd; // AND starts true (short-circuits on first false)
        int evaluatedCount = 0;
        for (auto& c : children) {
            bool v = c->evaluate();
            evaluatedCount++;
            if (isAnd) {
                result = result && v;
                if (!result) {
                    break;
                }
            } else {
                result = result || v;
                if (result) {
                    break;
                }
            }
        }
        bool finalResult = negate ? !result : result;
        // evaluatedCount vs children.size() shows short-circuiting in
        // action - e.g. an AND group logging 2 of 5 tells you child #3 was
        // the one that failed and the rest were never even evaluated.
        LogDebug(VB_COMMAND, "If: Group(%s%s) evaluated %d/%zu children, raw=%s -> %s\n",
                 isAnd ? "AND" : "OR", negate ? " NOT" : "", evaluatedCount, children.size(),
                 result ? "true" : "false", finalResult ? "true" : "false");
        return finalResult;
    }
};

static std::string EvaluateConditionExpression(const std::string& expr) {
    ExpressionProcessor proc;
    std::map<std::string, ExpressionProcessor::ExpressionVariable> boundVars;
    // getAllVariableNames() (User + read-only fpp-/mqtt- vars), matching what
    // GET /api/variables?validateExpression binds (Variables.cpp) - otherwise
    // an expression referencing an fpp-/mqtt- variable validates as green in
    // the editor but silently evaluates as unbound at runtime.
    for (auto const& varName : Variables::INSTANCE.getAllVariableNames()) {
        auto inserted = boundVars.try_emplace(varName, varName);
        inserted.first->second.setValue(Variables::INSTANCE.getVariable(varName));
        proc.bindVariable(&inserted.first->second);
    }
    if (!proc.compile(expr)) {
        // Debug, not Warn - a broken Expression source is re-evaluated on
        // every check of an If wired to a frame/GPIO trigger, so a Warn
        // here would spam the log continuously instead of once.
        LogDebug(VB_COMMAND, "If: Expression source \"%s\" failed to compile\n", TruncateForLog(expr).c_str());
    }
    return proc.evaluate("string");
}

static std::string CurrentTimeHHMM() {
    std::time_t t = std::time(nullptr);
    struct tm local;
    localtime_r(&t, &local);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", local.tm_hour, local.tm_min);
    return buf;
}

// Same Latitude/Longitude settings + fallback coordinates + SunRise usage as
// ScheduleEntry::GetTimeFromSun (ScheduleEntry.cpp:705-741).
static std::string SunTimeHHMM(const std::string& which) {
    std::string latStr = getSetting("Latitude");
    std::string lonStr = getSetting("Longitude");
    double lat = 38.938524, lon = -104.600945;
    if (!latStr.empty() && !lonStr.empty()) {
        try {
            lat = std::stod(latStr);
            lon = std::stod(lonStr);
        } catch (...) {
        }
    }
    SunRise sr;
    time_t now = std::time(nullptr);
    sr.calculate(lat, lon, now);
    time_t t = (which == "Sunrise") ? sr.riseTime : sr.setTime;
    struct tm local;
    localtime_r(&t, &local);
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", local.tm_hour, local.tm_min);
    return buf;
}

// Shared by LeafNode::readSource() (evaluate() path) and
// ConditionNode::PreviewSourceValue() (the If editor's "Show Current Value"
// preview button) - a single source of truth for what each Check source
// actually reads.
static std::string ReadConditionSourceValue(const std::string& source, const std::string& name, bool& found) {
    if (source == "Variable") {
        return Variables::INSTANCE.getVariable(name, "");
    } else if (source == "Expression") {
        return EvaluateConditionExpression(name);
    } else if (source == "Setting") {
        return getSetting(name.c_str());
    } else if (source == "Time") {
        return CurrentTimeHHMM();
    } else if (source == "MQTT") {
        std::string topic = name;
        std::string message;
        if (!mqtt || !mqtt->CacheCheckMessage(topic, message)) {
            found = false;
            return "";
        }
        return message;
    } else if (source == "GPIO Pin") {
        // fppCommandLastValue only tracks pins explicitly set via the "GPIO"
        // command/API (an output) - it knows nothing about a configured
        // GPIO Input's actual live state. Check that too so this source
        // works for testing a real input, not just a previously-commanded
        // output.
        auto it = GPIOManager::INSTANCE.fppCommandLastValue.find(name);
        if (it != GPIOManager::INSTANCE.fppCommandLastValue.end()) {
            return it->second ? "1" : "0";
        }
        int inputValue = 0;
        if (GPIOManager::INSTANCE.GetInputPinValue(name, inputValue)) {
            return inputValue ? "1" : "0";
        }
        found = false;
        return "";
    } else if (source == "Sensor") {
        Json::Value report;
        Sensors::INSTANCE.reportSensors(report);
        for (auto& s : report["sensors"]) {
            if (s["label"].asString() == name) {
                return s["value"].asString();
            }
        }
        found = false;
        return "";
    } else if (source == "Sun") {
        return SunTimeHHMM(name); // name: "Sunrise" or "Sunset"
    }
    found = false;
    return "";
}

std::string ConditionNode::PreviewSourceValue(const std::string& source, const std::string& name, bool& found) {
    found = true;
    return ReadConditionSourceValue(source, name, found);
}

class LeafNode : public ConditionNode {
public:
    std::string source;
    std::string name;
    std::string comparatorStr;
    std::string value;
    bool negate = false;

    bool evaluate() const override {
        bool found = true;
        std::string lhs = ReadConditionSourceValue(source, name, found);
        if (!found) {
            bool result = negate; // "not found" reads as false, unless negated
            LogDebug(VB_COMMAND, "If: Leaf[%s:%s] not found -> %s\n", source.c_str(), name.c_str(),
                     result ? "true" : "false");
            return result;
        }
        bool cmp = compare(lhs, value);
        bool result = negate ? !cmp : cmp;
        // lhs/value are unbounded (e.g. a Variable can hold an entire fetched
        // web page) - truncate before logging, see TruncateForLog() (log.h).
        LogDebug(VB_COMMAND, "If: Leaf[%s:%s] = \"%s\" %s%s \"%s\" -> %s\n", source.c_str(), name.c_str(),
                 TruncateForLog(lhs).c_str(), negate ? "NOT " : "", comparatorStr.c_str(),
                 TruncateForLog(value).c_str(), result ? "true" : "false");
        return result;
    }

private:
    bool compare(const std::string& lhs, const std::string& rhs) const {
        if (comparatorStr == "equal to") {
            return lhs == rhs;
        }
        if (comparatorStr == "not equal to") {
            return lhs != rhs;
        }
        if (comparatorStr == "contains") {
            return lhs.find(rhs) != std::string::npos;
        }
        if (comparatorStr == "between") {
            auto commaPos = rhs.find(',');
            if (commaPos == std::string::npos) {
                return false;
            }
            try {
                double mn = std::stod(rhs.substr(0, commaPos));
                double mx = std::stod(rhs.substr(commaPos + 1));
                double v = std::stod(lhs);
                if (mx < mn) {
                    std::swap(mn, mx);
                }
                return v >= mn && v <= mx;
            } catch (...) {
                return false;
            }
        }
        // Numeric comparators - HH:MM strings also compare correctly here
        // since std::stod will fail on them; fall back to string compare
        // for the Time/Sun sources' "HH:MM" lhs/rhs specifically.
        try {
            double a = std::stod(lhs);
            double b = std::stod(rhs);
            if (comparatorStr == "greater than") {
                return a > b;
            }
            if (comparatorStr == "greater or equal") {
                return a >= b;
            }
            if (comparatorStr == "less than") {
                return a < b;
            }
            if (comparatorStr == "less or equal") {
                return a <= b;
            }
        } catch (...) {
            if (comparatorStr == "greater than") {
                return lhs > rhs;
            }
            if (comparatorStr == "greater or equal") {
                return lhs >= rhs;
            }
            if (comparatorStr == "less than") {
                return lhs < rhs;
            }
            if (comparatorStr == "less or equal") {
                return lhs <= rhs;
            }
        }
        return false;
    }
};

std::unique_ptr<ConditionNode> ConditionNode::FromJSON(const Json::Value& j) {
    if (!j.isObject()) {
        return nullptr;
    }
    if (j.isMember("op")) {
        auto group = std::make_unique<GroupNode>();
        group->isAnd = j["op"].asString() != "OR";
        group->negate = j.get("not", false).asBool();
        if (j.isMember("conditions") && j["conditions"].isArray()) {
            for (auto& c : j["conditions"]) {
                auto child = FromJSON(c);
                if (child) {
                    group->children.push_back(std::move(child));
                } else {
                    // Silently dropping a malformed child would otherwise look
                    // identical to the user's tree just having fewer
                    // conditions than they configured - log it so a bad edit
                    // (e.g. a hand-edited preset) is diagnosable.
                    LogWarn(VB_COMMAND, "If: dropping malformed condition entry under %s group: %s\n",
                            group->isAnd ? "AND" : "OR", TruncateForLog(SaveJsonToString(c)).c_str());
                }
            }
        }
        return group;
    }
    if (j.isMember("source")) {
        auto leaf = std::make_unique<LeafNode>();
        leaf->source = j["source"].asString();
        leaf->name = j.get("name", "").asString();
        leaf->comparatorStr = j.get("comparator", "").asString();
        leaf->value = j.get("value", "").asString();
        leaf->negate = j.get("not", false).asBool();
        return leaf;
    }
    LogWarn(VB_COMMAND, "If: could not parse condition JSON (no \"op\" or \"source\" key): %s\n",
            TruncateForLog(SaveJsonToString(j)).c_str());
    return nullptr;
}
