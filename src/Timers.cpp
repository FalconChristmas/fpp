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
#include <climits>

#include "Timers.h"
#include "commands/Commands.h"

Timers Timers::INSTANCE;

Timers::Timers() {
}
Timers::~Timers() {
    for (auto a : timers) {
        if (a) {
            delete a;
        }
    }
    timers.clear();
}
void Timers::addTimer(const std::string &name, long long fireTimeMS, std::function<void()>& callback) {
    if (!name.empty()) {
        for (auto &a : timers) {
            if (a->id == name) {
                a->fireTimeMS = fireTimeMS;
                a->callback = callback;
                a->commandPreset = "";
                updateTimers();
                return;
            }
        }
    }
    TimerInfo *i = new TimerInfo;
    i->id = name;
    i->callback = callback;
    i->fireTimeMS = fireTimeMS;
    timers.push_back(i);
    updateTimers();
}
void Timers::addTimer(const std::string &name, long long fireTimeMS, const std::string &preset) {
    if (preset == "") {
        return;
    }
    if (!name.empty()) {
        for (auto &a : timers) {
            if (a->id == name) {
                a->fireTimeMS = fireTimeMS;
                a->commandPreset = preset;
                updateTimers();
                return;
            }
        }
    }
    TimerInfo *i = new TimerInfo;
    i->id = name;
    i->commandPreset = preset;
    i->fireTimeMS = fireTimeMS;
    timers.push_back(i);
    updateTimers();
}

void Timers::fireTimersInternal(long long t) {
    int m = timers.size();
    bool fired = false;
    for (int x = 0; x < m; ++x) {
        auto a = timers[x];
        if (a && (a->fireTimeMS < t)) {
            fireTimer(a);
            fired = true;
            delete a;
            timers[x] = nullptr;
        }
    }
    if (fired) {
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
    hasTimers = !timers.empty();
    if (hasTimers) {
        nextTimer = LLONG_MAX;
        for (int x = 0; x < timers.size(); x++) {
            nextTimer = std::min(timers[x]->fireTimeMS, nextTimer);
        }
    }
}

void Timers::fireTimer(TimerInfo *t) {
    if (t->commandPreset.empty()) {
        t->callback();
    } else {
        CommandManager::INSTANCE.TriggerPreset(t->commandPreset);
    }
}