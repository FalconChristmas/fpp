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
#include <list>
#include <map>
#include <string>

using namespace std;

class OLEDMenuController {
public:
    enum OLEDMenuControlOption {
        OLED_M_CMD_UP = 1,
        OLED_M_CMD_DOWN,
        OLED_M_CMD_BACK,
        OLED_M_CMD_RIGHT,
        OLED_M_CMD_ENTER,
        OLED_M_CMD_TEST,
        OLED_M_CMD_MODE,
        OLED_M_CMD_TESTMUTLISYNC
    };

    void SendActionToOLEDProcess(int z);
    void setOLEDMemoryAction(int i);
    void setFPPOLEDMemoryBit(int memberToSet, bool bitvalue);

    static std::map<std::string, OLEDMenuControlOption> s_mapStringValues;
    static OLEDMenuController INSTANCE;

private:
};
