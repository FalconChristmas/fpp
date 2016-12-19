/*
 *   PlaylistEntryURL Class for Falcon Player (FPP)
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
#include "PlaylistEntryURL.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryURL::PlaylistEntryURL()
  : m_url(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::PlaylistEntryURL()\n");

	m_type = "url";
}

/*
 *
 */
PlaylistEntryURL::~PlaylistEntryURL()
{
}

/*
 *
 */
int PlaylistEntryURL::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::Init()\n");

	m_method = config["method"].asString();
	m_url = config["url"].asString();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryURL::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryURL::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	// FIXME PLAYLIST, generate the full URL and call it

	return PlaylistEntryBase::StartPlaying();;
}

/*
 *
 */
void PlaylistEntryURL::Dump(void)
{
	LogDebug(VB_PLAYLIST, "URL     : %d\n", m_method);
	LogDebug(VB_PLAYLIST, "Method  : %d\n", m_url);
}

/*
 *
 */
Json::Value PlaylistEntryURL::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["url"] = m_url;
	result["method"] = m_method;

	return result;
}
