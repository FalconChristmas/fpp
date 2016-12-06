/*
 *   Playlist Class for Falcon Player (FPP)
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

#include "string.h"
#include "unistd.h"

#include <fstream>
#include <sstream>
#include <string>

#include <jsoncpp/json/json.h>

#include "common.h"
#include "fpp.h"
#include "log.h"
#include "Player.h"
#include "Playlist.h"
#include "settings.h"

#include "PlaylistEntryBoth.h"
//#include "PlaylistEntryBranch.h"
#include "PlaylistEntryBrightness.h"
#include "PlaylistEntryChannelTest.h"
#include "PlaylistEntryEffect.h"
#include "PlaylistEntryEvent.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryPixelOverlay.h"
//#include "PlaylistEntryPlaylist.h"
#include "PlaylistEntryPlugin.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryVolume.h"

/*
 *
 */
Playlist::Playlist(Player *parent)
  : m_player(parent),
	m_repeat(0),
	m_loop(0),
	m_loopCount(0),
	m_random(0),
	m_blankBetweenSequences(0),
	m_blankBetweenIterations(0),
	m_blankAtEnd(1),
	m_startTime(0),
	m_subPlaylistDepth(0),
	m_currentState("idle"),
	m_currentSectionStr("New"),
	m_sectionPosition(0)
{
}

/*
 *
 */
Playlist::~Playlist()
{
	while (m_leadIn.size())
	{
		PlaylistEntryBase *entry = m_leadIn.back();
		m_leadIn.pop_back();
		delete entry;
	}

	while (m_mainPlaylist.size())
	{
		PlaylistEntryBase *entry = m_mainPlaylist.back();
		m_mainPlaylist.pop_back();
		delete entry;
	}

	while (m_leadOut.size())
	{
		PlaylistEntryBase *entry = m_leadOut.back();
		m_leadOut.pop_back();
		delete entry;
	}
}

/*
 *
 */
int Playlist::LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*> &playlistPart, const Json::Value &entries)
{
	PlaylistEntryBase *plEntry = NULL;

	for (int c = 0; c < entries.size(); c++)
	{
		// Long-term handle sub-playlists on-demand instead of at load time
		if (entries[c]["type"].asString() == "playlist")
		{
			m_subPlaylistDepth++;

			if (m_subPlaylistDepth < 3)
			{
				Json::Value subPlaylist = LoadJSON(entries[c]["playlistName"].asString().c_str());

				if (subPlaylist.isMember("leadIn"))
					LoadJSONIntoPlaylist(playlistPart, subPlaylist["leadIn"]);

				if (subPlaylist.isMember("mainPlaylist"))
					LoadJSONIntoPlaylist(playlistPart, subPlaylist["mainPlaylist"]);

				if (subPlaylist.isMember("leadOut"))
					LoadJSONIntoPlaylist(playlistPart, subPlaylist["leadOut"]);
			}
			else
			{
				LogErr(VB_PLAYLIST, "Error, sub-playlist depth exceeded 3 trying to include '%s'\n", entries[c]["playlistName"].asString().c_str());
			}

			m_subPlaylistDepth--;
		}
		else
		{
			plEntry = LoadPlaylistEntry(entries[c]);
			if (plEntry)
				playlistPart.push_back(plEntry);
		}
	}
	
	return 1;
}

/*
 *
 */
int Playlist::Load(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "Playlist::Load(JSON)\n");

	m_name = config["name"].asString();
	m_repeat = config["repeat"].asInt();
	m_loopCount = config["loopCount"].asInt();
	m_subPlaylistDepth = 0;

	PlaylistEntryBase *plEntry = NULL;

	if (config.isMember("leadIn"))
	{
		LogDebug(VB_PLAYLIST, "Loading LeadIn:\n");
		const Json::Value leadIn = config["leadIn"];

		LoadJSONIntoPlaylist(m_leadIn, leadIn);
	}

	if (config.isMember("mainPlaylist"))
	{
		LogDebug(VB_PLAYLIST, "Loading MainPlaylist:\n");
		const Json::Value playlist = config["mainPlaylist"];

		LoadJSONIntoPlaylist(m_mainPlaylist, playlist);
	}

	if (config.isMember("leadOut"))
	{
		LogDebug(VB_PLAYLIST, "Loading LeadOut:\n");
		const Json::Value leadOut = config["leadOut"];

		LoadJSONIntoPlaylist(m_leadOut, leadOut);
	}

	m_sectionPosition = 0;

	if ((logLevel & LOG_DEBUG) && (logMask & VB_PLAYLIST))
		Dump();

	return 1;
}

/*
 *
 */
Json::Value Playlist::LoadJSON(const char *filename)
{
	LogDebug(VB_PLAYLIST, "Playlist::LoadJSON(%s)\n", filename);

	Json::Value root;
	Json::Reader reader;

	char fullFilename[2048];
	strcpy(fullFilename, getPlaylistDirectory());
	strcat(fullFilename, "/");
	strcat(fullFilename, filename);
	strcat(fullFilename, ".json");

	if (!FileExists(fullFilename))
	{
		LogErr(VB_PLAYLIST, "Playlist %s does not exist\n", fullFilename);
		return root;
	}

	std::ifstream t(fullFilename);
	std::stringstream buffer;

	buffer << t.rdbuf();

	bool success = reader.parse(buffer.str(), root);
	if (!success)
	{
		LogErr(VB_PLAYLIST, "Error parsing %s\n", fullFilename);
		return root;
	}

	return root;
}

/*
 *
 */
int Playlist::Load(const char *filename)
{
	LogDebug(VB_PLAYLIST, "Playlist::Load(%s)\n", filename);

	Json::Value root = LoadJSON(filename);

	return Load(root);
}

/*
 *
 */
PlaylistEntryBase* Playlist::LoadPlaylistEntry(Json::Value entry)
{
	PlaylistEntryBase *result = NULL;

	if (entry["type"].asString() == "both")
		result = new PlaylistEntryBoth();
//	else if (entry["type"].asString() == "branch")
//		result = new PlaylistEntryBranch();
	else if (entry["type"].asString() == "brightness")
		result = new PlaylistEntryBrightness();
	else if (entry["type"].asString() == "channelTest")
		result = new PlaylistEntryChannelTest();
	else if (entry["type"].asString() == "effect")
		result = new PlaylistEntryEffect();
	else if (entry["type"].asString() == "event")
		result = new PlaylistEntryEvent();
	else if (entry["type"].asString() == "media")
		result = new PlaylistEntryMedia();
	else if (entry["type"].asString() == "pause")
		result = new PlaylistEntryPause();
	else if (entry["type"].asString() == "pixelOverlay")
		result = new PlaylistEntryPixelOverlay();
//	else if (entry["type"].asString() == "playlist")
//		result = new PlaylistEntryPlaylist();
	else if (entry["type"].asString() == "plugin")
		result = new PlaylistEntryPlugin();
	else if (entry["type"].asString() == "sequence")
		result = new PlaylistEntrySequence();
	else if (entry["type"].asString() == "volume")
		result = new PlaylistEntryVolume();
	else
	{
		LogErr(VB_PLAYLIST, "Unknown Playlist Entry Type: %s\n", entry["Type"].asString().c_str());
		return NULL;
	}

	if (result->Init(entry))
		return result;

	return result;
}

/*
 *
 */
int Playlist::Start(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::Start()\n");

	if ((!m_leadIn.size()) &&
		(!m_mainPlaylist.size()) &&
		(!m_leadOut.size()))
	{
		LogErr(VB_PLAYLIST, "Tried to play an empty playlist\n");
		SetIdle();
		return 0;
	}

	if (m_leadIn.size())
	{
		m_currentSectionStr = "LeadIn";
		m_currentSection = &m_leadIn;
	}
	else if (m_mainPlaylist.size())
	{
		m_currentSectionStr = "MainPlaylist";
		m_currentSection = &m_mainPlaylist;
	}
	else // must be only lead Out
	{
		m_currentSectionStr = "LeadOut";
		m_currentSection = &m_leadOut;
	}

	// FIXME PLAYLIST, get rid of this
	FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;

	m_currentState = "playing";

	if (m_sectionPosition >= m_currentSection->size())
		m_sectionPosition = 0;

	m_startTime = GetTime();
	m_loop = 0;
	m_currentSection->at(m_sectionPosition)->StartPlaying();

	return 1;
}

/*
 *
 */
int Playlist::StopNow(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::StopNow()\n");

	FPPstatus = FPP_STATUS_STOPPING_NOW;

	return 1;
}

/*
 *
 */
int Playlist::StopGracefully(int afterCurrentLoop)
{
	LogDebug(VB_PLAYLIST, "Playlist::StopGracefully()\n");

	// FIXME PLAYLIST, get rid of this
	FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;

	if (afterCurrentLoop)
		m_currentState = "stoppingAfterLoop";
	else
		m_currentState = "stoppingGracefully";

	return 1;
}

/*
 *
 */
int Playlist::Process(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::Process\n");

	if (m_sectionPosition >= m_currentSection->size())
	{
		LogErr(VB_PLAYLIST, "Section position %d is outside of section %s\n",
			m_sectionPosition, m_currentSectionStr.c_str());
		StopNow();
		return 0;
	}

	if (FPPstatus == FPP_STATUS_STOPPING_NOW)
	{
		if (m_currentSection->at(m_sectionPosition)->IsPlaying())
			m_currentSection->at(m_sectionPosition)->Stop();

		SetIdle();
		return 1;
	}

	if (m_currentSection->at(m_sectionPosition)->IsPlaying())
		m_currentSection->at(m_sectionPosition)->Process();

	if (m_currentSection->at(m_sectionPosition)->IsFinished())
	{
		LogDebug(VB_PLAYLIST, "Playlist entry finished\n");
		if ((logLevel & LOG_DEBUG) && (logMask & VB_PLAYLIST))
			m_currentSection->at(m_sectionPosition)->Dump();

		if (FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)
		{
			LogDebug(VB_PLAYLIST, "Current state is stopping gracefully, switching to idle\n");
			SetIdle();
			return 1;
		}

		m_sectionPosition++;
		if (m_sectionPosition >= m_currentSection->size())
		{
			if (m_currentSectionStr == "LeadIn")
			{
				LogDebug(VB_PLAYLIST, "At end of leadIn.\n");
				if (m_mainPlaylist.size())
				{
					LogDebug(VB_PLAYLIST, "Switching to mainPlaylist section.\n");
					m_currentSectionStr = "MainPlaylist";
					m_currentSection    = &m_mainPlaylist;
	 				m_sectionPosition   = 0;
					m_mainPlaylist[0]->StartPlaying();
				}
				else if (m_leadOut.size())
				{
					LogDebug(VB_PLAYLIST, "Switching to leadOut section.\n");
					m_currentSectionStr = "LeadOut";
					m_currentSection    = &m_leadOut;
					m_sectionPosition   = 0;
					m_leadOut[0]->StartPlaying();
				}
				else
				{
					LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
					SetIdle();
				}
			}
			else if (m_currentSectionStr == "MainPlaylist")
			{
				m_loop++;
				LogDebug(VB_PLAYLIST, "mainPlaylist loop now: %d\n", m_loop);
				if ((m_repeat) && (!m_loopCount || (m_loop < m_loopCount)))
				{
					if (FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)
					{
						LogDebug(VB_PLAYLIST, "Current state is stopping gracefully after loop, switching to idle\n");
						// FIXME PLAYLIST, do we want to run the leadout here??
						SetIdle();
						return 1;
					}

					LogDebug(VB_PLAYLIST, "mainPlaylist repeating for another loop, %d <= %d\n", m_loop, m_loopCount);
					m_sectionPosition = 0;
					m_mainPlaylist[0]->StartPlaying();
				}
				else if (m_leadOut.size())
				{
					LogDebug(VB_PLAYLIST, "Switching to leadOut\n");
					m_currentSectionStr = "LeadOut";
					m_currentSection    = &m_leadOut;
					m_sectionPosition   = 0;
					m_leadOut[0]->StartPlaying();
				}
				else
				{
					LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
					SetIdle();
				}
			}
			else
			{
				LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
				SetIdle();
			}
		}
		else
		{
			// Start the next item in the current section
			m_currentSection->at(m_sectionPosition)->StartPlaying();
		}
	}

//LogDebug(VB_PLAYLIST, "NPL: Checking if current item needs starting\n");
//	if (!m_currentSection->at(m_sectionPosition)->IsStarted())
//	{
//LogDebug(VB_PLAYLIST, "NPL: Starting current item\n");
//		m_currentSection->at(m_sectionPosition)->StartPlaying();
//	}

	return 1;
}

/*
 *
 */
void Playlist::SetIdle(void)
{
	FPPstatus = FPP_STATUS_IDLE;
	m_currentState = "idle";
	m_name = "";
}

/*
 *
 */
int Playlist::Cleanup(void)
{
	SetIdle();
	return 1;
}

/*
 *
 */
int Playlist::Play(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::Play()\n");

	// FIXME PLAYLIST, just loop through now for testing
	if (m_leadIn.size())
	{
		LogDebug(VB_PLAYLIST, "Playing Lead In:\n");
		for (int c = 0; c < m_leadIn.size(); c++)
		{
			m_leadIn[c]->Dump();
			if (m_leadIn[c]->StartPlaying())
			{
				while (m_leadIn[c]->IsPlaying())
					usleep(250000);
			}
		}
	}

	if (m_mainPlaylist.size())
	{
		LogDebug(VB_PLAYLIST, "  Main Playlist:\n");
		for (int c = 0; c < m_mainPlaylist.size(); c++)
		{
			m_mainPlaylist[c]->Dump();
			if (m_mainPlaylist[c]->StartPlaying())
			{
				while (m_mainPlaylist[c]->IsPlaying())
					usleep(250000);
			}
		}
	}

	if (m_leadOut.size())
	{
		LogDebug(VB_PLAYLIST, "  Lead Out:\n");
		for (int c = 0; c < m_leadOut.size(); c++)
		{
			m_leadOut[c]->Dump();
			if (m_leadOut[c]->StartPlaying())
			{
				while (m_leadOut[c]->IsPlaying())
					usleep(250000);
			}
		}
	}
}

/*
 *
 */
void Playlist::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Playlist: %s\n", m_name.c_str());

	LogDebug(VB_PLAYLIST, "  Repeat           : %d\n", m_repeat);
	LogDebug(VB_PLAYLIST, "  Loop Count       : %d\n", m_loopCount);
	LogDebug(VB_PLAYLIST, "  Current Section  : %s\n", m_currentSectionStr.c_str());
	LogDebug(VB_PLAYLIST, "  Section Position : %d\n", m_sectionPosition);

	if (m_leadIn.size())
	{
		LogDebug(VB_PLAYLIST, "  Lead In:\n");
		for (int c = 0; c < m_leadIn.size(); c++)
			m_leadIn[c]->Dump();
	}
	else
	{
		LogDebug(VB_PLAYLIST, "  Lead In          : (No Lead In)\n");
	}

	if (m_mainPlaylist.size())
	{
		LogDebug(VB_PLAYLIST, "  Main Playlist:\n");
		for (int c = 0; c < m_mainPlaylist.size(); c++)
			m_mainPlaylist[c]->Dump();
	}
	else
	{
		LogDebug(VB_PLAYLIST, "  Main Playlist    : (No Main Playlist)\n");
	}

	if (m_leadOut.size())
	{
		LogDebug(VB_PLAYLIST, "  Lead Out:\n");
		for (int c = 0; c < m_leadOut.size(); c++)
			m_leadOut[c]->Dump();
	}
	else
	{
		LogDebug(VB_PLAYLIST, "  Lead Out         : (No Lead Out)\n");
	}
}

/*
 *
 */
void Playlist::NextItem(void)
{
	if (m_currentState == "idle")
		return;

	// FIXME PLAYLIST
}

/*
 *
 */
void Playlist::PrevItem(void)
{
	if (m_currentState == "idle")
		return;

	// FIXME PLAYLIST
}

/*
 *
 */
int Playlist::GetPosition(void)
{
	int result = 0;

	if (m_currentState == "idle")
		return result;

	if (m_currentSectionStr == "LeadIn")
		return m_sectionPosition + 1;

	if (m_currentSectionStr == "MainPlaylist")
		return m_leadIn.size() + m_sectionPosition + 1;

	if (m_currentSectionStr == "LeadOut")
		return m_leadIn.size() + m_mainPlaylist.size() + m_sectionPosition + 1;

	return result;
}

/*
 *
 */
int Playlist::GetSize(void)
{
	if (m_currentState == "idle")
		return 0;

	return m_leadIn.size() + m_mainPlaylist.size() + m_leadOut.size();
}

/*
 *
 */
Json::Value Playlist::GetCurrentEntry(void)
{
	Json::Value result;

	if (m_currentState == "idle")
		return result;

	result = m_currentSection->at(m_sectionPosition)->GetConfig();

	return result;
}

/*
 *
 */
Json::Value Playlist::GetInfo(void)
{
	Json::Value result;

	result["currentState"] = m_currentState;

	if (m_currentState == "idle")
	{
		result["name"] = "";
		result["repeat"] = 0;
		result["loop"] = 0;
		result["loopCount"] = 0;
		result["random"] = 0;
		result["blankBetweenSequences"] = 0;
		result["blankBetweenIterations"] = 0;
		result["blankAtEnd"] = 0;
		result["currentSection"] = "";
		result["sectionPosition"] = 0;
		result["currentPosition"] = 0;
		result["size"] = 0;
		result["elapsed"] = 0;
		result["remaining"] = 0;
	}
	else
	{
		result["name"] = m_name;
		result["repeat"] = m_repeat;
		result["loop"] = m_loop;
		result["loopCount"] = m_loopCount;
		result["random"] = m_random;
		result["blankBetweenSequences"] = m_blankBetweenSequences;
		result["blankBetweenIterations"] = m_blankBetweenIterations;
		result["blankAtEnd"] = m_blankAtEnd;
		result["currentSection"] = m_currentSectionStr;
		result["sectionPosition"] = m_sectionPosition;
		result["currentPosition"] = GetPosition();
		result["size"] = GetSize();
		result["elapsed"] = (int)((GetTime() - m_startTime) / 1000000);

		// FIXME PLAYLIST
		result["remaining"] = 0;
	}

	result["currentEntry"] = GetCurrentEntry();

	return result;
}

/*
 *
 */
std::string Playlist::GetConfigStr(void)
{
	Json::FastWriter fastWriter;
	Json::Value result = GetConfig();

	return fastWriter.write(result);
}

/*
 *
 */
Json::Value Playlist::GetConfig(void)
{
	Json::Value result = GetInfo();

	if (m_leadIn.size())
	{
		Json::Value jsonArray(Json::arrayValue);
		for (int c = 0; c < m_leadIn.size(); c++)
			jsonArray.append(m_leadIn[c]->GetConfig());

		result["leadIn"] = jsonArray;
	}

	if (m_mainPlaylist.size())
	{
		Json::Value jsonArray(Json::arrayValue);
		for (int c = 0; c < m_mainPlaylist.size(); c++)
			jsonArray.append(m_mainPlaylist[c]->GetConfig());

		result["mainPlaylist"] = jsonArray;
	}

	if (m_leadOut.size())
	{
		Json::Value jsonArray(Json::arrayValue);
		for (int c = 0; c < m_leadOut.size(); c++)
			jsonArray.append(m_leadOut[c]->GetConfig());

		result["leadOut"] = jsonArray;
	}

	return result;
}

void Playlist::StopPlaylistGracefully(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL Playlist::StopPlaylistGracefully()\n");
}

void Playlist::StopPlaylistNow(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL Playlist::StopPlaylistNow()\n");
}

void Playlist::PlayListPlayingInit(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL Playlist::PlayListPlayingInit()\n");
}

void Playlist::PlayListPlayingProcess(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL Playlist::PlayListPlayingProcess()\n");
}

void Playlist::PlayListPlayingCleanup(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL Playlist::PlayListPlayingCleanup()\n");
}

void Playlist::PlaylistProcessMediaData(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL Playlist::PlaylistProcessMediaData()\n");
}

