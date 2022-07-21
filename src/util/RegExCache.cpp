/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#include "fpp-pch.h"

#include "RegExCache.h"

std::map<std::string, std::list<std::unique_ptr<std::regex>>> RegExCache::REGEX_CACHE;
std::mutex RegExCache::REGEX_CACHE_LOCK;


RegExCache::RegExCache(const std::string &s) : regexString(s) {
    std::unique_lock<std::mutex> lock(REGEX_CACHE_LOCK);
    auto &l = REGEX_CACHE[regexString];
    if (l.empty()) {
        regex = new std::regex(s);
    } else {
        regex = l.front().release();
        l.pop_front();
    }
}

RegExCache::~RegExCache() {
    std::unique_lock<std::mutex> lock(REGEX_CACHE_LOCK);
    auto &l = REGEX_CACHE[regexString];
    l.emplace_back(regex);
}

