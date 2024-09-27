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

#include <functional>
#include <map>
#include <string>

int ping(std::string target, int timeoutms);

class PingManager {
public:
    class PingInfo;

    static PingManager INSTANCE;

    PingManager();
    ~PingManager();

    void Initialize();
    void Cleanup();

    void ping(const std::string& target, int timeout, std::function<void(int)>&& callback);

    void addPeriodicPing(const std::string& target, int timeout, int period, std::function<void(int)>&& callback);
    void removePeriodicPing(const std::string& target);

private:
    PingInfo* info = nullptr;
};