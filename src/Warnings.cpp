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
#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <semaphore>
#include <set>
#include <stdio.h>
#include <string>
#include <thread>
#include <utility>

#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"

std::list<FPPWarning> WarningHolder::warnings;
std::shared_mutex WarningHolder::warningsLock;
std::mutex WarningHolder::listenerListLock;
std::set<WarningListener*> WarningHolder::listenerList;
uint64_t WarningHolder::timeToRemove = 0;

namespace
{
std::atomic_bool runNotifyThread{ false };
std::thread notifyThread;
std::binary_semaphore sema{ 0 };
}

FPPWarning::FPPWarning(int i, const std::string& m, const std::string& p) {
    (*this)["id"] = i;
    (*this)["message"] = m;
    if (!p.empty()) {
        (*this)["plugin"] = p;
    }
    timeout = -1;
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
    std::set<WarningListener*>::const_iterator it = listenerList.find(l);
    if (it != listenerList.end()) {
        listenerList.erase(it);
        LogDebug(VB_GENERAL, "Removing Warning Listener Complete\n");
    } else {
        LogWarn(VB_GENERAL, "Requested to remove Listener, but it wasn't found\n");
    }
}

void WarningHolder::StartNotifyThread() {
    runNotifyThread = true;
    notifyThread = std::thread(WarningHolder::NotifyListenersMain);
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

void WarningHolder::NotifyListenersMain() {
    SetThreadName("FPP-Warnings");
    UpdateWarningsAndNotify(false);
    WriteWarningsFile();
    while (runNotifyThread) {
        sema.acquire();
        if (!runNotifyThread) {
            return;
        }
        UpdateWarningsAndNotify(false);
        WriteWarningsFile();

        std::shared_lock<std::shared_mutex> lock(warningsLock);
        std::list<FPPWarning> copy(warnings);
        lock.unlock();

        LogDebug(VB_GENERAL, "Warning has changed, notifying other threads of %d Warnings\n", copy.size());
        std::unique_lock<std::mutex> llock(listenerListLock);
        std::for_each(listenerList.begin(), listenerList.end(), [&](WarningListener* l) { l->handleWarnings(copy); });
        llock.unlock();
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

void WarningHolder::AddWarningTimeout(int seconds, int id, const std::string& w, const std::map<std::string, std::string>& data, const std::string& plugin) {
    LogDebug(VB_GENERAL, "Adding Warning: %s\n", w.c_str());
    if (seconds != -1) {
        auto nowtime = std::chrono::steady_clock::now();
        int s = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
        seconds += s;
    }
    std::unique_lock<std::shared_mutex> lock(warningsLock);
    for (auto it = warnings.begin(); it != warnings.end(); ++it) {
        if (it->id() == id && it->message() == w) {
            return;
        }
    }
    auto& ref = warnings.emplace_back(id, w, plugin);
    if (!data.empty()) {
        for (auto& i : data) {
            ref["data"][i.first] = i.second;
        }
    }
    ref.timeout = seconds;
    if (seconds != -1) {
        if (seconds < timeToRemove || timeToRemove == 0) {
            timeToRemove = seconds;
        }
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
    timeToRemove = 0;
    lock.unlock();

    // Notify Listeners
    sema.release();
}

std::list<FPPWarning> WarningHolder::GetWarnings() {
    UpdateWarningsAndNotify(true);
    std::shared_lock<std::shared_mutex> lock(warningsLock);
    std::list<FPPWarning> copy(warnings);
    lock.unlock();

    return copy;
}
void WarningHolder::UpdateWarningsAndNotify(bool notify) {
    if (timeToRemove > 0) {
        bool madeChange = false;
        auto nowtime = std::chrono::steady_clock::now();
        int now = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
        if (now > timeToRemove) {
            std::unique_lock<std::shared_mutex> lock(warningsLock);
            int nt = 0;
            for (auto it = warnings.begin(); it != warnings.end();) {
                if (it->timeout > 0 && it->timeout < now) {
                    it = warnings.erase(it);
                    madeChange = true;
                } else {
                    if (it->timeout > 0 && (nt == 0 || it->timeout < nt)) {
                        nt = it->timeout;
                    }
                    ++it;
                }
            }
            timeToRemove = nt;
        }
        if (notify && madeChange) {
            // Notify Listeners
            sema.release();
        }
    }
}
