#include "fpp-pch.h"

std::map<std::string, int> WarningHolder::warnings;
std::mutex WarningHolder::warningsLock;
std::mutex WarningHolder::listenerListLock;
std::mutex WarningHolder::notifyLock;
std::condition_variable WarningHolder::notifyCV;
std::thread notifyThread = std::thread(WarningHolder::NotifyListenersMain); 
std::set<WarningListener *> WarningHolder::listenerList;

void WarningHolder::AddWarningListener(WarningListener *l){
    std::unique_lock<std::mutex> lock(listenerListLock);
    listenerList.insert(l);
}

void WarningHolder::RemoveWarningListener(WarningListener *l){
    std::unique_lock<std::mutex> lock(listenerListLock);
    std::set<WarningListener *>::const_iterator it = listenerList.find(l);
    if (it != listenerList.end()) {
       listenerList.erase(it);
    } else {
       LogWarn(VB_GENERAL, "Requested to remove Listener, but it wasn't found\n");
    }
}

void WarningHolder::NotifyListenersMain() {
   std::unique_lock<std::mutex> lck(notifyLock);
   while(true) {
      notifyCV.wait(lck);  // sleep here
      std::list<std::string> warnings = WarningHolder::GetWarnings();
      LogDebug(VB_GENERAL, "Warning has changed, notifying other threads of %d Warnings\n", warnings.size());
      std::for_each(listenerList.begin(), listenerList.end(), [&](WarningListener *l) {l->handleWarnings(warnings); });
   }
}


void WarningHolder::AddWarning(const std::string &w) {
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings[w] = -1;
    // Notify Listeners
    std::unique_lock<std::mutex> lck(notifyLock);
    notifyCV.notify_all();
}
void WarningHolder::AddWarningTimeout(const std::string &w, int toSeconds) {
    std::unique_lock<std::mutex> lock(warningsLock);
    auto nowtime = std::chrono::steady_clock::now();
    int seconds = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    warnings[w] = seconds + toSeconds;
    // Notify Listeners
    std::unique_lock<std::mutex> lck(notifyLock);
    notifyCV.notify_all();
}


void WarningHolder::RemoveWarning(const std::string &w) {
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings.erase(w);
    // Notify Listeners
    std::unique_lock<std::mutex> lck(notifyLock);
    notifyCV.notify_all();
}
std::list<std::string> WarningHolder::GetWarnings() {
    std::list<std::string> ret;
    bool madeChange = false;
    auto nowtime = std::chrono::steady_clock::now();
    int now = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    std::list<std::string> remove;
    std::unique_lock<std::mutex> lock(warningsLock);
    for (auto &a : warnings) {
        if (a.second == -1 || a.second > now) {
            ret.push_back(a.first);
        } else {
            remove.push_back(a.first);
        }
    }
    for (auto &a : remove) {
        warnings.erase(a);
	madeChange = true;
    }

    if (madeChange) {
        // Notify Listeners
        std::unique_lock<std::mutex> lck(notifyLock);
        notifyCV.notify_all();
    }
    return ret;
}

