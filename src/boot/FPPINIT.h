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

#include "fpp-json-fwd.h"

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

// Require the parsed root to be the shape the caller expects. A mismatch
// yields an empty value of the expected shape and returns false, so callers
// that ignore the return value iterate over nothing instead of aborting.
enum class JsonRoot {
    Object,
    Array
};
bool LoadJsonFromString(const std::string& str, Json::Value& root, JsonRoot expected);

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
// captureDefaults=true only from boot, while sysfs still holds the device tree values
void applyThermalSettings(bool captureDefaults = false);
void resetThermalSettings();
// Force each fan on and read its tachometer, recording which fans actually
// report RPM so fppd can suppress the display for absent/dead fans. Boot only.
void probeFanPresence();
void setFileOwnership();
bool checkUnpartitionedSpace();
void resizeRootFS();
void setupTimezone();
void cleanupChromiumFiles();
void setupKiosk(bool force = false);
void checkInstallKiosk();
void installKiosk();
void checkInstallPackages();
void checkConfigMigrations();
void startZRAMSwap();
void startDiskSwap();
void setupChannelOutputs();
void handleRebootActions();

// ---------------------------------------------------------------------------
// Networking (defined in FPPINIT_Network.cpp)
// ---------------------------------------------------------------------------
std::string FindTetherWIFIAdapater();
void setupNetwork(bool fullReload = false);
void consumePendingDhcpLeaseReset();
void handleBootDelay();
void handleTimeSyncWait();
void checkWLANInterface();
// allowUsbRecovery: permit one USB re-enumeration of an adapter that came up
// dead. Boot path only -- the networkd-dispatcher-driven tether callers must
// never set it, since a reset causes the carrier change that re-invokes them.
bool waitForInterfacesUp(int timeOut, bool allowUsbRecovery = false);
// True once a wedged USB network adapter has caused a reboot to be queued. The
// boot is being thrown away, so callers should stop doing network setup work.
bool usbWedgeRebootPending();
void announceIPAddresses();
// Turn off 802.11 power saving on all wireless interfaces.
void disableWLANPowerManagement();
void maybeEnableTethering();
void detectNetworkModules();
void removeDummyInterface();

// ---------------------------------------------------------------------------
// Audio (defined in FPPINIT_Audio.cpp)
// ---------------------------------------------------------------------------
// skipFppdRestart: the caller will restart fppd itself once it has finished its
// own PipeWire service restarts, so setupAudio must not do it in the middle.
void setupAudio(bool skipFppdRestart = false);
