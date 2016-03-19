/*
 *   Playlist Entry Volume Class for Falcon Player (FPP)
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

#include "log.h"
#include "PlaylistEntryVolume.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryVolume::PlaylistEntryVolume()
  : m_volume(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::PlaylistEntryVolume()\n");

	m_type = "volume";
}

/*
 *
 */
PlaylistEntryVolume::~PlaylistEntryVolume()
{
}

/*
 *
 */
int PlaylistEntryVolume::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::Init()\n");

	m_volume = config["volume"].asInt();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryVolume::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	setVolume(m_volume);

	return PlaylistEntryBase::StartPlaying();;
}

/*
 *
 */
void PlaylistEntryVolume::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Volume: %d\n", m_volume);
}

/*
 *
 */
Json::Value PlaylistEntryVolume::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["volume"] = m_volume;

	return result;
}
