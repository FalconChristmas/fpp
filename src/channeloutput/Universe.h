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

typedef struct {
    uint32_t active;
    uint32_t universe;
    uint32_t size;
    uint32_t startAddress;
    uint32_t type;
    char unicastAddress[16];
    uint32_t bytesReceived;
    uint32_t packetsReceived;
    uint32_t errorPackets;
    uint32_t lastSequenceNumber;
    uint32_t priority;
} UniverseEntry;
