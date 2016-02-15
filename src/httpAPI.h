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

#ifndef _HTTPAPI_H

#include <httpserver.hpp>

#define FPP_HTTP_PORT 32321

using namespace httpserver;

class HttpStatusResource : public http_resource<HttpStatusResource> {
  public:
	void render(const http_request&, http_response**);
};

class HttpStopResource : public http_resource<HttpStopResource> {
  public:
	void render_POST(const http_request&, http_response**);
};

class APIServer {
  public:
  	APIServer();
	~APIServer();

	void Init();

  private:
	create_webserver params;
	webserver   *ws;

	HttpStatusResource        *hsr1;
	HttpStopResource          *hsr2;
};

#endif
