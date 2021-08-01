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

#include "fpp-pch.h"

#include "fpp.h"

#include <sys/stat.h>
#include <ctype.h>
#include <filesystem>
#include <getopt.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

const char* fpp_bool_to_string[] = { "false", "true", "default" };

SettingsConfig settings;

SettingsConfig::SettingsConfig() {
    LoadSettingsInfo();
}

SettingsConfig::~SettingsConfig() {
}

void SettingsConfig::Init() {
    fppMode = PLAYER_MODE;

    // load UUID File
    if (access("/opt/fpp/scripts/get_uuid", F_OK) == 0) {
        FILE* uuid_fp;
        char temp_uuid[500];
        uuid_fp = popen("/opt/fpp/scripts/get_uuid", "r");
        if (uuid_fp == NULL) {
            LogWarn(VB_SETTING, "Couldn't execute get_uuid\n");
        }
        if (fgets(temp_uuid, sizeof(temp_uuid), uuid_fp) != NULL) {
            int pos = strlen(temp_uuid) - 1;
            // Strip newline
            if ('\n' == temp_uuid[pos]) {
                temp_uuid[pos] = '\0';
            }
            settings["SystemUUID"] = temp_uuid;
        }

        if (uuid_fp != NULL) {
            pclose(uuid_fp);
        }
    }

    // Set to unknown if not found
    if (!settings.isMember("SystemUUID")) {
        settings["SystemUUID"] = "Unknown";
        LogWarn(VB_SETTING, "Couldn't find UUID\n");
    }
}

void SettingsConfig::LoadSettingsInfo() {
    settingsInfo = LoadJsonFromFile("/opt/fpp/www/settings.json");

    Json::Value s = settingsInfo["settings"];
    Json::Value::Members memberNames = s.getMemberNames();

    for (int i = 0; i < memberNames.size(); i++) {
        std::string type = s[memberNames[i]]["type"].asString();
        std::string def = "";

        if (s[memberNames[i]].isMember("default")) {
            def = s[memberNames[i]]["default"].asString();
        } else {
            if ((type == "checkbox") ||
                (type == "number")) {
                def = "0";
            } else {
                def = "";
            }
        }

        settings[memberNames[i]] = def;
        // The following will never be logged due to startup order
        LogExcess(VB_SETTING, "Setting default for '%s' setting to '%s'\n",
                  memberNames[i].c_str(), def.c_str());
    }
}

// Returns a string that's the white-space trimmed version
// of the input string.  Also trim double quotes now that the
// settings file will have double quotes in it.
char* trimwhitespace(const char* str, int quotesAlso) {
    const char* end;
    size_t out_size;
    char* out;

    // Trim leading space
    while ((isspace(*str)) || (quotesAlso && (*str == '"')))
        str++;

    if (*str == 0) // All spaces?
        return strdup("");

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && ((isspace(*end)) || (quotesAlso && (*end == '"'))))
        end--;
    end++;

    // Set output size to minimum of trimmed string length
    out_size = end - str;

    // All whitespace
    if (out_size == 0)
        return strdup("");

    out = (char*)malloc(out_size + 1);
    // If we didn't allocate anything, die!
    if (!out) {
        fprintf(stderr, "Failed to allocate memory for string trimming\n");
        exit(EXIT_FAILURE);
    }

    // Copy trimmed string and add null terminator
    memcpy(out, str, out_size);
    out[out_size] = 0;

    return out;
}

// Caller must free the char array we give them
char* modeToString(int mode) {
    if (mode == PLAYER_MODE)
        return strdup("player");
    else if (mode == REMOTE_MODE)
        return strdup("remote");
    else if (mode == BRIDGE_MODE)
        return strdup("bridge");
    else if (mode == MASTER_MODE)
        return strdup("master");

    return strdup("unknown");
}

int SetSetting(const std::string key, const int value) {
    return SetSetting(key, std::to_string(value));
}

static std::map<std::string, std::string> UPDATES;

int SetSetting(const std::string key, const std::string value) {
    settings.settings[key] = value;

    if (key == "fppMode") {
        if (value == "player") {
            settings.fppMode = PLAYER_MODE;
        } else if (value == "bridge") {
            // legacy, remap to PLAYER
            settings.fppMode = PLAYER_MODE;
            UPDATES[key] = "player";
        } else if (value == "master") {
            // legacy, remap to player, but turn on MultiSync setting
            settings.fppMode = PLAYER_MODE;
            settings.settings[key] = "player";
            settings.settings["MultiSyncEnabled"] = "1";

            UPDATES[key] = "fppMode = \"player\"";
            UPDATES["MultiSyncEnabled"] = "MultiSyncEnabled = \"1\"";
        } else if (value == "remote") {
            settings.fppMode = REMOTE_MODE;
        } else {
            fprintf(stderr, "Error parsing mode\n");
            exit(EXIT_FAILURE);
        }
    } else if (startsWith(key, "LogLevel_")) {
        // Starts with LogLevel_
        if (value != "") {
            FPPLogger::INSTANCE.SetLevel(key.c_str(), value.c_str());
        } else {
            FPPLogger::INSTANCE.SetLevel(key.c_str(), "warn");
        }
    }

    return 1;
}

int LoadSettings() {
    settings.Init();

    if (!FileExists(FPP_FILE_SETTINGS)) {
        LogWarn(VB_SETTING,
                "Attempted to load settings file %s which does not exist!\n", FPP_FILE_SETTINGS);
        return -1;
    }

    FILE* file = fopen(FPP_FILE_SETTINGS, "r");

    if (file != NULL) {
        char* line = (char*)calloc(256, 1);
        size_t len = 256;
        ssize_t read;
        int sIndex = 0;

        while ((read = getline(&line, &len, file)) != -1) {
            if ((!line) || (!read) || (read == 1))
                continue;

            char *key = NULL, *value = NULL; // These are values we're looking for and will
                                             // run through trimwhitespace which means they
                                             // must be freed before we are done.

            char* token = strtok(line, "=");
            if (!token)
                continue;

            key = trimwhitespace(token);
            if (!strlen(key)) {
                free(key);
                continue;
            }

            token = strtok(NULL, "=");
            if (!token) {
                fprintf(stderr, "Error tokenizing value for %s setting\n", key);
                free(key);
                continue;
            }
            value = trimwhitespace(token);

            SetSetting(key, value);

            if (key) {
                free(key);
                key = NULL;
            }

            if (value) {
                free(value);
                value = NULL;
            }
        }

        if (line) {
            free(line);
        }

        fclose(file);
    } else {
        LogWarn(VB_SETTING, "Warning: couldn't open settings file: '%s'!\n", FPP_FILE_SETTINGS);
        return -1;
    }

    UpgradeSettings();

    return 0;
}

int SaveSettings() {
    // When SaveSettings() is implemented, it should only save settings
    // defined in settings.json since there are other ephemeral values
    // stored in the settings Json::Value.
    return 0;
}

void UpgradeSettings() {
    if (!UPDATES.empty()) {
        std::string inbuf;
        std::fstream input_file(FPP_FILE_SETTINGS, std::ios::in);
        std::ofstream output_file("/tmp/upgradedsettings.txt");

        while (!input_file.eof()) {
            getline(input_file, inbuf);
            int spot = inbuf.find(" = ");
            if (spot >= 0) {
                std::string tmpstring = inbuf.substr(0, spot);
                if (UPDATES.find(tmpstring) != UPDATES.end()) {
                    inbuf = UPDATES[tmpstring];
                    UPDATES.erase(tmpstring);
                }
            }
            if (inbuf != "") {
                output_file << inbuf << std::endl;
            }
        }
        for (auto& e : UPDATES) {
            output_file << e.second << std::endl;
        }
        output_file.close();
        input_file.close();
        std::filesystem::copy("/tmp/upgradedsettings.txt", FPP_FILE_SETTINGS, std::filesystem::copy_options::overwrite_existing);
    }
}

std::string getSetting(const char* setting, const char* defaultVal) {
    std::string result;

    if ((!setting) || (setting[0] == 0x00)) {
        LogErr(VB_SETTING, "getSetting() called with NULL or empty value\n");
        return defaultVal;
    }

    if (settings.settings.isMember(setting))
        result = settings.settings[setting].asString().c_str();

    LogExcess(VB_SETTING, "getSetting(%s) returning %d\n", setting, result.c_str());
    return result;
}

int getSettingInt(const char* setting, int defaultVal) {
    int result = defaultVal;

    if ((!setting) || (setting[0] == 0x00)) {
        LogErr(VB_SETTING, "getSettingInt() called with NULL or empty value\n");
        return defaultVal;
    }

    if (settings.settings.isMember(setting))
        result = atoi(settings.settings[setting].asString().c_str());

    LogExcess(VB_SETTING, "getSettingInt(%s) returning %d\n", setting, result);

    return result;
}

FPPMode getFPPmode(void) {
    return settings.fppMode;
}

const std::string getFPPmodeStr(FPPMode mode) {
    if (mode == UNKNOWN_MODE)
        mode = settings.fppMode;

    std::string result;
    char* modePtr = modeToString((int)mode);
    if (modePtr) {
        result = modePtr;
        free(modePtr);
    } else {
        result = "unknown";
    }

    return result;
}
