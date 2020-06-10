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

#include <string>

#include "fpp-pch.h"

#include "MultiSync.h"

class NetworkController {
  public:
    NetworkController(const std::string &ipStr);
    ~NetworkController() {};

    static NetworkController *DetectControllerViaHTML(const std::string &ip, const std::string &html);

    std::string ip;
    std::string hostname;
    std::string vendor;
    std::string vendorURL;
    systemType  typeId;
    std::string typeStr;
    std::string ranges;
    std::string version;
    unsigned int majorVersion;
    unsigned int minorVersion;
    FPPMode systemMode;

  private:
    bool DetectFalconController(const std::string &ip, const std::string &html);
    bool DetectSanDevicesController(const std::string &ip, const std::string &html);
    bool DetectESPixelStickController(const std::string &ip, const std::string &html);
    bool DetectFPP(const std::string &ip, const std::string &html);

    void DumpControllerInfo(void);
};

