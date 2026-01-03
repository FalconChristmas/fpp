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

#include "common.h"
#include "settings.h"
#include "commands/Commands.h"

#include "FPPLocale.h"

Json::Value LocaleHolder::locale;
std::mutex LocaleHolder::localeLock;

Json::Value LocaleHolder::GetLocale() {
    std::unique_lock<std::mutex> lock(localeLock);

    if (locale.size())
        return locale;

    std::string localeName = getSetting("Locale");

    if (localeName == "")
        localeName = "Global";

    std::string localeFile = "/opt/fpp/etc/locale/";
    localeFile += localeName + ".json";

    if (!FileExists(localeFile)) {
        localeFile = "/opt/fpp/etc/locale/Global.json";
    }

    bool result = LoadJsonFromFile(localeFile, locale);

    if (result) {
        // Load user-defined holidays and merge them with locale holidays
        std::string userHolidaysFile = FPP_DIR_CONFIG("/user-holidays.json");
        if (FileExists(userHolidaysFile)) {
            Json::Value userHolidays;
            if (LoadJsonFromFile(userHolidaysFile, userHolidays)) {
                if (userHolidays.isArray() && userHolidays.size() > 0) {
                    // Ensure the locale has a holidays array
                    if (!locale.isMember("holidays")) {
                        locale["holidays"] = Json::Value(Json::arrayValue);
                    }
                    
                    // Append user-defined holidays to the locale holidays
                    for (Json::ArrayIndex i = 0; i < userHolidays.size(); i++) {
                        locale["holidays"].append(userHolidays[i]);
                    }
                }
            }
        }
        return locale;
    }

    Json::Value empty;

    return empty;
}

void LocaleHolder::ClearCache() {
    std::unique_lock<std::mutex> lock(localeLock);
    locale.clear();
}

/**********************************************************************/
// Command to clear the locale cache
class ClearLocaleCacheCommand : public Command {
public:
    ClearLocaleCacheCommand() : Command("Clear Locale Cache") {
        args.push_back(CommandArg("force", "bool", "Force Clear").setDefaultValue("true"));
    }
    
    virtual std::unique_ptr<Command::Result> run(const std::vector<std::string>& args) override {
        LocaleHolder::ClearCache();
        return std::make_unique<Command::Result>("Locale cache cleared");
    }
};

void LocaleHolder::RegisterCommands() {
    CommandManager::INSTANCE.addCommand(new ClearLocaleCacheCommand());
}
