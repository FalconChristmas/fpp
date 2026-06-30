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
FileMonitor& FileMonitor::AddFile(const std::string& id, const std::string& file, const std::function<void()>& callback, bool modificationsOnly) {
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
            // IN_MOVED_TO catches files that appear via an atomic rename
            // (write-temp-then-rename), which is how config files are written so
            // a reader never sees a partially-written file. Without it, watchers
            // on rename-replaced files would silently stop firing.
            int inotify_watch_fd = inotify_add_watch(inotify_fd_, dir.c_str(), IN_CREATE | IN_DELETE | IN_MOVED_TO);
            fileMapping_[inotify_watch_fd] = dir;
            files_[dir].inotify_watch_fd = inotify_watch_fd;
            files_[dir].isDirectory = true; // Mark as directory for create/delete events
        }
    }
    if (FileExists(file) && fileInfo.inotify_watch_fd < 0) {
        int inotify_watch_fd = inotify_add_watch(inotify_fd_, file.c_str(), IN_MODIFY);
        if (inotify_watch_fd < 0) {
            return *this;
        }
        fileMapping_[inotify_watch_fd] = file;
        fileInfo.inotify_watch_fd = inotify_watch_fd;
    }
#endif
    return *this;
}
FileMonitor& FileMonitor::RemoveFile(const std::string& id, const std::string& file) {
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
    return *this;
}

FileMonitor& FileMonitor::TriggerFileChanged(const std::string& file) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = files_.find(file);
    if (it != files_.end()) {
        for (const auto& callback : it->second.callbacks) {
            callback.second();
        }
    }
    return *this;
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

        // printf("%d) %s: (%s)    id: %d   mask: %X\n", buffer_i, path.c_str(), pevent->name, pevent->wd, pevent->mask);

        if (pevent->mask & IN_IGNORED) {
            // The kernel auto-removed this watch -- e.g. the file was deleted, or
            // it was replaced via an atomic rename (write-temp-then-rename), which
            // unlinks the old inode. Drop our stale wd mapping and clear the
            // file's watch descriptor so a subsequent IN_CREATE/IN_MOVED_TO for
            // the same name re-arms an inode watch on the new file.
            auto mit = fileMapping_.find(pevent->wd);
            if (mit != fileMapping_.end()) {
                auto fit = files_.find(mit->second);
                if (fit != files_.end() && fit->second.inotify_watch_fd == pevent->wd) {
                    fit->second.inotify_watch_fd = -1;
                }
                fileMapping_.erase(mit);
            }
            continue;
        } else if (pevent->mask & (IN_CREATE | IN_MOVED_TO)) {
            // A watched file appeared: either created in place (IN_CREATE) or
            // atomically renamed into place (IN_MOVED_TO). For a rename over an
            // existing file the previous inode watch is gone (its IN_IGNORED is
            // handled above or still queued), so re-arm an IN_MODIFY watch on the
            // current inode before notifying.
            path += pevent->name;
            auto it = files_.find(path);
            if (it != files_.end()) {
                bool notify = false;
                if (pevent->mask & IN_MOVED_TO) {
                    // New inode in place: drop any stale watch on the replaced
                    // inode (the kernel auto-removes it) and always notify.
                    if (it->second.inotify_watch_fd >= 0) {
                        fileMapping_.erase(it->second.inotify_watch_fd);
                        it->second.inotify_watch_fd = -1;
                    }
                    notify = true;
                }
                if (it->second.inotify_watch_fd < 0) {
                    int wd = inotify_add_watch(inotify_fd_, path.c_str(), IN_MODIFY);
                    if (wd >= 0) {
                        it->second.inotify_watch_fd = wd;
                        fileMapping_[wd] = path;
                    }
                    notify = true;
                }
                if (notify) {
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
        if (buffer_i >= r) {
            r = read(inotify_fd_, buffer, sizeof(buffer));
            if (r <= 0) {
                break; // No more events to process
            }
            buffer_i = 0;
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