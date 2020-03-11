/*
 *   Playlist Class for Falcon Player (FPP)
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

#include "string.h"
#include <unistd.h>
#include <sys/wait.h>

#include <fstream>
#include <sstream>
#include <string>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <jsoncpp/json/json.h>

#include "common.h"
#include "fpp.h"
#include "log.h"
#include "mqtt.h"
#include "Plugins.h"
#include "Playlist.h"
#include "settings.h"
#include "Sequence.h"

#include "PlaylistEntryBoth.h"
#include "PlaylistEntryBranch.h"
#include "PlaylistEntryChannelTest.h"
#include "PlaylistEntryCommand.h"
#include "PlaylistEntryDynamic.h"
#include "PlaylistEntryEffect.h"
#include "PlaylistEntryEvent.h"
#include "PlaylistEntryImage.h"
#include "PlaylistEntryMedia.h"
#include "PlaylistEntryMQTT.h"
#include "PlaylistEntryPause.h"
#include "PlaylistEntryPlaylist.h"
#include "PlaylistEntryPlugin.h"
#include "PlaylistEntryRemap.h"
#include "PlaylistEntryScript.h"
#include "PlaylistEntrySequence.h"
#include "PlaylistEntryURL.h"
#include "PlaylistEntryVolume.h"

static std::list<Playlist*> PL_CLEANUPS;
Playlist *playlist = NULL;

/*
 *
 */
Playlist::Playlist(Playlist *parent)
  : m_parent(parent),
	m_repeat(0),
	m_loop(0),
	m_loopCount(0),
	m_random(0),
	m_blankBetweenSequences(0),
	m_blankBetweenIterations(0),
	m_blankAtEnd(1),
	m_startTime(0),
	m_subPlaylistDepth(0),
	m_forceStop(0),
	m_fileTime(0),
	m_configTime(0),
	m_currentState("idle"),
    m_currentSection(nullptr),
	m_currentSectionStr("New"),
	m_sectionPosition(0),
	m_startPosition(0),
    m_status(FPP_STATUS_IDLE)
{
	SetIdle(false);

	SetRepeat(0);
    
    if (mqtt) {
        //Legacy callbacks
        std::function<void(const std::string &t, const std::string &payload)> f1 = [this] (const std::string &t, const std::string &payload) {
            std::string emptyStr;
            LogDebug(VB_CONTROL, "Received deprecated MQTT Topic: '%s' \n", t.c_str());
            std::string topic = t;
            topic.replace(0, 10, emptyStr); // Replace until /#
            this->MQTTHandler(topic, payload);
        };
        mqtt->AddCallback("/playlist/name/set", f1);
        mqtt->AddCallback("/playlist/repeat/set", f1);
        mqtt->AddCallback("/playlist/selectionPosition/set", f1);

        std::function<void(const std::string &t, const std::string &payload)> f2 = [this] (const std::string &t, const std::string &payload) {
            std::string emptyStr;
            std::string topic = t;
            topic.replace(0, 14, emptyStr); // Replace until /#
            this->MQTTHandler(topic, payload);
        };
        mqtt->AddCallback("/set/playlist/#", f2);
    }
}

/*
 *
 */
Playlist::~Playlist()
{
	Cleanup();
}

/*
 *
 */
int Playlist::LoadJSONIntoPlaylist(std::vector<PlaylistEntryBase*> &playlistPart, const Json::Value &entries)
{
	PlaylistEntryBase *plEntry = NULL;

	for (int c = 0; c < entries.size(); c++) {
		// Long-term handle sub-playlists on-demand instead of at load time
		if (entries[c]["type"].asString() == "playlist") {
			m_subPlaylistDepth++;
			if (m_subPlaylistDepth < 5) {
				std::string filename = getPlaylistDirectory();
				filename += "/";
				filename += entries[c]["name"].asString();
				filename += ".json";

				Json::Value subPlaylist = LoadJSON(filename.c_str());

				if (subPlaylist.isMember("leadIn"))
					LoadJSONIntoPlaylist(playlistPart, subPlaylist["leadIn"]);

				if (subPlaylist.isMember("mainPlaylist"))
					LoadJSONIntoPlaylist(playlistPart, subPlaylist["mainPlaylist"]);

				if (subPlaylist.isMember("leadOut"))
					LoadJSONIntoPlaylist(playlistPart, subPlaylist["leadOut"]);
            } else {
				LogErr(VB_PLAYLIST, "Error, recursive playlist.  Sub-playlist depth exceeded 5 trying to include '%s'\n", entries[c]["name"].asString().c_str());
			}

			m_subPlaylistDepth--;
        } else {
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

	Cleanup();

	if (config.isMember("name"))
		m_name = config["name"].asString();

	if (config.isMember("desc"))
		m_desc = config["desc"].asString();

	m_repeat = config["repeat"].asInt();
	m_loopCount = config["loopCount"].asInt();
	m_subPlaylistDepth = 0;

	m_playlistInfo = config["playlistInfo"];

	PlaylistEntryBase *plEntry = NULL;

	if (config.isMember("leadIn")) {
		LogDebug(VB_PLAYLIST, "Loading LeadIn:\n");
		const Json::Value leadIn = config["leadIn"];

		LoadJSONIntoPlaylist(m_leadIn, leadIn);
	}

	if (config.isMember("mainPlaylist")) {
		LogDebug(VB_PLAYLIST, "Loading MainPlaylist:\n");
		const Json::Value playlist = config["mainPlaylist"];

		LoadJSONIntoPlaylist(m_mainPlaylist, playlist);
	}

	if (config.isMember("leadOut")) {
		LogDebug(VB_PLAYLIST, "Loading LeadOut:\n");
		const Json::Value leadOut = config["leadOut"];

		LoadJSONIntoPlaylist(m_leadOut, leadOut);
	}

	m_sectionPosition = 0;
    m_currentSection = nullptr;

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

	if (!LoadJsonFromFile(filename, root)) {
		LogErr(VB_PLAYLIST, "Error loading %s\n", filename);
		return root;
	}

	if (m_filename == filename)
	{
		struct stat attr;
		stat(filename, &attr);

		LogDebug(VB_PLAYLIST, "Playlist Last Modified: %s\n", ctime(&attr.st_mtime));

		m_fileTime = attr.st_mtime;
	}

	return root;
}

/*
 *
 */
int Playlist::Load(const char *filename)
{
	LogDebug(VB_PLAYLIST, "Playlist::Load(%s)\n", filename);

    if (mqtt) {
		mqtt->Publish("playlist/name/status", filename);
    }

    Json::Value root;
    std::string tmpFilename = filename;

    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);

    if (endsWith(tmpFilename, ".fseq")) {
        m_filename = getSequenceDirectory();
        m_filename += "/";
        m_filename += tmpFilename;

        root["name"] = tmpFilename;
        root["repeat"] = 0;
        root["loopCount"] = 0;

        std::string mediaName;
        FSEQFile *src = FSEQFile::openFSEQFile(m_filename);
        if (src && !src->getVariableHeaders().empty()) {
            for (auto &head : src->getVariableHeaders()) {
                if ((head.code[0] == 'm') && (head.code[1] == 'f')) {
                    if (strchr((char *)&head.data[0], '/')) {
                        mediaName = (char *)(strrchr((char *)&head.data[0], '/') + 1);

                        std::string tmpMedia = getMusicDirectory();
                        tmpMedia += "/";
                        tmpMedia += mediaName;

                        if (!FileExists(tmpMedia)) {
                            tmpMedia = getVideoDirectory();
                            tmpMedia += "/";
                            tmpMedia += mediaName;

                            if (!FileExists(tmpMedia)) {
                                LogDebug(VB_PLAYLIST, "fseq %s lists a media file of %s, but it can not be found\n", tmpFilename.c_str(), mediaName.c_str());
                                mediaName = "";
                            }
                        }
                    }
                }
            }
        }

        Json::Value mp(Json::arrayValue);
        Json::Value pe;
        if (mediaName.empty()) {
            pe["type"]         = "sequence";
            LogDebug(VB_PLAYLIST, "Generated an on-the-fly playlist for %s\n", tmpFilename.c_str());
        } else {
            pe["type"]         = "both";
            pe["mediaName"]    = mediaName;
            LogDebug(VB_PLAYLIST, "Generated an on-the-fly playlist for %s/%s\n", tmpFilename.c_str(), mediaName.c_str());
        }

        pe["enabled"]      = 1;
        pe["playOnce"]     = 0;
        pe["sequenceName"] = tmpFilename;
        pe["videoOut"]     = "--Default--";

        mp.append(pe);
        root["mainPlaylist"] = mp;
    } else {
        m_filename = getPlaylistDirectory();
        m_filename += "/";
        m_filename += filename;
        m_filename += ".json";

	    root = LoadJSON(m_filename.c_str());
    }

	int res = Load(root);

	GetConfigStr();

	return res;
}

/*
 *
 */
PlaylistEntryBase* Playlist::LoadPlaylistEntry(Json::Value entry)
{
	PlaylistEntryBase *result = NULL;

	if (entry["type"].asString() == "both")
		result = new PlaylistEntryBoth();
	else if (entry["type"].asString() == "branch")
		result = new PlaylistEntryBranch();
	else if (entry["type"].asString() == "channelTest")
		result = new PlaylistEntryChannelTest();
	else if (entry["type"].asString() == "dynamic")
		result = new PlaylistEntryDynamic();
	else if (entry["type"].asString() == "effect")
		result = new PlaylistEntryEffect();
	else if (entry["type"].asString() == "event")
		result = new PlaylistEntryEvent();
	else if (entry["type"].asString() == "image")
		result = new PlaylistEntryImage();
	else if (entry["type"].asString() == "media")
		result = new PlaylistEntryMedia();
	else if (entry["type"].asString() == "mqtt")
		result = new PlaylistEntryMQTT();
	else if (entry["type"].asString() == "pause")
		result = new PlaylistEntryPause();
	else if (entry["type"].asString() == "playlist")
		result = new PlaylistEntryPlaylist();
	else if (entry["type"].asString() == "plugin")
		result = new PlaylistEntryPlugin();
	else if (entry["type"].asString() == "remap")
		result = new PlaylistEntryRemap();
	else if (entry["type"].asString() == "script")
		result = new PlaylistEntryScript();
	else if (entry["type"].asString() == "sequence")
		result = new PlaylistEntrySequence();
	else if (entry["type"].asString() == "url")
		result = new PlaylistEntryURL();
	else if (entry["type"].asString() == "volume")
		result = new PlaylistEntryVolume();
    else if (entry["type"].asString() == "command")
        result = new PlaylistEntryCommand();
    else {
		LogErr(VB_PLAYLIST, "Unknown Playlist Entry Type: %s\n", entry["type"].asString().c_str());
		return NULL;
	}

	if (result->Init(entry))
		return result;

	return NULL;
}

/*
 *
 */
int Playlist::ReloadPlaylist(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::ReloadPlaylist()\n");

	std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);
	std::string currentSectionStr = m_currentSectionStr;
	int repeat = m_repeat;
	int loopCount = m_loopCount;
	long long startTime = m_startTime;

	Json::Value root = LoadJSON(m_filename.c_str());

	if (!Load(root))
		return 0;

	m_repeat = repeat;
	m_loopCount = loopCount;
	m_startTime = startTime;
	m_sectionPosition = 0;
	m_currentSectionStr = "MainPlaylist";
	m_currentSection = &m_mainPlaylist;

	GetConfigStr();

	return 1;
}

/*
 *
 */
void Playlist::ReloadIfNeeded(void)
{
	if (FileHasBeenModified()) {
		LogDebug(VB_PLAYLIST, "Playlist .json file has been modified, reloading playlist\n");

		if (!ReloadPlaylist())
			LogErr(VB_PLAYLIST, "Error reloading playlist, continuing with existing copy.");
	}
}

/*
 *
 */
void Playlist::SwitchToMainPlaylist(void)
{
	LogDebug(VB_PLAYLIST, "Switching to MainPlaylist\n");

	m_currentSectionStr = "MainPlaylist";
	m_currentSection    = &m_mainPlaylist;
	m_sectionPosition   = 0;
	m_mainPlaylist[0]->StartPlaying();
}

/*
 *
 */
void Playlist::SwitchToLeadOut(void)
{
	LogDebug(VB_PLAYLIST, "Switching to LeadOut\n");

	m_currentSectionStr = "LeadOut";
	m_currentSection    = &m_leadOut;
	m_sectionPosition   = 0;
	m_leadOut[0]->StartPlaying();
}

/*
 *
 */
int Playlist::Start(void)
{
	LogDebug(VB_PLAYLIST, "Playlist::Start()\n");

	if ((!m_leadIn.size()) &&
		(!m_mainPlaylist.size()) &&
		(!m_leadOut.size())) {
		SetIdle();
		return 0;
	}

    m_status = FPP_STATUS_PLAYLIST_PLAYING;

    std::string origCurState = m_currentState;
	m_currentState = "playing";

	m_startTime = GetTime();
	m_loop = 0;
	m_forceStop = 0;

	LogDebug(VB_PLAYLIST, "============================================================================\n");

	if (m_startPosition > 0) {
		if (m_startPosition >= (m_leadIn.size() + m_mainPlaylist.size() + m_leadOut.size()))
			m_startPosition = 0;

		if (m_startPosition >= (m_leadIn.size() + m_mainPlaylist.size())) {
			m_sectionPosition = m_startPosition - (m_leadIn.size() + m_mainPlaylist.size());
			m_currentSectionStr = "LeadOut";
			m_currentSection = &m_leadOut;
		} else if (m_startPosition >= m_leadIn.size()) {
			m_sectionPosition = m_startPosition - m_leadIn.size();
			m_currentSectionStr = "MainPlaylist";
			m_currentSection = &m_mainPlaylist;
        } else {
			m_sectionPosition = m_startPosition;
			m_currentSectionStr = "LeadIn";
			m_currentSection = &m_leadIn;
		}
    } else {
		if (m_leadIn.size()) {
			m_currentSectionStr = "LeadIn";
			m_currentSection = &m_leadIn;
        } else if (m_mainPlaylist.size()) {
			m_currentSectionStr = "MainPlaylist";
			m_currentSection = &m_mainPlaylist;
		} else {// must be only lead Out
			m_currentSectionStr = "LeadOut";
			m_currentSection = &m_leadOut;
		}

		m_sectionPosition = 0;
	}
    
    if (origCurState == "playing") {
        PluginManager::INSTANCE.playlistCallback(GetInfo(), "playing", m_currentSectionStr, m_sectionPosition);
    } else {
        PluginManager::INSTANCE.playlistCallback(GetInfo(), "start", m_currentSectionStr, m_sectionPosition);
    }
    if (mqtt) {
        mqtt->Publish("status", m_currentState.c_str());
        mqtt->Publish("playlist/section/status", m_currentSectionStr);
        mqtt->Publish("playlist/sectionPosition/status", m_sectionPosition);
    }

	m_currentSection->at(m_sectionPosition)->StartPlaying();


	return 1;
}

/*
 *
 */
int Playlist::StopNow(int forceStop)
{
	LogDebug(VB_PLAYLIST, "Playlist::StopNow(%d)\n", forceStop);
    
    if (m_status == FPP_STATUS_IDLE) {
        return 1;
    }


    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);
    m_status = FPP_STATUS_STOPPING_NOW;

	if (m_currentSection && m_currentSection->at(m_sectionPosition)->IsPlaying())
		m_currentSection->at(m_sectionPosition)->Stop();

	SetIdle();

	m_forceStop = forceStop;
    m_currentSection = nullptr;

	return 1;
}

/*
 *
 */
int Playlist::StopGracefully(int forceStop, int afterCurrentLoop)
{
	LogDebug(VB_PLAYLIST, "Playlist::StopGracefully(%d, %d)\n", forceStop, afterCurrentLoop);

	if (afterCurrentLoop)
	{
		m_status = FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP;
		m_currentState = "stoppingAfterLoop";
	}
	else
	{
		m_status = FPP_STATUS_STOPPING_GRACEFULLY;
		m_currentState = "stoppingGracefully";
	}

	m_forceStop = forceStop;

	return 1;
}

/*
 *
 */
int Playlist::IsPlaying(void)
{
	if ((m_status == FPP_STATUS_PLAYLIST_PLAYING) ||
		(m_status == FPP_STATUS_STOPPING_GRACEFULLY) ||
		(m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP))
		return 1;

	return 0;
}

/*
 *
 */
int Playlist::FileHasBeenModified(void)
{
    if (endsWith(m_filename, ".fseq"))
        return 0;

	struct stat attr;
	stat(m_filename.c_str(), &attr);

	LogDebug(VB_PLAYLIST, "Playlist Last Modified: %s\n", ctime(&attr.st_mtime));

	if (attr.st_mtime > m_fileTime)
		return 1;

	return 0;
}

/*
 *
 */
int Playlist::Process(void)
{
	//LogExcess(VB_PLAYLIST, "Playlist::Process: %s, section %s, position: %d\n", m_name.c_str(), m_currentSectionStr.c_str(), m_sectionPosition);
    
    while (!PL_CLEANUPS.empty()) {
        Playlist *p = PL_CLEANUPS.front();
        delete p;
        PL_CLEANUPS.pop_front();
    }

    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);

	if (m_currentSection == nullptr || m_sectionPosition >= m_currentSection->size()) {
		LogErr(VB_PLAYLIST, "Section position %d is outside of section %s\n",
			m_sectionPosition, m_currentSectionStr.c_str());
		StopNow();
		return 0;
	}


    if (m_currentSection->at(m_sectionPosition)->IsPlaying()) {
		m_currentSection->at(m_sectionPosition)->Process();
    }

	if (m_currentSection->at(m_sectionPosition)->IsFinished()) {
		LogDebug(VB_PLAYLIST, "Playlist entry finished\n");
		if ((logLevel & LOG_DEBUG) && (logMask & VB_PLAYLIST))
			m_currentSection->at(m_sectionPosition)->Dump();

		LogDebug(VB_PLAYLIST, "============================================================================\n");

		if (m_status == FPP_STATUS_STOPPING_GRACEFULLY) {
			if ((m_currentSectionStr == "LeadIn") ||
				(m_currentSectionStr == "MainPlaylist")) {
				ReloadIfNeeded();

				if (m_leadOut.size()) {
					LogDebug(VB_PLAYLIST, "Stopping Gracefully\n");
					SwitchToLeadOut();
				} else {
                    m_currentSection = nullptr;
					SetIdle();
				}
				return 1;
			}
		}
        if (m_insertedPlaylist != ""
            && SwitchToInsertedPlaylist()) {
            playlist->Start();
            return playlist->Process();
        }

        
        auto currentEntry = m_currentSection->at(m_sectionPosition);
        if (currentEntry->GetNextBranchType() == PlaylistEntryBase::PlaylistBranchType::Index) {
        
            if (currentEntry->GetNextSection() != "") {
                LogDebug(VB_PLAYLIST, "Attempting Switch to %s section.\n",
                    currentEntry->GetNextSection().c_str());

                if (currentEntry->GetNextSection() == "leadIn") {
                    m_currentSectionStr = "LeadIn";
                    m_currentSection = &m_leadIn;
                } else if (currentEntry->GetNextSection() == "leadOut") {
                    m_currentSectionStr = "LeadOut";
                    m_currentSection = &m_leadOut;
                } else {
                    m_currentSectionStr = "MainPlaylist";
                    m_currentSection = &m_mainPlaylist;
                }

                if (currentEntry->GetNextItem() == -1) {
                    m_sectionPosition = 0;
                } else if (currentEntry->GetNextItem() < m_currentSection->size()) {
                    m_sectionPosition = currentEntry->GetNextItem();
                } else {
                    m_sectionPosition = 0;
                }
            } else if (currentEntry->GetNextItem() != -1) {
                if (currentEntry->GetNextItem() < m_currentSection->size()) {
                    m_sectionPosition = currentEntry->GetNextItem();
                } else {
                    m_sectionPosition = m_currentSection->size();
                }
            } else {
                m_sectionPosition++;
            }
        } else if (currentEntry->GetNextBranchType() == PlaylistEntryBase::PlaylistBranchType::Offset) {
            int nextPosition = m_sectionPosition + currentEntry->GetNextItem();
            LogDebug(VB_PLAYLIST, "Attempting Offset Branch to section position %d\n", nextPosition);
            if (nextPosition < 0) {
                LogDebug(VB_PLAYLIST, "New position negative, branching to first position in section\n");
                m_sectionPosition = 0;
            } else if (nextPosition > m_currentSection->size()) {
                m_sectionPosition = m_currentSection->size();
                LogDebug(VB_PLAYLIST, "Offset outside current section, ending section\n");
            } else {
                m_sectionPosition = nextPosition;
            }
        } else {
            m_sectionPosition++;
        }

		if (m_sectionPosition >= m_currentSection->size()) {
			if (m_currentSectionStr == "LeadIn") {
				LogDebug(VB_PLAYLIST, "At end of leadIn.\n");

				ReloadIfNeeded();

				if (m_mainPlaylist.size()) {
					SwitchToMainPlaylist();
				} else if (m_leadOut.size()) {
					SwitchToLeadOut();
                } else {
					LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
					SetIdle();
				}
			} else if (m_currentSectionStr == "MainPlaylist") {
				m_loop++;
				LogDebug(VB_PLAYLIST, "mainPlaylist loop now: %d\n", m_loop);
				if ((m_repeat) && (!m_loopCount || (m_loop < m_loopCount))) {
					ReloadIfNeeded();

					if (m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP) {
						if (m_leadOut.size()) {
							LogDebug(VB_PLAYLIST, "Stopping Gracefully after loop\n");
							SwitchToLeadOut();
                        } else {
							LogDebug(VB_PLAYLIST, "Stopping Gracefully after loop. Empty leadOut, setting to Idle state\n");
							SetIdle();
						}

						return 1;
					}

					if (!m_loopCount)
						LogDebug(VB_PLAYLIST, "mainPlaylist repeating for another loop, loopCount == 0\n");
					else
						LogDebug(VB_PLAYLIST, "mainPlaylist repeating for another loop, %d <= %d\n", m_loop, m_loopCount);

					m_sectionPosition = 0;
					m_mainPlaylist[0]->StartPlaying();
				} else if (m_leadOut.size()) {
					ReloadIfNeeded();

					SwitchToLeadOut();
                } else {
					LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
					SetIdle();
				}
            } else {
				LogDebug(VB_PLAYLIST, "No more playlist entries, switching to idle.\n");
                m_currentSection = nullptr;
				SetIdle();
			}
        } else {
			// Start the next item in the current section
			m_currentSection->at(m_sectionPosition)->StartPlaying();
		}

        PluginManager::INSTANCE.playlistCallback(GetInfo(), "playing", m_currentSectionStr, m_sectionPosition);
		if (mqtt) {
		 	mqtt->Publish("playlist/section/status", m_currentSectionStr);
			mqtt->Publish("playlist/sectionPosition/status", m_sectionPosition);
		}
	}

	return 1;
}

bool Playlist::SwitchToInsertedPlaylist() {
    if (m_insertedPlaylist != "") {
        Playlist *pl = new Playlist(this);
        pl->Play(m_insertedPlaylist.c_str(), m_insertedPlaylistPosition, 0, m_scheduled);
        m_insertedPlaylist = "";
        if (pl->IsPlaying()) {
            LogDebug(VB_PLAYLIST, "Switching to inserted playlist %s\n", m_insertedPlaylist.c_str());
            playlist = pl;
            return true;
        } else {
            delete pl;
        }
    }
    return false;
}

/*
 *
 */
void Playlist::ProcessMedia(void)
{
	// FIXME, find a better way to do this
	sigset_t blockset;

	sigemptyset(&blockset);
	sigaddset(&blockset, SIGCHLD);
	sigprocmask(SIG_BLOCK, &blockset, NULL);

	pthread_mutex_lock(&mediaOutputLock);
	if (mediaOutput)
		mediaOutput->Process();
	pthread_mutex_unlock(&mediaOutputLock);

	sigprocmask(SIG_UNBLOCK, &blockset, NULL);
}

/*
 *
 */
void Playlist::SetIdle(bool exit)
{
	m_status = FPP_STATUS_IDLE;
    
	m_currentState = "idle";
	m_name = "";
	m_desc = "";
	m_startPosition = 0;
	m_sectionPosition = 0;
	m_repeat = 0;

	// Remoted per issue #506
	//Cleanup();

    PluginManager::INSTANCE.playlistCallback(GetInfo(), "stop", m_currentSectionStr, m_sectionPosition);
	if (mqtt) {
		mqtt->Publish("status", "idle");
		mqtt->Publish("playlist/name/status", "");
		mqtt->Publish("playlist/section/status", "");
		mqtt->Publish("playlist/sectionPosition/status", 0);
		mqtt->Publish("playlist/repeat/status", 0);
	}
    if (m_parent && exit) {
        playlist = m_parent;
        PL_CLEANUPS.push_back(this);
    } else if (exit) {
        sequence->SendBlankingData();
    }
}

/*
 *
 */
int Playlist::Cleanup(void)
{
	while (m_leadIn.size()) {
		PlaylistEntryBase *entry = m_leadIn.back();
		m_leadIn.pop_back();
		delete entry;
	}

	while (m_mainPlaylist.size()) {
		PlaylistEntryBase *entry = m_mainPlaylist.back();
		m_mainPlaylist.pop_back();
		delete entry;
	}

	while (m_leadOut.size()) {
		PlaylistEntryBase *entry = m_leadOut.back();
		m_leadOut.pop_back();
		delete entry;
	}

	m_name = "";
	m_desc = "";
	m_startPosition = 0;
	m_sectionPosition = 0;
	m_repeat = 0;
	m_loopCount = 0;
	m_startTime = 0;
	m_currentSectionStr = "New";
    m_currentSection = nullptr;

	return 1;
}


void Playlist::InsertPlaylistAsNext(const std::string &filename, const int position) {
    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);
    if (m_status == FPP_STATUS_IDLE) {
        Play(filename.c_str(), position, 0, m_scheduled);
    } else {
        m_insertedPlaylist = filename;
        m_insertedPlaylistPosition = position;
    }
}


/*
 *
 */
int Playlist::Play(const char *filename, const int position, const int repeat, const int scheduled)
{
	int hadToStop = 0;

	if (!strlen(filename))
		return 0;

	LogDebug(VB_PLAYLIST, "Playlist::Play('%s', %d, %d, %d)\n",
		filename, position, repeat, scheduled);

    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);

	if ((m_status == FPP_STATUS_PLAYLIST_PLAYING) ||
		(m_status == FPP_STATUS_STOPPING_GRACEFULLY) ||
		(m_status == FPP_STATUS_STOPPING_GRACEFULLY_AFTER_LOOP)) {
        
        std::string fullfilename = getPlaylistDirectory();
        fullfilename += "/";
        fullfilename += filename;
        fullfilename += ".json";
        
        if ((m_filename == fullfilename)
            && (repeat == m_repeat)
            && m_currentSection
            && position >= 0) {
            //the requested playlist is already running and loaded, we can jump right to the index
            if (m_currentSection->at(m_sectionPosition)->IsPlaying()) {
                m_currentSection->at(m_sectionPosition)->Stop();
            }

            m_sectionPosition = 0;
            SetPosition(position);
            m_status = FPP_STATUS_PLAYLIST_PLAYING;
            m_currentState = "playing";
            Start();
            return 1;
        } else if (m_currentSection) {
            hadToStop = 1;
            StopNow(1);
            sleep(1);
        }
	}
    m_scheduled = scheduled;
	m_forceStop = 0;

	Load(filename);

    int p = position;
    if (p == -2) {
        //random
        int l = m_mainPlaylist.size();
        if (l > 2) {
            p = rand() % l;
            p = p + m_leadIn.size();
        }
    }
	if (p >= 0)
		SetPosition(p);

	if (repeat >= 0)
		SetRepeat(repeat);

	m_status = FPP_STATUS_PLAYLIST_PLAYING;
	m_currentState = "playing";

    if (hadToStop) {
		Start();
    }

	return 1;
}


/*
 *
 */
void Playlist::SetPosition(int position)
{
	m_startPosition = position;

    if (mqtt) {
		mqtt->Publish("playlist/position/status", position);
    }
}


/*
 *
 */
void Playlist::SetRepeat(int repeat)
{
	m_repeat = repeat;

    if (mqtt) {
		mqtt->Publish("playlist/repeat/status", repeat);
    }
}


/*
 *
 */
void Playlist::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Playlist: %s\n", m_name.c_str());

	LogDebug(VB_PLAYLIST, "  Description      : %s\n", m_desc.c_str());
	LogDebug(VB_PLAYLIST, "  Repeat           : %d\n", m_repeat);
	LogDebug(VB_PLAYLIST, "  Loop Count       : %d\n", m_loopCount);
	LogDebug(VB_PLAYLIST, "  Current Section  : %s\n", m_currentSectionStr.c_str());
	LogDebug(VB_PLAYLIST, "  Section Position : %d\n", m_sectionPosition);

	if (m_leadIn.size()) {
		LogDebug(VB_PLAYLIST, "  Lead In:\n");
		for (int c = 0; c < m_leadIn.size(); c++)
			m_leadIn[c]->Dump();
    } else {
		LogDebug(VB_PLAYLIST, "  Lead In          : (No Lead In)\n");
	}

	if (m_mainPlaylist.size()) {
		LogDebug(VB_PLAYLIST, "  Main Playlist:\n");
		for (int c = 0; c < m_mainPlaylist.size(); c++)
			m_mainPlaylist[c]->Dump();
    } else {
		LogDebug(VB_PLAYLIST, "  Main Playlist    : (No Main Playlist)\n");
	}

	if (m_leadOut.size()) {
		LogDebug(VB_PLAYLIST, "  Lead Out:\n");
		for (int c = 0; c < m_leadOut.size(); c++)
			m_leadOut[c]->Dump();
    } else {
		LogDebug(VB_PLAYLIST, "  Lead Out         : (No Lead Out)\n");
	}
}

void Playlist::RestartItem(void)
{
    LogDebug(VB_PLAYLIST, "RestartItem called for '%s'\n", m_name.c_str());
    if (m_currentState == "idle") {
        return;
    }
    if (m_status != FPP_STATUS_PLAYLIST_PLAYING) {
        return;
    }
    
    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);
    int pos = GetPosition() - 1;
    if (m_currentSection->at(m_sectionPosition)->IsPlaying())
        m_currentSection->at(m_sectionPosition)->Stop();

    m_sectionPosition = 0;
    m_startPosition = pos;
    Start();
}
void Playlist::NextItem(void)
{
    LogDebug(VB_PLAYLIST, "NextItem called for '%s'\n", m_name.c_str());
    if (m_currentState == "idle") {
		return;
    }
    if (m_status != FPP_STATUS_PLAYLIST_PLAYING) {
        return;
    }
    
    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);
    int pos = GetPosition() - 1;
    if (m_currentSectionStr == "MainPlaylist") {
        if (pos < (m_leadIn.size() + m_mainPlaylist.size())) {
            pos++;
        } else {
            pos = m_leadIn.size();
        }
    }
    
    if (m_currentSection->at(m_sectionPosition)->IsPlaying())
        m_currentSection->at(m_sectionPosition)->Stop();

    m_sectionPosition = 0;
    m_startPosition = pos;
    Start();
}

/*
 *
 */
void Playlist::PrevItem(void)
{
    LogDebug(VB_PLAYLIST, "PrevItem called for '%s'\n", m_name.c_str());
    if (m_currentState == "idle") {
        return;
    }
    if (m_status != FPP_STATUS_PLAYLIST_PLAYING) {
        return;
    }

    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);
    int pos = GetPosition() - 1;
    if (m_currentSectionStr == "MainPlaylist") {
        if (pos > m_leadIn.size()) {
            pos--;
        } else {
            pos = m_leadIn.size() + m_mainPlaylist.size() - 1;
        }
    }

    if (m_currentSection->at(m_sectionPosition)->IsPlaying())
    m_currentSection->at(m_sectionPosition)->Stop();
    
    m_sectionPosition = 0;
    m_startPosition = pos;
    Start();
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

	if (m_currentState == "idle" || m_currentSection == nullptr)
		return result;

	result = m_currentSection->at(m_sectionPosition)->GetConfig();

	return result;
}

Json::Value Playlist::GetMqttStatusJSON(void){
    // this is called on background thread, need to lock
    std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);

	Json::Value result;
	result["status"] = m_currentState; // Works because single playlist
	Json::Value playlistArray =  Json::Value(Json::arrayValue);

	if (m_currentState != "idle") {
		Json::Value entryArray =  Json::Value(Json::arrayValue);
		Json::Value playlist;
		// Only one entry right now.
		Json::Value playlistEntry = m_currentSection->at(m_sectionPosition)->GetMqttStatus();
		entryArray.append(playlistEntry);

		playlist["name"] = m_name;
		playlist["repeat"] = m_repeat;
		playlist["currentItems"] = entryArray;
		playlistArray.append(playlist);
	}

	result["activePlaylists"] = playlistArray;
	return result;
}

/*
 *
 */
Json::Value Playlist::GetInfo(void)
{
	Json::Value result;

	result["currentState"] = m_currentState;

	if (m_currentState == "idle") {
		result["name"] = "";
		result["repeat"] = 0;
		result["loop"] = 0;
		result["loopCount"] = 0;
		result["random"] = 0;
		result["blankBetweenSequences"] = 0;
		result["blankBetweenIterations"] = 0;
		result["blankAtEnd"] = 0;
		result["size"] = 0;
    } else {
		result["name"] = m_name;
		result["desc"] = m_desc;
		result["repeat"] = m_repeat;
		result["loop"] = m_loop;
		result["loopCount"] = m_loopCount;
		result["random"] = m_random;
		result["blankBetweenSequences"] = m_blankBetweenSequences;
		result["blankBetweenIterations"] = m_blankBetweenIterations;
		result["blankAtEnd"] = m_blankAtEnd;
		result["size"] = GetSize();
	}

	result["currentEntry"] = GetCurrentEntry();

	return result;
}

/*
 *
 */
std::string Playlist::GetConfigStr(void)
{
	std::unique_lock<std::recursive_mutex> lck (m_playlistMutex);

	return SaveJsonToString(GetConfig());
}

/*
 *
 */
Json::Value Playlist::GetConfig(void)
{
	Json::Value result = GetInfo();

// FIXME, need to test the performance of not having this on the Pi/BBB
//	if (m_configTime > m_fileTime)
//		return m_config;

	if (m_leadIn.size()) {
		Json::Value jsonArray(Json::arrayValue);
		for (int c = 0; c < m_leadIn.size(); c++)
			jsonArray.append(m_leadIn[c]->GetConfig());

		result["leadIn"] = jsonArray;
	}

	if (m_mainPlaylist.size()) {
		Json::Value jsonArray(Json::arrayValue);
		for (int c = 0; c < m_mainPlaylist.size(); c++)
			jsonArray.append(m_mainPlaylist[c]->GetConfig());

		result["mainPlaylist"] = jsonArray;
	}

	if (m_leadOut.size()) {
		Json::Value jsonArray(Json::arrayValue);
		for (int c = 0; c < m_leadOut.size(); c++)
			jsonArray.append(m_leadOut[c]->GetConfig());

		result["leadOut"] = jsonArray;
	}

	m_configTime = time(NULL);
	result["configTime"] = (Json::UInt64)m_configTime;

	result["playlistInfo"] = m_playlistInfo;
	m_config = result;

	return result;
}


/*
 *
 */
int Playlist::MQTTHandler(std::string topic, std::string msg)
{
	LogDebug(VB_PLAYLIST, "Playlist::MQTTHandler('%s', '%s') while playing '%s'\n",
		topic.c_str(), msg.c_str(), m_name.c_str());

	// note the leading /set/playlist will be removed from topic by now

	int pos = topic.find("/");
	if (pos == std::string::npos) {
		LogWarn(VB_PLAYLIST, "Ignoring Invalid playlist topic: playlist/%s\n", 
			topic.c_str());
		return 0;
	}
	std::string newPlaylistName = topic.substr(0,pos);
	std::string topicEnd = topic.substr(pos);

	/*
	 * NOTE: This because multiple playlist are not supported, the newPlaylistname value
	 * is only considered when starting a playlist.  All other actions will
	 * apply to the current running playlist even if the names don't match
	 */


	// ALLPLAYLIST should be checked first to avoid name colision.
	if (topic == "ALLPLAYLISTS/stop/now") {
		StopNow(1);

	} else if (topic == "ALLPLAYLISTS/stop/graceful") {
		StopGracefully(1);

	} else if (topic == "ALLPLAYLISTS/stop/afterloop") {
		StopGracefully(1,1);

	// Playlist specific versions
	} else if (topicEnd == "/start") {
		// Play from begging keeping previous value of repeate
        int pos = 0;
        if (!msg.empty()) {
            pos = std::atoi(msg.c_str());
        }
		Play(newPlaylistName.c_str(), pos, m_repeat);
	} else if (topicEnd == "/next") {
		NextItem();

	} else if (topicEnd == "/prev") {
		PrevItem();

	} else if (topicEnd == "/repeat") {
		SetRepeat(atoi(msg.c_str()));

	} else if (topicEnd == "/startPosition") {
		SetPosition(atoi(msg.c_str()));

	} else if (topicEnd == "/stop/now") {
		StopNow(1);

	} else if (topicEnd == "/stop/graceful") {
		StopGracefully(1);

	} else if (topicEnd == "/stop/afterloop") {
		StopGracefully(1);

	// These three are depgrecated and should be removed
	} else if (topic == "name/set") {
	        LogInfo(VB_PLAYLIST, "playlist/%s is deprecated and will be removed in a future release\n",
		topic.c_str());
		Play(msg.c_str(), m_sectionPosition, m_repeat);

	} else if (topic == "repeat/set") {
	        LogInfo(VB_PLAYLIST, "playlist/%s is deprecated and will be removed in a future release\n",
		topic.c_str());
		SetRepeat(atoi(msg.c_str()));
		
	} else if (topic == "sectionPosition/set") {
	        LogInfo(VB_PLAYLIST, "playlist/%s is deprecated and will be removed in a future release\n",
		topic.c_str());
		SetPosition(atoi(msg.c_str()));

	} else {
		LogWarn(VB_PLAYLIST, "Ignoring Invalid playlist topic: playlist/%s\n", topic.c_str());
		return 0;	
	}

	return 1;
}


/*
 *
 */
std::string Playlist::ReplaceMatches(std::string in)
{
	std::string out = in;

	LogDebug(VB_PLAYLIST, "In: '%s'\n", in.c_str());

	// FIXME, Playlist

	LogDebug(VB_PLAYLIST, "Out: '%s'\n", out.c_str());

	return out;
}

