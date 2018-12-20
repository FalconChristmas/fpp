/*
 *   PlaylistEntryDynamic Class for Falcon Player (FPP)
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
	m_currentEntry(-1)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::PlaylistEntryDynamic()\n");

	m_type = "dynamic";
}

/*
 *
 */
PlaylistEntryDynamic::~PlaylistEntryDynamic()
{
	ClearPlaylistEntries();
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

	if (!IsPrepped() && !Prep())
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

	m_isPrepped = 0;

	if (m_currentEntry >= 0)
	{
		m_playlistEntries[m_currentEntry]->StartPlaying();
		Started();
	}
	else
	{
		FinishPlay();
		return 0;
	}

	return PlaylistEntryBase::StartPlaying();;
}

/*
 *
 */
int PlaylistEntryDynamic::Process(void)
{
	LogExcess(VB_PLAYLIST, "PlaylistEntryDynamic::Process()\n");

	if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry]))
	{
		m_playlistEntries[m_currentEntry]->Process();
		if (m_playlistEntries[m_currentEntry]->IsFinished())
		{
			if (m_currentEntry < (m_playlistEntries.size() - 1))
			{
				m_currentEntry++;
				m_playlistEntries[m_currentEntry]->StartPlaying();
			}
			else // no more entries
			{
				FinishPlay();
			}
		}

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

	if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry]))
	{
		m_playlistEntries[m_currentEntry]->Stop();
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
	if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry]))
		result["dynamic"] = m_playlistEntries[m_currentEntry]->GetConfig();

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

	url = "http://127.0.0.1/plugin.php?plugin=";
	url += m_data;
	url += "&page=playlistCallback.php&nopage=1&command=loadNextItem";

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
	PlaylistEntryBase *playlistEntry = NULL;

	LogDebug(VB_PLAYLIST, "ReadFromString()\n%s\n", jsonStr.c_str());

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

	ClearPlaylistEntries();

	if (!root.isMember("playlistEntries"))
	{
		return 0;
	}

	for (int i = 0; i < root["playlistEntries"].size(); i++)
	{
		Json::Value pe = root["playlistEntries"][i];
		playlistEntry = NULL;

		if (pe["type"].asString() == "both")
			playlistEntry = new PlaylistEntryBoth();
		else if (pe["type"].asString() == "effect")
			playlistEntry = new PlaylistEntryEffect();
		else if (pe["type"].asString() == "event")
			playlistEntry = new PlaylistEntryEvent();
		else if (pe["type"].asString() == "media")
			playlistEntry = new PlaylistEntryMedia();
		else if (pe["type"].asString() == "mqtt")
			playlistEntry = new PlaylistEntryMQTT();
		else if (pe["type"].asString() == "pause")
			playlistEntry = new PlaylistEntryPause();
		else if (pe["type"].asString() == "pixelOverlay")
			playlistEntry = new PlaylistEntryPixelOverlay();
		else if (pe["type"].asString() == "remap")
			playlistEntry = new PlaylistEntryRemap();
		else if (pe["type"].asString() == "script")
			playlistEntry = new PlaylistEntryScript();
		else if (pe["type"].asString() == "sequence")
			playlistEntry = new PlaylistEntrySequence();
		else if (pe["type"].asString() == "url")
			playlistEntry = new PlaylistEntryURL();
		else if (pe["type"].asString() == "volume")
			playlistEntry = new PlaylistEntryVolume();
		else
		{
			LogErr(VB_PLAYLIST, "Invalid Playlist Entry Type: %s\n", pe["type"].asString().c_str());
			ClearPlaylistEntries();
			return 0;
		}

		if (!playlistEntry->Init(pe))
		{
			LogErr(VB_PLAYLIST, "Error initializing %s Playlist Entry\n", pe["type"].asString().c_str());
			ClearPlaylistEntries();
			return 0;
		}

		m_playlistEntries.push_back(playlistEntry);
	}

	if (!m_playlistEntries.size())
	{
		LogErr(VB_PLAYLIST, "Error, no valid playlistEntries in dynamic data!\n");
		return 0;
	}

	m_currentEntry = 0;

	return 1;
}

/*
 *
 */
int PlaylistEntryDynamic::Started(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Started()\n");

	int res = 1;
	if (m_subType == "plugin")
		res = StartedPlugin();

	if (!res)
	{
		return 0;
	}

	return res;
}

/*
 *
 */
int PlaylistEntryDynamic::StartedPlugin(void)
{
	CURL *curl;
	CURLcode res;

	std::string url;

	url = "http://127.0.0.1/plugin.php?plugin=";
	url += m_data;
	url += "&page=playlistCallback.php&nopage=1&command=startedNextItem";

	curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			LogErr(VB_PLAYLIST, "curl_easy_perform returned an error: %d\n", res);
			return 0;
		}

		curl_easy_cleanup(curl);
	}
	else
	{
		LogErr(VB_PLAYLIST, "curl_easy_init() returned an error.\n" );
		return 0;
	}

	return 1;
}

/*
 *
 */
int PlaylistEntryDynamic::Prep(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Prep()\n");

	int res = 1;
	if (m_subType == "plugin")
		res = PrepPlugin();

	if (!res)
	{
		return 0;
	}

	return PlaylistEntryBase::Prep();;
}

/*
 *
 */
int PlaylistEntryDynamic::PrepPlugin(void)
{
	CURL *curl;
	CURLcode res;

	std::string url;

	url = "http://127.0.0.1/plugin.php?plugin=";
	url += m_data;
	url += "&page=playlistCallback.php&nopage=1&command=prepNextItem";

	curl = curl_easy_init();
	if (curl)
	{
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			LogErr(VB_PLAYLIST, "curl_easy_perform returned an error: %d\n", res);
			return 0;
		}

		curl_easy_cleanup(curl);
	}
	else
	{
		LogErr(VB_PLAYLIST, "curl_easy_init() returned an error.\n" );
		return 0;
	}

	return 1;
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
void PlaylistEntryDynamic::ClearPlaylistEntries(void)
{
	while (!m_playlistEntries.empty())
	{
		delete m_playlistEntries.back();
		m_playlistEntries.pop_back();
	}

	m_currentEntry = -1;
}

/*
 *
 */
size_t PlaylistEntryDynamic::write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
	PlaylistEntryDynamic *peDynamic = (PlaylistEntryDynamic*)userp;

	return static_cast<PlaylistEntryDynamic*>(userp)->ProcessData(buffer, size, nmemb);
}

