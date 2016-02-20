/*
 *   HTTP API for the Falcon Player Daemon 
 *   Falcon Player project (FPP) 
 *
 *   Copyright (C) 2013 the Falcon Player Developers
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

#include "fppd.h"
#include "httpAPI.h"
#include "log.h"
#include "settings.h"
#include "Player.h"

#include <iostream>

#include <jsoncpp/json/json.h>

// from command.c
extern int fppdStartTime;

/*
 *
 */
APIServer::APIServer()
{
	fppdStartTime = time(NULL);
}

/*
 *
 */
APIServer::~APIServer()
{
	delete hsr1;
	delete hsr2;
	delete hcpr1;

	delete ws;
	ws = NULL;
}

/*
 *
 */
void APIServer::Init(void)
{
	params = create_webserver(FPP_HTTP_PORT).max_threads(5);

	ws = new webserver(params);

	hsr1 = new HttpStatusResource;
	ws->register_resource("/api/status", hsr1, true);

	hsr2 = new HttpStopResource;
	ws->register_resource("/api/stop", hsr2, true);

	hcpr1 = new HttpCurrentPlaylistResource;
	ws->register_resource("/api/playlist/current", hcpr1, true);

	ws->start(false);
}

/*
 *
 */
void LogRequest(const http_request &req)
{
	LogDebug(VB_HTTP, "API Req: %s%s\n", req.get_path().c_str(),
		req.get_querystring().c_str());
}

/*
 *
 */
void LogResponse(const http_request &req, http_response **res = NULL)
{
	if ((res) && LogLevelIsSet(LOG_EXCESSIVE))
	{
		http_response *resp = *res;
		LogDebug(VB_HTTP, "API Res: %s%s %d %s\n",
			req.get_path().c_str(),
			req.get_querystring().c_str(),
			resp->get_response_code(),
			resp->get_content().c_str());
	}
}

/*
 *
 */
void HttpStatusResource::render(const http_request &req, http_response **res)
{
	LogRequest(req);

	Json::Value result;

	result["mode"] = getFPPmodeStr();
	result["uptime"] = (int)(time(NULL) - fppdStartTime);
	result["state"] = "Unknown";

	Json::FastWriter fastWriter;
	std::string resultStr = fastWriter.write(result);

	*res = new http_response(http_response_builder(resultStr.c_str(), 200).string_response());

	LogResponse(req, res);
}

/*
 *
 */
void HttpStopResource::render_POST(const http_request &req, http_response **res)
{
	LogRequest(req);

	ShutdownFPPD();

	// we never make it here since shutdown happens so quick
	*res = new http_response(http_response_builder("OK", 200).string_response());

	LogResponse(req, res);
}

/*
 *
 */
void HttpCurrentPlaylistResource::render(const http_request &req, http_response **res)
{
	LogRequest(req);

	Json::Value result = player->GetCurrentPlaylist();

	Json::FastWriter fastWriter;
	std::string resultStr = fastWriter.write(result);

	*res = new http_response(http_response_builder(resultStr.c_str(), 200).string_response());

	LogResponse(req, res);
}

