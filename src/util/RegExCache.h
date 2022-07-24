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
#include <list>
#include <regex>

class RegExCache {
public:
    RegExCache(const std::string &regex);
    ~RegExCache();

    std::string regexString;
    std::regex *regex;

private:
    static std::map<std::string, std::list<std::unique_ptr<std::regex>>> REGEX_CACHE;
    static std::mutex REGEX_CACHE_LOCK;
};
