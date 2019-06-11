/*
 *   Playlist Entry Remap Class for Falcon Player (FPP)
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

#include <stdlib.h>
#include <string.h>

#include "channeloutput/channeloutput.h"
#include "channeloutput/processors/RemapOutputProcessor.h"
#include "log.h"
#include "PlaylistEntryRemap.h"

/*
 *
 */
PlaylistEntryRemap::PlaylistEntryRemap(PlaylistEntryBase *parent)
  : PlaylistEntryBase(parent),
	m_srcChannel(0),
	m_dstChannel(0),
	m_channelCount(0),
	m_loops(0),
	m_reverse(0),
    m_processor(nullptr)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryRemap::PlaylistEntryRemap()\n");

	m_type = "remap";
}

/*
 *
 */
PlaylistEntryRemap::~PlaylistEntryRemap()
{
    if (m_processor) {
        delete m_processor;
    }
}

/*
 *
 */
int PlaylistEntryRemap::Init(Json::Value &config)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryRemap::Init()\n");

	if (!config.isMember("action"))
	{
		LogErr(VB_PLAYLIST, "Missing action entry\n");
		return 0;
	}

	if (!config.isMember("source"))
	{
		LogErr(VB_PLAYLIST, "Missing source entry\n");
		return 0;
	}

	if (!config.isMember("destination"))
	{
		LogErr(VB_PLAYLIST, "Missing destination entry\n");
		return 0;
	}

	if (!config.isMember("count"))
	{
		LogErr(VB_PLAYLIST, "Missing count entry\n");
		return 0;
	}

	if (!config.isMember("loops"))
	{
		LogErr(VB_PLAYLIST, "Missing loops entry\n");
		return 0;
	}

	m_action       = config["action"].asString();
	m_srcChannel   = config["source"].asInt();
	m_dstChannel   = config["destination"].asInt();
	m_channelCount = config["count"].asInt();
	m_loops        = config["loops"].asInt();
	m_reverse      = config["reverse"].asInt();
    
    m_processor = new RemapOutputProcessor(m_srcChannel, m_dstChannel, m_channelCount, m_loops, m_reverse);
	return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryRemap::StartPlaying(void)
{
    LogDebug(VB_PLAYLIST, "PlaylistEntryRemap::StartPlaying()\n");

	if (!CanPlay())
	{
		FinishPlay();
		return 0;
	}

	PlaylistEntryBase::StartPlaying();

    if (m_action == "add") {
        outputProcessors.addProcessor(m_processor);
    } else if (m_action == "remove") {
        auto f = [this] (OutputProcessor*p2) -> bool {
            if (p2->getType() == OutputProcessor::REMAP) {
                RemapOutputProcessor *rp = (RemapOutputProcessor*)p2;
                return rp->getSourceChannel() == this->m_srcChannel
                && rp->getDestChannel() == this->m_dstChannel
                && rp->getLoops() == this->m_loops
                && rp->getCount() == this->m_channelCount
                && rp->getReverse() == this->m_reverse;
            }
            return false;
        };
        OutputProcessor * p = outputProcessors.find(f);
        if (p) {
            outputProcessors.removeProcessor(p);
        }
    }
	FinishPlay();
	return 1;
}

/*
 *
 */
void PlaylistEntryRemap::Dump(void)
{
	PlaylistEntryBase::Dump();

	LogDebug(VB_PLAYLIST, "Source Channel     : %d\n", m_srcChannel);
	LogDebug(VB_PLAYLIST, "Destination Channel: %d\n", m_dstChannel);
	LogDebug(VB_PLAYLIST, "Channel Count      : %d\n", m_channelCount);
	LogDebug(VB_PLAYLIST, "Loop Count         : %d\n", m_loops);
}

/*
 *
 */
Json::Value PlaylistEntryRemap::GetConfig(void)
{
	Json::Value result = PlaylistEntryBase::GetConfig();

	result["source"]   = m_srcChannel;
	result["destination"] = m_dstChannel;
	result["count"] = m_channelCount;
	result["loops"] = m_loops;

	return result;
}

