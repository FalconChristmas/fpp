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

// PLUGIN API: native plugins compile against this header and are loaded with
// dlopen().
//   - Plugins call Timers::INSTANCE.addPeriodicTimer()/stopPeriodicTimer()
//     by symbol.
// Data layout here is NOT plugin ABI, so members can change freely. Changing
// or removing a signature a plugin calls is: keep the old one as an overload
// (as Timers::addPeriodicTimer does) or bump FPP_PLUGIN_API_VERSION in
// Plugin.h. See .claude/PLUGIN-ABI.md for which plugins use what.

#include <atomic>
#include <mutex>
#include <vector>
#include <functional>

extern long long GetTimeMS();
extern long long GetMonotonicTimeMS();

class Timers {
public:
    static Timers INSTANCE;

    Timers();
    ~Timers();

    void fireTimers() {
        if (hasTimers) {
            long long t = GetMonotonicTimeMS();
            if (t >= nextTimer) {
                fireTimersInternal(t);
            }
        }
    }

    // fireTimeMS is a deadline on the GetTimeMS() wall clock, as callers
    // (plugins included) have always passed: "GetTimeMS() + delay".  It is
    // turned into a delay on arrival, and the timer then runs on the
    // monotonic clock, so a later clock step (NTP, an RTC that boots in the
    // past) doesn't hold it back for the size of the step or fire it early.
    void addTimer(const std::string& name, long long fireTimeMS, std::function<void()>&& callback);
    void addTimer(const std::string& name, long long fireTimeMS, const std::string& preset);

    // initialDelayMS lets the FIRST fire land somewhere other than exactly
    // fireTimeMS from now, without changing the recurring rate (periodicRate
    // stays fireTimeMS regardless) - e.g. RecurringTasks::startTimers() uses
    // this to stagger tasks that share the same interval so they don't all
    // fire in lockstep forever. Defaults to fireTimeMS (original behavior)
    // when omitted.
    void addPeriodicTimer(const std::string& name, long long fireTimeMS, std::function<void()>&& callback);
    void addPeriodicTimer(const std::string& name, long long fireTimeMS, std::function<void()>&& callback,
                           long long initialDelayMS);
    void stopPeriodicTimer(const std::string& name);

private:
    // Read by fireTimers() on the main loop without the lock; written under
    // it by updateTimers() from whichever thread adds or stops a timer.
    std::atomic<bool> hasTimers{ false };
    std::atomic<long long> nextTimer{ 0 };

    class TimerInfo {
    public:
        TimerInfo() {}
        ~TimerInfo() {}

        std::string id;
        long long fireTimeMS; // GetMonotonicTimeMS() deadline
        long long periodicRate = 0;
        std::string commandPreset;
        std::function<void()> callback;
    };
    std::vector<TimerInfo*> timers;
    std::mutex lock;

    void fireTimersInternal(long long t);

    // these methods must be called with the lock held
    void fireTimer(TimerInfo*);
    void updateTimers();
};
