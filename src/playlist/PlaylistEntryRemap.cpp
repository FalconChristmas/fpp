/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the LGPL v2.1 as described in the
 * included LICENSE.LGPL file.
 */

#include "fpp-pch.h"

#include "../log.h"

#include "PlaylistEntryRemap.h"
#include "channeloutput/ChannelOutputSetup.h"
#include "channeloutput/processors/RemapOutputProcessor.h"

/*
 *
 */
PlaylistEntryRemap::PlaylistEntryRemap(Playlist* playlist, PlaylistEntryBase* parent) :
    PlaylistEntryBase(playlist, parent),
    m_srcChannel(0),
    m_dstChannel(0),
    m_channelCount(0),
    m_loops(0),
    m_reverse(0) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryRemap::PlaylistEntryRemap()\n");

    m_type = "remap";
}

/*
 *
 */
PlaylistEntryRemap::~PlaylistEntryRemap() {
}

/*
 *
 */
int PlaylistEntryRemap::Init(Json::Value& config) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryRemap::Init()\n");

    if (!config.isMember("action")) {
        LogErr(VB_PLAYLIST, "Missing action entry\n");
        return 0;
    }

    if (!config.isMember("source")) {
        LogErr(VB_PLAYLIST, "Missing source entry\n");
        return 0;
    }

    if (!config.isMember("destination")) {
        LogErr(VB_PLAYLIST, "Missing destination entry\n");
        return 0;
    }

    if (!config.isMember("count")) {
        LogErr(VB_PLAYLIST, "Missing count entry\n");
        return 0;
    }

    if (!config.isMember("loops")) {
        LogErr(VB_PLAYLIST, "Missing loops entry\n");
        return 0;
    }

    m_action = config["action"].asString();
    m_srcChannel = config["source"].asInt();
    m_dstChannel = config["destination"].asInt();
    m_channelCount = config["count"].asInt();
    m_loops = config["loops"].asInt();
    m_reverse = config["reverse"].asInt();

    // need to be 0 based
    if (m_srcChannel) {
        m_srcChannel--;
    }
    if (m_dstChannel) {
        m_dstChannel--;
    }

    return PlaylistEntryBase::Init(config);
}

/*
 *
 */
int PlaylistEntryRemap::StartPlaying(void) {
    LogDebug(VB_PLAYLIST, "PlaylistEntryRemap::StartPlaying()\n");

    if (!CanPlay()) {
        FinishPlay();
        return 0;
    }

    PlaylistEntryBase::StartPlaying();

    if (m_action == "add") {
        RemapOutputProcessor* processor = new RemapOutputProcessor(m_srcChannel, m_dstChannel, m_channelCount, m_loops, m_reverse);
        outputProcessors.addProcessor(processor);
    } else if (m_action == "remove") {
        auto f = [this](OutputProcessor* p2) -> bool {
            if (p2->getType() == OutputProcessor::REMAP) {
                RemapOutputProcessor* rp = (RemapOutputProcessor*)p2;
                return rp->getSourceChannel() == this->m_srcChannel && rp->getDestChannel() == this->m_dstChannel && rp->getLoops() == this->m_loops && rp->getCount() == this->m_channelCount && rp->getReverse() == this->m_reverse;
            }
            return false;
        };
        OutputProcessor* p = outputProcessors.find(f);
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
void PlaylistEntryRemap::Dump(void) {
    PlaylistEntryBase::Dump();

    LogDebug(VB_PLAYLIST, "Source Channel     : %d\n", m_srcChannel);
    LogDebug(VB_PLAYLIST, "Destination Channel: %d\n", m_dstChannel);
    LogDebug(VB_PLAYLIST, "Channel Count      : %d\n", m_channelCount);
    LogDebug(VB_PLAYLIST, "Loop Count         : %d\n", m_loops);
}

/*
 *
 */
Json::Value PlaylistEntryRemap::GetConfig(void) {
    Json::Value result = PlaylistEntryBase::GetConfig();

    result["source"] = m_srcChannel + 1;
    result["destination"] = m_dstChannel + 1;
    result["count"] = m_channelCount;
    result["loops"] = m_loops;

    return result;
}
