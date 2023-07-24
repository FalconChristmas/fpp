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

#include <arpa/inet.h>
#include <curl/curl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>

#include "../Warnings.h"
#include "../common.h"
#include "../fppversion.h"
#include "../log.h"

#include "Twinkly.h"

#include "../CurlManager.h"
#include "../Timers.h"

static const std::string TWINKLYTYPE = "Twinkly";

constexpr int TOKEN_LEN = 8;
constexpr int HEADER_LEN = (1 + TOKEN_LEN + 2 + 1);
constexpr int TWINKLY_TOKEN_VALIDATE_TIME = 120; // check the token every 120s

constexpr uint32_t MAXPACKETS = 40 * 60 * 60 * 4; // about 4 hours at 40fps,

const std::string& TwinklyOutputData::GetOutputTypeString() const {
    return TWINKLYTYPE;
}

TwinklyOutputData::TwinklyOutputData(const Json::Value& config) :
    UDPOutputData(config) {
    memset((char*)&twinklyAddress, 0, sizeof(sockaddr_in));
    twinklyAddress.sin_family = AF_INET;
    twinklyAddress.sin_port = htons(TWINKLY_PORT);
    twinklyAddress.sin_addr.s_addr = toInetAddr(ipAddress, valid);

    if (!valid && active) {
        WarningHolder::AddWarning("Could not resolve host name " + ipAddress + " - disabling output");
        active = false;
    }

    portCount = (channelCount + 899) / 900;

    twinklyIovecs = (struct iovec*)calloc(portCount * 2, sizeof(struct iovec));
    twinklyBuffers = (uint8_t**)calloc(portCount, sizeof(unsigned char*));

    int chanToOutput = channelCount;
    int chan = startChannel - 1;
    for (int x = 0; x < portCount; x++) {
        twinklyBuffers[x] = (unsigned char*)malloc(HEADER_LEN);
        twinklyBuffers[x][0] = 3;
        twinklyBuffers[x][9] = 0;
        twinklyBuffers[x][10] = 0;
        twinklyBuffers[x][11] = x;

        // use scatter/gather for the packet.   One IOV will contain
        // the header, the second will point into the raw channel data
        // and will be set at output time.   This avoids any memcpy.
        twinklyIovecs[x * 2].iov_base = twinklyBuffers[x];
        twinklyIovecs[x * 2].iov_len = HEADER_LEN;
        twinklyIovecs[x * 2 + 1].iov_base = nullptr;
        twinklyIovecs[x * 2 + 1].iov_len = chanToOutput >= 900 ? 900 : chanToOutput;

        if (chanToOutput >= 900) {
            chanToOutput -= 900;
            chan += 900;
        }
    }
}
TwinklyOutputData::~TwinklyOutputData() {
    for (int x = 0; x < portCount; x++) {
        free(twinklyBuffers[x]);
    }
    free(twinklyBuffers);
    free(twinklyIovecs);
}

void TwinklyOutputData::GetRequiredChannelRange(int& min, int& max) {
    min = startChannel - 1;
    max = startChannel + (channelCount * portCount) - 1;
}

void TwinklyOutputData::PrepareData(unsigned char* channelData, UDPOutputMessages& msgs) {
    if (valid && active) {
        reauthCount++;
        if (reauthCount > MAXPACKETS) {
            // need to re-authenticate or lights will stop eventually
            StartingOutput();
        }

        int start = 0;
        struct mmsghdr msg;
        memset(&msg, 0, sizeof(msg));

        msg.msg_hdr.msg_name = &twinklyAddress;
        msg.msg_hdr.msg_namelen = sizeof(sockaddr_in);
        bool skipped = false;
        bool allSkipped = true;
        for (int p = 0; p < portCount; p++) {
            bool nto = NeedToOutputFrame(channelData, startChannel - 1, start, twinklyIovecs[p * 2 + 1].iov_len);
            if (nto) {
                msg.msg_hdr.msg_iov = &twinklyIovecs[p * 2];
                msg.msg_hdr.msg_iovlen = 2;
                msg.msg_len = twinklyIovecs[p * 2 + 1].iov_len + twinklyIovecs[p * 2].iov_len;

                msgs[twinklyAddress.sin_addr.s_addr].push_back(msg);

                // set the pointer to the channelData for the universe
                twinklyIovecs[p * 2 + 1].iov_base = (void*)(&channelData[startChannel - 1 + start]);
                allSkipped = false;
            } else {
                skipped = true;
            }
            start += twinklyIovecs[p * 2 + 1].iov_len;
        }
        if (skipped) {
            skippedFrames++;
        }
        if (!allSkipped) {
            SaveFrame(&channelData[startChannel - 1], start);
        }
    }
}

void TwinklyOutputData::StartingOutput() {
    authenticate();
    Timers::INSTANCE.addPeriodicTimer("Twinkly" + ipAddress, TWINKLY_TOKEN_VALIDATE_TIME * 1000, [this]() {
        verifyToken();
    });
}
void TwinklyOutputData::StoppingOutput() {
    callRestAPI(true, "xled/v1/led/mode", "{\"mode\": \"off\"}");
    authToken = "";
    Timers::INSTANCE.stopPeriodicTimer("Twinkly" + ipAddress);
}
void TwinklyOutputData::authenticate() {
    Json::Value r = callRestAPI(true, "xled/v1/login", "{\"challenge\": \"AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8=\"}");
    authToken = r["authentication_token"].asString();
    if (authToken != "") {
        reauthCount = 0;
        std::vector<uint8_t> at = base64Decode(authToken);
        memcpy(authTokenBytes, &at[0], std::min(TOKEN_LEN, (int)at.size()));
        for (int x = 0; x < portCount; x++) {
            memcpy(&twinklyBuffers[x][1], &at[0], std::min(TOKEN_LEN, (int)at.size()));
        }
    }
    callRestAPI(true, "xled/v1/verify", "");
    callRestAPI(true, "xled/v1/led/mode", "{\"mode\": \"rt\"}");
}
void TwinklyOutputData::verifyToken() {
    std::string url = "http://" + ipAddress + "/xled/v1/verify";
    std::list<std::string> extraHeaders;
    std::string xat = "X-Auth-Token: " + authToken;
    extraHeaders.push_back(xat);
    CurlManager::INSTANCE.add(url, "GET", "", extraHeaders, [this](int rc, const std::string& resp) {
        if (rc == 200) {
            // printf("%d:  %s\n", rc, resp.c_str());
            Json::Value v = LoadJsonFromString(resp);
            if (v["code"].asInt() != 1000) {
                authenticate();
            }
        }
    });
}

// URL Helpers
size_t urlWriteData(void* buffer, size_t size, size_t nmemb, void* userp) {
    std::string* str = (std::string*)userp;

    str->append(static_cast<const char*>(buffer), size * nmemb);

    return size * nmemb;
}
Json::Value TwinklyOutputData::callRestAPI(bool isPost, const std::string& path, const std::string& data) {
    std::string url = "http://" + ipAddress + "/" + path;

    CURL* curl = curl_easy_init();
    struct curl_slist* headers = NULL;
    std::string userAgent = "FPP/";
    userAgent += getFPPVersionTriplet();
    std::string resp = "";

    CURLcode status;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, urlWriteData);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
    if (startsWith(data, "{") && endsWith(data, "}")) {
        headers = curl_slist_append(headers, "Accept: application/json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
    }
    if (authToken != "") {
        const std::string at = "X-Auth-Token: " + authToken;
        headers = curl_slist_append(headers, at.c_str());
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    if (isPost) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1);
    }

    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)5);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());

    status = curl_easy_perform(curl);
    if (status != CURLE_OK) {
        LogErr(VB_GENERAL, "curl_easy_perform() failed: %s\n", curl_easy_strerror(status));
        return false;
    }

    // printf("%s:   %s\n", url.c_str(), resp.c_str());
    LogDebug(VB_GENERAL, "%s %s resp: %s\n", isPost ? "POST" : "GET", url.c_str(), resp.c_str());

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return LoadJsonFromString(resp);
}

void TwinklyOutputData::DumpConfig() {
    LogDebug(VB_CHANNELOUT, "Twinkly: %s   %d:%d:%d:%d  %s\n",
             description.c_str(),
             active,
             startChannel,
             channelCount,
             type,
             ipAddress.c_str());
}
