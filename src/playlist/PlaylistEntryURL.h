/*
 *   PlaylistEntryURL Class for Falcon Player (FPP)
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

#ifndef _PLAYLISTENTRYURL_H
#define _PLAYLISTENTRYURL_H

#include <string>

#include <curl/curl.h>

#include "PlaylistEntryBase.h"

class PlaylistEntryURL : public PlaylistEntryBase {
  public:
	PlaylistEntryURL(PlaylistEntryBase *parent = NULL);
	virtual ~PlaylistEntryURL();

	virtual int  Init(Json::Value &config) override;

	virtual int  StartPlaying(void) override;
	virtual int  Process(void) override;
	virtual int  Stop(void) override;

	virtual std::string ReplaceMatches(std::string in) override;

	virtual void Dump(void) override;

	Json::Value GetConfig(void) override;

  private:
	int ProcessData(void *buffer, size_t size, size_t nmemb);

	static size_t write_data(void *ptr, size_t size, size_t nmemb,
	                             void *ourpointer);

	std::string            m_url;
	std::string            m_method;
	std::string            m_data;
	std::string            m_response;

	CURL                  *m_curl;
	CURLM                 *m_curlm;
};

#endif
