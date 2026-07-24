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

#include <chrono>
#include <thread>

#include "RecurringTasks.h"
#include "Timers.h"
#include "Variables.h"
#include "common.h"
#include "log.h"
#include "settings.h"
#include "util/RegExCache.h"

// A single async task's Result shouldn't be allowed to accumulate forever if
// its command never finishes (e.g. a hung URL fetch) - drop it and warn past
// this bound.
static constexpr long long PENDING_RESULT_MAX_MS = 60000;

// Server-side floor on task interval, independent of the admin UI's own
// 1-second-granularity input field - see load()'s use of this. Raised from
// 1000ms: most real commands (URL fetches, scripts) can easily take longer
// than 1s, and runTask()'s overlap guard means a too-low interval just
// means a task spends most of its life being skipped rather than actually
// running - 30s is a more realistic floor for I/O-bound work.
static constexpr long long MIN_TASK_INTERVAL_MS = 30000;

RecurringTasks RecurringTasks::INSTANCE;

RecurringTasks::RecurringTasks() {
}
RecurringTasks::~RecurringTasks() {
}

RecurringTasks::RecurringTask RecurringTasks::ParseTask(const Json::Value& j) {
    RecurringTask task;
    task.name = j["name"].asString();
    task.enabled = j.get("enabled", true).asBool();
    task.intervalMS = j.get("intervalMS", 60000).asInt64();
    task.type = j["type"].asString();
    task.preset = j["preset"].asString();
    task.command = j["command"].asString();
    task.args = j["args"];
    task.resultVariable = j["resultVariable"].asString();
    task.persistResult = j.get("persistResult", false).asBool();
    task.filterType = j["filterType"].asString();
    task.filterJsonPath = j["filterJsonPath"].asString();
    task.filterBetweenStart = j["filterBetweenStart"].asString();
    task.filterBetweenEnd = j["filterBetweenEnd"].asString();
    task.filterRegex = j["filterRegex"].asString();
    return task;
}

// filterType == "json": walk a dot-path ("data.temperature") through the
// parsed raw response. Object-key traversal only, no array indices - covers
// the common "pluck one field out of a JSON API response" case without
// pulling in a real JSON-path library. filterType == "between": substring
// between two literal markers (either may be empty to mean start/end of raw
// instead). filterType == "regex": capture group 1 of the first match, or
// the whole match if the pattern has no group - listed last since it's the
// most powerful but least approachable option (regex syntax vs. a plain
// field name or literal markers). Anything else (empty/"none"/unrecognized)
// passes raw through unchanged.
std::string RecurringTasks::ApplyResultFilter(const std::string& raw, const std::string& filterType,
                                              const std::string& jsonPath, const std::string& betweenStart,
                                              const std::string& betweenEnd, const std::string& regexPattern) {
    if (filterType == "json") {
        if (jsonPath.empty()) {
            return raw;
        }
        Json::Value cur = LoadJsonFromString(raw);
        for (auto const& part : split(jsonPath, '.')) {
            if (part.empty()) {
                continue;
            }
            if (!cur.isObject() || !cur.isMember(part)) {
                LogDebug(VB_GENERAL, "RecurringTasks: JSON filter path \"%s\" - \"%s\" not found in the result\n",
                         jsonPath.c_str(), part.c_str());
                return "";
            }
            cur = cur[part];
        }
        if (cur.isString()) {
            return cur.asString();
        }
        if (cur.isNull()) {
            return "";
        }
        return SaveJsonToString(cur);
    }
    if (filterType == "between") {
        size_t startPos = 0;
        if (!betweenStart.empty()) {
            size_t p = raw.find(betweenStart);
            if (p == std::string::npos) {
                LogDebug(VB_GENERAL, "RecurringTasks: between-filter start marker \"%s\" not found in the result\n",
                         betweenStart.c_str());
                return "";
            }
            startPos = p + betweenStart.length();
        }
        if (betweenEnd.empty()) {
            return raw.substr(startPos);
        }
        size_t endPos = raw.find(betweenEnd, startPos);
        if (endPos == std::string::npos) {
            LogDebug(VB_GENERAL, "RecurringTasks: between-filter end marker \"%s\" not found in the result\n",
                     betweenEnd.c_str());
            return "";
        }
        return raw.substr(startPos, endPos - startPos);
    }
    if (filterType == "regex") {
        if (regexPattern.empty()) {
            return raw;
        }
        try {
            RegExCache re(regexPattern);
            std::smatch m;
            if (!std::regex_search(raw, m, *re.regex)) {
                return "";
            }
            return m.size() > 1 ? m[1].str() : m[0].str();
        } catch (const std::regex_error& e) {
            LogErr(VB_GENERAL, "RecurringTasks: invalid filter regex \"%s\": %s\n", regexPattern.c_str(), e.what());
            return "";
        }
    }
    return raw;
}

void RecurringTasks::load() {
    std::vector<RecurringTask> tasks;
    std::string file = FPP_DIR_CONFIG("/recurringtasks.json");
    if (!FileExists(file)) {
        std::unique_lock<std::mutex> l(m_tasksLock);
        m_tasks = std::move(tasks);
        return;
    }
    Json::Value root = LoadJsonFromFile(file);
    for (auto const& j : root) {
        RecurringTask task = ParseTask(j);
        if (task.name.empty() || task.intervalMS <= 0) {
            LogWarn(VB_GENERAL, "RecurringTasks: skipping invalid task entry (missing name or non-positive intervalMS)\n");
            continue;
        }
        // The admin UI's interval field has a 1-second floor (whole seconds,
        // min='1'), but that's UI-only - config/recurringtasks.json can be
        // hand-edited or written directly via the API. A task misconfigured
        // with e.g. intervalMS=1 would fire near-continuously (unbounded
        // concurrent URL fetches/script forks piling up faster than they can
        // complete), so enforce the same floor server-side rather than
        // trusting the UI to be the only writer.
        if (task.intervalMS < MIN_TASK_INTERVAL_MS) {
            LogWarn(VB_GENERAL, "RecurringTasks: task \"%s\" intervalMS %lld is below the %lldms floor, clamping\n",
                    task.name.c_str(), (long long)task.intervalMS, (long long)MIN_TASK_INTERVAL_MS);
            task.intervalMS = MIN_TASK_INTERVAL_MS;
        }
        tasks.push_back(task);
    }
    std::unique_lock<std::mutex> l(m_tasksLock);
    m_tasks = std::move(tasks);
}

// Simple 1s-per-task stagger for a task's FIRST run - without this, every
// task sharing the same interval fires at exactly now+intervalMS on
// startup (Timers::addPeriodicTimer's default), and stays in lockstep
// forever after (each re-arm is relative to its own previous fire, so
// whatever phase the first run lands on persists indefinitely). Offset by
// config-file order (1s, 2s, 3s, ...), wrapped into [0, intervalMS) so it
// still means something once index*1000 would otherwise exceed a task's
// own interval.
void RecurringTasks::startTimers() {
    std::unique_lock<std::mutex> l(m_tasksLock);
    long long index = 0;
    for (auto& task : m_tasks) {
        if (!task.enabled) {
            continue;
        }
        long long stagger = (index * 1000LL) % task.intervalMS;
        Timers::INSTANCE.addPeriodicTimer(
            task.name, task.intervalMS, [this, task]() {
                runTask(task);
            },
            stagger);
        ++index;
    }
}

void RecurringTasks::Init() {
    load();
    startTimers();
}

void RecurringTasks::Close() {
    std::unique_lock<std::mutex> l(m_tasksLock);
    for (auto& task : m_tasks) {
        Timers::INSTANCE.stopPeriodicTimer(task.name);
    }
}

void RecurringTasks::Reload() {
    Close();
    load();
    startTimers();
}

void RecurringTasks::runTask(const RecurringTask& task) {
    if (task.type == "preset") {
        if (!task.preset.empty()) {
            CommandManager::INSTANCE.TriggerPreset(task.preset);
        }
        recordStatus(task.name, true, "");
        return;
    }
    if (task.type != "command" || task.command.empty()) {
        return;
    }

    // Refuse to overlap: a slow command (URL fetch, script, ...) still
    // in flight when the next tick fires would otherwise get a second
    // (third, fourth, ...) concurrent run stacked on top of it, with
    // nothing to stop that piling up indefinitely if the command
    // consistently takes longer than the task's own interval. Skip this
    // tick instead and say so loudly (log + Status column) - the fix is
    // a longer interval or a faster command, not silently pretending it
    // didn't happen.
    {
        std::unique_lock<std::mutex> l(m_pendingLock);
        for (auto const& p : m_pending) {
            if (p.taskName == task.name) {
                l.unlock();
                LogErr(VB_GENERAL, "RecurringTasks: \"%s\" skipped this run - the previous run hasn't finished yet "
                                   "(command is taking longer than the task's interval)\n",
                       task.name.c_str());
                recordStatus(task.name, false, "Skipped - previous run still in progress");
                return;
            }
        }
    }

    std::unique_ptr<Command::Result> result = CommandManager::INSTANCE.run(task.command, task.args);
    if (!result) {
        recordStatus(task.name, false, "command returned no result");
        return;
    }

    PendingResult pending;
    pending.result = std::move(result);
    pending.resultVariable = task.resultVariable;
    pending.persistResult = task.persistResult;
    pending.startTimeMS = GetTimeMS();
    pending.taskName = task.name;
    pending.filterType = task.filterType;
    pending.filterJsonPath = task.filterJsonPath;
    pending.filterBetweenStart = task.filterBetweenStart;
    pending.filterBetweenEnd = task.filterBetweenEnd;
    pending.filterRegex = task.filterRegex;

    if (pending.result->isDone()) {
        finish(pending);
        return;
    }

    std::unique_lock<std::mutex> l(m_pendingLock);
    m_pending.push_back(std::move(pending));
}

void RecurringTasks::finish(PendingResult& pending) {
    bool ok = !pending.result->isError();
    if (!pending.resultVariable.empty() && ok) {
        std::string filtered = ApplyResultFilter(pending.result->get(), pending.filterType, pending.filterJsonPath,
                                                 pending.filterBetweenStart, pending.filterBetweenEnd, pending.filterRegex);
        Variables::INSTANCE.setVariable(pending.resultVariable, filtered, pending.persistResult);
    }
    recordStatus(pending.taskName, ok, ok ? "" : pending.result->get());
}

Json::Value RecurringTasks::TestRunTask(const Json::Value& taskJson) {
    Json::Value result;
    RecurringTask task = ParseTask(taskJson);

    if (task.type == "preset") {
        if (!task.preset.empty()) {
            CommandManager::INSTANCE.TriggerPreset(task.preset);
        }
        if (!task.name.empty()) {
            recordStatus(task.name, true, "");
        }
        result["ok"] = true;
        result["raw"] = "";
        result["filtered"] = "";
        result["error"] = "";
        result["note"] = "Command Preset tasks have no output to filter.";
        return result;
    }

    if (task.type != "command" || task.command.empty()) {
        result["ok"] = false;
        result["raw"] = "";
        result["filtered"] = "";
        result["error"] = "No FPP Command selected";
        return result;
    }

    std::unique_ptr<Command::Result> r = CommandManager::INSTANCE.run(task.command, task.args);
    if (!r) {
        result["ok"] = false;
        result["raw"] = "";
        result["filtered"] = "";
        result["error"] = "command returned no result";
        return result;
    }

    // Same ~1s busy-poll convention as CommandManager's own synchronous
    // command execution (Commands.cpp) - this runs on an HTTP handler
    // thread, not fppd's main loop, so blocking briefly here is safe.
    int count = 0;
    while (!r->isDone() && count < 1000) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        count++;
    }
    if (!r->isDone()) {
        result["ok"] = false;
        result["raw"] = "";
        result["filtered"] = "";
        result["error"] = "Timeout running command";
        if (!task.name.empty()) {
            recordStatus(task.name, false, "Timeout running command");
        }
        return result;
    }

    bool ok = !r->isError();
    std::string raw = r->get();
    result["ok"] = ok;
    result["raw"] = raw;
    if (ok) {
        std::string filtered = ApplyResultFilter(raw, task.filterType, task.filterJsonPath,
                                                 task.filterBetweenStart, task.filterBetweenEnd, task.filterRegex);
        result["filtered"] = filtered;
        result["error"] = "";
        if (!task.resultVariable.empty()) {
            Variables::INSTANCE.setVariable(task.resultVariable, filtered, task.persistResult);
        }
    } else {
        result["filtered"] = "";
        result["error"] = raw;
    }
    if (!task.name.empty()) {
        recordStatus(task.name, ok, ok ? "" : raw);
    }
    return result;
}

void RecurringTasks::recordStatus(const std::string& name, bool ok, const std::string& err) {
    if (name.empty()) {
        return;
    }
    std::unique_lock<std::mutex> l(m_statusLock);
    TaskStatus& s = m_status[name];
    s.hasRun = true;
    s.lastRunMS = GetTimeMS();
    s.lastOk = ok;
    s.lastError = err;
}

void RecurringTasks::reportStatus(Json::Value& root) {
    std::unique_lock<std::mutex> tl(m_tasksLock);
    std::unique_lock<std::mutex> sl(m_statusLock);
    for (auto const& task : m_tasks) {
        Json::Value j;
        j["name"] = task.name;
        j["enabled"] = task.enabled;
        j["intervalMS"] = (Json::Int64)task.intervalMS;
        j["type"] = task.type;
        j["preset"] = task.preset;
        j["command"] = task.command;
        j["args"] = task.args;
        j["resultVariable"] = task.resultVariable;
        j["persistResult"] = task.persistResult;
        j["filterType"] = task.filterType;
        j["filterJsonPath"] = task.filterJsonPath;
        j["filterBetweenStart"] = task.filterBetweenStart;
        j["filterBetweenEnd"] = task.filterBetweenEnd;
        j["filterRegex"] = task.filterRegex;

        auto it = m_status.find(task.name);
        if (it != m_status.end()) {
            j["hasRun"] = it->second.hasRun;
            j["lastRunMS"] = (Json::Int64)it->second.lastRunMS;
            j["lastOk"] = it->second.lastOk;
            j["lastError"] = it->second.lastError;
        } else {
            j["hasRun"] = false;
            j["lastRunMS"] = 0;
            j["lastOk"] = true;
            j["lastError"] = "";
        }
        root.append(j);
    }
}

void RecurringTasks::tick() {
    std::vector<PendingResult> toFinish;
    {
        std::unique_lock<std::mutex> l(m_pendingLock);
        std::vector<PendingResult> stillPending;
        for (auto& pending : m_pending) {
            bool timedOut = (GetTimeMS() - pending.startTimeMS) > PENDING_RESULT_MAX_MS;
            if (pending.result->isDone() || timedOut) {
                if (timedOut && !pending.result->isDone()) {
                    LogWarn(VB_GENERAL, "RecurringTasks: a task's Result did not finish within %lldms, giving up\n", PENDING_RESULT_MAX_MS);
                }
                toFinish.push_back(std::move(pending));
            } else {
                stillPending.push_back(std::move(pending));
            }
        }
        m_pending = std::move(stillPending);
    }
    for (auto& pending : toFinish) {
        if (pending.result->isDone()) {
            finish(pending);
        }
        // else: timed out without finishing - drop without touching the
        // Variable, already logged above.
    }
}
