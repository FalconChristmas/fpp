/*
 *   Playlist Entry Brightness Class for Falcon Player (FPP)
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

#include "channeloutput.h"
#include "log.h"
#include "PlaylistEntryBrightness.h"
//#include "Player.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryBrightness::PlaylistEntryBrightness(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_brightness(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBrightness::PlaylistEntryBrightness()\n");

	m_type = "brightness";
}

/*
 *
 */
PlaylistEntryBrightness::~PlaylistEntryBrightness()
{
}

/*
 *
 */
int PlaylistEntryBrightness::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBrightness::Init()\n");

	m_brightness = config["brightness"].asInt();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryBrightness::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryBrightness::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	//player->SetBrightness(m_brightness);

	PlaylistEntryBase::StartPlaying();

	FinishPlay();

	return 1;
}

/*
 *
 */
void PlaylistEntryBrightness::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Brightness: %d\n", m_brightness);
}

/*
 *
 */
Json::Value PlaylistEntryBrightness::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["brightness"] = m_brightness;

	return result;
}
