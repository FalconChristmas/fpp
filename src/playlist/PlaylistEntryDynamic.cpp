/*
 *   PlaylistEntryDynamic Class for Falcon Player (FPP)
 *
 *   Copyright (C) 2016 the Falcon Player Developers
 *      Initial development by:
 *      - David Pitts (dpitts)
 *      - Tony Mace (MyKroFt)
 *      - Mathew Mrosko (Materdaddy)
 *      - Chris Pinkham (CaptainMurdoch)
 *      For additional credits and developers, see credits.php.
 *
 *   The Falcon Pi Player (FPP) is free software; you can redistribute it
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

#include <fstream>
#include <sstream>
#include <string>

#include <boost/algorithm/string/replace.hpp>

#include "common.h"
#include "log.h"
#include "PlaylistEntryBoth.h"
#include "PlaylistEntryEffect.h"
#include "PlaylistEntryEvent.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryMQTT.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryPixelOverlay.h"
#include "PlaylistEntryRemap.h"
#include "PlaylistEntryScript.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryURL.h"
#include "PlaylistEntryVolume.h"
#include "PlaylistEntryDynamic.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryDynamic::PlaylistEntryDynamic(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_playlistEntry(NULL)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::PlaylistEntryDynamic()\n");

	m_type = "dynamic";
}

/*
 *
 */
PlaylistEntryDynamic::~PlaylistEntryDynamic()
{
	if (m_playlistEntry)
	{
		delete m_playlistEntry;
		m_playlistEntry = NULL;
	}

}

/*
 *
 */
int PlaylistEntryDynamic::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Init()\n");

	m_subType = config["subType"].asString();
	m_data = config["data"].asString();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryDynamic::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	int res = 0;
	if (m_subType == "command")
		res = ReadFromCommand();
	else if (m_subType == "file")
		res = ReadFromFile();
	else if (m_subType == "plugin")
		res = ReadFromPlugin();
	else if (m_subType == "url")
		res = ReadFromURL(m_data);

	if (!res)
	{
		FinishPlay();
		return 0;
	}

	m_playlistEntry->StartPlaying();

	return PlaylistEntryBase::StartPlaying();;
}

/*
 *
 */
int PlaylistEntryDynamic::Process(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Process()\n");

	if (m_playlistEntry)
	{
		m_playlistEntry->Process();
		if (m_playlistEntry->IsFinished())
			FinishPlay();

		return 1;
	}

	return 0;
}

/*
 *
 */
int PlaylistEntryDynamic::Stop(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Stop()\n");

	if (m_playlistEntry)
	{
		m_playlistEntry->Stop();
		return 1;
	}

	return 0;
}

/*
 *
 */
void PlaylistEntryDynamic::Dump(void)
{
	LogDebug(VB_PLAYLIST, "SubType : %s\n", m_subType.c_str());
	LogDebug(VB_PLAYLIST, "Data    : %s\n", m_data.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryDynamic::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["subType"] = m_subType;
	result["data"] = m_data;

	return result;
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromCommand(void)
{
	LogDebug(VB_PLAYLIST, "ReadFromCommand: %s\n", m_data.c_str());

	// FIXME, implement this and change return to 1
	return 0;
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromFile(void)
{
	LogDebug(VB_PLAYLIST, "ReadFromFile: %s\n", m_data.c_str());

	if (!FileExists(m_data.c_str()))
	{
		LogErr(VB_PLAYLIST, "Filename %s does not exist\n", m_data.c_str());
		return 0;
	}

	std::ifstream t(m_data.c_str());
	std::stringstream buffer;

	buffer << t.rdbuf();

	return ReadFromString(buffer.str());
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromPlugin(void)
{
	LogDebug(VB_PLAYLIST, "ReadFromPlugin: %s\n", m_data.c_str());

	std::string url;

	// FIXME Playlist: what should this URL be?
	url = "http://127.0.0.1/plugin.php?plugin=";
	url += m_data;
	url += "&page=dynamicPlaylistEntryJSON.php&nopage=1";

	return ReadFromURL(url);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromURL(std::string url)
{
	CURLcode status;

	LogDebug(VB_PLAYLIST, "ReadFromURL: %s\n", url.c_str());

	CURL *curl = curl_easy_init();
	if (!curl)
	{
		LogErr(VB_PLAYLIST, "Unable to create curl instance\n");
		return 0;
	}

	status = curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &PlaylistEntryDynamic::write_data);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting write callback function: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting class pointer: %s\n", curl_easy_strerror(status));
		return 0;
	}

	m_response = "";

	status = curl_easy_perform(curl);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_perform() failed: %s\n", curl_easy_strerror(status));
		return 0;
	}

	curl_easy_cleanup(curl);

	return ReadFromString(m_response);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromString(std::string jsonStr)
{
	Json::Value root;
	Json::Reader reader;

	if (jsonStr.empty())
	{
		LogDebug(VB_PLAYLIST, "Empty string in ReadFromString()\n");
		return 0;
	}

	bool success = reader.parse(jsonStr, root);
	if (!success)
	{
		LogErr(VB_PLAYLIST, "Error parsing JSON: %s\n", jsonStr.c_str());
		return 0;
	}

	if (m_playlistEntry)
	{
		delete m_playlistEntry;
		m_playlistEntry = NULL;
	}

	if (root["type"].asString() == "both")
		m_playlistEntry = new PlaylistEntryBoth();
	else if (root["type"].asString() == "effect")
		m_playlistEntry = new PlaylistEntryEffect();
	else if (root["type"].asString() == "event")
		m_playlistEntry = new PlaylistEntryEvent();
	else if (root["type"].asString() == "media")
		m_playlistEntry = new PlaylistEntryMedia();
	else if (root["type"].asString() == "mqtt")
		m_playlistEntry = new PlaylistEntryMQTT();
	else if (root["type"].asString() == "pause")
		m_playlistEntry = new PlaylistEntryPause();
	else if (root["type"].asString() == "pixelOverlay")
		m_playlistEntry = new PlaylistEntryPixelOverlay();
	else if (root["type"].asString() == "remap")
		m_playlistEntry = new PlaylistEntryRemap();
	else if (root["type"].asString() == "script")
		m_playlistEntry = new PlaylistEntryScript();
	else if (root["type"].asString() == "sequence")
		m_playlistEntry = new PlaylistEntrySequence();
	else if (root["type"].asString() == "url")
		m_playlistEntry = new PlaylistEntryURL();
	else if (root["type"].asString() == "volume")
		m_playlistEntry = new PlaylistEntryVolume();
	else
	{
		LogErr(VB_PLAYLIST, "Invalid Playlist Entry Type: %s\n", root["type"].asString().c_str());
		m_playlistEntry = NULL;
		return 0;
	}

	if (m_playlistEntry->Init(root))
		return 1;

	m_playlistEntry = NULL;

	return 0;
}

/*
 *
 */
int PlaylistEntryDynamic::ProcessData(void *buffer, size_t size, size_t nmemb)
{
	LogDebug(VB_PLAYLIST, "ProcessData( %p, %d, %d)\n", buffer, size, nmemb);

	m_response.append(static_cast<const char*>(buffer), size * nmemb);

	LogDebug(VB_PLAYLIST, "m_response length: %d\n", m_response.size());

	return size * nmemb;
}

/*
 *
 */
size_t PlaylistEntryDynamic::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	PlaylistEntryDynamic *peDynamic = (PlaylistEntryDynamic*)userp;

	return static_cast<PlaylistEntryDynamic*>(userp)->ProcessData(buffer, size, nmemb);
}

