/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#if __has_include(<sys/inotify.h>)
#define HAS_INOTIFY 1
#include <sys/inotify.h>
#endif

#include <cstring>
#include <unistd.h>

#include "FileMonitor.h"
#include "Timers.h"
#include "common_mini.h"

class FileMonitorInfo {
public:
    FileMonitorInfo() = default;
    ~FileMonitorInfo() = default;

    bool checkForChanges(const std::string& fn) {
        uint64_t newTimestamp = FileTimestamp(fn);
        if (newTimestamp != timestamp) {
            timestamp = newTimestamp;
            return true; // File has changed
        }
        return false; // No change
    }

    uint64_t timestamp = 0;    // Last modified timestamp
    int inotify_watch_fd = -1; // Inotify watch descriptor
    bool isDirectory = false;  // Is this a directory, just looking for create/delete events

    std::map<std::string, std::function<void()>> callbacks; // Callbacks for this file
};

FileMonitor FileMonitor::INSTANCE;
FileMonitor::FileMonitor() :
    inotify_fd_(-1) {
#ifdef HAS_INOTIFY
    inotify_fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#endif
}
FileMonitor::~FileMonitor() {
#ifdef HAS_INOTIFY
    if (inotify_fd_ >= 0) {
        close(inotify_fd_);
        inotify_fd_ = -1;
    }
#endif
}

void FileMonitor::Initialize(std::map<int, std::function<bool(int)>>& callbacks) {
    if (inotify_fd_ >= 0) {
        callbacks[inotify_fd_] = [this](int fd) {
            this->fileChangedEvent();
            return false;
        };
    } else {
        Timers::INSTANCE.addPeriodicTimer("FileMonitor", 2000, [this]() {
            this->fileChangedEvent();
        });
    }
}
void FileMonitor::Cleanup() {
#ifdef HAS_INOTIFY
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& entry : fileMapping_) {
        inotify_rm_watch(inotify_fd_, entry.first);
    }
#endif
    if (inotify_fd_ < 0) {
        Timers::INSTANCE.stopPeriodicTimer("FileMonitor");
    }
    fileMapping_.clear();
    files_.clear();
}
void FileMonitor::AddFile(const std::string& id, const std::string& file, const std::function<void()>& callback, bool modificationsOnly) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = files_.find(file);
    if (it == files_.end()) {
        files_[file].checkForChanges(file);
    }
    auto& fileInfo = files_[file];
    fileInfo.callbacks[id] = callback;
#ifdef HAS_INOTIFY
    if (!modificationsOnly) {
        std::string dir = file.substr(0, file.find_last_of("/") + 1);
        auto dit = files_.find(dir);
        if (dit == files_.end()) {
            int inotify_watch_fd = inotify_add_watch(inotify_fd_, dir.c_str(), IN_CREATE | IN_DELETE);
            fileMapping_[inotify_watch_fd] = dir;
            files_[dir].inotify_watch_fd = inotify_watch_fd;
            files_[dir].isDirectory = true; // Mark as directory for create/delete events
        }
    }
    if (FileExists(file) && fileInfo.inotify_watch_fd < 0) {
        int inotify_watch_fd = inotify_add_watch(inotify_fd_, file.c_str(), IN_MODIFY);
        if (inotify_watch_fd < 0) {
            return;
        }
        fileMapping_[inotify_watch_fd] = file;
        fileInfo.inotify_watch_fd = inotify_watch_fd;
    }
#endif
}
void FileMonitor::RemoveFile(const std::string& id, const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(file);
    if (it != files_.end()) {
        it->second.callbacks.erase(id);
        if (it->second.callbacks.empty() && !it->second.isDirectory) {
            // If no callbacks left, remove the file from monitoring
#ifdef HAS_INOTIFY
            if (it->second.inotify_watch_fd >= 0) {
                inotify_rm_watch(inotify_fd_, it->second.inotify_watch_fd);
                fileMapping_.erase(it->second.inotify_watch_fd);
            }
#endif
            files_.erase(it);
        }
    }
}

void FileMonitor::fileChangedEvent() {
#ifdef HAS_INOTIFY
    char buffer[16384];
    size_t buffer_i;
    struct inotify_event* pevent;
    ssize_t r;
    size_t event_size;

    memset(buffer, 0, sizeof(buffer));
    r = read(inotify_fd_, buffer, sizeof(buffer));
    if (r <= 0) {
        return;
    }
    buffer_i = 0;
    while (buffer_i < r) {
        /* Parse events and queue them. */
        pevent = (struct inotify_event*)&buffer[buffer_i];
        event_size = offsetof(struct inotify_event, name) + pevent->len;
        buffer_i += event_size;

        std::lock_guard<std::mutex> lock(mutex_);
        std::string path = fileMapping_[pevent->wd];

        // printf("%d) %s: (%s)   mask: %X\n", buffer_i, path.c_str(), pevent->name, pevent->mask);

        if (pevent->mask & IN_IGNORED) {
            // The watch was removed, ignore this event
            continue;
        } else if (pevent->mask & IN_CREATE) {
            path += pevent->name;
            auto it = files_.find(path);
            if (it != files_.end()) {
                if (it->second.inotify_watch_fd < 0) {
                    it->second.inotify_watch_fd = inotify_add_watch(inotify_fd_, path.c_str(), IN_MODIFY);
                    fileMapping_[it->second.inotify_watch_fd] = path;
                    for (const auto& callback : it->second.callbacks) {
                        callback.second();
                    }
                }
            }
        } else if (pevent->mask & IN_DELETE) {
            path += pevent->name;
            auto it = files_.find(path);
            if (it != files_.end()) {
                if (it->second.inotify_watch_fd >= 0) {
                    inotify_rm_watch(inotify_fd_, it->second.inotify_watch_fd);
                    fileMapping_.erase(it->second.inotify_watch_fd);
                    it->second.inotify_watch_fd = -1; // Reset watch descriptor
                    for (const auto& callback : it->second.callbacks) {
                        callback.second();
                    }
                }
            }
        } else if (pevent->mask & IN_MODIFY) {
            auto it = files_.find(path);
            if (it != files_.end()) {
                for (const auto& callback : it->second.callbacks) {
                    callback.second();
                }
            }
        }
    }
#else
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& fileEntry : files_) {
        const std::string& file = fileEntry.first;
        FileMonitorInfo& fileInfo = fileEntry.second;
        if (fileInfo.checkForChanges(file)) {
            for (const auto& callback : fileInfo.callbacks) {
                callback.second();
            }
        }
    }
#endif
}