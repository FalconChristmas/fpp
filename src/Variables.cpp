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
#include "fpphttp.h"

#include "MultiSync.h"
#include "Player.h"
#include "Scheduler.h"
#include "Variables.h"
#include "Warnings.h"
#include "commands/Condition.h"
#include "common.h"
#include "log.h"
#include "mqtt.h"
#include "settings.h"
#include "mediaoutput/mediaoutput.h"
#include "util/ExpressionProcessor.h"

#include <ctime>
#include <map>

// std::to_string(double) always pads to 6 decimals ("45.000000"); use a
// format that drops trailing zeros so integer-valued results stay readable.
static std::string DoubleToCleanString(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%g", v);
    return std::string(buf);
}

// Same "static initialized at process start" convention as httpAPI.cpp's own
// (private) startupTime - duplicated rather than shared across translation
// units for one time_t.
static std::time_t FPP_STARTUP_TIME = std::time(nullptr);

// FPP_STATUS_* -> name, mirroring GetCurrentFPPDStatus()'s switch
// (httpAPI.cpp) - not shared code since it's a small, stable 6-case mapping
// and pulling in httpAPI.cpp's own dependencies here isn't worth it.
static std::string PlaylistStatusName(int status) {
    switch (status) {
    case 0:
        return "idle";
    case 1:
        return "playing";
    case 2:
        return "stopping gracefully";
    case 3:
        return "stopping gracefully after loop";
    case 4:
        return "stopping now";
    case 5:
        return "paused";
    default:
        return "unknown";
    }
}

// Playlist::GetCurrentStatus() sets some fields (e.g. "repeat_mode",
// "random") as a JSON int in its active-playlist branch but as a JSON
// string in its idle-state default branch - Json::Value::asString() throws
// if the value isn't actually a string, so a plain .asString() here would
// throw depending on player state. Coerce defensively regardless of type.
static std::string JsonToStr(const Json::Value& v) {
    if (v.isString()) {
        return v.asString();
    }
    if (v.isBool()) {
        return v.asBool() ? "1" : "0";
    }
    if (v.isIntegral()) {
        return std::to_string(v.asInt64());
    }
    if (v.isDouble()) {
        return DoubleToCleanString(v.asDouble());
    }
    return "";
}

// Computes the current value of every "fpp_" read-only status variable.
// Cheap (a handful of in-memory reads, no I/O) so recomputed fresh on every
// call rather than cached - avoids any staleness question.
//
// Underscore, not the old hyphen ("fpp_") this family used previously: a
// bare "fpp_next_playlist_start" is directly usable in pure-math mode
// (lowercase-led, a-z0-9_ only - see ExpressionProcessor's
// isValidExprIdentifier()) with no aliasing needed, unlike the old
// hyphenated form. Renamed outright, not kept as a dual-registered alias -
// per explicit direction, no old "fpp-*" references remain anywhere.
static std::map<std::string, std::string> ComputeFppStatusVariables() {
    std::map<std::string, std::string> vars;

    int status = (int)Player::INSTANCE.GetStatus();
    vars["fpp_status"] = std::to_string(status);
    vars["fpp_status_name"] = PlaylistStatusName(status);
    vars["fpp_mode_name"] = toStdStringAndFree(modeToString(getFPPmode()));
    vars["fpp_volume"] = std::to_string(getVolume());
    vars["fpp_multisync"] = (multiSync && multiSync->isMultiSyncEnabled()) ? "1" : "0";
    vars["fpp_uptime_seconds"] = std::to_string((long long)(std::time(nullptr) - FPP_STARTUP_TIME));
    vars["fpp_is_playing"] = Player::INSTANCE.IsPlaying() ? "1" : "0";
    vars["fpp_was_scheduled"] = Player::INSTANCE.WasScheduled() ? "1" : "0";
    vars["fpp_scheduler_enabled"] = (scheduler && scheduler->IsEnabled()) ? "1" : "0";
    {
        // Same "HH:MM" convention as the "Time" Condition source
        // (Condition.cpp's CurrentTimeHHMM()) so a plain fpp_current_time
        // comparison lines up with what that source's Value examples show.
        // Date is ISO 8601 (YYYY-MM-DD) - unambiguous and sorts/compares
        // correctly as a plain string. Day name matches strftime's locale-
        // independent full weekday name (English), same as the rest of this
        // list rather than the system locale.
        static const char* kDayNames[7] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };
        static const char* kMonthNames[12] = { "January", "February", "March", "April", "May", "June",
                                                "July", "August", "September", "October", "November", "December" };
        std::time_t now = std::time(nullptr);
        struct tm local;
        localtime_r(&now, &local);
        char buf[16];
        snprintf(buf, sizeof(buf), "%02d:%02d", local.tm_hour, local.tm_min);
        vars["fpp_current_time"] = buf;
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
        vars["fpp_current_date"] = buf;
        vars["fpp_day_of_week"] = kDayNames[local.tm_wday];
        vars["fpp_current_month"] = kMonthNames[local.tm_mon];
    }
    vars["fpp_warning_count"] = std::to_string(WarningHolder::GetWarnings().size());
    if (scheduler) {
        vars["fpp_next_playlist"] = scheduler->GetNextPlaylistName();
        vars["fpp_next_playlist_start"] = scheduler->GetNextPlaylistStartStr();
    } else {
        vars["fpp_next_playlist"] = "";
        vars["fpp_next_playlist_start"] = "";
    }

    Json::Value pl;
    Player::INSTANCE.GetCurrentStatus(pl);
    vars["fpp_current_playlist"] = JsonToStr(pl["current_playlist"]["playlist"]);
    vars["fpp_current_playlist_count"] = JsonToStr(pl["current_playlist"]["count"]);
    vars["fpp_current_playlist_index"] = JsonToStr(pl["current_playlist"]["index"]);
    vars["fpp_current_sequence"] = JsonToStr(pl["current_sequence"]);
    vars["fpp_current_song"] = JsonToStr(pl["current_song"]);
    vars["fpp_seconds_played"] = JsonToStr(pl["seconds_played"]);
    vars["fpp_seconds_remaining"] = JsonToStr(pl["seconds_remaining"]);
    vars["fpp_time_elapsed"] = JsonToStr(pl["time_elapsed"]);
    vars["fpp_time_remaining"] = JsonToStr(pl["time_remaining"]);
    vars["fpp_repeat_mode"] = JsonToStr(pl["repeat_mode"]);
    vars["fpp_random"] = JsonToStr(pl["random"]);

    return vars;
}

Variables Variables::INSTANCE;

Variables::Variables() {
}
Variables::~Variables() {
}

void Variables::Init() {
    load();
}

void Variables::Close() {
    save();
}

void Variables::load() {
    std::string file = FPP_DIR_CONFIG("/variables.json");
    if (!FileExists(file)) {
        return;
    }
    Json::Value root = LoadJsonFromFile(file);
    std::unique_lock<std::mutex> lock(m_lock);
    for (auto const& name : root.getMemberNames()) {
        VariableEntry e;
        e.value = root[name]["value"].asString();
        e.persist = root[name]["persist"].asBool();
        e.lastUpdated = (time_t)root[name]["lastUpdated"].asInt64();
        m_variables[name] = e;
    }
}

void Variables::save() {
    Json::Value root;
    std::unique_lock<std::mutex> lock(m_lock);
    for (auto const& kv : m_variables) {
        if (kv.second.persist) {
            root[kv.first]["value"] = kv.second.value;
            root[kv.first]["persist"] = true;
            root[kv.first]["lastUpdated"] = (Json::Int64)kv.second.lastUpdated;
        }
    }
    lock.unlock();
    SaveJsonToFile(root, FPP_DIR_CONFIG("/variables.json"), "\t");
}

bool Variables::IsFppStatusVariableName(const std::string& name) {
    // "fpp_" (underscore) - a bare "fpp_next_playlist_start" is directly
    // usable in pure-math mode with no aliasing needed (lowercase-led,
    // a-z0-9_ only - see ExpressionProcessor's isValidExprIdentifier()),
    // unlike the old hyphenated "fpp-" form this replaced.
    return name.rfind("fpp_", 0) == 0;
}

// "mqtt-<topic>" is a live, read-only variable backed directly by
// MosquittoClient's own last-message-per-topic cache (mqtt.h's
// messageCache) rather than the persisted m_variables map - same shape as
// the "fpp_" branch above. <topic> is the raw MQTT topic string (may
// contain '/'), so it's only usable where the value is looked up by name
// (variable condition leaf, %VAR:name% substitution) - it won't parse as
// an ExpressionProcessor/tinyexpr identifier, which requires alnum/underscore.
bool Variables::IsMqttVariableName(const std::string& name) {
    return name.rfind("mqtt-", 0) == 0;
}

std::string Variables::getVariable(const std::string& name, const std::string& def) {
    if (IsFppStatusVariableName(name)) {
        auto vars = ComputeFppStatusVariables();
        auto it = vars.find(name);
        return it == vars.end() ? def : it->second;
    }
    if (IsMqttVariableName(name)) {
        std::string topic = name.substr(5);
        std::string value;
        if (mqtt && mqtt->GetCachedMessage(topic, value)) {
            return value;
        }
        return def;
    }
    std::unique_lock<std::mutex> lock(m_lock);
    auto it = m_variables.find(name);
    if (it == m_variables.end()) {
        return def;
    }
    return it->second.value;
}
int Variables::getVariableInt(const std::string& name, int def) {
    std::string v = getVariable(name, "");
    if (v.empty()) {
        return def;
    }
    try {
        return std::stoi(v);
    } catch (...) {
        return def;
    }
}
double Variables::getVariableDouble(const std::string& name, double def) {
    std::string v = getVariable(name, "");
    if (v.empty()) {
        return def;
    }
    try {
        return std::stod(v);
    } catch (...) {
        return def;
    }
}
bool Variables::getVariableBool(const std::string& name, bool def) {
    std::string v = getVariable(name, "");
    if (v.empty()) {
        return def;
    }
    return v == "true" || v == "1";
}
std::vector<std::string> Variables::getVariableNames() {
    std::vector<std::string> names;
    std::unique_lock<std::mutex> lock(m_lock);
    names.reserve(m_variables.size());
    for (auto const& kv : m_variables) {
        names.push_back(kv.first);
    }
    return names;
}

std::vector<std::string> Variables::getAllVariableNames() {
    std::vector<std::string> names = getVariableNames();

    Json::Value fppVars;
    reportFppVariables(fppVars);
    for (auto const& name : fppVars.getMemberNames()) {
        names.push_back(name);
    }

    Json::Value mqttVars;
    reportMqttVariables(mqttVars);
    for (auto const& name : mqttVars.getMemberNames()) {
        names.push_back(name);
    }

    return names;
}

void Variables::setVariable(const std::string& name, const std::string& value, bool persist) {
    if (IsFppStatusVariableName(name)) {
        LogWarn(VB_GENERAL, "Variables::setVariable(): \"%s\" starts with the reserved \"fpp_\" prefix "
                            "(read-only computed status) and cannot be set\n",
                name.c_str());
        return;
    }
    if (IsMqttVariableName(name)) {
        LogWarn(VB_GENERAL, "Variables::setVariable(): \"%s\" starts with the reserved \"mqtt-\" prefix "
                            "(read-only, backed by the MQTT message cache) and cannot be set\n",
                name.c_str());
        return;
    }
    bool wasPersisted = false;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        VariableEntry& e = m_variables[name];
        wasPersisted = e.persist;
        e.value = value;
        e.persist = persist;
        e.lastUpdated = time(nullptr);
    }
    fireListeners(name, value);
    // Save whenever the variable is (or was) persisted, so demoting persist
    // true->false actually removes the stale entry from variables.json
    // instead of leaving it there until the next persisted write elsewhere.
    if (persist || wasPersisted) {
        save();
    }
}
void Variables::setVariable(const std::string& name, int value, bool persist) {
    setVariable(name, std::to_string(value), persist);
}

bool Variables::deleteVariable(const std::string& name) {
    bool existed = false;
    bool wasPersisted = false;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        auto it = m_variables.find(name);
        if (it != m_variables.end()) {
            existed = true;
            wasPersisted = it->second.persist;
            m_variables.erase(it);
        }
    }
    if (wasPersisted) {
        save();
    }
    return existed;
}
double Variables::incrementVariable(const std::string& name, double delta, bool persist) {
    double newValue = 0.0;
    bool wasPersisted = false;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        VariableEntry& e = m_variables[name];
        wasPersisted = e.persist;
        double cur = 0.0;
        try {
            cur = e.value.empty() ? 0.0 : std::stod(e.value);
        } catch (...) {
            cur = 0.0;
        }
        newValue = cur + delta;
        e.value = DoubleToCleanString(newValue);
        e.persist = persist;
        e.lastUpdated = time(nullptr);
    }
    fireListeners(name, DoubleToCleanString(newValue));
    if (persist || wasPersisted) {
        save();
    }
    return newValue;
}

void Variables::registerListener(const std::string& id, const std::string& name,
                                 std::function<void(const std::string&, const std::string&)>&& cb) {
    std::unique_lock<std::mutex> lock(m_lock);
    m_listeners[name][id] = std::move(cb);
}
void Variables::unregisterListener(const std::string& id, const std::string& name) {
    std::unique_lock<std::mutex> lock(m_lock);
    auto it = m_listeners.find(name);
    if (it != m_listeners.end()) {
        it->second.erase(id);
    }
}
void Variables::fireListeners(const std::string& name, const std::string& value) {
    std::vector<std::function<void(const std::string&, const std::string&)>> cbs;
    {
        std::unique_lock<std::mutex> lock(m_lock);
        auto it = m_listeners.find(name);
        if (it != m_listeners.end()) {
            for (auto const& kv : it->second) {
                cbs.push_back(kv.second);
            }
        }
    }
    for (auto& cb : cbs) {
        cb(name, value);
    }
}

std::set<std::string> Variables::getReferencedNamesInConfig() {
    std::set<std::string> referenced;
    std::string presetsFile = FPP_DIR_CONFIG("/commandPresets.json");
    if (!FileExists(presetsFile)) {
        return referenced;
    }
    Json::Value root = LoadJsonFromFile(presetsFile);
    if (!root.isMember("commands")) {
        return referenced;
    }
    for (auto& cmd : root["commands"]) {
        std::string commandName = cmd["command"].asString();
        for (int i = 0; i < (int)cmd["args"].size(); i++) {
            if (cmd["args"][i].isNull()) {
                continue;
            }
            std::string arg = cmd["args"][i].asString();
            // "Set Variable" writes to whatever's in its first arg.
            if (commandName == "Set Variable" && i == 0 && !arg.empty()) {
                referenced.insert(arg);
            }
            // %VAR:name% substitution reference, anywhere in any arg.
            std::size_t pos = 0;
            while ((pos = arg.find("%VAR:", pos)) != std::string::npos) {
                std::size_t end = arg.find('%', pos + 5);
                if (end == std::string::npos) {
                    break;
                }
                referenced.insert(arg.substr(pos + 5, end - pos - 5));
                pos = end + 1;
            }
        }
    }
    return referenced;
}

// Values at or under this size are reported inline in the bulk listing
// endpoints below. Bigger ones are reported as a truncated preview plus
// "truncated"/"size" so the listing page's default (non-View) load stays
// small even when a Variable holds a full fetched web page or similar -
// the full value is still one GET /api/variables/{name} away. Kept small
// (not just "under some KB") since these values also render directly in a
// table cell - variables.php additionally CSS-truncates the cell itself,
// but there's no point shipping bytes the row can't usefully show anyway.
static const size_t kInlineValueMaxBytes = 200;

static void reportValueField(Json::Value& entry, const std::string& value) {
    if (value.size() <= kInlineValueMaxBytes) {
        entry["value"] = value;
        entry["truncated"] = false;
    } else {
        entry["value"] = value.substr(0, kInlineValueMaxBytes);
        entry["truncated"] = true;
        entry["size"] = (Json::UInt64)value.size();
    }
}

void Variables::reportVariables(Json::Value& root) {
    std::set<std::string> referenced = getReferencedNamesInConfig();
    std::unique_lock<std::mutex> lock(m_lock);
    for (auto const& kv : m_variables) {
        reportValueField(root[kv.first], kv.second.value);
        root[kv.first]["persist"] = kv.second.persist;
        root[kv.first]["lastUpdated"] = (Json::Int64)kv.second.lastUpdated;
        root[kv.first]["used"] = referenced.count(kv.first) > 0;
    }
}

// fpp_* status variables are computed fresh from live Player/system state on
// every call (ComputeFppStatusVariables() above) - there's no persisted
// "last updated" the way User/MQTT Variables have. Approximate one by
// tracking each name's last-seen value here and only stamping a new time
// when it actually changes, scoped to this bulk-listing report so it doesn't
// add tracking overhead to the getVariable()/If-condition hot path. The
// first observation after an fppd (re)start always stamps "now" - there's no
// way to know when a value last changed before we started watching it.
static std::mutex fppVarChangeLock;
static std::map<std::string, std::pair<std::string, time_t>> fppVarLastChange;

void Variables::reportFppVariables(Json::Value& root) {
    std::unique_lock<std::mutex> lock(fppVarChangeLock);
    time_t now = time(nullptr);
    for (auto const& kv : ComputeFppStatusVariables()) {
        auto it = fppVarLastChange.find(kv.first);
        if (it == fppVarLastChange.end() || it->second.first != kv.second) {
            fppVarLastChange[kv.first] = { kv.second, now };
        }
        reportValueField(root[kv.first], kv.second);
        root[kv.first]["lastUpdated"] = (Json::Int64)fppVarLastChange[kv.first].second;
    }
}

void Variables::reportMqttVariables(Json::Value& root) {
    if (!mqtt) {
        return;
    }
    Json::Value cache;
    mqtt->dumpMessageCache(cache, true);
    for (auto const& topic : cache.getMemberNames()) {
        std::string name = "mqtt-" + topic;
        reportValueField(root[name], cache[topic]["value"].asString());
        root[name]["lastUpdated"] = cache[topic]["lastUpdated"];
    }
}

// --------------------------------------------------------------------------
// OpenAPI docs for the /variables/* endpoints handled below.
// --------------------------------------------------------------------------

/**
 * List all User Variables and their current values. Pass ?validateExpression
 * instead to syntax-check an expression (for the Set Variable "Expression"
 * field) against currently-known variables, without setting anything. Pass
 * ?fpp=true instead to list the read-only "fpp_" status variables (current
 * playlist/sequence, play state, volume, etc.) instead of User Variables.
 *
 * @route GET /api/variables
 * @param string validateExpression If set, validate this expression instead of listing variables.
 * @param boolean conditionExpr If "true" alongside validateExpression, classify it the way the If
 *   Check editor's Name/Value fields actually evaluate it (exact variable match / formula / inert
 *   literal text) instead of a bare compile check.
 * @param boolean fpp If "true", list the read-only fpp- status variables instead of User Variables.
 * @param boolean mqtt If "true", list the read-only mqtt-<topic> variables (MQTT's own last-message-
 *   per-topic cache) instead of User Variables.
 * @response 200 Object keyed by variable name, each with `value`, `truncated`,
 *   `persist`, `lastUpdated` (unix timestamp) and `used` (true if referenced
 *   anywhere in config/commandPresets.json, either as a Set Variable target or
 *   via %VAR:name%). If `validateExpression` was passed, `{"valid": true|false}` (or, with
 *   `conditionExpr=true`, `{"valid": true|false, "kind": "variable"|"formula"|"literal"}`)
 *   instead. If `fpp=true`, each entry has `value`/`truncated`/`lastUpdated`
 *   (no persist/used - these are live-computed, not stored; `lastUpdated` is
 *   approximated as the last time this endpoint observed the value actually
 *   change, not a true change time, and resets on an fppd restart). If
 *   `mqtt=true`, each entry has `value`/`truncated`/`lastUpdated` (the time
 *   the topic's last MQTT message was received; no persist/used). Values over
 *   200 bytes are reported truncated (`truncated: true`, plus `size` = the
 *   real byte count) to keep this listing endpoint cheap - fetch GET
 *   /api/variables/{name} for the full value.
 */

/**
 * Read a single User Variable's current value.
 *
 * @route GET /api/variables/{name}
 * @response 200 The variable's value as plain text (empty string if unset).
 */
// getPathPieces() splits the whole path on '/', so a name containing '/'
// (e.g. an mqtt-<topic> Variable) lands across multiple pieces instead of
// just parts[1]. Rejoin everything after the resource segment (parts[0],
// "variables") back into one name.
static std::string joinNameFromPathPieces(const std::vector<std::string>& parts) {
    std::string name;
    for (size_t i = 1; i < parts.size(); ++i) {
        if (i > 1) {
            name += "/";
        }
        name += parts[i];
    }
    return name;
}

HttpResponsePtr Variables::render_GET(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    int plen = parts.size();

    // ?validateExpression=<expr> - live syntax check for the Set Variable
    // "Expression" field, without setting anything. Binds every currently
    // known Variable (User Variables plus the read-only fpp-/mqtt- ones)
    // first, same as SetVariableCommand::run(), so a reference to an
    // existing variable name validates correctly and a reference to a
    // nonexistent one is correctly flagged invalid.
    std::string exprArg = getRequestArg(req, "validateExpression");
    if (!exprArg.empty()) {
        Json::Value result;
        // conditionExpr=true: the If Check editor's Name/Value fields, which
        // go through ConditionNode::ClassifyNameOrExpression's auto-detect at
        // runtime (Condition.cpp's EvaluateNameOrExpression) rather than a
        // bare ExpressionProcessor::compile() - a raw compile() call always
        // "succeeds" on ordinary unmatched text (it's just treated as inert
        // literal, per that function's own documented behavior), so it can't
        // tell a real variable match, an actual formula, and a typo'd/
        // unmatched name apart the way the Check editor's icon needs to.
        if (getRequestArg(req, "conditionExpr") == "true") {
            bool compileOk = true;
            auto kind = ConditionNode::ClassifyNameOrExpression(exprArg, compileOk);
            result["valid"] = compileOk;
            result["kind"] = kind == ConditionNode::ExpressionKind::Variable ? "variable" : (kind == ConditionNode::ExpressionKind::Formula ? "formula" : "literal");
        } else {
            // Set Variable's "Expression" field (VariableCommands.cpp's
            // SetVariableCommand::run(), type=="Expression" branch) really is
            // just a bare ExpressionProcessor::compile() call with no
            // auto-detect - so validating it the same way here is correct,
            // not a shortcut.
            ExpressionProcessor proc;
            std::map<std::string, ExpressionProcessor::ExpressionVariable> boundVars;
            for (auto const& varName : getAllVariableNames()) {
                auto inserted = boundVars.try_emplace(varName, varName);
                inserted.first->second.setValue(getVariable(varName));
                proc.bindVariable(&inserted.first->second);
            }
            result["valid"] = proc.compile(exprArg);
        }
        std::string resultStr = SaveJsonToString(result);
        return makeStringResponse(resultStr, 200, "application/json");
    }

    if (plen > 1) {
        std::string name = joinNameFromPathPieces(parts);
        std::string v = getVariable(name, "");
        return makeStringResponse(v, 200, "text/plain");
    }

    Json::Value result;
    if (getRequestArg(req, "fpp") == "true") {
        reportFppVariables(result);
    } else if (getRequestArg(req, "mqtt") == "true") {
        reportMqttVariables(result);
    } else {
        reportVariables(result);
    }
    std::string resultStr = SaveJsonToString(result, "  ");
    return makeStringResponse(resultStr, 200, "application/json");
}

/**
 * Set a User Variable's value. Body is the raw new value (plain text, not JSON).
 * Add ?persist=true to save it to config/variables.json so it survives an
 * fppd restart; omitted or any other value means the variable is in-memory only.
 *
 * @route POST /api/variables/{name}
 * @param boolean persist Persist the value to disk so it survives a restart.
 * @body new-value-goes-here
 * @response 200 "OK"
 * @response 400 Missing variable name in the path.
 */
HttpResponsePtr Variables::render_POST(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    int plen = parts.size();

    if (plen > 1) {
        std::string name = joinNameFromPathPieces(parts);
        if (IsFppStatusVariableName(name)) {
            return makeStringResponse("\"" + name + "\" is a read-only fpp- status variable and cannot be set", 400, "text/plain");
        }
        std::string value = std::string(getRequestContent(req));
        bool persist = getRequestArg(req, "persist") == "true";
        setVariable(name, value, persist);
        return makeStringResponse("OK", 200, "text/plain");
    }
    return makeStringResponse("Bad Request", 400, "text/plain");
}

/**
 * Delete a User Variable entirely - unlike POSTing an empty body (the UI's
 * "Clear"), which only resets its value/persist flag and leaves the row
 * behind, this removes the name from the list altogether.
 *
 * @route DELETE /api/variables/{name}
 * @response 200 "OK"
 * @response 400 Missing variable name in the path, or name is a read-only
 *   fpp-/mqtt- variable (nothing to delete - those aren't stored).
 */
HttpResponsePtr Variables::render_DELETE(const HttpRequestPtr& req) {
    auto parts = getPathPieces(req->path());
    if (parts.size() <= 1) {
        return makeStringResponse("Bad Request", 400, "text/plain");
    }
    std::string name = joinNameFromPathPieces(parts);
    if (IsFppStatusVariableName(name) || IsMqttVariableName(name)) {
        return makeStringResponse("\"" + name + "\" is read-only and cannot be deleted", 400, "text/plain");
    }
    deleteVariable(name);
    return makeStringResponse("OK", 200, "text/plain");
}
