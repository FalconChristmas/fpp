#pragma once
/*
 * This file is part of the Falcon Player (FPP) and is Copyright (C)
 * 2013-2022 by the Falcon Player Developers.
 *
 * The Falcon Player (FPP) is free software, and is covered under
 * multiple Open Source licenses.  Please see the included 'LICENSES'
 * file for descriptions of what files are covered by each license.
 *
 * This source file is covered under the GPL v2 as described in the
 * included LICENSE.GPL file.
 */

#define E131_TYPE_MULTICAST 0
#define E131_TYPE_UNICAST 1
#define ARTNET_TYPE_BROADCAST 2
#define ARTNET_TYPE_UNICAST 3
#define DMX_TYPE 4

class UniverseEntry {
public:
    uint32_t active = 0;
    uint32_t universe = -1;
    uint32_t size = 512;
    uint32_t startAddress = 0;
    uint32_t type = 0;
    char unicastAddress[16];
    uint32_t bytesReceived = 0;
    uint32_t packetsReceived = 0;
    uint32_t errorPackets = 0;
    uint32_t lastSequenceNumber = 0;
    uint32_t priority = 0;

    std::string dmxDevice;
    uint64_t lastTimestamp = 0;
    uint32_t lastIndex = 0;
};
