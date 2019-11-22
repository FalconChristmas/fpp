#include <list>
#include "Warnings.h"

std::map<std::string, int> WarningHolder::warnings;
std::mutex WarningHolder::warningsLock;




void WarningHolder::AddWarning(const std::string &w) {
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings[w] = -1;
}
void WarningHolder::AddWarningTimeout(const std::string &w, int toSeconds) {
    std::unique_lock<std::mutex> lock(warningsLock);
    auto nowtime = std::chrono::steady_clock::now();
    int seconds = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    warnings[w] = seconds + toSeconds;
}


void WarningHolder::RemoveWarning(const std::string &w) {
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings.erase(w);
}

void WarningHolder::AddWarningsToStatus(Json::Value &root) {
    auto nowtime = std::chrono::steady_clock::now();
    int now = std::chrono::duration_cast<std::chrono::seconds>(nowtime.time_since_epoch()).count();
    std::list<std::string> remove;
    std::unique_lock<std::mutex> lock(warningsLock);
    for (auto &a : warnings) {
        if (a.second == -1 || a.second > now) {
            root["warnings"].append(a.first);
        } else {
            remove.push_back(a.first);
        }
    }
    for (auto &a : remove) {
        warnings.erase(a);
    }
}

