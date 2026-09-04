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

    // Fire every callback registered against `file`.  This is what an inotify
    // event does, so it is the right call when the file really did change.
    FileMonitor& TriggerFileChanged(const std::string& file);

    // Fire only the callback registered under `id`.  This is the one to use for
    // the AddFile(...).TriggerFileChanged(...) "prime it now" idiom: the whole-
    // file overload above also runs every OTHER watcher's callback, so a second
    // registrant re-runs work the first one already did at startup.  That was
    // costing a duplicate pass over co-universes.json -- including its blocking
    // hostname resolution -- every time a new watcher was added to the file.
    FileMonitor& TriggerFileChanged(const std::string& id, const std::string& file);

private:
    FileMonitor();
    ~FileMonitor();

    FileMonitor(const FileMonitor&) = delete;
    FileMonitor& operator=(const FileMonitor&) = delete;

    void fileChangedEvent();
    // `id` empty fires every callback on `file`, otherwise just that one.
    void fireCallbacks(const std::string& id, const std::string& file);

    std::map<std::string, FileMonitorInfo> files_;
    std::map<int, std::string> fileMapping_;

    std::mutex mutex_;
    // Serializes callback invocation without holding mutex_ across the
    // callbacks themselves, so Add/RemoveFile stay callable while (or after)
    // a callback runs -- including from the process exit path if a callback
    // crashes.
    std::mutex callbackMutex_;
    int inotify_fd_ = -1;
};
