/*
 *   Playlist Entry Playlist Class for Falcon Player (FPP)
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

#include "common.h"
#include "log.h"
//#include "Player.h"
#include "PlaylistEntryPlaylist.h"

PlaylistEntryPlaylist::PlaylistEntryPlaylist(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_playlist(NULL)
{
	m_type = "playlist";
}

PlaylistEntryPlaylist::~PlaylistEntryPlaylist()
{
	if (m_playlist)
	{
		m_playlist->Cleanup();
		delete m_playlist;
	}
}

/*
 *
 */
int PlaylistEntryPlaylist::Init(Json::Value &config)
{
	m_playlist = new Playlist(NULL);

	if (!m_playlist)
	{
		LogErr(VB_PLAYLIST, "Error creating sub playlist\n");
		return 0;
	}

	if ((config.isMember("name")) &&
		(!config["name"].asString().empty()))
	{
		m_playlistName = config["name"].asString();

		if (m_playlistName.empty())
		{
			LogErr(VB_PLAYLIST, "Empty sub playlist name\n");
			return 0;
		}
	}
	else if (config.isMember("playlist"))
	{
		m_playlist->Load(config["playlist"]);
	}

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryPlaylist::StartPlaying(void)
{
	if (!m_playlist)
		return 0;

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	if (!m_playlistName.empty())
	{
		if (!m_playlist->Load(m_playlistName.c_str()))
			return 0;
	}

	if (!m_playlist->Start())
		return 0;

	return PlaylistEntryBase::StartPlaying();
}

/*
 *
 */
int PlaylistEntryPlaylist::Process(void)
{
	if (!m_playlist->Process())
		FinishPlay();

	return PlaylistEntryBase::Process();
}

/*
 *
 */
int PlaylistEntryPlaylist::Stop(void)
{
	m_playlist->StopNow();

	return PlaylistEntryBase::Stop();
}

/*
 *
 */
void PlaylistEntryPlaylist::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Playlist Name: %s\n", m_playlistName.c_str());
}

/*
 *
 */
Json::Value PlaylistEntryPlaylist::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();
	Json::Value playlistConfig = m_playlist->GetConfig();

	MergeJsonValues(result, playlistConfig);
	
	return result;
}

