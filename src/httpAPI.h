/*
 *   HTTP API for the Falcon Player Daemon 
 *   Falcon Player project (FPP) 
 *
 *   Copyright (C) 2013-2018 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Player (FPP) is free software; you can redistribute it
 *   and/or modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _HTTPAPI_H

#include <httpserver.hpp>
#include <jsoncpp/json/json.h>

#define FPP_HTTP_PORT 32322

#define FPPD_API_VERSION "v1"

using namespace httpserver;

class PlayerResource : public http_resource {
  public:
	const std::shared_ptr<http_response> render_GET(const http_request &req);
	const std::shared_ptr<http_response> render_DELETE(const http_request &req);
	const std::shared_ptr<http_response> render_POST(const http_request &req);
	const std::shared_ptr<http_response> render_PUT(const http_request &req);

  private:
	void GetRunningEffects(Json::Value &result);
	void GetRunningEvents(Json::Value &result);
	void GetLogSettings(Json::Value &result);
	void GetCurrentStatus(Json::Value &result);
	void GetCurrentPlaylists(Json::Value &result);
	void GetE131BytesReceived(Json::Value &result);
	void GetMultiSyncSystems(Json::Value &result);
	void GetPlaylistFileTime(Json::Value &result);
	void GetPlaylistConfig(Json::Value &result);

	void PostEffects(const std::string &effectName, const Json::Value &data,
					Json::Value &result);
	void PostEvents(const std::string &eventID, const Json::Value &data,
					Json::Value &result);
	void PostFalconHardware(Json::Value &result);
	void PostGPIOExt(const Json::Value &data, Json::Value &result);
	void PostOutputs(const Json::Value &data, Json::Value &result);
	void PostOutputsRemap(const Json::Value &data, Json::Value &result);
	void PostSchedule(const Json::Value data, Json::Value &result);
	void PostTesting(const Json::Value data, Json::Value &result);

	void SetOKResult(Json::Value &result, const std::string &msg);
	void SetErrorResult(Json::Value &result, const int respCode, const std::string &msg);
};

class APIServer {
  public:
	APIServer();
	~APIServer();

	void Init();

  private:
	create_webserver   m_params;
	webserver         *m_ws;
	PlayerResource    *m_pr;
};

#endif
