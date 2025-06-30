#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2025 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include <functional>
#include <map>
#include <mutex>
#include <string>

class FileMonitorInfo;

class FileMonitor {
public:
    static FileMonitor INSTANCE;

    void Initialize(std::map<int, std::function<bool(int)>>& callbacks);
    void Cleanup();

    FileMonitor& AddFile(const std::string& id, const std::string& file, const std::function<void()>& callback, bool modificationsOnly = false);
    FileMonitor& RemoveFile(const std::string& id, const std::string& file);

    FileMonitor& TriggerFileChanged(const std::string& file);

private:
    FileMonitor();
    ~FileMonitor();

    FileMonitor(const FileMonitor&) = delete;
    FileMonitor& operator=(const FileMonitor&) = delete;

    void fileChangedEvent();

    std::map<std::string, FileMonitorInfo> files_;
    std::map<int, std::string> fileMapping_;

    std::mutex mutex_;
    int inotify_fd_ = -1;
};
