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

    if (result)
        return locale;

    Json::Value empty;

    return empty;
}
