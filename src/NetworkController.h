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

#include "MultiSync.h"

class NetworkController {
public:
    NetworkController(const std::string& ipStr);
    ~NetworkController(){};

    static NetworkController* DetectControllerViaHTML(const std::string& ip, const std::string& html);

    std::string ip;
    std::string hostname;
    std::string vendor;
    std::string vendorURL;
    systemType typeId;
    std::string typeStr;
    std::string ranges;
    std::string version;
    std::string uuid;
    unsigned int majorVersion;
    unsigned int minorVersion;
    FPPMode systemMode;
    bool sendingMultiSync = false;

private:
    bool DetectFalconController(const std::string& ip, const std::string& html);
    bool DetectSanDevicesController(const std::string& ip, const std::string& html);
    bool DetectESPixelStickController(const std::string& ip, const std::string& html);
    bool DetectAlphaPixController(const std::string& ip, const std::string& html);
    bool DetectHinksPixController(const std::string& ip, const std::string& html);
    bool DetectDIYLEDExpressController(const std::string& ip, const std::string& html);
    bool DetectWLEDController(const std::string& ip, const std::string& html);
    bool DetectFPP(const std::string& ip, const std::string& html);

    void DumpControllerInfo(void);
};
