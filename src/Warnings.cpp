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
#include <chrono>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <stdio.h>
#include <string>
#include <thread>
#include <utility>

#include "Warnings.h"
#include "common.h"
#include "log.h"
#include "settings.h"

std::map<std::string, int> WarningHolder::warnings;
std::mutex WarningHolder::warningsLock;
std::mutex WarningHolder::listenerListLock;
std::mutex WarningHolder::notifyLock; // Must always lock this before WarningLock
std::condition_variable WarningHolder::notifyCV;
std::thread* WarningHolder::notifyThread = nullptr;
volatile bool WarningHolder::runNotifyThread = false;
std::set<WarningListener*> WarningHolder::listenerList;

void WarningHolder::AddWarningListener(WarningListener* l) {
    LogDebug(VB_GENERAL, "About to add listner\n");
    std::unique_lock<std::mutex> lock(listenerListLock);
    listenerList.insert(l);
    LogDebug(VB_GENERAL, "Adding Listner Done\n");
}

void WarningHolder::RemoveWarningListener(WarningListener* l) {
    std::unique_lock<std::mutex> lock(listenerListLock);
    std::set<WarningListener*>::const_iterator it = listenerList.find(l);
    if (it != listenerList.end()) {
        listenerList.erase(it);
        LogDebug(VB_GENERAL, "Removing Warning Listner Complete\n");
    } else {
        LogWarn(VB_GENERAL, "Requested to remove Listener, but it wasn't found\n");
    }
}

void WarningHolder::StartNotifyThread() {
    runNotifyThread = true;
    notifyThread = new std::thread(WarningHolder::NotifyListenersMain);
}

void WarningHolder::StopNotifyThread() {
    runNotifyThread = false;
    std::unique_lock<std::mutex> lck(notifyLock);
    notifyCV.notify_all();
    lck.unlock();
    notifyThread->join();
}
void WarningHolder::writeWarningsFile(const std::list<std::string>& warnings) {
    Json::Value result = Json::Value(Json::ValueType::arrayValue);
    for (auto& warn : warnings) {
        result.append(warn);
    }
    SaveJsonToFile(result, getFPPDDir("/www/warnings.json"));
}
void WarningHolder::writeWarningsFile(const std::string& s) {
    PutFileContents(getFPPDDir("/www/warnings.json"), s);
}
void WarningHolder::clearWarningsFile() {
    remove(getFPPDDir("/www/warnings.json").c_str());
}

void WarningHolder::NotifyListenersMain() {
    std::unique_lock<std::mutex> lck(notifyLock);
    writeWarningsFile(WarningHolder::GetWarningsAndNotify(false));
    while (runNotifyThread) {
        notifyCV.wait(lck); // sleep here
        if (!runNotifyThread) {
            return;
        }
        std::list<std::string> warnings = WarningHolder::GetWarningsAndNotify(false); // Calls warning Lock
        writeWarningsFile(warnings);
        LogDebug(VB_GENERAL, "Warning has changed, notifying other threads of %d Warnings\n", warnings.size());
        std::for_each(listenerList.begin(), listenerList.end(), [&](WarningListener* l) { l->handleWarnings(warnings); });
    }
}

void WarningHolder::AddWarning(const std::string& w) {
    LogDebug(VB_GENERAL, "Adding Warning: %s\n", w.c_str());
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings[w] = -1;
    // Notify Listeners
    notifyCV.notify_all();
}
void WarningHolder::AddWarningTimeout(const std::string& w, int toSeconds) {
    LogDebug(VB_GENERAL, "Adding Warning with Timeout: %s\n", w.c_str());
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::mutex> lock(warningsLock);
    auto nowtime = std::chrono::steady_clock::now();
    int seconds = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    warnings[w] = seconds + toSeconds;
    // Notify Listeners
    notifyCV.notify_all();
}

void WarningHolder::RemoveWarning(const std::string& w) {
    LogDebug(VB_GENERAL, "Removing Warning: %s\n", w.c_str());
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings.erase(w);
    // Notify Listeners
    notifyCV.notify_all();
}

std::list<std::string> WarningHolder::GetWarnings() {
    return GetWarningsAndNotify(true);
}
std::list<std::string> WarningHolder::GetWarningsAndNotify(bool notify) {
    std::list<std::string> ret;
    bool madeChange = false;
    auto nowtime = std::chrono::steady_clock::now();
    int now = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    std::list<std::string> remove;
    std::unique_lock<std::mutex> lock(warningsLock);
    for (auto& a : warnings) {
        if (a.second == -1 || a.second > now) {
            ret.push_back(a.first);
        } else {
            remove.push_back(a.first);
        }
    }
    for (auto& a : remove) {
        warnings.erase(a);
        madeChange = true;
    }

    if (notify && madeChange) {
        // Notify Listeners
        std::unique_lock<std::mutex> lck(notifyLock);
        notifyCV.notify_all();
    }
    return ret;
}
