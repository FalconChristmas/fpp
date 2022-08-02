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

#include <map>
#include <stdbool.h>
#include <string>

std::string getFPPDDir(const std::string& path = "");
std::string getFPPMediaDir(const std::string& path = "");

#define FPP_DIR getFPPDDir()
#define FPP_DIR_MEDIA(a) getFPPMediaDir(a)
#define FPP_DIR_CONFIG(a) getFPPMediaDir(std::string("/config") + a)
#define FPP_DIR_EFFECT(a) getFPPMediaDir(std::string("/effects") + a)
#define FPP_DIR_IMAGE(a) getFPPMediaDir(std::string("/images") + a)
#define FPP_DIR_MUSIC(a) getFPPMediaDir(std::string("/music") + a)
#define FPP_DIR_PLAYLIST(a) getFPPMediaDir(std::string("/playlists") + a)
#define FPP_DIR_PLUGIN(a) getFPPMediaDir(std::string("/plugins") + a)
#define FPP_DIR_SCRIPT(a) getFPPMediaDir(std::string("/scripts") + a)
#define FPP_DIR_SEQUENCE(a) getFPPMediaDir(std::string("/sequences") + a)
#define FPP_DIR_VIDEO(a) getFPPMediaDir(std::string("/videos") + a)
#define FPP_DIR_CRASHES(a) getFPPMediaDir(std::string("/crashes") + a)
#define FPP_FILE_LOG getFPPMediaDir("/logs/fppd.log")
#define FPP_FILE_PIXELNET getFPPMediaDir("/config/Falcon.FPDV1")
#define FPP_FILE_SETTINGS getFPPMediaDir("/settings")

typedef enum fppMode {
    UNKNOWN_MODE = 0x00,
    BRIDGE_MODE = 0x01, //deprecated, not used by FPP 5.x+, but external controllers
                        //may report this mode
    PLAYER_MODE = 0x02,
    MASTER_MODE = 0x06, //deprecated, FPP <=4.x instance may report
                        //this mode
    REMOTE_MODE = 0x08
} FPPMode;

class SettingsConfig {
public:
    SettingsConfig();
    ~SettingsConfig();

    void Init();

    FPPMode fppMode;

    Json::Value settingsInfo;
    Json::Value settings;

private:
    void LoadSettingsInfo();
};

// Helpers
char* trimwhitespace(const char* str, int quotesAlso = 1);
char* modeToString(int mode);
const std::string getFPPmodeStr(FPPMode mode = UNKNOWN_MODE);

// Action functions
int LoadSettings(const char* base);
int SaveSettings();
void UpgradeSettings();
int SetSetting(const std::string &key, const std::string &value);
int SetSetting(const std::string &key, const int value);

// Setters & Getters
std::string getSetting(const char* setting, const char* defaultVal = "");
int getSettingInt(const char* setting, int defaultVal = 0);

FPPMode getFPPmode(void);
