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
#include "NewPlaylist.h"
#include "settings.h"

#include "PlaylistEntryBoth.h"
#include "PlaylistEntryBranch.h"
#include "PlaylistEntryEffect.h"
#include "PlaylistEntryEvent.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryPixelOverlay.h"
#include "PlaylistEntryPlaylist.h"
#include "PlaylistEntryPlugin.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryVolume.h"

NewPlaylist *newPlaylist = NULL;

/*
 *
 */
NewPlaylist::NewPlaylist()
  : m_repeat(0),
	m_loop(0),
	m_loopCount(0),
	m_random(0),
	m_blankBetweenSequences(0),
	m_blankBetweenIterations(0),
	m_blankAtEnd(1),
	m_startTime(0),
	m_currentState("New"),
	m_currentSectionStr("New"),
	m_sectionPosition(0)
{
}

/*
 *
 */
NewPlaylist::~NewPlaylist()
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
int NewPlaylist::Load(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "Playlist::Load(JSON)\n");

	m_name = config["name"].asString();
	m_repeat = config["repeat"].asInt();
	m_loopCount = config["loopCount"].asInt();

	PlaylistEntryBase *plEntry = NULL;

	if (config.isMember("leadIn"))
	{
		const Json::Value leadIn = config["leadIn"];
		for (int c = 0; c < leadIn.size(); c++)
		{
			LogDebug(VB_PLAYLIST, "LeadIn: %s\n", leadIn[c]["type"].asString().c_str());
			plEntry = LoadPlaylistEntry(leadIn[c]);
			if (plEntry)
				m_leadIn.push_back(plEntry);
		}
	}

	if (config.isMember("mainPlaylist"))
	{
		const Json::Value playlist = config["mainPlaylist"];
		for (int c = 0; c < playlist.size(); c++)
		{
			LogDebug(VB_PLAYLIST, "MainPlaylist: %s\n", playlist[c]["type"].asString().c_str());
			plEntry = LoadPlaylistEntry(playlist[c]);
			if (plEntry)
				m_mainPlaylist.push_back(plEntry);
		}
	}

	if (config.isMember("leadOut"))
	{
		const Json::Value leadOut = config["leadOut"];
		for (int c = 0; c < leadOut.size(); c++)
		{
			LogDebug(VB_PLAYLIST, "LeadOut: %s\n", leadOut[c]["type"].asString().c_str());
			plEntry = LoadPlaylistEntry(leadOut[c]);
			if (plEntry)
				m_leadOut.push_back(plEntry);
		}
	}

	m_sectionPosition = 0;

// FIXME PLAYLIST
Dump();

	return 1;
}

/*
 *
 */
int NewPlaylist::Load(const char *filename)
{
	LogDebug(VB_PLAYLIST, "Playlist::Load(%s)\n", filename);

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
		return 0;
	}

	std::ifstream t(fullFilename);
	std::stringstream buffer;

	buffer << t.rdbuf();

	bool success = reader.parse(buffer.str(), root);
	if (!success)
	{
		LogErr(VB_PLAYLIST, "Error parsing %s\n", fullFilename);
		return 0;
	}

	return Load(root);
}

/*
 *
 */
PlaylistEntryBase* NewPlaylist::LoadPlaylistEntry(Json::Value entry)
{
	PlaylistEntryBase *result = NULL;

	if (entry["type"].asString() == "both")
		result = new PlaylistEntryBoth();
	else if (entry["type"].asString() == "branch")
		result = new PlaylistEntryBranch();
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
	else if (entry["type"].asString() == "playlist")
		result = new PlaylistEntryPlaylist();
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
int NewPlaylist::Start(void)
{
	if ((!m_leadIn.size()) &&
		(!m_mainPlaylist.size()) &&
		(!m_leadOut.size()))
	{
		LogErr(VB_PLAYLIST, "Tried to play an empty playlist\n");
		FPPstatus = FPP_STATUS_IDLE;
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

	// FIXME, get rid of this
	FPPstatus = FPP_STATUS_PLAYLIST_PLAYING;

	m_currentState = "Playing";

	if (m_sectionPosition >= m_currentSection->size())
		m_sectionPosition = 0;

	m_startTime = GetTime();
	m_loop = 0;
LogDebug(VB_PLAYLIST, "there\n");
	m_currentSection->at(m_sectionPosition)->StartPlaying();
LogDebug(VB_PLAYLIST, "there\n");

	return 1;
}

/*
 *
 */
int NewPlaylist::StopNow(void)
{
	FPPstatus = FPP_STATUS_STOPPING_NOW;

	return 1;
}

/*
 *
 */
int NewPlaylist::StopGracefully(int afterCurrentLoop)
{
	// FIXME, get rid of this
	FPPstatus = FPP_STATUS_STOPPING_GRACEFULLY;

	if (afterCurrentLoop)
		m_currentState = "StoppingAfterLoop";
	else
		m_currentState = "StoppingGracefully";

	return 1;
}

/*
 *
 */
int NewPlaylist::Process(void)
{
LogDebug(VB_PLAYLIST, "NewPlaylist::Process\n");

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

		FPPstatus = FPP_STATUS_IDLE;
		m_currentState = "Idle";

		return 1;
	}

	if (m_currentSection->at(m_sectionPosition)->IsPlaying())
		m_currentSection->at(m_sectionPosition)->Process();

	if (m_currentSection->at(m_sectionPosition)->IsFinished())
	{
		if (FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY)
		{
			FPPstatus = FPP_STATUS_IDLE;
			m_currentState = "Idle";
			return 1;
		}

		m_sectionPosition++;
LogDebug(VB_PLAYLIST, "NPL: Playlist Entry Finished, new pos: %d\n", m_sectionPosition);
		if (m_sectionPosition >= m_currentSection->size())
		{
LogDebug(VB_PLAYLIST, "NPL: At end of section '%s'\n", m_currentSectionStr.c_str());
			if (m_currentSectionStr == "LeadIn")
			{
				if (m_mainPlaylist.size())
				{
LogDebug(VB_PLAYLIST, "NPL: Switching to MainPlaylist\n");
					m_currentSectionStr = "MainPlaylist";
					m_currentSection    = &m_mainPlaylist;
	 				m_sectionPosition   = 0;
					m_mainPlaylist[0]->StartPlaying();
				}
				else if (m_leadOut.size())
				{
LogDebug(VB_PLAYLIST, "NPL: Switching to LeadOut\n");
					m_currentSectionStr = "LeadOut";
					m_currentSection    = &m_leadOut;
					m_sectionPosition   = 0;
					m_leadOut[0]->StartPlaying();
				}
				else
				{
LogDebug(VB_PLAYLIST, "NPL: Nothing else to play, going idle\n");
					// Done with playlist, handle cleanup
					// FIXME
					FPPstatus = FPP_STATUS_IDLE;
					m_currentState = "Idle";
				}
			}
			else if (m_currentSectionStr == "MainPlaylist")
			{
				m_loop++;
LogDebug(VB_PLAYLIST, "NPL: Main Playlist loop now: %d\n", m_loop);
				if ((m_repeat) && (!m_loopCount || (m_loop < m_loopCount)))
				{
					if (FPPstatus == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)
					{
						FPPstatus = FPP_STATUS_IDLE;
						m_currentState = "Idle";
						return 1;
					}

LogDebug(VB_PLAYLIST, "NPL: Main Playlist repeating for another loop, %d <= %d\n", m_loop, m_loopCount);
					m_sectionPosition = 0;
					m_mainPlaylist[0]->StartPlaying();
				}
				else if (m_leadOut.size())
				{
LogDebug(VB_PLAYLIST, "NPL: Switching to LeadOut\n");
					m_currentSectionStr = "LeadOut";
					m_currentSection    = &m_leadOut;
					m_sectionPosition   = 0;
					m_leadOut[0]->StartPlaying();
				}
				else
				{
LogDebug(VB_PLAYLIST, "NPL: Nothing else to play, going idle\n");
					// Done with playlist, handle cleanup
					// FIXME
					FPPstatus = FPP_STATUS_IDLE;
					m_currentState = "Idle";
				}
			}
			else
			{
LogDebug(VB_PLAYLIST, "NPL: Nothing else to play, going idle\n");
				// Done with playlist, handle cleanup
				// FIXME
				FPPstatus = FPP_STATUS_IDLE;
				m_currentState = "Idle";
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
int NewPlaylist::Cleanup(void)
{
	// FIXME, get rid of this
	FPPstatus = FPP_STATUS_IDLE;

	m_currentState = "Idle";

	return 1;
}

/*
 *
 */
int NewPlaylist::Play(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::Play()\n");

	// FIXME, just loop through now for testing
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
void NewPlaylist::Dump(void)
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
std::string NewPlaylist::GetConfigStr(void)
{
	Json::FastWriter fastWriter;
	Json::Value result = GetConfig();

	return fastWriter.write(result);
}

/*
 *
 */
Json::Value NewPlaylist::GetConfig(void)
{
	Json::Value result;

	result["name"] = m_name;
	result["repeat"] = m_repeat;
	result["loop"] = m_loop;
	result["loopCount"] = m_loopCount;
	result["random"] = m_random;
	result["blankBetweenSequences"] = m_blankBetweenSequences;
	result["blankBetweenIterations"] = m_blankBetweenIterations;
	result["blankAtEnd"] = m_blankAtEnd;
	result["currentState"] = m_currentState;
	result["currentSection"] = m_currentSectionStr;
	result["sectionPosition"] = m_sectionPosition;
	result["elapsed"] = (int)((GetTime() - m_startTime) / 1000000);

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

void NewPlaylist::StopPlaylistGracefully(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL NewPlaylist::StopPlaylistGracefully()\n");
}

void NewPlaylist::StopPlaylistNow(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL NewPlaylist::StopPlaylistNow()\n");
}

void NewPlaylist::PlayListPlayingInit(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL NewPlaylist::PlayListPlayingInit()\n");
}

void NewPlaylist::PlayListPlayingProcess(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL NewPlaylist::PlayListPlayingProcess()\n");
}

void NewPlaylist::PlayListPlayingCleanup(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL NewPlaylist::PlayListPlayingCleanup()\n");
}

void NewPlaylist::PlaylistProcessMediaData(void)
{
	LogDebug(VB_PLAYLIST, "OLD CALL NewPlaylist::PlaylistProcessMediaData()\n");
}

