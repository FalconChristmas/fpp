#pragma once
/*
 *   Setting manager for Falcon Player (FPP)
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
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

#include <map>
#include <stdbool.h>
#include <string>


std::string getFPPDDir(const std::string &path = "");
std::string getFPPMediaDir(const std::string &path = "");

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
int LoadSettings(const char *base);
int SaveSettings();
void UpgradeSettings();
int SetSetting(const std::string key, const std::string value);
int SetSetting(const std::string key, const int value);

// Setters & Getters
std::string getSetting(const char* setting, const char* defaultVal = "");
int getSettingInt(const char* setting, int defaultVal = 0);

FPPMode getFPPmode(void);
