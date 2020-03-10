#include "common.h"
#include "FPPLocale.h"
#include "settings.h"

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

    if (result)
        return locale;

    Json::Value empty;

    return empty;
}

