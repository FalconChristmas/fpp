/*
 *   Playlist Entry Pixel Overlay Class for Falcon Player (FPP)
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
#include "PixelOverlay.h"
#include "PlaylistEntryPixelOverlay.h"
#include "Sequence.h"
#include "settings.h"

/*
 *
 */
PlaylistEntryPixelOverlay::PlaylistEntryPixelOverlay(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_action("NoOp"),
	m_modelName("None Specified"),
	m_startChannel(1),
	m_endChannel(FPPD_MAX_CHANNELS),
	m_value(0)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPixelOverlay::PlaylistEntryPixelOverlay()\n");

	m_type = "pixelOverlay";
}

/*
 *
 */
PlaylistEntryPixelOverlay::~PlaylistEntryPixelOverlay()
{
}

/*
 *
 */
int PlaylistEntryPixelOverlay::Init(Json::Value &config)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPixelOverlay::Init()\n");

	if (!config.isMember("action"))
	{
		LogErr(VB_PLAYLIST, "No Action specified\n");
		return 0;
	}

	if (!config.isMember("modelName"))
	{
		LogErr(VB_PLAYLIST, "No ModelName specified\n");
		return 0;
	}

	m_action = config["action"].asString();
	m_modelName = config["modelName"].asString();
	m_startChannel = config["startChannel"].asInt();
	m_endChannel = config["endChannel"].asInt();
	m_value = (char)config["value"].asInt();

	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryPixelOverlay::StartPlaying(void)
{
	LogDebug(VB_PLAYLIST, "PlaylistEntryPixelOverlay::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

    PixelOverlayModel *model = PixelOverlayManager::INSTANCE.getModel(m_modelName);
    if (model == nullptr) {
        return 0;
    }

	if ((m_action == "Disabled") ||
		(m_action == "Enabled") ||
		(m_action == "Transparent") ||
        (m_action == "TransparentRGB")) {
        model->setState(m_action);
        return 1;
    } else if (m_action == "Value") {
        model->setValue(m_value, m_startChannel, m_endChannel);
        return 1;
    }
	return 0;
}

/*
 *
 */
int PlaylistEntryPixelOverlay::Process(void)
{
	FinishPlay();

	return PlaylistEntryBase::Process();
}

/*
 *
 */
void PlaylistEntryPixelOverlay::Dump(void)
{
	LogDebug(VB_PLAYLIST, "Model Name: %s\n", m_modelName.c_str());
	LogDebug(VB_PLAYLIST, "Action    : %s\n", m_action.c_str());

	if (m_action == "Value")
	{
		LogDebug(VB_PLAYLIST, "Start Ch. : %d\n", m_startChannel);
		LogDebug(VB_PLAYLIST, "End Ch.   : %d\n", m_endChannel);
		LogDebug(VB_PLAYLIST, "Value     : %d\n", m_value);
	}
}

/*
 *
 */
Json::Value PlaylistEntryPixelOverlay::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["modelName"]    = m_modelName;
	result["action"]       = m_action;
	result["startChannel"] = m_startChannel;
	result["endChannel"]   = m_endChannel;
	result["value"]        = m_value;

	return result;
}
