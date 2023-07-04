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

#include <ctime>
#include <httpserver.hpp>

#define FPP_HTTP_PORT 32322

#define FPPD_API_VERSION "v1"

using namespace httpserver;

class PlayerResource : public http_resource {
public:
    PlayerResource();

    HTTP_RESPONSE_CONST std::shared_ptr<http_response> render_GET(const http_request& req);
    HTTP_RESPONSE_CONST std::shared_ptr<http_response> render_DELETE(const http_request& req);
    HTTP_RESPONSE_CONST std::shared_ptr<http_response> render_POST(const http_request& req);
    HTTP_RESPONSE_CONST std::shared_ptr<http_response> render_PUT(const http_request& req);

private:
    void GetRunningEffects(Json::Value& result);
    void GetLogSettings(Json::Value& result);
    void GetCurrentStatus(Json::Value& result);
    void GetCurrentPlaylists(Json::Value& result);
    void GetE131BytesReceived(Json::Value& result);
    void GetMultiSyncSystems(Json::Value& result, bool localOnly = false);
    void GetMultiSyncStats(Json::Value& result, bool reset = false);
    void GetPlaylistFileTime(Json::Value& result);
    void GetPlaylistConfig(Json::Value& result);

    void PostEffects(const std::string& effectName, const Json::Value& data,
                     Json::Value& result);
    void PostFalconHardware(Json::Value& result);
    void PostGPIOExt(const Json::Value& data, Json::Value& result);
    void PostOutputs(const Json::Value& data, Json::Value& result);
    void PostOutputsRemap(const Json::Value& data, Json::Value& result);
    void PostSchedule(const Json::Value data, Json::Value& result);

    void SetOKResult(Json::Value& result, const std::string& msg);
    void SetErrorResult(Json::Value& result, const int respCode, const std::string& msg);
};

class APIServer {
public:
    APIServer();
    ~APIServer();

    void Init();

private:
    create_webserver m_params;
    webserver* m_ws;
    PlayerResource* m_pr;
};
