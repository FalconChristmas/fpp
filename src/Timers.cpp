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
#include <climits>
#include <list>
#include <mutex>
#include <string>
#include <map>
#include <vector>

#include "common.h"
#include "commands/Commands.h"

#include "EPollManager.h"
#include "Timers.h"

Timers Timers::INSTANCE;

Timers::Timers() {
}

// Named timers that fireTimersInternal() has copied out to fire and not yet
// fired, with an epoch bumped (under Timers::lock) whenever one is stopped or
// replaced.  fireTimersInternal() copies every due timer before running any of
// them, so without this a timer that an earlier callback in the same pass
// stopped -- a plugin's unload stopping its own periodic timer, then deleting
// the plugin -- still ran once from its copy.  An entry lives only while a
// copy is pending: a name per fired timer would grow without bound, since
// some timers take a fresh name every time (a per-play media stop, each pass
// of looping media).  Kept here rather than in TimerInfo because Timers.h is
// plugin ABI.
struct PendingFire {
    uint64_t epoch = 0;
    int copies = 0;
};
static std::map<std::string, PendingFire> s_pendingFires;

static void BumpTimerEpoch(const std::string& name) {
    auto it = s_pendingFires.find(name);
    if (it != s_pendingFires.end()) {
        it->second.epoch++;
    }
}

// A caller's GetTimeMS() deadline as a GetMonotonicTimeMS() one.
static long long WallToMonotonic(long long wallMS) {
    return GetMonotonicTimeMS() + (wallMS - GetTimeMS());
}

Timers::~Timers() {
    for (auto a : timers) {
        if (a) {
            delete a;
        }
    }
    timers.clear();
}
void Timers::addTimer(const std::string& name, long long fireTimeMS, std::function<void()>&& callback) {
    std::unique_lock<std::mutex> l(lock);
    if (!name.empty()) {
        for (auto& a : timers) {
            if (a && a->id == name) {
                BumpTimerEpoch(name);
                a->fireTimeMS = WallToMonotonic(fireTimeMS);
                a->callback = callback;
                a->commandPreset = "";
                updateTimers();
                return;
            }
        }
    }
    TimerInfo* i = new TimerInfo;
    i->id = name;
    i->callback = callback;
    i->fireTimeMS = WallToMonotonic(fireTimeMS);
    timers.push_back(i);
    updateTimers();
}
void Timers::addTimer(const std::string& name, long long fireTimeMS, const std::string& preset) {
    if (preset == "") {
        return;
    }
    std::unique_lock<std::mutex> l(lock);
    if (!name.empty()) {
        for (auto& a : timers) {
            if (a && a->id == name) {
                // A preset timer is a one-shot preset and nothing else: keeping
                // the old entry's callback and periodic rate turned a preset
                // aimed at an internal timer's id ("FileMonitor") into a preset
                // fired every 2s for good, and that timer's own work never ran.
                BumpTimerEpoch(name);
                a->fireTimeMS = WallToMonotonic(fireTimeMS);
                a->commandPreset = preset;
                a->callback = nullptr;
                a->periodicRate = 0;
                updateTimers();
                return;
            }
        }
    }
    TimerInfo* i = new TimerInfo;
    i->id = name;
    i->commandPreset = preset;
    i->fireTimeMS = WallToMonotonic(fireTimeMS);
    timers.push_back(i);
    updateTimers();
}

void Timers::addPeriodicTimer(const std::string& name, long long fireTimeMS, std::function<void()>&& callback) {
    // Kept as a distinct overload (rather than a default argument) so the original
    // 3-arg mangled symbol remains in libfpp.so for pre-built external plugins.
    addPeriodicTimer(name, fireTimeMS, std::move(callback), -1);
}
void Timers::addPeriodicTimer(const std::string& name, long long fireTimeMS, std::function<void()>&& callback,
                               long long initialDelayMS) {
    long long firstDelay = initialDelayMS < 0 ? fireTimeMS : initialDelayMS;
    std::unique_lock<std::mutex> l(lock);
    if (!name.empty()) {
        for (auto& a : timers) {
            if (a && a->id == name) {
                BumpTimerEpoch(name);
                a->fireTimeMS = GetMonotonicTimeMS() + firstDelay;
                a->periodicRate = fireTimeMS;
                a->callback = callback;
                a->commandPreset = "";
                updateTimers();
                return;
            }
        }
    }
    TimerInfo* i = new TimerInfo;
    i->id = name;
    i->callback = callback;
    i->fireTimeMS = GetMonotonicTimeMS() + firstDelay;
    i->periodicRate = fireTimeMS;
    timers.push_back(i);
    updateTimers();
}
void Timers::stopPeriodicTimer(const std::string& name) {
    std::unique_lock<std::mutex> l(lock);
    if (!name.empty()) {
        // Even if it isn't in the list: a one-shot already taken out to fire
        // in the current pass must not run after being stopped either.
        BumpTimerEpoch(name);
        for (int x = 0; x < timers.size(); ++x) {
            auto a = timers[x];
            if (a && a->id == name) {
                delete a;
                timers[x] = nullptr;
                updateTimers();
                return;
            }
        }
    }
}

void Timers::fireTimersInternal(long long t) {
    // Callbacks run with the lock released (they add and stop timers), so they
    // must not run out of a TimerInfo that is still in the list: a
    // stopPeriodicTimer() from another thread deletes it, and a periodic timer
    // that stops or re-adds itself from its own callback destroys or replaces
    // the std::function that is executing. Fire private copies instead.
    std::list<std::pair<TimerInfo, uint64_t>> toFire; // copy, epoch when copied

    std::unique_lock<std::mutex> l(lock);
    int m = timers.size();
    bool fired = false;
    for (int x = 0; x < m; ++x) {
        auto a = timers[x];
        if (a && (a->fireTimeMS <= t)) {
            uint64_t epoch = 0;
            if (!a->id.empty()) {
                PendingFire& pf = s_pendingFires[a->id];
                pf.copies++;
                epoch = pf.epoch;
            }
            if (a->periodicRate) {
                a->fireTimeMS = GetMonotonicTimeMS() + a->periodicRate;
                toFire.emplace_back(*a, epoch);
            } else {
                toFire.emplace_back(std::move(*a), epoch);
                delete a;
                timers[x] = nullptr;
            }
        }
    }
    l.unlock();
    // The list changed (one-shots taken out, periodic deadlines moved) even
    // if every callback below turns out to be skipped, so re-arm regardless.
    fired = !toFire.empty();
    for (auto& [a, epoch] : toFire) {
        if (!a.id.empty()) {
            std::unique_lock<std::mutex> el(lock);
            auto it = s_pendingFires.find(a.id);
            bool stale = it->second.epoch != epoch;
            if (--it->second.copies == 0) {
                s_pendingFires.erase(it);
            }
            if (stale) {
                continue; // stopped or replaced by an earlier callback this pass
            }
        }
        fireTimer(&a);
    }
    if (fired) {
        l.lock();
        updateTimers();
    }
}
void Timers::updateTimers() {
    for (int x = 0; x < timers.size(); x++) {
        while (x < timers.size() && timers[x] == nullptr) {
            timers[x] = timers[timers.size() - 1];
            timers.resize(timers.size() - 1);
        }
    }
    if (!timers.empty()) {
        // Worked out locally and stored once: fireTimers() reads nextTimer
        // without the lock.
        long long next = LLONG_MAX;
        for (int x = 0; x < timers.size(); x++) {
            next = std::min(timers[x]->fireTimeMS, next);
        }
        nextTimer = next;
        hasTimers = true;
        // armTimer() takes a GetTimeMS() deadline; converted at this moment
        // it is the same delay, whatever the wall clock does later.
        EPollManager::INSTANCE.armTimer(GetTimeMS() + (next - GetMonotonicTimeMS()));
    } else {
        hasTimers = false;
        EPollManager::INSTANCE.disarmTimer();
    }
}

void Timers::fireTimer(TimerInfo* t) {
    if (t->commandPreset.empty()) {
        t->callback();
    } else {
        LogDebug(VB_COMMAND, "\"Trigger Command Preset In Future\" identifier \"%s\" triggering preset \"%s\"\n", t->id.c_str(), t->commandPreset.c_str());
        CommandManager::INSTANCE.TriggerPreset(t->commandPreset);
    }
}
