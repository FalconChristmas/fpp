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

#include <algorithm>
#include <atomic>
#include <mutex>
#include <semaphore>
#include <set>
#include <shared_mutex>
#include <stdio.h>
#include <thread>
#include <utility>

#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"

using namespace std::chrono;

namespace
{
    std::list<FPPWarning> warnings;
    std::shared_mutex warningsLock;
    std::mutex listenerListLock;
    std::set<WarningListener*> listenerList;
    steady_clock::time_point wakeUpTime = steady_clock::time_point::max();
    std::atomic_bool runNotifyThread{ false };
    std::thread notifyThread;
    std::binary_semaphore sema{ 0 };
}

static void PruneWarnings();

FPPWarning::FPPWarning(int i, const std::string& m, const std::string& p) {
    (*this)["id"] = i;
    (*this)["message"] = m;
    if (!p.empty()) {
        (*this)["plugin"] = p;
    }
    timeout = steady_clock::time_point::max();
}

std::string FPPWarning::message() const {
    return (*this)["message"].asString();
}
std::string FPPWarning::plugin() const {
    if (this->isMember("plugin")) {
        return (*this)["plugin"].asString();
    }
    return "";
}

int FPPWarning::id() const {
    return (*this)["id"].asInt();
}

void WarningHolder::AddWarningListener(WarningListener* l) {
    LogDebug(VB_GENERAL, "About to add listener\n");
    std::unique_lock<std::mutex> lock(listenerListLock);
    listenerList.insert(l);
    LogDebug(VB_GENERAL, "Adding Listener Done\n");
}

void WarningHolder::RemoveWarningListener(WarningListener* l) {
    std::unique_lock<std::mutex> lock(listenerListLock);
    if (listenerList.erase(l)) {
        LogDebug(VB_GENERAL, "Removing Warning Listener Complete\n");
    } else {
        LogWarn(VB_GENERAL, "Requested to remove Listener, but it wasn't found\n");
    }
}

void WarningHolder::StartNotifyThread() {
    runNotifyThread = true;
    notifyThread = std::thread(WarningHolder::WarningsThreadMain);
}

void WarningHolder::StopNotifyThread() {
    runNotifyThread = false;
    sema.release();
    notifyThread.join();
}
void WarningHolder::WriteWarningsFile() {
    std::shared_lock<std::shared_mutex> lock(warningsLock);
    Json::Value result = Json::Value(Json::ValueType::arrayValue);
    Json::Value resultFull = Json::Value(Json::ValueType::arrayValue);
    for (auto& warn : warnings) {
        result.append(warn.message());
        resultFull.append(warn);
    }
    SaveJsonToFile(result, getFPPDDir("/www/warnings.json"));
    SaveJsonToFile(resultFull, getFPPDDir("/www/warnings_full.json"));
}
void WarningHolder::ClearWarningsFile() {
    remove(getFPPDDir("/www/warnings.json").c_str());
    remove(getFPPDDir("/www/warnings_full.json").c_str());
}

void WarningHolder::WarningsThreadMain() {
    SetThreadName("FPP-Warnings");
    PruneWarnings();
    WriteWarningsFile();
    while (runNotifyThread) {
        sema.try_acquire_until(wakeUpTime);
        if (!runNotifyThread) {
            break;
        }
        PruneWarnings();
        WriteWarningsFile();

        auto copy = GetWarnings();

        LogDebug(VB_GENERAL, "Warnings may have changed; notifying other threads of %zu Warnings\n", copy.size());

        std::unique_lock lock(listenerListLock);
        std::for_each(listenerList.begin(), listenerList.end(), [&](WarningListener* l) { l->handleWarnings(copy); });
    }
}

void WarningHolder::AddWarning(const std::string& w) {
    AddWarningTimeout(-1, UNKNOWN_WARNING_ID, w);
}
void WarningHolder::AddWarningTimeout(const std::string& w, int seconds) {
    AddWarningTimeout(seconds, UNKNOWN_WARNING_ID, w);
}
void WarningHolder::RemoveWarning(const std::string& w) {
    RemoveWarning(UNKNOWN_WARNING_ID, w);
}
void WarningHolder::AddWarning(int id, const std::string& w, const std::map<std::string, std::string>& data) {
    AddWarningTimeout(-1, id, w, data);
}

void WarningHolder::AddWarningTimeout(int sec, int id, const std::string& w, const std::map<std::string, std::string>& data, const std::string& plugin) {
    LogDebug(VB_GENERAL, "Adding Warning: %s\n", w.c_str());
    steady_clock::time_point timeoutTime = steady_clock::time_point::max();
    if (sec != -1) {
        timeoutTime = steady_clock::now() + seconds(sec);
    }
    std::unique_lock<std::shared_mutex> lock(warningsLock);
    for (auto it = warnings.begin(); it != warnings.end(); ++it) {
        if (it->id() == id && it->message() == w) {
            // Timeout is allowed to move later, but not earlier
            if (timeoutTime > it->timeout)
                it->timeout = timeoutTime;
            return;
        }
    }

    auto& ref = warnings.emplace_back(id, w, plugin);
    if (!data.empty()) {
        for (auto& i : data) {
            ref["data"][i.first] = i.second;
        }
    }

    ref.timeout = timeoutTime;
    if (timeoutTime < wakeUpTime) {
        wakeUpTime = timeoutTime;
    }
    lock.unlock();

    // Notify Listeners
    sema.release();
}
void WarningHolder::RemoveWarning(int id, const std::string& w, const std::string& plugin) {
    LogDebug(VB_GENERAL, "Removing Warning: %s\n", w.c_str());
    std::unique_lock<std::shared_mutex> lock(warningsLock);
    for (auto it = warnings.begin(); it != warnings.end(); ++it) {
        if (it->id() == id && it->message() == w && it->plugin() == plugin) {
            warnings.erase(it);
            lock.unlock();
            // Notify Listeners
            sema.release();
            return;
        }
    }
}
void WarningHolder::RemoveAllWarnings() {
    std::unique_lock<std::shared_mutex> lock(warningsLock);
    warnings.clear();
    wakeUpTime = steady_clock::time_point::max();
    lock.unlock();

    // Notify Listeners
    sema.release();
}

std::list<FPPWarning> WarningHolder::GetWarnings() {
    std::shared_lock<std::shared_mutex> lock(warningsLock);
    return warnings;
}

void PruneWarnings() {
    // warnings are not stored in a heap or sorted, so we must walk over the warnings to find expired
    std::unique_lock lock(warningsLock);
    wakeUpTime = steady_clock::time_point::max();
    if (!warnings.empty()) {
        auto now = steady_clock::now();
        for (auto it = warnings.begin(); it != warnings.end(); /* in loop */) {
            if (it->timeout <= now) {
                // warning has expired
                it = warnings.erase(it);
                continue;
            } else if (it->timeout < wakeUpTime) {
                // our next warning
                wakeUpTime = it->timeout;
            }
            ++it;
        }
    }
}
