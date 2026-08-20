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

#include "fpp-json.h"
#include "commands/Commands.h"
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// The "data pump": runs a Command or preset on a fixed interval, optionally
// landing the Command's Result into a Variable (Variables.h) so an If check
// a moment later can read fresh external data (a URL fetch, a script's
// output) without ever blocking FPP's main loop on I/O. Config-driven
// (config/recurringtasks.json), editable live via the Recurring Tasks admin
// page (www/recurringtasks.php, api/configfile/recurringtasks.json) + a
// Reload() call so edits take effect without an fppd restart.
class RecurringTasks {
public:
    static RecurringTasks INSTANCE;
    RecurringTasks();
    ~RecurringTasks();

    void Init();
    void Close();

    // Re-reads config/recurringtasks.json and re-schedules all timers to
    // match. Safe to call after the admin UI saves a new config.
    void Reload();

    // Merges each configured task's settings with last-run bookkeeping
    // (lastRunMS/lastStatus/lastError), keyed by task name, for the admin
    // page's status column.
    void reportStatus(Json::Value& root);

    // Runs the task described by taskJson (same shape as one entry of
    // config/recurringtasks.json - see ParseTask()) immediately and
    // synchronously, blocking the calling (HTTP handler) thread up to ~1s,
    // same convention as CommandManager's own synchronous command execution
    // (Commands.cpp). Takes the task definition inline rather than a saved
    // task's name so "Test Run" on www/recurringtasks.php reflects whatever
    // is in the row right now, including unsaved edits. On success, actually
    // writes the filtered result into resultVariable (if set) and updates
    // status, same as a real scheduled run - this is a live test, not a dry
    // run. Returns {"ok":bool, "raw":string, "filtered":string, "error":string}.
    Json::Value TestRunTask(const Json::Value& taskJson);

private:
    struct RecurringTask {
        std::string name;
        bool enabled = true;
        long long intervalMS = 60000;
        std::string type; // "preset" or "command"

        // type == "preset"
        std::string preset;

        // type == "command"
        std::string command;
        Json::Value args;
        std::string resultVariable;
        bool persistResult = false;

        // Applied to the command's raw Result::get() before it lands in
        // resultVariable. filterType: "" (none), "json" (dot-path field
        // pluck, e.g. "data.temperature"), "between" (substring between
        // filterBetweenStart/End markers, empty marker = start/end of raw),
        // or "regex" (filterRegex against raw - capture group 1 if the
        // pattern has one, else the whole match).
        std::string filterType;
        std::string filterJsonPath;
        std::string filterBetweenStart;
        std::string filterBetweenEnd;
        std::string filterRegex;
    };

    struct PendingResult {
        std::unique_ptr<Command::Result> result;
        std::string resultVariable;
        bool persistResult = false;
        long long startTimeMS = 0;
        std::string taskName;
        std::string filterType;
        std::string filterJsonPath;
        std::string filterBetweenStart;
        std::string filterBetweenEnd;
        std::string filterRegex;
    };

    struct TaskStatus {
        bool hasRun = false;
        long long lastRunMS = 0;
        bool lastOk = true;
        std::string lastError;
    };

    static RecurringTask ParseTask(const Json::Value& j);
    static std::string ApplyResultFilter(const std::string& raw, const std::string& filterType,
                                         const std::string& jsonPath, const std::string& betweenStart,
                                         const std::string& betweenEnd, const std::string& regexPattern);
    void load();
    void startTimers();
    // Polls in-flight (async) task results to completion. Driven by a
    // self-re-arming one-shot Timers timer that exists only while m_pending
    // is non-empty - see armPollTimer() in RecurringTasks.cpp.
    void pollPending();
    void armPollTimer();
    void runTask(const RecurringTask& task);
    void finish(PendingResult& pending);
    void recordStatus(const std::string& name, bool ok, const std::string& err);

    std::mutex m_tasksLock;
    std::vector<RecurringTask> m_tasks;

    std::mutex m_pendingLock;
    std::vector<PendingResult> m_pending;

    std::mutex m_statusLock;
    std::map<std::string, TaskStatus> m_status;
};
