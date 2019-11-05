
#include "Warnings.h"

std::set<std::string> WarningHolder::warnings;
std::mutex WarningHolder::warningsLock;

void WarningHolder::AddWarning(const std::string &w) {
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings.insert(w);
}
void WarningHolder::RemoveWarning(const std::string &w) {
    std::unique_lock<std::mutex> lock(warningsLock);
    warnings.erase(w);
}

void WarningHolder::AddWarningsToStatus(Json::Value &root) {
    std::unique_lock<std::mutex> lock(warningsLock);
    for (auto &a : warnings) {
        root["warnings"].append(a);
    }
}

