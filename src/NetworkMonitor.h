/*
*   Falcon Player Daemon
*
*   Copyright (C) 2013-2019 the Falcon Player Developers
*
*   The Falcon Player (FPP) is free software; you can redistribute it
*   and/or modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation; either version 2 of
*   the License, or (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __FPP_NETWORK_MONITOR__
#define __FPP_NETWORK_MONITOR__

#include <map>
#include <list>
#include <string>
#include <functional>

class NetworkMonitor {
public:
    enum class NetEventType {
        NEW_LINK,
        DEL_LINK,
        NEW_ADDR,
        DEL_ADDR
    };
    
    
    NetworkMonitor() {}
    ~NetworkMonitor() {}
    
    void Init(std::map<int, std::function<bool(int)>> &callbacks);
    
    void registerCallback(std::function<void(NetEventType, int, const std::string &)> &callback);
    void addCallback(std::function<void(NetEventType, int, const std::string &)> &callback) { registerCallback(callback); }

    static NetworkMonitor INSTANCE;
    
    
private:
    void callCallbacks(NetEventType, int, const std::string &n);
    std::list<std::function<void(NetEventType, int, const std::string &)>> callbacks;
};

#endif
