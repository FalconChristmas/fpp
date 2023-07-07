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
#include <climits>

#include "CurlManager.h"


CurlManager CurlManager::INSTANCE;

CurlManager::CurlManager() {

}

CurlManager::~CurlManager() {
    if (curlMulti) {
        curl_multi_cleanup(curlMulti);
    }
}

void CurlManager::addCURL(CURL *curl, std::function<void(CURL *)>&& callback, bool autoCleanCurl) {
    std::unique_lock<std::mutex> l(lock);
    if (curlMulti == nullptr) {
        curlMulti = curl_multi_init();
        curl_multi_setopt(curlMulti, CURLMOPT_PIPELINING, CURLPIPE_MULTIPLEX);
    }

    CurlInfo* i = new CurlInfo;
    i->callback = callback;
    i->curl = curl;
    i->cleanCurl = autoCleanCurl;
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

class CurlPrivateData {
public:
    std::string url;
    std::string resp;
};

static size_t urlWriteData(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::string* str = (std::string*)userp;
    str->append(static_cast<const char*>(buffer), size * nmemb);
    return size * nmemb;
}
static CURL *createCurl(const std::string &url) {
    CURL *c = curl_easy_init();        
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_CONNECTTIMEOUT_MS, 3000L);
    curl_easy_setopt(c, CURLOPT_TIMEOUT_MS, 10000L);
    curl_easy_setopt(c, CURLOPT_TCP_FASTOPEN, 1L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1);
    //curl_easy_setopt(c, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);
    curl_easy_setopt(c, CURLOPT_UPKEEP_INTERVAL_MS, 5000L);

    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, urlWriteData);
    CurlPrivateData *data = new CurlPrivateData();
    data->url = url;
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &data->resp);
    curl_easy_setopt(c, CURLOPT_PRIVATE, data);
    return c;
}

void CurlManager::add(const std::string &url, const std::string &method, const std::string &data, 
        const std::list<std::string>& extraHeaders,
        std::function<void(int rc, const std::string &resp)>&& callback) {

    static std::string USERAGENT =  std::string("FPP/") + getFPPVersionTriplet();
    CURL *curl = createCurl(url);

    curl_easy_setopt(curl, CURLOPT_USERAGENT, USERAGENT.c_str());
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1);
    } else if (method == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }

    if ((method == "POST") || (method == "PUT")) {
        curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, data.c_str());
    }

    struct curl_slist* headers = NULL;
    if (startsWith(data, "{") && endsWith(data, "}")) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    for (auto& h : extraHeaders) {
        headers = curl_slist_append(headers, h.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_slist_free_all(headers);
    }

    addCURL(curl, [callback](CURL *c) {
        CurlPrivateData *data = nullptr;
        long rc = 0;
        curl_easy_getinfo(c, CURLINFO_PRIVATE, &data);
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &rc);

        char *urlp = nullptr;
        curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &urlp);
        callback(rc, data->resp);
        delete data;
    });
            
}
void CurlManager::addGet(const std::string &url, std::function<void(int rc, const std::string &resp)>&& callback) {
    add(url, "GET", "", {}, [callback](int rc, const std::string &resp) {callback(rc, resp);});
}

void CurlManager::addPost(const std::string &url, const std::string &data, std::function<void(int rc, const std::string &resp)>&& callback) {
    add(url, "POST", data, {}, [callback](int rc, const std::string &resp) {callback(rc, resp);});
}


void CurlManager::processCurls() {
    if (numCurls) {
        CURLMcode mc;
        int stillRunning = 0;
        std::unique_lock<std::mutex> l(lock);
        mc = curl_multi_perform(curlMulti, &stillRunning);
        if (stillRunning != numCurls) {
            CURLMsg *m = nullptr;
            do {
                int msgq = 0;
                m = curl_multi_info_read(curlMulti, &msgq);
                if (m && (m->msg == CURLMSG_DONE)) {
                    CURL *e = m->easy_handle;
                    curl_multi_remove_handle(curlMulti, e);
                    CurlInfo *ci = nullptr;
                    for (int x = 0; x < curls.size(); x++) {
                        if (curls[x] && curls[x]->curl == e) {
                            ci = curls[x];
                            curls[x] = nullptr;
                            --numCurls;
                            break;
                        }
                    }
                    l.unlock();
                    if (ci) {
                        ci->callback(e);
                        if (ci->cleanCurl) {
                            curl_easy_cleanup(e);
                        }
                        delete ci;
                    }
                    l.lock();
                }
            } while(m);
        }
    }
}