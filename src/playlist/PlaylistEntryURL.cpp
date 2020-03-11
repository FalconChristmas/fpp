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

#include "log.h"
#include "PlaylistEntryURL.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryURL::PlaylistEntryURL(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_curl(NULL)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::PlaylistEntryURL()\n");

	m_type = "url";
}

/*
 *
 */
PlaylistEntryURL::~PlaylistEntryURL()
{
	if (m_curl)
		curl_easy_cleanup(m_curl);
}

/*
 *
 */
int PlaylistEntryURL::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::Init()\n");

	m_method = config["method"].asString();
	m_url = config["url"].asString();

	if (config.isMember("data"))
		m_data = config["data"].asString();

	m_curl = curl_easy_init();
	if (!m_curl)
	{
		LogErr(VB_PLAYLIST, "Unable to create curl instance\n");
		return 0;
	}

	CURLcode status;

	status = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &PlaylistEntryURL::write_data);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting write callback function: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting class pointer: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 30L);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting timeout: %s\n", curl_easy_strerror(status));
		return 0;
	}

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryURL::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	std::string repURL;
	std::string repData;

	repURL = ReplaceMatches(m_url);

	if (m_data.size())
		repData = ReplaceMatches(m_data);

	m_response = "";

	CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, repURL.c_str());
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
		return 0;
	}

	if (m_method == "POST")
	{
		curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, 4096);
		status = curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, repData.c_str());
		if (status != CURLE_OK)
		{
			LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting post data: %s\n", curl_easy_strerror(status));
			return 0;
		}
	}

	status = curl_easy_perform(m_curl);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_perform() failed: %s\n", curl_easy_strerror(status));
		return 0;
	}

	LogDebug(VB_PLAYLIST, "m_resp: %s\n", m_response.c_str());

	PlaylistEntryBase::StartPlaying();

	FinishPlay();

	return 1;
}

/*
 *
 */
int PlaylistEntryURL::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::Stop()\n");
    return PlaylistEntryBase::Stop();
}

/*
 *
 */
std::string PlaylistEntryURL::ReplaceMatches(std::string in)
{
	std::string out = in;

	LogDebug(VB_PLAYLIST, "In: '%s'\n", in.c_str());


	LogDebug(VB_PLAYLIST, "Out: '%s'\n", out.c_str());

	return PlaylistEntryBase::ReplaceMatches(out);
}


/*
 *
 */
void PlaylistEntryURL::Dump(void)
{
	LogDebug(VB_PLAYLIST, "URL     : %s\n", m_method.c_str());
	LogDebug(VB_PLAYLIST, "Method  : %s\n", m_url.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryURL::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["url"] = m_url;
	result["method"] = m_method;

	return result;
}

/*
 *
 */
int PlaylistEntryURL::ProcessData(void *buffer, size_t size, size_t nmemb)
{
	LogDebug(VB_PLAYLIST, "ProcessData( %p, %d, %d)\n", buffer, size, nmemb);

	m_response.append(static_cast<const char*>(buffer), size * nmemb);

	LogDebug(VB_PLAYLIST, "m_response length: %d\n", m_response.size());

	return size * nmemb;
}

/*
 *
 */
size_t PlaylistEntryURL::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	PlaylistEntryURL *peURL = (PlaylistEntryURL*)userp;

	return static_cast<PlaylistEntryURL*>(userp)->ProcessData(buffer, size, nmemb);
}

