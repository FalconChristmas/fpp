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

#include <mutex>
#include <string>
#include "fpp-json.h"

class LocaleHolder {
public:
    static Json::Value GetLocale();
    static void ClearCache();
    static void RegisterCommands();

private:
    static std::mutex localeLock;
    static Json::Value locale;
    // What the cached locale was built from: the Locale setting and the
    // user holidays file's size and modification time.
    static std::string localeKey;
};

