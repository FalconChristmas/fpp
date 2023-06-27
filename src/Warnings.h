#pragma once
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

#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <thread>

class WarningListener {
public:
    virtual void handleWarnings(std::list<std::string>& warnings) = 0;
};

class WarningHolder {
public:
    static void AddWarning(const std::string& w);
    static void RemoveWarning(const std::string& w);
    static void AddWarningTimeout(const std::string& w, int seconds);

    static void AddWarningListener(WarningListener* l);
    static void RemoveWarningListener(WarningListener* l);

    static std::list<std::string> GetWarnings();
    static void StartNotifyThread();
    static void StopNotifyThread();
    static void NotifyListenersMain(); // main for notify thread

private:
    static std::mutex warningsLock;
    static std::mutex notifyLock;
    static std::condition_variable notifyCV;
    static std::map<std::string, int> warnings;
    static std::thread* notifyThread;
    static std::set<WarningListener*> listenerList;
    static std::mutex listenerListLock;
    static volatile bool runNotifyThread;


    static void writeWarningsFile(const std::list<std::string> &warnings);
    static std::list<std::string> GetWarningsAndNotify(bool notify);

};
