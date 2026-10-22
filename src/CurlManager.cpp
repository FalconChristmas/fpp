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

#include "fpp-pch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "CurlManager.h"
#include "CurlSocket.h"
#include "fppversion.h"
#include "log.h"

CurlManager CurlManager::INSTANCE;

CurlManager::CurlManager() {
}

CurlManager::~CurlManager() {
    if (curlMulti) {
        curl_multi_cleanup(curlMulti);
    }
    // No handle should still reference these shares by the time the singleton is
    // torn down at process exit; free whatever cookie sessions were left behind.
    for (auto& p : cookieSessions) {
        if (p.second->share) {
            curl_share_cleanup(p.second->share);
        }
        delete p.second;
    }
    cookieSessions.clear();
}

// curl calls these around every access to shared data (only COOKIE is shared
// here). The userdata is the session's own std::mutex; a single mutex is
// sufficient because only one data class is shared, so curl never nests locks.
static void curlShareLockCB(CURL* handle, curl_lock_data data, curl_lock_access access, void* userptr) {
    static_cast<std::mutex*>(userptr)->lock();
}
static void curlShareUnlockCB(CURL* handle, curl_lock_data data, void* userptr) {
    static_cast<std::mutex*>(userptr)->unlock();
}

CURLSH* CurlManager::getCookieShare(const std::string& token) {
    std::unique_lock<std::mutex> l(lock);
    CookieSession*& cs = cookieSessions[token];
    if (cs == nullptr) {
        cs = new CookieSession();
        cs->share = curl_share_init();
        curl_share_setopt(cs->share, CURLSHOPT_SHARE, CURL_LOCK_DATA_COOKIE);
        curl_share_setopt(cs->share, CURLSHOPT_LOCKFUNC, curlShareLockCB);
        curl_share_setopt(cs->share, CURLSHOPT_UNLOCKFUNC, curlShareUnlockCB);
        curl_share_setopt(cs->share, CURLSHOPT_USERDATA, &cs->mutex);
    }
    return cs->share;
}

void CurlManager::releaseCookieSession(const std::string& token) {
    CookieSession* cs = nullptr;
    {
        std::unique_lock<std::mutex> l(lock);
        auto it = cookieSessions.find(token);
        if (it == cookieSessions.end()) {
            return;
        }
        cs = it->second;
        cookieSessions.erase(it);
    }
    // Caller contract: no easy handle still has CURLOPT_SHARE pointing at this
    // share (the owner's requests have completed or been cancelled). If curl
    // still sees it in use we must NOT free it - the share holds &cs->mutex, so
    // deleting cs would leave curl calling into a freed mutex. Leak + shout.
    CURLSHcode sc = curl_share_cleanup(cs->share);
    if (sc == CURLSHE_IN_USE) {
        LogErr(VB_GENERAL, "CurlManager cookie session '%s' still in use at release; leaking share to avoid a UAF\n", token.c_str());
        return;
    }
    delete cs;
}

void CurlManager::addCURL(const std::string& furl, CURL* curl, std::function<void(CURL*)>&& callback, bool autoCleanCurl,
                          const std::string& owner) {
    std::unique_lock<std::mutex> l(lock);
    if (curlMulti == nullptr) {
        curlMulti = curl_multi_init();
        curl_multi_setopt(curlMulti, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    }

    CurlInfo* i = new CurlInfo;
    i->host = getHost(furl);
    i->url = furl;
    i->callback = callback;
    i->curl = curl;
    i->cleanCurl = autoCleanCurl;
    i->owner = owner;
    curl_multi_add_handle(curlMulti, curl);
    numCurls++;
    for (int x = 0; x < curls.size(); x++) {
        if (curls[x] == nullptr) {
            curls[x] = i;
            return;
        }
    }
    curls.push_back(i);
}

static size_t urlWriteData(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::vector<uint8_t>* str = (std::vector<uint8_t>*)userp;
    size_t pos = str->size();
    str->resize(str->size() + size * nmemb);
    memcpy(&str->data()[pos], buffer, size * nmemb);
    return size * nmemb;
}

// The requests FPP makes itself (add*(), do*()) and its discovery probes hold
// the whole response in memory, and discovery reaches devices that are merely
// on the LAN: one that streamed an endless body grew fppd until the OOM killer
// stepped in (the timeout alone allows ~100MB at LAN speed).  Returning short
// aborts the transfer with CURLE_WRITE_ERROR -- but the HTTP status already
// received still reads 200, so the request paths below report that result,
// like any other failed transfer, as a failure themselves (failedTransfer()).
// A plugin's own handle from createCurl() keeps no
// limit: it reads the status and the body itself, and a cut-off body with a
// 200 is what its proxy then served (limitResponseSize() opts a handle in).
static constexpr size_t MAX_CURL_RESPONSE = 16 * 1024 * 1024;

static size_t urlWriteDataLimited(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::vector<uint8_t>* str = (std::vector<uint8_t>*)userp;
    if (size * nmemb > MAX_CURL_RESPONSE - std::min(str->size(), MAX_CURL_RESPONSE)) {
        return 0;
    }
    return urlWriteData(buffer, size, nmemb, userp);
}

void CurlManager::limitResponseSize(CURL* c) {
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, urlWriteDataLimited);
}

// The result of the transfer whose callback is running on this thread, set by
// doProcessCurls() around it: the callbacks take only the handle, and the
// result isn't one of its infos.
static thread_local CURLcode t_completedResult = CURLE_OK;

// Why a transfer failed, or "" if it completed.  CURLINFO_RESPONSE_CODE is
// the last status received even when the transfer then failed: a body cut
// off by the size cap, a timeout or a reset partway through, or a server
// that closed before its Content-Length all read as a 200 with part of the
// body.  So the requests below go by the transfer's result, not the status.
static std::string failedTransfer(CURLcode result, const CurlManager::CurlPrivateData* data) {
    if (result == CURLE_OK) {
        return "";
    }
    if (result == CURLE_WRITE_ERROR) {
        // only urlWriteDataLimited() returns short on these handles
        return "the response was larger than " + std::to_string(MAX_CURL_RESPONSE / (1024 * 1024)) + " MB";
    }
    if (data && data->errorResp[0]) {
        return data->errorResp;
    }
    return curl_easy_strerror(result);
}
static size_t urlReadData(void* ptr, size_t size, size_t nmemb, void* userp) {
    size_t buffer_size = size * nmemb;
    CurlManager::CurlPrivateData* dt = (CurlManager::CurlPrivateData*)userp;
    size_t numb = dt->req->size() - dt->curPos;
    if (numb > buffer_size) {
        numb = buffer_size;
    }
    if (numb > 0) {
        memcpy(ptr, &(*dt->req)[dt->curPos], numb);
    }
    dt->curPos += numb;
    return numb;
}
CURL* CurlManager::createCurl(const std::string& fullUrl, CurlPrivateData** cpd, bool upload, CURLSH* share) {
    static std::string USERAGENT = std::string("FPP/") + getFPPVersionTriplet();

    const std::string host = getHost(fullUrl);
    // Under the lock: requests are created from API and command threads as
    // well as the main loop, and getHostData() inserts into the map.  Copy the
    // credentials out (curl copies string options anyway).
    std::string username;
    std::string password;
    {
        std::unique_lock<std::mutex> l(lock);
        HostData* hd = getHostData(host);
        username = hd->username;
        password = hd->password;
    }
    CURL* c = curl_easy_init();
    SetCurlCloseOnExec(c);
    curl_easy_setopt(c, CURLOPT_URL, fullUrl.c_str());
    if (share) {
        // Join the cookie session: arm the cookie engine (empty COOKIEFILE) and
        // point at the shared jar so cookies persist across handles in the group.
        curl_easy_setopt(c, CURLOPT_COOKIEFILE, "");
        curl_easy_setopt(c, CURLOPT_SHARE, share);
    }
    curl_easy_setopt(c, CURLOPT_USERAGENT, USERAGENT.c_str());
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1);
    //curl_easy_setopt(c, CURLOPT_VERBOSE, 2L);
    curl_easy_setopt(c, CURLOPT_UPKEEP_INTERVAL_MS, 5000L);

    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, urlWriteData);
    CurlPrivateData* data = new CurlPrivateData();
    data->host = host;
    curl_easy_setopt(c, CURLOPT_ERRORBUFFER, data->errorResp);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &data->resp);
    curl_easy_setopt(c, CURLOPT_PRIVATE, data);
    if (username != "") {
        curl_easy_setopt(c, CURLOPT_USERNAME, username.c_str());
        curl_easy_setopt(c, CURLOPT_PASSWORD, password.c_str());
        curl_easy_setopt(c, CURLOPT_HTTPAUTH, CURLAUTH_BASIC | CURLAUTH_DIGEST | CURLAUTH_NEGOTIATE);
    }
    if (upload) {
        data->req = new std::vector<uint8_t>();
        curl_easy_setopt(c, CURLOPT_READDATA, data);
        curl_easy_setopt(c, CURLOPT_READFUNCTION, urlReadData);
    }
    if (cpd) {
        *cpd = data;
    }
    return c;
}

void CurlManager::add(const std::string& furl, const std::string& method, const std::string& data,
                      const std::list<std::string>& extraHeaders,
                      std::function<void(int rc, const std::string& resp)>&& callback,
                      const std::string& owner, const std::string& cookieSession) {
    CURLSH* share = cookieSession.empty() ? nullptr : getCookieShare(cookieSession);
    CURL* curl = createCurl(furl, nullptr, false, share);
    limitResponseSize(curl);

    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (method == "PATCH") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    if ((method == "POST") || (method == "PUT")) {
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, data.c_str());
    }

    struct curl_slist* headers = NULL;
    for (auto& h : extraHeaders) {
        headers = curl_slist_append(headers, h.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    // The header list is owned by the callback rather than freed inside it, so
    // it is released whether the request completes or is cancelled (which
    // destroys the callback without running it). Either way that happens after
    // curl_easy_cleanup(), which is also the right order - the old code freed
    // the list while the easy handle still referenced it.
    std::shared_ptr<curl_slist> headerList(headers, [](curl_slist* h) {
        if (h) {
            curl_slist_free_all(h);
        }
    });

    addCURL(furl, curl, [callback, headerList](CURL* c) {
        CurlPrivateData* data = nullptr;
        curl_easy_getinfo(c, CURLINFO_PRIVATE, &data);
        std::string failure = failedTransfer(t_completedResult, data);
        if (!failure.empty()) {
            callback(0, failure);
            return;
        }
        long rc = 0;
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &rc);

        char* urlp = nullptr;
        curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &urlp);
        std::string resp(reinterpret_cast<char*>(data->resp.data()), data->resp.size());
        callback(rc, resp);
    }, true, owner);
}
void CurlManager::addGet(const std::string& url, std::function<void(int rc, const std::string& resp)>&& callback,
                         const std::string& owner, const std::string& cookieSession) {
    add(url, "GET", "", {}, [callback](int rc, const std::string& resp) { callback(rc, resp); }, owner, cookieSession);
}

void CurlManager::addPost(const std::string& url, const std::string& data, const std::string& contentType,
                          std::function<void(int rc, const std::string& resp)>&& callback, const std::string& owner) {
    add(url, "POST", data, { "Content-Type: " + contentType }, [callback](int rc, const std::string& resp) { callback(rc, resp); }, owner);
}
void CurlManager::addPut(const std::string& url, const std::string& data, const std::string& contentType,
                         std::function<void(int rc, const std::string& resp)>&& callback, const std::string& owner) {
    add(url, "PUT", data, { "Content-Type: " + contentType }, [callback](int rc, const std::string& resp) { callback(rc, resp); }, owner);
}

int CurlManager::cancelRequests(const std::string& owner) {
    if (owner.empty()) {
        return 0; // "" is FPP's own traffic; cancelling all of it is never wanted
    }
    // The callbacks are moved out and destroyed after the lock is dropped: they
    // belong to a plugin, and a captured object's destructor that touched
    // CurlManager again would deadlock on this non-recursive mutex.
    std::vector<std::function<void(CURL*)>> doomed;
    {
        std::unique_lock<std::mutex> l(lock);
        for (size_t x = 0; x < curls.size(); x++) {
            CurlInfo* ci = curls[x];
            if (!ci || ci->owner != owner) {
                continue;
            }
            curls[x] = nullptr;
            --numCurls;
            curl_multi_remove_handle(curlMulti, ci->curl);
            if (ci->cleanCurl) {
                CurlPrivateData* data = nullptr;
                curl_easy_getinfo(ci->curl, CURLINFO_PRIVATE, &data);
                if (data) {
                    delete data;
                }
                curl_easy_cleanup(ci->curl);
            }
            doomed.push_back(std::move(ci->callback));
            delete ci;
        }
    }
    LogDebug(VB_GENERAL, "CurlManager cancelled %d request(s) for %s\n", (int)doomed.size(), owner.c_str());
    return (int)doomed.size();
}

// Runs one synchronous request to completion.  The main loop pumps curl
// too, and when it completes this request it drops numCurls before calling
// the callback: processCurls() here then returned false with `done` still
// unset, this frame returned, and the callback wrote `done` into it.  The
// flag is shared instead, and the wait is for this request only (doPut
// waited for every request in flight).
// Returns the transfer's result.
static CURLcode runToCompletion(CurlManager& mgr, const std::string& furl, CURL* curl) {
    struct State {
        std::atomic<bool> done{ false };
        CURLcode result = CURLE_OK; // written before done, read after
    };
    auto state = std::make_shared<State>();
    mgr.addCURL(
        furl, curl, [state](CURL* c) { state->result = t_completedResult; state->done = true; }, false);
    while (!state->done) {
        if (!mgr.processCurls()) {
            // another thread holds it between completion and the callback
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } else {
            std::this_thread::yield();
        }
    }
    return state->result;
}

std::string CurlManager::doGet(const std::string& furl, int& rc, const std::string& cookieSession) {
    CURLSH* share = cookieSession.empty() ? nullptr : getCookieShare(cookieSession);
    CURL* curl = createCurl(furl, nullptr, false, share);
    limitResponseSize(curl);

    CURLcode result = runToCompletion(*this, furl, curl);

    CurlPrivateData* data = nullptr;
    long rc2 = 0;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &data);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc2);
    // On a failed transfer, rc 0 and the reason, as for one that never connected.
    std::string resp = failedTransfer(result, data);
    rc = resp.empty() ? rc2 : 0;
    if (resp.empty()) {
        resp.assign(reinterpret_cast<char*>(data->resp.data()), data->resp.size());
    }
    delete data;
    curl_easy_cleanup(curl);

    return resp;
}

struct ReadDataInfo {
    const std::vector<uint8_t>* data;
    size_t curPos = 0;
};
static size_t read_callback(void* ptr, size_t size, size_t nmemb, void* userp) {
    size_t buffer_size = size * nmemb;
    struct ReadDataInfo* dt = (struct ReadDataInfo*)userp;
    int numb = dt->data->size() - dt->curPos;
    if (numb > buffer_size) {
        numb = buffer_size;
    }
    memcpy(ptr, &(*dt->data)[dt->curPos], numb);
    dt->curPos += numb;
    return numb;
}
std::string CurlManager::doPost(const std::string& furl, const std::string& contentType, const std::vector<uint8_t>& data, int& rc) {
    CURL* curl = createCurl(furl);
    limitResponseSize(curl);

    struct curl_slist* head = nullptr;
    std::string ct = "Content-Type: " + contentType;
    head = curl_slist_append(head, ct.c_str());
    std::string cl = "Content-Length: " + std::to_string(data.size());
    head = curl_slist_append(head, cl.c_str());

    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);

    ReadDataInfo dta;
    dta.data = &data;
    dta.curPos = 0;

    curl_easy_setopt(curl, CURLOPT_READDATA, &dta);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());

    CURLcode result = runToCompletion(*this, furl, curl);

    CurlPrivateData* cdata = nullptr;
    long rc2 = 0;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &cdata);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc2);
    // On a failed transfer, rc 0 and the reason, as for one that never connected.
    std::string resp = failedTransfer(result, cdata);
    rc = resp.empty() ? rc2 : 0;
    if (resp.empty()) {
        resp.assign(reinterpret_cast<char*>(cdata->resp.data()), cdata->resp.size());
    }
    delete cdata;
    curl_slist_free_all(head);
    curl_easy_cleanup(curl);
    return resp;
}
std::string CurlManager::doPut(const std::string& furl, const std::string& contentType, const std::vector<uint8_t>& data, int& rc) {
    CURL* curl = createCurl(furl);
    limitResponseSize(curl);

    struct curl_slist* head = nullptr;
    std::string ct = "Content-Type: " + contentType;
    head = curl_slist_append(head, ct.c_str());
    std::string cl = "Content-Length: " + std::to_string(data.size());
    head = curl_slist_append(head, cl.c_str());

    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    //curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, head);

    ReadDataInfo dta;
    dta.data = &data;
    dta.curPos = 0;

    curl_easy_setopt(curl, CURLOPT_READDATA, &dta);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)data.size());

    CURLcode result = runToCompletion(*this, furl, curl);

    CurlPrivateData* cdata = nullptr;
    long rc2 = 0;
    curl_easy_getinfo(curl, CURLINFO_PRIVATE, &cdata);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &rc2);
    // On a failed transfer, rc 0 and the reason, as for one that never connected.
    std::string resp = failedTransfer(result, cdata);
    rc = resp.empty() ? rc2 : 0;
    if (resp.empty()) {
        resp.assign(reinterpret_cast<char*>(cdata->resp.data()), cdata->resp.size());
    }
    delete cdata;
    curl_slist_free_all(head);
    curl_easy_cleanup(curl);
    return resp;
}

bool CurlManager::doProcessCurls() {
    CURLMcode mc;
    int stillRunning = 0;
    std::unique_lock<std::mutex> l(lock);
    mc = curl_multi_perform(curlMulti, &stillRunning);
    if (stillRunning != numCurls) {
        CURLMsg* m = nullptr;
        do {
            int msgq = 0;
            m = curl_multi_info_read(curlMulti, &msgq);
            if (m && (m->msg == CURLMSG_DONE)) {
                CURL* e = m->easy_handle;
                // m is invalid once the handle is removed
                CURLcode result = m->data.result;
                curl_multi_remove_handle(curlMulti, e);
                CurlInfo* ci = nullptr;
                for (int x = 0; x < curls.size(); x++) {
                    if (curls[x] && curls[x]->curl == e) {
                        ci = curls[x];
                        curls[x] = nullptr;
                        --numCurls;
                        break;
                    }
                }
                if (ci) {
                    l.unlock();
                    t_completedResult = result;
                    // The callbacks parse what a remote or a probed device sent
                    // back, on the main loop with nothing above to catch: one
                    // jsoncpp throw on a field of the wrong type ({"Platform":{}}
                    // from a MultiSync remote, {"majorVersion":"9"} from a
                    // discovery probe) ended fppd.  Contain it here, for every
                    // callback, and carry on with the cleanup.
                    try {
                        ci->callback(e);
                    } catch (const std::exception& ex) {
                        char* url = nullptr;
                        curl_easy_getinfo(e, CURLINFO_EFFECTIVE_URL, &url);
                        LogWarn(VB_GENERAL, "Ignoring the reply from %s: handling it failed: %s\n",
                                url ? url : "(unknown)", ex.what());
                    } catch (...) {
                        char* url = nullptr;
                        curl_easy_getinfo(e, CURLINFO_EFFECTIVE_URL, &url);
                        LogWarn(VB_GENERAL, "Ignoring the reply from %s: handling it failed\n", url ? url : "(unknown)");
                    }
                    t_completedResult = CURLE_OK;
                    l.lock();
                    if (ci->cleanCurl) {
                        CurlPrivateData* data = nullptr;
                        curl_easy_getinfo(e, CURLINFO_PRIVATE, &data);
                        if (data) {
                            delete data;
                        }
                        curl_easy_cleanup(e);
                    }
                    delete ci;
                }
            }
        } while (m);
    }
    return numCurls > 0;
}

CurlManager::HostData* CurlManager::getHostData(const std::string& host) {
    HostData* h = hostData[host];
    if (h == nullptr) {
        h = new HostData();
        hostData[host] = h;
    }
    return h;
}

void CurlManager::setHostUsernamePassword(const std::string& host, const std::string& username, const std::string password) {
    std::unique_lock<std::mutex> l(lock);
    HostData* h = getHostData(host);
    h->username = username;
    h->password = password;
}

std::string CurlManager::getHost(const std::string& url) {
    int idx = url.find("://");
    std::string host = url.substr(idx + 3);
    idx = host.find("/");
    host = host.substr(0, idx);
    return host;
}
