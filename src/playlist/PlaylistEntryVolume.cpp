/*
 *   Playlist Entry Volume Class for Falcon Player (FPP)
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
#include "PlaylistEntryVolume.h"
#include "settings.h"
#include "mediaoutput/mediaoutput.h"

/*
 *
 */
PlaylistEntryVolume::PlaylistEntryVolume(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_volume(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryVolume::PlaylistEntryVolume()\n");

	m_type = "volume";
	m_deprecated = 1;
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

    std::string vol = config["volume"].asString();
    m_volAdjust = ((vol[0] == '-') || (vol[0] == '+'));
    m_volume = std::atoi(vol.c_str());


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


    if (!m_volAdjust) {
        setVolume(m_volume);
    } else {
        int vol = getVolume();
        vol += m_volume;
        if (vol  < 0) {
            vol = 0;
        } else if (vol > 100) {
            vol = 100;
        }
        setVolume(vol);
    }

	PlaylistEntryBase::StartPlaying();

	FinishPlay();

	return 1;
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
