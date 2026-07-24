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

// Does `text` contain a math/comparison operator character that ISN'T
// wholly explained by being part of a real variable name? Removes every
// real variable name found as a substring (longest-first, same rationale as
// ExpressionProcessor's own aliasExpr()) before checking what's left - so
// "mqtt-homeassistant/sensor/outside_temperature/state" alone (the whole variable
// name, hyphen/slashes and all) doesn't look like a formula, but
// "fpp_next_playlist_start+1" does (the "+1" left over after removing the
// real name is the actual signal).
// Deliberately conservative: plain non-matching literal text like "ON" or
// "5-star-review" that happens to contain '-' with no real variable name
// nearby will also be flagged - see EvaluateNameOrExpression's caller
// comment for why that's an accepted, documented trade-off rather than a
// silent bug.
static bool LooksLikeFormula(const std::string& text) {
    std::vector<std::string> names = Variables::INSTANCE.getAllVariableNames();
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return a.size() > b.size(); });
    std::string remaining = text;
    for (auto& n : names) {
        if (n.empty()) {
            continue;
        }
        size_t pos;
        while ((pos = remaining.find(n)) != std::string::npos) {
            remaining.erase(pos, n.size());
        }
    }
    return remaining.find_first_of("+-*/()<>=!^") != std::string::npos;
}

// The merged "Variable"/"Expression" behavior: a naive "does this contain
// math-operator characters" heuristic doesn't work as the PRIMARY signal
// here, because real variable names on this system already contain '-' and
// '/' - both of which are also math operators (subtract, divide).
// "fpp_next_playlist_start" and "mqtt-homeassistant/sensor/outside_temperature/state"
// are both completely ordinary variable names. The reliable primary signal
// is exact match against the live variable list; LooksLikeFormula() above is
// only a secondary check, applied after a whole-string exact match has
// already been ruled out:
//
//   1. Trimmed text exactly matches a real, currently-known variable name ->
//      plain literal lookup (Variables::getVariable), exactly the old
//      "Variable" Source behavior. Keeps arbitrary content (blobs, JSON,
//      anything) reaching the comparator completely unmangled - never
//      routed through the expression engine when the whole field is just
//      naming one real variable.
//   2. Text already starts with '=', or already contains a "%%...%%"/
//      "==...=="  marker -> hand to EvaluateConditionExpression() unchanged
//      (old "Expression" Source behavior, advanced/manual syntax keeps
//      working exactly as before).
//   3. LooksLikeFormula(text) -> prepend '=' and hand to
//      EvaluateConditionExpression(). Routes through the pure-math branch,
//      whose ExpressionProcessor::compile() already aliases any embedded
//      real variable name (see aliasExpr() there) via plain substring
//      find/replace BEFORE tinyexpr ever tokenizes the string - so
//      "fpp_next_playlist_start+1" typed bare (no %%/=) correctly computes
//      "that variable's value, plus one" instead of comparing against the
//      literal text, with no new aliasing code needed here.
//   4. Otherwise -> hand to EvaluateConditionExpression() unchanged, which
//      is template mode: safe literal passthrough for ordinary non-formula
//      text like "ON" that doesn't match any real variable and doesn't look
//      like a formula. Without this step, forcing '=' on every non-matching
//      string would break any plain literal Value that isn't purely
//      numeric/formula-shaped (compile failure -> empty result, not a
//      crash, but silently wrong).
static std::string EvaluateNameOrExpression(const std::string& text) {
    size_t start = text.find_first_not_of(" \t");
    if (start != std::string::npos) {
        size_t end = text.find_last_not_of(" \t");
        std::string trimmed = text.substr(start, end - start + 1);
        for (auto const& varName : Variables::INSTANCE.getAllVariableNames()) {
            if (varName == trimmed) {
                return Variables::INSTANCE.getVariable(trimmed, "");
            }
        }
    }
    if (!text.empty() && text[0] == '=') {
        return EvaluateConditionExpression(text);
    }
    if (text.find("%%") != std::string::npos || text.find("==") != std::string::npos) {
        return EvaluateConditionExpression(text);
    }
    if (LooksLikeFormula(text)) {
        return EvaluateConditionExpression("=" + text);
    }
    return EvaluateConditionExpression(text);
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
    if (source == "Expression") {
        // The old, separate "Variable" Source was fully removed (per
        // explicit direction, not kept as a backward-compatible alias) - its
        // behavior lives here now, handled by EvaluateNameOrExpression()'s
        // own exact-match check: a plain literal lookup for anything that's
        // an exact match to a real variable name (old "Variable" behavior,
        // including what a dedicated "MQTT" source used to do -
        // Variables::getVariable() already reads a cached mqtt-<topic>
        // message transparently), falling through to expression evaluation
        // otherwise (old "Expression" behavior, now also reachable without
        // typing '=' or "%%...%%" by hand - see EvaluateNameOrExpression's
        // own comment). Any existing saved condition still using
        // "source":"Variable" now reads as not-found until re-edited.
        return EvaluateNameOrExpression(name);
    } else if (source == "Time") {
        return CurrentTimeHHMM();
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

// std::stod() alone isn't a valid "is this string a number" check - it
// happily parses a leading numeric prefix and silently ignores the rest
// (e.g. std::stod("18:15") == 18.0, no exception), which previously made
// LeafNode::compare()'s numeric comparators do the wrong thing for anything
// that merely starts with digits, most notably HH:MM values (Time/Sun) being
// compared by hour only. Require the ENTIRE string to have been consumed.
static bool ParseFullyNumeric(const std::string& s, double& out) {
    if (s.empty()) {
        return false;
    }
    size_t pos = 0;
    try {
        out = std::stod(s, &pos);
    } catch (...) {
        return false;
    }
    return pos == s.size();
}

// Hoisted out of LeafNode (was a private member function there) so both
// LeafNode::evaluate() and ConditionNode::PreviewLeafResult() (the Check
// editor's consolidated eye-preview modal) share exactly one implementation
// instead of a second copy risking drift.
static bool CompareValues(const std::string& comparatorStr, const std::string& lhs, const std::string& rhs) {
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
        double mn, mx, v;
        if (!ParseFullyNumeric(rhs.substr(0, commaPos), mn) ||
            !ParseFullyNumeric(rhs.substr(commaPos + 1), mx) ||
            !ParseFullyNumeric(lhs, v)) {
            return false;
        }
        if (mx < mn) {
            std::swap(mn, mx);
        }
        return v >= mn && v <= mx;
    }
    // Numeric comparators. ParseFullyNumeric (not a bare std::stod) is
    // what actually makes HH:MM values (Time/Sun) fall through to the
    // string-compare branch below - std::stod alone does NOT throw on
    // "18:15", it silently parses just the "18" prefix and stops at the
    // colon, so a bare try/stod/catch here would (and, before this fix,
    // did) compare HH:MM values by HOUR ONLY, discarding minutes: e.g.
    // "18:20" vs "18:15" both truncate to 18, so "greater than" wrongly
    // returned false for a time that genuinely is later. Falling through
    // to plain string comparison instead is not just a safe fallback -
    // it's the CORRECT comparison for this format, since Time/Sun always
    // emit zero-padded HH:MM, which sorts identically to chronological
    // order as plain text.
    double a, b;
    if (ParseFullyNumeric(lhs, a) && ParseFullyNumeric(rhs, b)) {
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
    } else {
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
        // Same auto-detecting path as the merged Variable/Expression Source
        // (EvaluateNameOrExpression, above): an exact match to a real
        // variable name is a plain lookup, a bare formula like
        // "fpp_next_playlist_start+1" is computed automatically, and
        // anything else (e.g. a fixed literal like "1" or "ON") passes
        // through unchanged - so Value can reference variables/math (e.g.
        // comparing one computed value against another) without needing any
        // manual "=" or "%%...%%" syntax.
        std::string rhs = EvaluateNameOrExpression(value);
        bool cmp = CompareValues(comparatorStr, lhs, rhs);
        bool result = negate ? !cmp : cmp;
        // lhs/rhs are unbounded (e.g. a Variable can hold an entire fetched
        // web page) - truncate before logging, see TruncateForLog() (log.h).
        LogDebug(VB_COMMAND, "If: Leaf[%s:%s] = \"%s\" %s%s \"%s\" -> %s\n", source.c_str(), name.c_str(),
                 TruncateForLog(lhs).c_str(), negate ? "NOT " : "", comparatorStr.c_str(),
                 TruncateForLog(rhs).c_str(), result ? "true" : "false");
        return result;
    }
};

// Full ad-hoc-leaf evaluation for the Check editor's consolidated eye-preview
// modal (see Part 3 of the source-merge plan): given the same
// Source/Name/Comparator/Value/Not a saved leaf would have, returns the LHS
// and RHS values it currently resolves to and the comparator's result -
// reuses ReadConditionSourceValue/EvaluateNameOrExpression/CompareValues, the
// exact same functions LeafNode::evaluate() itself calls, so this can never
// drift from what an actual saved condition would do.
void ConditionNode::PreviewLeafResult(const std::string& source, const std::string& name,
                                       const std::string& comparatorStr, const std::string& value, bool negate,
                                       bool& lhsFound, std::string& lhsValue, std::string& rhsValue, bool& result) {
    lhsFound = true;
    lhsValue = ReadConditionSourceValue(source, name, lhsFound);
    rhsValue = EvaluateNameOrExpression(value);
    if (!lhsFound) {
        result = negate;
        return;
    }
    bool cmp = CompareValues(comparatorStr, lhsValue, rhsValue);
    result = negate ? !cmp : cmp;
}

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
