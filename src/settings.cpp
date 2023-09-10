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

#include "fpp-pch.h"

#include <cstring>
#include <ctype.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <pwd.h>

#include "common.h"
#include "log.h"
#include "commands/Commands.h"

#include "settings.h"

#ifdef PLATFORM_OSX
#include <mach-o/dyld.h>
#endif

static std::string FPP_BIN_DIR("");
static std::string FPP_MEDIA_DIR("");

std::string getFPPDDir(const std::string& path) {
    if (FPP_BIN_DIR == "") {
        char exePath[2048];
        exePath[0] = 0;
#ifdef PLATFORM_OSX
        uint32_t len = sizeof(exePath);
        _NSGetExecutablePath(exePath, &len);
#else
        ssize_t len = ::readlink("/proc/self/exe", exePath, sizeof(exePath));
        exePath[len] = '\0';
#endif
        FPP_BIN_DIR = exePath;
        FPP_BIN_DIR = FPP_BIN_DIR.substr(0, FPP_BIN_DIR.rfind("/"));
        FPP_BIN_DIR = FPP_BIN_DIR.substr(0, FPP_BIN_DIR.rfind("/"));
    }
    return FPP_BIN_DIR + path;
}
std::string getFPPMediaDir(const std::string& path) {
    if (FPP_MEDIA_DIR == "") {
        std::string filename = getFPPDDir("/www/media_root.txt");
        std::string fc = "";
        if (FileExists(filename)) {
            fc = GetFileContents(filename);
            FPP_MEDIA_DIR = fc;
            TrimWhiteSpace(FPP_MEDIA_DIR);
        }
        if (FPP_MEDIA_DIR == "") {
#ifdef PLATFORM_OSX
            const char* homedir;
            if ((homedir = getenv("HOME")) == NULL) {
                homedir = getpwuid(getuid())->pw_dir;
            }
            FPP_MEDIA_DIR = homedir;
            FPP_MEDIA_DIR += "/Documents/fpp";
#else
            FPP_MEDIA_DIR = "/home/fpp/media";
#endif
        }
        if (FPP_MEDIA_DIR != fc) {
            PutFileContents(filename, FPP_MEDIA_DIR);
        }
    }
    return FPP_MEDIA_DIR + path;
}

SettingsConfig settings;

SettingsConfig::SettingsConfig() {
    LoadSettingsInfo();
}

SettingsConfig::~SettingsConfig() {
}

void SettingsConfig::Init() {
    fppMode = PLAYER_MODE;

    // load UUID File
    if (access(getFPPDDir("/scripts/get_uuid").c_str(), F_OK) == 0) {
        FILE* uuid_fp;
        char temp_uuid[500];
        uuid_fp = popen(getFPPDDir("/scripts/get_uuid").c_str(), "r");
        if (uuid_fp == NULL) {
            LogWarn(VB_SETTING, "Couldn't execute get_uuid\n");
        }
        if (fgets(temp_uuid, sizeof(temp_uuid), uuid_fp) != NULL) {
            int pos = strlen(temp_uuid) - 1;
            // Strip newline
            if ('\n' == temp_uuid[pos]) {
                temp_uuid[pos] = '\0';
            }
            settings["SystemUUID"] = std::string(temp_uuid);
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
    settingsInfo = LoadJsonFromFile(getFPPDDir("/www/settings.json"));

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
#ifdef PLATFORM_OSX
    char hostname[256];
    gethostname(hostname, 256);
    std::string hn = hostname;
    int idx = hn.find(".");
    if (idx != std::string::npos) {
        hn = hn.substr(0, idx);
    }
    settings["HostName"] = hn;
#endif
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

int SetSetting(const std::string& key, const int value) {
    return SetSetting(key, std::to_string(value));
}

static std::map<std::string, std::string> UPDATES;

int SetSetting(const std::string& key, const std::string& value) {
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

int LoadSettings(const char* base) {
    settings.Init();

    if (!FileExists(FPP_FILE_SETTINGS)) {
        LogWarn(VB_SETTING,
                "Attempted to load settings file %s which does not exist!\n", FPP_FILE_SETTINGS.c_str());
        return -1;
    }

    FILE* file = fopen(FPP_FILE_SETTINGS.c_str(), "r");

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
            if (!token) {
                if (line) {
                    free(line);
                }
                continue;
            }

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
        LogWarn(VB_SETTING, "Warning: couldn't open settings file: '%s'!\n", FPP_FILE_SETTINGS.c_str());
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
        CopyFileContents("/tmp/upgradedsettings.txt", FPP_FILE_SETTINGS);
    }
}

std::string getSetting(const char* setting, const char* defaultVal) {
    std::string result = defaultVal;

    if ((!setting) || (setting[0] == 0x00)) {
        LogErr(VB_SETTING, "getSetting() called with NULL or empty value\n");
        return result;
    }

    if (settings.settings.isMember(setting))
        result = settings.settings[setting].asString().c_str();

    LogExcess(VB_SETTING, "getSetting(%s) returning '%s'\n", setting, result.c_str());
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
