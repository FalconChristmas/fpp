#pragma once
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

#include "fpp-json-fwd.h"
#include "fpphttp_types.h"
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

class Variables {
public:
    static Variables INSTANCE;
    Variables();
    ~Variables();

    void Init();
    void Close();

    std::string getVariable(const std::string& name, const std::string& def = "");
    int getVariableInt(const std::string& name, int def = 0);
    double getVariableDouble(const std::string& name, double def = 0.0);
    bool getVariableBool(const std::string& name, bool def = false);
    std::vector<std::string> getVariableNames();
    // getVariableNames() plus the read-only "fpp-"/"mqtt-<topic>" names -
    // everything that can be *read* in an Expression field (only the plain
    // User Variables from getVariableNames() can be written by "Set Variable").
    std::vector<std::string> getAllVariableNames();

    void setVariable(const std::string& name, const std::string& value, bool persist = false);
    void setVariable(const std::string& name, int value, bool persist = false);
    // Removes the variable entirely (unlike setVariable(name, "") /the UI's
    // "Clear", which only resets its value and still leaves an empty row
    // behind forever - this actually erases the map entry). Returns false if
    // the name didn't exist.
    bool deleteVariable(const std::string& name);
    double incrementVariable(const std::string& name, double delta, bool persist = false);

    void registerListener(const std::string& id, const std::string& name,
                          std::function<void(const std::string& name, const std::string& value)>&& cb);
    void unregisterListener(const std::string& id, const std::string& name);

    void reportVariables(Json::Value& root);

    // Live-computed, read-only status variables (name starts with "fpp-"),
    // backed directly by Player/system state rather than the persisted
    // m_variables map - see the "fpp-" branch in getVariable() and
    // Variables.cpp's ComputeFppStatusVariables(). Curated from the fields
    // FPP already publishes on its "fppd_status"/"playlist_details" MQTT
    // topics, since those are exactly the status values useful in an If
    // condition (current playlist/sequence, play state, volume, etc.).
    // setVariable() refuses to write an "fpp-" name - it would silently
    // never be readable back through getVariable() otherwise, since that
    // checks the live computation first.
    void reportFppVariables(Json::Value& root);
    static bool IsFppStatusVariableName(const std::string& name);

    // "mqtt-<topic>" variables: live, read-only, backed by MQTT's own
    // last-message-per-topic cache (MosquittoClient::messageCache) instead
    // of m_variables - same shape as the "fpp-" status variables above.
    // See the IsMqttVariableName definition in Variables.cpp for the
    // reasoning (topic must be known exactly). Usable inside Expression
    // fields same as anything else - ExpressionProcessor aliases any
    // non-identifier-safe name (hyphens, slashes) for tinyexpr's benefit.
    void reportMqttVariables(Json::Value& root);
    static bool IsMqttVariableName(const std::string& name);

    HttpResponsePtr render_GET(const HttpRequestPtr& req);
    HttpResponsePtr render_POST(const HttpRequestPtr& req);
    HttpResponsePtr render_DELETE(const HttpRequestPtr& req);

private:
    struct VariableEntry {
        std::string value;
        bool persist = false;
        time_t lastUpdated = 0;
    };

    void load();
    void save();
    void fireListeners(const std::string& name, const std::string& value);
    // Names referenced anywhere in config/commandPresets.json - either as the
    // target of a "Set Variable" entry, or via a %VAR:name% substitution in
    // any command's args. Used to flag a defined Variable as "used" vs
    // "abandoned" (defined but referenced nowhere) on the Variables page.
    std::set<std::string> getReferencedNamesInConfig();

    std::mutex m_lock;
    std::map<std::string, VariableEntry> m_variables;
    std::map<std::string, std::map<std::string, std::function<void(const std::string&, const std::string&)>>> m_listeners;
};
