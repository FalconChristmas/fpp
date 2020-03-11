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

#include "common.h"
#include "log.h"
#include "PlaylistEntryBoth.h"
#include "PlaylistEntryCommand.h"
#include "PlaylistEntryEffect.h"
#include "PlaylistEntryEvent.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryMQTT.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryRemap.h"
#include "PlaylistEntryScript.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryURL.h"
#include "PlaylistEntryVolume.h"
#include "PlaylistEntryDynamic.h"
#include "settings.h"
#include "Playlist.h"


/*
 *
 */
PlaylistEntryDynamic::PlaylistEntryDynamic(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_curl(NULL),
	m_drainQueue(0),
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

	if (m_curl)
		curl_easy_cleanup(m_curl);
}

/*
 *
 */
int PlaylistEntryDynamic::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryDynamic::Init()\n");

	m_subType = config["subType"].asString();

	m_drainQueue = config["drainQueue"].asInt();

	if (config.isMember("pluginHost"))
		m_pluginHost = config["pluginHost"].asString();

	if (m_subType == "file")
		m_data = config["dataFile"].asString();
	else if (m_subType == "plugin")
		m_data = config["dataPlugin"].asString();
	else if (m_subType == "url")
		m_data = config["dataURL"].asString();

	if ((m_subType == "plugin") || (m_subType == "url"))
	{
		CURLcode status;
		m_curl = curl_easy_init();
		if (!m_curl)
		{
			LogErr(VB_PLAYLIST, "Unable to create curl instance\n");
			return 0;
		}

		status = curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, &PlaylistEntryDynamic::write_data);
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

		status = curl_easy_setopt(m_curl, CURLOPT_COOKIEFILE, "");
		if (status != CURLE_OK)
		{
			LogErr(VB_PLAYLIST, "curl_easy_setopt() Error initializing cookie jar: %s\n", curl_easy_strerror(status));
			return 0;
		}

		status = curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 5L);
		if (status != CURLE_OK)
		{
			LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting timeout: %s\n", curl_easy_strerror(status));
			return 0;
		}
	}

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
	if ((m_currentEntry >= 0) && (m_playlistEntries[m_currentEntry]))
	{
		m_playlistEntries[m_currentEntry]->Process();
		if (m_playlistEntries[m_currentEntry]->IsFinished())
		{
			if ((playlist->getPlaylistStatus() == FPP_STATUS_STOPPING_GRACEFULLY) ||
				(playlist->getPlaylistStatus() == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP))
			{
				FinishPlay();
			}
			else if (m_currentEntry < (m_playlistEntries.size() - 1))
			{
				m_currentEntry++;
				m_playlistEntries[m_currentEntry]->StartPlaying();
			}
			else if (m_drainQueue)
			{
				// Check for more entries to play, if there are none,
				// then StartPlaying() will call FinishPlay().
				StartPlaying();
			}
			else
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

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryDynamic::Dump(void)
{
	LogDebug(VB_PLAYLIST, "SubType    : %s\n", m_subType.c_str());
	LogDebug(VB_PLAYLIST, "Data       : %s\n", m_data.c_str());
	LogDebug(VB_PLAYLIST, "Drain Queue: %d\n", m_drainQueue);
	LogDebug(VB_PLAYLIST, "Plugin Host: %s\n", m_pluginHost.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryDynamic::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["subType"] = m_subType;
	result["data"] = m_data;
	result["pluginHost"] = m_pluginHost;
	result["drainQueue"] = m_drainQueue;
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

	url = "http://";
	if (m_pluginHost != "")
		url += m_pluginHost;
	else
		url += "127.0.0.1";
	url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=loadNextItem";

	return ReadFromURL(url);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromURL(std::string url)
{
	LogDebug(VB_PLAYLIST, "ReadFromURL: %s\n", url.c_str());

	m_response = "";

	if (!m_curl)
	{
		LogErr(VB_PLAYLIST, "m_curl is null\n");
		return 0;
	}

	CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_perform(m_curl);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_perform() failed: %s\n", curl_easy_strerror(status));
		return 0;
	}

	return ReadFromString(m_response);
}

/*
 *
 */
int PlaylistEntryDynamic::ReadFromString(std::string jsonStr)
{
	Json::Value root;
	PlaylistEntryBase *playlistEntry = NULL;

	LogDebug(VB_PLAYLIST, "ReadFromString(): String:\n%s\n", jsonStr.c_str());

	if (jsonStr.empty())
	{
		LogDebug(VB_PLAYLIST, "Empty string in ReadFromString()\n");
		return 0;
	}

	if (!LoadJsonFromString(jsonStr, root))
	{
		LogErr(VB_PLAYLIST, "Error parsing JSON: %s\n", jsonStr.c_str());
		return 0;
	}

	ClearPlaylistEntries();

	Json::Value entries;
	if (root.isMember("mainPlaylist"))
		entries = root["mainPlaylist"];
	else if (root.isMember("leadIn"))
		entries = root["leadIn"];
	else if (root.isMember("leadOut"))
		entries = root["leadOut"];
	else if (root.isMember("playlistEntries"))
		entries = root["playlistEntries"];
	else
		return 0;

	for (int i = 0; i < entries.size(); i++)
	{
		Json::Value pe = entries[i];
		playlistEntry = NULL;

		if (pe["type"].asString() == "both")
			playlistEntry = new PlaylistEntryBoth();
		else if (pe["type"].asString() == "command")
			playlistEntry = new PlaylistEntryCommand();
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
	std::string url;

	url = "http://";
	if (m_pluginHost != "")
		url += m_pluginHost;
	else
		url += "127.0.0.1";
	url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=startedNextItem";

	CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_perform(m_curl);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_perform returned an error: %d\n", status);
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
	std::string url;

	url = "http://";
	if (m_pluginHost != "")
		url += m_pluginHost;
	else
		url += "127.0.0.1";
	url += "/plugin.php?plugin=" + m_data + "&page=playlistCallback.php&nopage=1&command=prepNextItem";

	LogDebug(VB_PLAYLIST, "URL: %s\n", url.c_str());

	CURLcode status = curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_setopt() Error setting URL: %s\n", curl_easy_strerror(status));
		return 0;
	}

	status = curl_easy_perform(m_curl);
	if (status != CURLE_OK)
	{
		LogErr(VB_PLAYLIST, "curl_easy_perform returned an error: %d\n", status);
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

