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

std::list<FPPWarning> WarningHolder::warnings;
std::recursive_mutex WarningHolder::warningsLock;
std::mutex WarningHolder::listenerListLock;
std::mutex WarningHolder::notifyLock; // Must always lock this before WarningLock
std::condition_variable WarningHolder::notifyCV;
std::thread* WarningHolder::notifyThread = nullptr;
volatile bool WarningHolder::runNotifyThread = false;
std::set<WarningListener*> WarningHolder::listenerList;



FPPWarning::FPPWarning(int i, const std::string &m, const std::string &p) {
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
void WarningHolder::WriteWarningsFile() {
    std::unique_lock<std::recursive_mutex> lock(warningsLock);    
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
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::recursive_mutex> lock(warningsLock);
    UpdateWarningsAndNotify(false);
    WriteWarningsFile();
    lock.unlock();
    while (runNotifyThread) {
        notifyCV.wait(lck); // sleep here
        if (!runNotifyThread) {
            return;
        }
        lock.lock();
        UpdateWarningsAndNotify(false);
        WriteWarningsFile();
        std::list<FPPWarning> copy(warnings);
        lock.unlock();
        LogDebug(VB_GENERAL, "Warning has changed, notifying other threads of %d Warnings\n", copy.size());
        std::for_each(listenerList.begin(), listenerList.end(), [&](WarningListener* l) { l->handleWarnings(copy); });
    }
}

void WarningHolder::AddWarning(const std::string& w) {
    AddWarningTimeout(-1, UNKNOWN_WARNING_ID, w);
}
void WarningHolder::AddWarningTimeout(const std::string& w, int seconds) {
    AddWarningTimeout(seconds, UNKNOWN_WARNING_ID, w);
}
void WarningHolder::RemoveWarning(const std::string& w){
    RemoveWarning(UNKNOWN_WARNING_ID, w);
}
void WarningHolder::AddWarning(int id, const std::string& w, const std::map<std::string, std::string>& data) {
    AddWarningTimeout(-1, id, w, data);
}

void WarningHolder::AddWarningTimeout(int seconds, int id, const std::string& w, const std::map<std::string, std::string>& data, const std::string &plugin) {
    LogDebug(VB_GENERAL, "Adding Warning: %s\n", w.c_str());
    if (seconds != -1) {
        auto nowtime = std::chrono::steady_clock::now();
        int s = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
        seconds += s;
    }
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::recursive_mutex> lock(warningsLock);
    for (auto it = warnings.begin(); it != warnings.end(); ++it) {
        if (it->id() == id && it->message() == w) {
            return;
        }
    }
    auto& ref = warnings.emplace_back(id, w, plugin);
    if (!data.empty()) {
        for (auto &i : data) {
            ref["data"][i.first] = i.second;
        }
    }
    ref.timeout = seconds;
    // Notify Listeners
    notifyCV.notify_all();
}
void WarningHolder::RemoveWarning(int id, const std::string& w, const std::string &plugin) {
    LogDebug(VB_GENERAL, "Removing Warning: %s\n", w.c_str());
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::recursive_mutex> lock(warningsLock);
    for (auto it = warnings.begin(); it != warnings.end(); ++it) {
        if (it->id() == id && it->message() == w && it->plugin() == plugin) {
            warnings.erase(it);
            // Notify Listeners
            notifyCV.notify_all();
            return;
        }
    }
}
void WarningHolder::RemoveAllWarnings() {
    std::unique_lock<std::mutex> lck(notifyLock);
    std::unique_lock<std::recursive_mutex> lock(warningsLock);
    warnings.clear();
    // Notify Listeners
    notifyCV.notify_all();
}

std::list<FPPWarning> WarningHolder::GetWarnings() {
    std::unique_lock<std::recursive_mutex> lock(warningsLock);
    UpdateWarningsAndNotify(true);
    std::list<FPPWarning> copy(warnings);
    lock.unlock();
    return copy;
}
void WarningHolder::UpdateWarningsAndNotify(bool notify) {
    bool madeChange = false;
    auto nowtime = std::chrono::steady_clock::now();
    int now = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    std::unique_lock<std::recursive_mutex> lock(warningsLock);
    for (auto it = warnings.begin(); it != warnings.end();) {
        if (it->timeout > 0 && it->timeout < now) {
            it = warnings.erase(it);
            madeChange = true;
        } else {
            ++it;
        }
    }
    if (notify && madeChange) {
        // Notify Listeners
        std::unique_lock<std::mutex> lck(notifyLock);
        notifyCV.notify_all();
    }
}
