/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#pragma once

#include <string>

#include "common_mini.h"

#if __has_include(<jsoncpp/json/json.h>)
#include <jsoncpp/json/json.h>
#elif __has_include(<json/json.h>)
#include <json/json.h>
#endif

#ifdef PLATFORM_BB64
inline const std::string SD_CARD_DEVICE = "/dev/mmcblk1";
#else
inline const std::string SD_CARD_DEVICE = "/dev/mmcblk0";
#endif

#ifdef PLATFORM_PI
inline const std::string I2C_DEV = "/dev/i2c-1";
#else
inline const std::string I2C_DEV = "/dev/i2c-2";
#endif

inline const std::string FPP_MEDIA_DIR = "/home/fpp/media";

// ---------------------------------------------------------------------------
// Shared utilities (defined in FPPINIT.cpp)
// ---------------------------------------------------------------------------
void exec(const std::string& cmd);
int execbg(const std::string& cmd);
std::string execAndReturn(const std::string& cmd);
bool LoadJsonFromString(const std::string& str, Json::Value& root);
std::string SaveJsonToString(const Json::Value& root);
void modprobe(const char* mod);

#ifdef PLATFORM_PI
inline bool isPi5() {
    return startsWith(GetFileContents("/proc/device-tree/model"), "Raspberry Pi 5") || startsWith(GetFileContents("/proc/device-tree/model"), "Raspberry Pi Compute Module 5");
}
inline bool isPiZero2W() {
    return contains(GetFileContents("/proc/device-tree/model"), "Raspberry Pi Zero 2 W");
}
#endif

// ---------------------------------------------------------------------------
// Configuration / system setup (defined in FPPINIT_Config.cpp)
// ---------------------------------------------------------------------------
void DetectCape();
void checkSSHKeys();
void handleBootPartition();
void checkHostName();
void runScripts(const std::string tp, bool userBefore = true);
void checkFSTAB();
void createDirectories();
void setupApache();
void handleBootActions();
void configureBBB();
void setFileOwnership();
bool checkUnpartitionedSpace();
void resizeRootFS();
void setupTimezone();
void cleanupChromiumFiles();
void setupKiosk(bool force = false);
void checkInstallKiosk();
void installKiosk();
void checkInstallPackages();
void startZRAMSwap();
void startDiskSwap();
void setupChannelOutputs();
void handleRebootActions();

// ---------------------------------------------------------------------------
// Networking (defined in FPPINIT_Network.cpp)
// ---------------------------------------------------------------------------
std::string FindTetherWIFIAdapater();
void setupNetwork(bool fullReload = false);
void handleBootDelay();
void handleTimeSyncWait();
void checkWLANInterface();
bool waitForInterfacesUp(int timeOut);
void announceIPAddresses();
void maybeEnableTethering();
void detectNetworkModules();
void removeDummyInterface();

// ---------------------------------------------------------------------------
// Audio (defined in FPPINIT_Audio.cpp)
// ---------------------------------------------------------------------------
void setupAudio();
