/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2024 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#pragma once

#include <curl/curl.h>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <vector>

class CurlManager {
public:
    static CurlManager INSTANCE;

    class CurlPrivateData {
    public:
        CurlPrivateData() {}
        virtual ~CurlPrivateData() {
            if (req) {
                delete req;
            }
        }

        std::string host;
        std::vector<uint8_t> resp;
        char errorResp[CURL_ERROR_SIZE] = { 0 };

        std::vector<uint8_t>* req = nullptr;
        size_t curPos = 0;
    };

    // The trailing `owner` on every add below tags the request so it can be
    // cancelled as a group later. A plugin that wants to be unloadable must pass
    // its plugin name: its callback is a std::function whose code and captured
    // state live in the plugin's .so, so an in-flight request is a call into
    // that library after it is gone. FPP's own callers leave it empty - core
    // code is never unloaded.

    // A non-empty `cookieSession` on the calls below joins the request to a
    // named cookie jar shared by every request using the same token: the handle
    // is given CURLOPT_SHARE on that session's CURLSH plus an empty
    // CURLOPT_COOKIEFILE to arm the cookie engine, so a PHPSESSID (or any other
    // Set-Cookie) established by one request is presented by the next. The
    // ordering the caller wants between requests is the caller's job (chain the
    // next add from the previous one's callback); the session only carries the
    // cookies. The CURLSH is created on first use and must outlive every handle
    // that references it - see releaseCookieSession().

    // Raw methods for working directly with the CURL* objects. `share`, when
    // non-null, is a CURLSH from getCookieShare()/a cookie session.
    CURL* createCurl(const std::string& fullUrl, CurlPrivateData** data = nullptr, bool upload = false, CURLSH* share = nullptr);
    void addCURL(const std::string& furl, CURL* curl, std::function<void(CURL*)>&& callback, bool autoCleanCurl = true,
                 const std::string& owner = "");

    // Asynchronous methods for string content
    void add(const std::string& furl, const std::string& method, const std::string& data,
             const std::list<std::string>& extraHeaders,
             std::function<void(int rc, const std::string& resp)>&& callback,
             const std::string& owner = "", const std::string& cookieSession = "");
    void addGet(const std::string& furl, std::function<void(int rc, const std::string& resp)>&& callback,
                const std::string& owner = "", const std::string& cookieSession = "");
    void addPost(const std::string& furl, const std::string& data, const std::string& contentType,
                 std::function<void(int rc, const std::string& resp)>&& callback, const std::string& owner = "");
    void addPut(const std::string& furl, const std::string& data, const std::string& contentType,
                std::function<void(int rc, const std::string& resp)>&& callback, const std::string& owner = "");

    // Abandons every outstanding request tagged with `owner`: the transfers are
    // removed from the multi handle and their callbacks are destroyed WITHOUT
    // being invoked. Destroying them is the point - that is what releases the
    // plugin-owned callable while its library is still mapped. Returns how many
    // were cancelled.
    //
    // Main loop only, same as doProcessCurls(), so it cannot run while a
    // callback is executing.
    int cancelRequests(const std::string& owner);

    // Ends a cookie session created via a `cookieSession` token and frees its
    // CURLSH. The caller MUST guarantee no handle still references the share:
    // cancel this session's owner (cancelRequests) or let its requests finish
    // first. Cleaning up a share still referenced by an easy handle is undefined
    // in libcurl, so this checks curl's CURLSHE_IN_USE and refuses (leaks + logs)
    // rather than free an in-use share. A missing/never-created token is a no-op.
    void releaseCookieSession(const std::string& token);

    // Synchronous methods. A non-empty `cookieSession` shares cookies as above.
    std::string doGet(const std::string& furl, int& rc, const std::string& cookieSession = "");
    std::string doPost(const std::string& furl, const std::string& contentType, const std::vector<uint8_t>& data, int& rc);
    std::string doPut(const std::string& furl, const std::string& contentType, const std::vector<uint8_t>& data, int& rc);

    void setHostUsernamePassword(const std::string& host, const std::string& username, const std::string password);
    bool processCurls() {
        if (numCurls) {
            return doProcessCurls();
        }
        return false;
    }

private:
    CurlManager();
    ~CurlManager();

    bool doProcessCurls();
    std::string getHost(const std::string& url);

    // Finds or creates the CURLSH backing a cookie session token. The share has
    // CURL_LOCK_DATA_COOKIE enabled and LOCK/UNLOCK callbacks wired to the
    // session's own mutex, so it is safe even if a transfer using it is driven
    // from a thread other than the one that armed it (processCurls() is called
    // from a few call sites). Takes `lock`.
    CURLSH* getCookieShare(const std::string& token);

    CURLM* curlMulti = nullptr;
    int numCurls = 0;

    class CurlInfo {
    public:
        CurlInfo() {}
        ~CurlInfo() {}
        std::string host;
        std::string url;
        std::string owner; // empty for FPP's own requests; see cancelRequests()
        CURL* curl = nullptr;
        bool cleanCurl = true;
        std::function<void(CURL*)> callback;
    };
    std::vector<CurlInfo*> curls;
    std::mutex lock;

    class HostData {
    public:
        HostData() {}
        ~HostData() {}
        std::string username;
        std::string password;
        int authStatus = 0;
    };
    std::map<std::string, HostData*> hostData;
    HostData* getHostData(const std::string& host);

    // One shared cookie jar per session token. `mutex` backs the share's
    // LOCK/UNLOCK callbacks; it lives here (heap, stable address) so the pointer
    // handed to CURLSHOPT_USERDATA stays valid for the share's whole life. The
    // map itself is guarded by `lock`.
    class CookieSession {
    public:
        CookieSession() {}
        ~CookieSession() {}
        CURLSH* share = nullptr;
        std::mutex mutex;
    };
    std::map<std::string, CookieSession*> cookieSessions;
};
