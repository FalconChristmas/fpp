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

#include <curl/curl.h>
#include <mutex>

class CurlManager {
public:
    static CurlManager INSTANCE;

    void addCURL(CURL* curl, std::function<void(CURL*)>&& callback, bool autoCleanCurl = true);

    void addGet(const std::string& url, std::function<void(int rc, const std::string& resp)>&& callback);
    void addPost(const std::string& url, const std::string& data, std::function<void(int rc, const std::string& resp)>&& callback);

    void add(const std::string& url, const std::string& method, const std::string& data,
             const std::list<std::string>& extraHeaders,
             std::function<void(int rc, const std::string& resp)>&& callback);

    void processCurls();

private:
    CurlManager();
    ~CurlManager();

    CURLM* curlMulti = nullptr;
    int numCurls = 0;

    class CurlInfo {
    public:
        CurlInfo() {}
        ~CurlInfo() {}
        CURL* curl = nullptr;
        bool cleanCurl = true;
        std::function<void(CURL*)> callback;
    };
    std::vector<CurlInfo*> curls;
    std::mutex lock;
};